#!/bin/bash
#
# 白盒网元 Docker 启动脚本 (优化版)
# 借鉴 VyOS 最佳实践
#
# 优化内容:
# 1. systemd 服务管理
# 2. 精细化初始化
# 3. 增强的健康检查
# 4. 自动服务恢复
# 5. 更好的日志管理

set -e

echo "======================================"
echo "白盒网元 (WhiteBox NE) - Docker 启动"
echo "版本: 2.0.0-optimized"
echo "======================================"

# 打印系统信息
echo "系统信息:"
echo "  主机名: $(hostname)"
echo "  内核: $(uname -r)"
echo "  架构: $(uname -m)"
echo "  时间: $(date '+%Y-%m-%d %H:%M:%S %Z')"
echo "  容器: ${container:-unknown}"
echo ""

# ============================================
# 网络配置
# ============================================

echo "配置网络..."

# 启用 IPv4 和 IPv6 转发
sysctl -w net.ipv4.ip_forward=1 2>/dev/null || true
sysctl -w net.ipv6.conf.all.forwarding=1 2>/dev/null || true
sysctl -w net.ipv6.conf.default.forwarding=1 2>/dev/null || true
sysctl -w net.ipv4.conf.all.accept_source_route=0 2>/dev/null || true

# 禁用 ICMP 重定向
sysctl -w net.ipv4.conf.all.accept_redirects=0 2>/dev/null || true
sysctl -w net.ipv4.conf.default.accept_redirects=0 2>/dev/null || true

# 启用 TCP BBR 拥塞控制
sysctl -w net.core.default_qdisc=fq_codel 2>/dev/null || true
sysctl -w net.ipv4.tcp_congestion_control=bbr 2>/dev/null || true

echo "✅ 网络配置完成"
echo ""

# ============================================
# 接口配置
# ============================================

echo "检查网络接口:"

# 显示所有接口
ip link show 2>/dev/null || echo "  警告: 无法获取接口列表"
echo ""

# 自动配置接口 (如果需要)
configure_interface() {
    local interface=$1
    local ip=$2
    
    if ip link show $interface &>/dev/null; then
        if ! ip addr show $interface | grep -q "inet "; then
            echo "  配置 $interface: $ip"
            ip addr add $ip dev $interface 2>/dev/null || echo "  警告: 配置 $interface 失败"
            ip link set $interface up 2>/dev/null || echo "  警告: 启动 $interface 失败"
        else
            echo "  $interface 已配置"
        fi
    else
        echo "  警告: 接口 $interface 不存在"
    fi
}

# 配置默认接口 (如果存在且未配置)
if [ "${AUTO_CONFIG_INTERFACES:-true}" = "true" ]; then
    configure_interface "eth0" "192.168.10.1/24"
    configure_interface "eth1" "192.168.20.1/24"
    configure_interface "eth2" "192.168.30.1/24"
fi

echo ""

# ============================================
# 服务管理
# ============================================

echo "初始化服务..."

# 检测是否使用 systemd
if [ -f /usr/lib/systemd/systemd ]; then
    SYSTEMD_AVAILABLE=true
    echo "✅ systemd 可用"
else
    SYSTEMD_AVAILABLE=false
    echo "⚠️  systemd 不可用，使用传统方式启动服务"
fi
echo ""

# ============================================
# 启动 SNMP 服务
# ============================================

echo "启动 SNMP 服务..."

# 配置 SNMP 监听地址
if ! grep -q "agentaddress" /etc/snmp/snmpd.conf 2>/dev/null; then
    echo "agentaddress 0.0.0.0" >> /etc/snmp/snmpd.conf
fi

# 确保 AgentX 主模式启用
if ! grep -q "master agentx" /etc/snmp/snmpd.conf 2>/dev/null; then
    echo "master agentx" >> /etc/snmp/snmpd.conf
fi

# 确保 rocommunity 配置存在
if ! grep -q "rocommunity public" /etc/snmp/snmpd.conf 2>/dev/null; then
    echo "rocommunity public" >> /etc/snmp/snmpd.conf
fi

# 启动 SNMP 服务
if [ "$SYSTEMD_AVAILABLE" = true ]; then
    # 使用 systemd 启动
    systemctl enable snmpd 2>/dev/null || true
    systemctl start snmpd 2>/dev/null || \
        /usr/sbin/snmpd -LS0-6d -Lf /dev/null -p /run/snmpd.pid 2>/dev/null &
else
    # 直接启动
    /usr/sbin/snmpd -LS0-6d -Lf /dev/null -p /run/snmpd.pid 2>/dev/null &
fi

# 等待 SNMP 完全启动
for i in {1..10}; do
    if pgrep -x snmpd >/dev/null; then
        echo "✅ SNMP 服务启动成功"
        break
    else
        echo "  等待 SNMP 启动... ($i/10)"
        sleep 1
    fi
done

echo ""

# ============================================
# 启动 FRR 服务
# ============================================

echo "启动 FRR 服务..."

# 确保配置文件权限正确
chown frr:frr /etc/frr/frr.conf 2>/dev/null || true
chmod 640 /etc/frr/frr.conf 2>/dev/null || true
chown frr:frr /etc/frr/daemons 2>/dev/null || true
chmod 640 /etc/frr/daemons 2>/dev/null || true

# 启动 FRR 服务
if [ "$systemD_AVAILABLE" = true ]; then
    # 使用 systemd 启动
    systemctl enable frr 2>/dev/null || true
    systemctl start frr 2>/dev/null || service frr start
else
    # 使用传统方式启动
    service frr start 2>/dev/null || \
        /usr/lib/frr/frr start 2>/dev/null
fi

# 等待 FRR 完全启动
for i in {1..15}; do
    if pgrep -x zebra >/dev/null; then
        echo "✅ FRR 服务启动成功"
        break
    else
        echo "  等待 FRR 启动... ($i/15)"
        sleep 1
    fi
done

echo ""

# ============================================
# 启动 Web 管理界面
# ============================================

WEB_PID=""

if [ "${ENABLE_WEB:-true}" = "true" ]; then
    echo "启动 Web 管理界面..."
    
    # 安装 Python 依赖 (如果需要)
    if [ -f /opt/whitebox-web/requirements.txt ]; then
        pip3 install -q -r /opt/whitebox-web/requirements.txt 2>/dev/null || true
    fi
    
    # 启动 Web 服务
    if [ -f /opt/whitebox-web/app.py ]; then
        cd /opt/whitebox-web
        nohup python3 app.py > /var/log/whitebox-web.log 2>&1 &
        WEB_PID=$!
        
        # 等待 Web 服务启动
        for i in {1..5}; do
            if kill -0 $WEB_PID 2>/dev/null; then
                echo "✅ Web 管理界面启动成功 (PID: $WEB_PID, 端口: ${WEB_PORT:-8080})"
                break
            else
                echo "  等待 Web 启动... ($i/5)"
                sleep 1
            fi
        done
    else
        echo "⚠️  Web 应用未找到，跳过启动"
    fi
else
    echo "Web 管理界面已禁用 (ENABLE_WEB=false)"
fi

echo ""

# ============================================
# 启动 Prometheus Exporter
# ============================================

PROMETHEUS_PID=""

if [ "${ENABLE_PROMETHEUS:-true}" = "true" ]; then
    echo "启动 Prometheus Exporter..."
    
    if [ -f /opt/whitebox-monitoring/prometheus_exporter.py ]; then
        nohup python3 /opt/whitebox-monitoring/prometheus_exporter.py \
            --port ${PROMETHEUS_PORT:-9090} \
            > /var/log/prometheus-exporter.log 2>&1 &
        PROMETHEUS_PID=$!
        echo "✅ Prometheus Exporter 启动成功 (PID: $PROMETHEUS_PID, 端口: ${PROMETHEUS_PORT:-9090})"
    else
        echo "⚠️  Prometheus Exporter 未找到，跳过启动"
    fi
else
    echo "Prometheus Exporter 已禁用 (ENABLE_PROMETHEUS=false)"
fi

echo ""

# ============================================
# 启动 gNMI Server
# ============================================

GNMI_PID=""

if [ "${ENABLE_GNMI:-false}" = "true" ]; then
    echo "启动 gNMI Server..."
    
    if [ -f /opt/whitebox-gnmi/gnmi_server ]; then
        nohup /opt/whitebox-gnmi/gnmi_server \
            --port ${GNMI_PORT:-9339} \
            > /var/log/gnmi-server.log 2>&1 &
        GNMI_PID=$!
        echo "✅ gNMI Server 启动成功 (PID: $GNMI_PID, 端口: ${GNMI_PORT:-9339})"
    else
        echo "⚠️  gNMI Server 未找到，跳过启动"
    fi
else
    echo "gNMI Server 已禁用 (ENABLE_GNMI=false)"
fi

echo ""

# ============================================
# 健康检查
# ============================================

echo "======================================"
echo "服务状态检查"
echo "======================================"

# 检查 FRR 服务
if pgrep -x zebra >/dev/null; then
    echo "✅ FRR Zebra: 运行中"
else
    echo "❌ FRR Zebra: 未运行"
fi

if pgrep -x bgpd >/dev/null; then
    echo "✅ FRR BGP: 运行中"
else
    echo "⚠️  FRR BGP: 未运行 (如不需要则正常)"
fi

if pgrep -x ospfd >/dev/null; then
    echo "✅ FRR OSPF: 运行中"
else
    echo "⚠️  FRR OSPF: 未运行 (如不需要则正常)"
fi

# 检查 SNMP 服务
if pgrep -x snmpd >/dev/null; then
    echo "✅ SNMP: 运行中"
else
    echo "❌ SNMP: 未运行"
fi

# 检查 Web 服务
if [ -n "$WEB_PID" ] && kill -0 $WEB_PID 2>/dev/null; then
    echo "✅ Web UI: 运行中 (PID: $WEB_PID)"
elif [ "${ENABLE_WEB:-true}" = "true" ]; then
    echo "⚠️  Web UI: 检查失败"
fi

# 检查 Prometheus Exporter
if [ -n "$PROMETHEUS_PID" ] && kill -0 $PROMETHEUS_PID 2>/dev/null; then
    echo "✅ Prometheus: 运行中 (PID: $PROMETHEUS_PID)"
elif [ "${ENABLE_PROMETHEUS:-true}" = "true" ]; then
    echo "⚠️  Prometheus: 检查失败"
fi

# 检查 gNMI Server
if [ -n "$GNMI_PID" ] && kill -0 $GNMI_PID 2>/dev/null; then
    echo "✅ gNMI: 运行中 (PID: $GNMI_PID)"
elif [ "${ENABLE_GNMI:-false}" = "true" ]; then
    echo "⚠️  gNMI: 检查失败"
fi

echo ""

# ============================================
# 显示访问信息
# ============================================

echo "======================================"
echo "访问方式"
echo "======================================"

# 显示接口 IP
echo "接口配置:"
ip addr show 2>/dev/null | grep -E "^[0-9]+:|inet " | sed 's/^/  /' || echo "  暂无接口信息"
echo ""

# 显示服务访问方式
echo "服务访问:"
echo "  Web 管理界面: http://<container_ip>:${WEB_PORT:-8080}"
echo "  Prometheus: http://<container_ip>:${PROMETHEUS_PORT:-9090}/metrics"
echo "  gNMI: <container_ip>:${GNMI_PORT:-9339}"
echo ""

echo "命令行访问:"
echo "  进入 FRR CLI: docker exec -it <container_name> vtysh"
echo "  查看日志: docker exec -it <container_name> cat /var/log/frr/frr.log"
echo "  重启服务: docker exec -it <container_name> service frr restart"
echo ""

echo "SNMP 测试:"
echo "  系统信息: snmpwalk -v2c -c public <container_ip> .1.3.6.1.2.1.1"
echo "  BGP 信息: snmpwalk -v2c -c public <container_ip> .1.3.6.1.2.1.15"
echo ""

# ============================================
# 进入运行模式
# ============================================

# 清理函数
cleanup() {
    echo ""
    echo "======================================"
    echo "收到停止信号，正在清理..."
    echo "======================================"
    
    # 停止所有服务
    [ -n "$GNMI_PID" ] && kill $GNMI_PID 2>/dev/null
    [ -n "$PROMETHEUS_PID" ] && kill $PROMETHEUS_PID 2>/dev/null
    [ -n "$WEB_PID" ] && kill $WEB_PID 2>/dev/null
    
    # 停止 FRR
    service frr stop 2>/dev/null || true
    
    # 停止 SNMP
    pkill -x snmpd 2>/dev/null || true
    
    echo "✅ 所有服务已停止"
    exit 0
}

# 捕获退出信号
trap cleanup SIGTERM SIGINT SIGQUIT

# 检查是否在交互模式
if [ -t 0 ]; then
    echo "======================================"
    echo "进入交互模式..."
    echo "输入 'exit' 退出容器"
    echo "输入 'vtysh' 进入 FRR 命令行"
    echo "======================================"
    echo ""
    exec /bin/bash
else
    echo "======================================"
    echo "容器运行中，等待信号..."
    echo "======================================"
    echo ""
    
    # 监控服务状态
    while true; do
        # 检查关键服务是否运行
        if ! pgrep -x zebra >/dev/null; then
            echo "⚠️  FRR Zebra 已停止，尝试重启..."
            service frr start 2>/dev/null || true
        fi
        
        if ! pgrep -x snmpd >/dev/null; then
            echo "⚠️  SNMP 已停止，尝试重启..."
            /usr/sbin/snmpd -LS0-6d -Lf /dev/null -p /run/snmpd.pid 2>/dev/null &
        fi
        
        sleep 30
    done
fi
