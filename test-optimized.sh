#!/bin/bash
#
# 白盒网元优化版测试脚本
# 测试优化后的 Docker 镜像
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "======================================"
echo "白盒网元优化版测试脚本"
echo "======================================"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 测试镜像名称
IMAGE_NAME="${1:-whitebox-ne:optimized}"
CONTAINER_NAME="whitebox-test-$$"

# 检查镜像是否存在
if ! docker images | grep -q "$IMAGE_NAME"; then
    echo -e "${RED}错误: 镜像 $IMAGE_NAME 不存在${NC}"
    echo "请先运行: ./build-optimized.sh"
    exit 1
fi

echo -e "${GREEN}✓ 镜像已找到: $IMAGE_NAME${NC}"
echo ""

# 测试结果记录
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# 测试函数
run_test() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo -n "[$TOTAL_TESTS] 测试: $test_name ... "
    
    if eval "$test_command" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ 通过${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        echo -e "${RED}✗ 失败${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

# 清理函数
cleanup() {
    echo ""
    echo "清理测试环境..."
    docker rm -f "$CONTAINER_NAME" 2>/dev/null || true
}

# 设置清理钩子
trap cleanup EXIT

# ============================================
# 启动容器
# ============================================

echo "启动测试容器..."
echo ""

if docker run -d \
  --name "$CONTAINER_NAME" \
  --privileged \
  --cap-add=NET_ADMIN,NET_RAW,SYS_ADMIN \
  --tmpfs /tmp \
  --tmpfs /run \
  -p 9091:9090 \
  "$IMAGE_NAME" > /dev/null 2>&1; then
    
    echo -e "${GREEN}✓ 容器启动成功${NC}"
else
    echo -e "${RED}✗ 容器启动失败${NC}"
    exit 1
fi

# 等待容器就绪
echo "等待容器就绪..."
sleep 15
echo ""

# ============================================
# 运行测试
# ============================================

echo "======================================"
echo "开始测试"
echo "======================================"
echo ""

# 测试 1: 容器运行状态
run_test "容器运行状态" \
  "docker ps | grep -q $CONTAINER_NAME"

# 测试 2: FRR 服务
run_test "FRR Zebra 守护进程" \
  "docker exec $CONTAINER_NAME pgrep -x zebra"

run_test "FRR BGP 守护进程" \
  "docker exec $CONTAINER_NAME pgrep -x bgpd"

run_test "FRR OSPF 守护进程" \
  "docker exec $CONTAINER_NAME pgrep -x ospfd"

# 测试 3: SNMP 服务
run_test "SNMP 守护进程" \
  "docker exec $CONTAINER_NAME pgrep -x snmpd"

# 测试 4: vtysh 命令行
run_test "vtysh 可用" \
  "docker exec $CONTAINER_NAME vtysh -c 'show version'"

# 测试 5: 路由表
run_test "路由表可查询" \
  "docker exec $CONTAINER_NAME vtysh -c 'show ip route'"

# 测试 6: BGP 配置
run_test "BGP 配置可用" \
  "docker exec $CONTAINER_NAME vtysh -c 'show ip bgp summary'"

# 测试 7: OSPF 配置
run_test "OSPF 配置可用" \
  "docker exec $CONTAINER_NAME vtysh -c 'show ip ospf neighbor'"

# 测试 8: Prometheus Exporter
run_test "Prometheus Exporter 运行" \
  "docker exec $CONTAINER_NAME pgrep -f prometheus_exporter"

# 测试 9: Prometheus 指标端点
run_test "Prometheus /metrics 端点" \
  "curl -s http://localhost:9091/metrics | grep -q frr"

# 测试 10: 健康检查端点
run_test "Prometheus /health 端点" \
  "curl -s http://localhost:9091/health | grep -q healthy"

# 测试 11: Web 服务 (如果启用)
if [ "${ENABLE_WEB:-true}" = "true" ]; then
    run_test "Web 管理界面运行" \
      "docker exec $CONTAINER_NAME pgrep -f 'python3 app.py'"
fi

# 测试 12: IPv4 转发
run_test "IPv4 转发已启用" \
  "docker exec $CONTAINER_NAME sysctl net.ipv4.ip_forward | grep -q 1"

# 测试 13: IPv6 转发
run_test "IPv6 转发已启用" \
  "docker exec $CONTAINER_NAME sysctl net.ipv6.conf.all.forwarding | grep -q 1"

# 测试 14: SNMP 查询
run_test "SNMP 系统信息查询" \
  "docker exec $CONTAINER_NAME snmpwalk -v2c -c public localhost .1.3.6.1.2.1.1 | grep -q STRING"

# 测试 15: BGP 邻居指标
run_test "BGP 邻居 Prometheus 指标" \
  "curl -s http://localhost:9091/metrics | grep -q frr_bgp_neighbor"

# 测试 16: 路由表指标
run_test "路由表 Prometheus 指标" \
  "curl -s http://localhost:9091/metrics | grep -q frr_route_count"

# 测试 17: 接口指标
run_test "接口 Prometheus 指标" \
  "curl -s http://localhost:9091/metrics | grep -q frr_interface"

# 测试 18: 系统指标
run_test "系统资源 Prometheus 指标" \
  "curl -s http://localhost:9091/metrics | grep -q frr_cpu_usage"

# 测试 19: 守护进程指标
run_test "守护进程 Prometheus 指标" \
  "curl -s http://localhost:9091/metrics | grep -q frr_daemon_uptime"

# 测试 20: 日志目录
run_test "日志目录存在" \
  "docker exec $CONTAINER_NAME ls -d /var/log/frr"

echo ""
echo "======================================"
echo "测试结果汇总"
echo "======================================"
echo ""

echo "总测试数: $TOTAL_TESTS"
echo -e "${GREEN}通过: $PASSED_TESTS${NC}"
echo -e "${RED}失败: $FAILED_TESTS${NC}"
echo ""

# 计算通过率
if [ $TOTAL_TESTS -gt 0 ]; then
    PASS_RATE=$((PASSED_TESTS * 100 / TOTAL_TESTS))
    echo "通过率: ${PASS_RATE}%"
    echo ""
fi

# 显示一些示例输出
echo "======================================"
echo "示例输出"
echo "======================================"
echo ""

echo "FRR 版本:"
docker exec "$CONTAINER_NAME" vtysh -c 'show version' 2>/dev/null | head -5
echo ""

echo "系统信息:"
docker exec "$CONTAINER_NAME" uname -a 2>/dev/null
echo ""

echo "Prometheus 指标 (前 10 行):"
curl -s http://localhost:9091/metrics 2>/dev/null | head -10
echo ""

# 最终判断
if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}======================================${NC}"
    echo -e "${GREEN}✓ 所有测试通过！${NC}"
    echo -e "${GREEN}======================================${NC}"
    exit 0
else
    echo -e "${RED}======================================${NC}"
    echo -e "${RED}✗ 有 $FAILED_TESTS 个测试失败${NC}"
    echo -e "${RED}======================================${NC}"
    exit 1
fi
