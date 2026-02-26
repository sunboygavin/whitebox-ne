#!/usr/bin/env python3
"""
WhiteBox NE Prometheus Exporter
导出 FRRouting 监控指标到 Prometheus 格式

功能:
- BGP 邻居状态
- 路由表统计
- 接口流量
- 系统资源使用
- 协议守护进程状态

作者: WhiteBox NE Team
版本: 1.0.0
"""

import os
import sys
import time
import subprocess
import re
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn
import psutil

# 配置
DEFAULT_PORT = 9090
METRIC_PREFIX = "frr"

# BGP 邻居指标
BGP_NEIGHBOR_UP = f"{METRIC_PREFIX}_bgp_neighbor_up"
BGP_NEIGHBOR_RECEIVED_ROUTES = f"{METRIC_PREFIX}_bgp_neighbor_received_routes"
BGP_NEIGHBOR_ADVERTISED_ROUTES = f"{METRIC_PREFIX}_bgp_neighbor_advertised_routes"
BGP_NEIGHBOR_STATE = f"{METRIC_PREFIX}_bgp_neighbor_state"

# 路由表指标
ROUTE_COUNT = f"{METRIC_PREFIX}_route_count"

# 接口指标
INTERFACE_BYTES = f"{METRIC_PREFIX}_interface_bytes_total"
INTERFACE_PACKETS = f"{METRIC_PREFIX}_interface_packets_total"
INTERFACE_ERRORS = f"{METRIC_PREFIX}_interface_errors_total"

# 系统指标
SYSTEM_UPTIME = f"{METRIC_PREFIX}_system_uptime_seconds"
MEMORY_USAGE = f"{METRIC_PREFIX}_memory_usage_bytes"
CPU_USAGE = f"{METRIC_PREFIX}_cpu_usage_percent"

# 守护进程指标
DAEMON_UPTIME = f"{METRIC_PREFIX}_daemon_uptime_seconds"


def run_vtysh_command(command):
    """
    执行 vtysh 命令并返回输出
    
    Args:
        command: vtysh 命令
        
    Returns:
        命令输出（字符串）
    """
    try:
        result = subprocess.run(
            ["vtysh", "-c", command],
            capture_output=True,
            text=True,
            timeout=10
        )
        return result.stdout.strip()
    except subprocess.TimeoutExpired:
        return ""
    except Exception as e:
        return f""


def get_bgp_neighbors():
    """
    获取 BGP 邻居信息
    
    Returns:
        BGP 邻居指标列表
    """
    metrics = []
    
    try:
        output = run_vtysh_command("show ip bgp summary")
        
        # 解析 BGP 邻居状态
        lines = output.split('\n')
        for line in lines:
            # 匹配 BGP 邻居行
            # 示例: "192.168.1.2 4 65002 1234 0 0 0 0 0 0 Establ"
            match = re.search(r'(\d+\.\d+\.\d+\.\d+).*?(\d+).*?(\d+).*?(\d+).*?(\w+)', line)
            if match:
                neighbor = match.group(1)
                received = match.group(2)
                advertised = match.group(3)
                uptime = match.group(4)
                state = match.group(5)
                
                # 邻居是否在线 (1=在线, 0=离线)
                is_up = 1 if state.lower() in ['established', 'establ'] else 0
                
                # 邻居状态码
                state_code = {
                    'idle': 0,
                    'connect': 1,
                    'active': 2,
                    'opensent': 3,
                    'openconfirm': 4,
                    'established': 5,
                    'establ': 5
                }.get(state.lower(), 0)
                
                metrics.append(f'# HELP {BGP_NEIGHBOR_UP} BGP neighbor status (1=up, 0=down)')
                metrics.append(f'# TYPE {BGP_NEIGHBOR_UP} gauge')
                metrics.append(f'{BGP_NEIGHBOR_UP}{{neighbor="{neighbor}"}} {is_up}')
                
                metrics.append(f'# HELP { {BGP_NEIGHBOR_RECEIVED_ROUTES}} Number of routes received from BGP neighbor')
                metrics.append(f'# TYPE {BGP_NEIGHBOR_RECEIVED_ROUTES} gauge')
                metrics.append(f'{BGP_NEIGHBOR_RECEIVED_ROUTES}{{neighbor="{neighbor}"}} {received}')
                
                metrics.append(f'# HELP {BGP_NEIGHBOR_ADVERTISED_ROUTES} Number of routes advertised to BGP neighbor')
                metrics.append(f'# TYPE {BGP_NEIGHBOR_ADVERTISED_ROUTES} gauge')
                metrics.append(f'{BGP_NEIGHBOR_ADVERTISED_ROUTES}{{neighbor="{neighbor}"}} {advertised}')
                
                metrics.append(f'# HELP {BGP_NEIGHBOR_STATE} BGP neighbor state code (0=idle, 5=established)')
                metrics.append(f'# TYPE {BGP_NEIGHBOR_STATE} gauge')
                metrics.append(f'{BGP_NEIGHBOR_STATE}{{neighbor="{neighbor}",state="{state}"}} {state_code}')
                
    except Exception as e:
        print(f"Error getting BGP neighbors: {e}", file=sys.stderr)
    
    return metrics


def get_route_counts():
    """
    获取路由表统计
    
    Returns:
        路由表指标列表
    """
    metrics = []
    
    # BGP 路由数量
    output = run_vtysh_command("show ip bgp summary")
    bgp_total = 0
    for line in output.split('\n'):
        # 查找总路由数
        if 'Total number of BGP prefixes' in line or 'Total' in line:
            match = re.search(r'(\d+)', line)
            if match:
                bgp_total = int(match.group(1))
                break
    
    metrics.append(f'# HELP {ROUTE_COUNT} Number of routes in routing table')
    metrics.append(f'# TYPE {ROUTE_COUNT} gauge')
    metrics.append(f'{ROUTE_COUNT}{{protocol="bgp"}} {bgp_total}')
    
    # OSPF 路由数量
    output = run_vtysh_command("show ip ospf route")
    ospf_total = 0
    for line in output.split('\n'):
        if 'N' in line or '>' in line:  # 路由条目标记
            ospf_total += 1
    
    metrics.append(f'{ROUTE_COUNT}{{protocol="ospf"}} {ospf_total}')
    
    # 总路由数 (从内核)
    try:
        result = subprocess.run(
            ["ip", "route", "show"],
            capture_output=True,
            text=True,
            timeout=5
        )
        total_routes = len(result.stdout.strip().split('\n')) if result.stdout.strip() else 0
        metrics.append(f'{ROUTE_COUNT}{{protocol="kernel"}} {total_routes}')
    except:
        metrics.append(f'{ROUTE_COUNT}{{protocol="kernel"}} 0')
    
    return metrics


def get_interface_stats():
    """
    获取接口流量统计
    
    Returns:
        接口指标列表
    """
    metrics = []
    
    try:
        net_io = psutil.net_io_counters(pernic=True)
        
        for interface, counters in net_io.items():
            # 跳过本地回环
            if interface == 'lo':
                continue
            
            metrics.append(f'# HELP {INTERFACE_BYTES} Interface traffic in bytes')
            metrics.append(f'# TYPE {INTERFACE_BYTES} counter')
            metrics.append(f'{INTERFACE_BYTES}{{interface="{interface}",direction="rx"}} {counters.bytes_recv}')
            metrics.append(f'{INTERFACE_BYTES}{{interface="{interface}",direction="tx"}} {counters.bytes_sent}')
            
            metrics.append(f'# HELP {INTERFACE_PACKETS} Interface packet count')
            metrics.append(f'# TYPE {INTERFACE_PACKETS} counter')
            metrics.append(f'{INTERFACE_PACKETS}{{interface="{interface}",direction="rx"}} {counters.packets_recv}')
            metrics.append(f'{INTERFACE_PACKETS}{{interface="{interface}",direction="tx"}} {counters.packets_sent}')
            
            metrics.append(f'# HELP {INTERFACE_ERRORS} Interface error count')
            metrics.append(f'# TYPE {INTERFACE_ERRORS} counter')
            metrics.append(f'{INTERFACE_ERRORS}{{interface="{interface}",direction="rx"}} {counters.errin}')
            metrics.append(f'{INTERFACE_ERRORS}{{interface="{interface}",direction="tx"}} {counters.errout}')
            
    except Exception as e:
        print(f"Error getting interface stats: {e}", file=sys.stderr)
    
    return metrics


def get_system_stats():
    """
    获取系统资源统计
    
    Returns:
        系统指标列表
    """
    metrics = []
    
    try:
        # 系统运行时间
        uptime = psutil.boot_time()
        uptime_seconds = time.time() - uptime
        
        metrics.append(f'# HELP {SYSTEM_UPTIME} System uptime in seconds')
        metrics.append(f'# TYPE {SYSTEM_UPTIME} gauge')
        metrics.append(f'{SYSTEM_UPTIME} {uptime_seconds:.0f}')
        
        # 内存使用
        memory = psutil.virtual_memory()
        
        metrics.append(f'# HELP {MEMORY_USAGE} Memory usage in bytes')
        metrics.append(f'# TYPE {MEMORY_USAGE} gauge')
        metrics.append(f'{MEMORY_USAGE}{{type="used"}} {memory.used}')
        metrics.append(f'{MEMORY_USAGE}{{type="free"}} {memory.available}')
        metrics.append(f'{MEMORY_USAGE}{{type="total"}} {memory.total}')
        
        # CPU 使用率
        cpu_percent = psutil.cpu_percent(interval=0.1)
        
        metrics.append(f'# HELP {CPU_USAGE} CPU usage percentage')
        metrics.append(f'# TYPE {CPU_USAGE} gauge')
        metrics.append(f'{CPU_USAGE} {cpu_percent:.1f}')
        
    except Exception as e:
        print(f"Error getting system stats: {e}", file=sys.stderr)
    
    return metrics


def get_daemon_stats():
    """
    获取 FRR 守护进程状态
    
    Returns:
        守护进程指标列表
    """
    metrics = []
    
    daemons = ['zebra', 'bgpd', 'ospfd', 'vrrpd']
    
    metrics.append(f'# HELP {DAEMON_UPTIME} FRR daemon uptime in seconds')
    metrics.append(f'# TYPE {DAEMON_UPTIME} gauge')
    
    for daemon in daemons:
        try:
            # 检查进程是否存在
            result = subprocess.run(
                ["pgrep", "-x", daemon],
                capture_output=True,
                text=True,
                timeout=2
            )
            
            if result.returncode == 0:
                # 获取进程运行时间
                pid = result.stdout.strip().split('\n')[0]
                p = psutil.Process(int(pid))
                uptime_seconds = time.time() - p.create_time()
                metrics.append(f'{DAEMON_UPTIME}{{daemon="{daemon}"}} {uptime_seconds:.0f}')
            else:
                metrics.append(f'{DAEMON_UPTIME}{{daemon="{daemon}"}} 0')
                
        except Exception as:
            metrics.append(f'{DAEMON_UPTIME}{{daemon="{daemon}"}} 0')
    
    return metrics


class PrometheusExporterHandler(BaseHTTPRequestHandler):
    """
    Prometheus HTTP 请求处理器
    """
    
    def do_GET(self):
        """
        处理 GET 请求
        """
        if self.path == '/metrics':
            self.send_metrics()
        elif self.path == '/health':
            self.send_health()
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'Not Found')
    
    def send_metrics(self):
        """
        发送 Prometheus 指标
        """
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain; version=0.0.4')
        self.end_headers()
        
        metrics = []
        
        # 收集所有指标
        metrics.extend(get_bgp_neighbors())
        metrics.extend(get_route_counts())
        metrics.extend(get_interface_stats())
        metrics.extend(get_system_stats())
        metrics.extend(get_daemon_stats())
        
        # 发送响应
        self.wfile.write('\n'.join(metrics).encode())
    
    def send_health(self):
        """
        发送健康检查响应
        """
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        
        import json
        
        health = {
            "status": "healthy",
            "timestamp": time.time(),
            "metrics_endpoint": "/metrics"
        }
        
        self.wfile.write(json.dumps(health, indent=2).encode())
    
    def log_message(self, format, *args):
        """
        禁用默认日志
        """
        pass


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    """
    多线程 HTTP 服务器
    """
    daemon_threads = True


def main():
    """
    主函数
    """
    import argparse
    
    parser = argparse.ArgumentParser(description='WhiteBox NE Prometheus Exporter')
    parser.add_argument(
        '--port',
        type=int,
        default=DEFAULT_PORT,
        help=f'HTTP server port (default: {DEFAULT_PORT})'
    )
    parser.add_argument(
        '--address',
        type=str,
        default='0.0.0.0',
        help='HTTP server address (default: 0.0.0.0)'
    )
    
    args = parser.parse_args()
    
    # 启动服务器
    server = ThreadedHTTPServer((args.address, args.port), PrometheusExporterHandler)
    
    print(f"WhiteBox NE Prometheus Exporter")
    print(f"Listening on http://{args.address}:{args.port}/metrics")
    print(f"Health check: http://{args.address}:{args.port}/health")
    print(f"Press Ctrl+C to stop")
    print()
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()


if __name__ == '__main__':
    main()
