#!/bin/bash
#
# 白盒网元 Docker 启动脚本
# 用于初始化和启动 FRRouting + Net-SNMP 服务

set -e

echo "======================================"
echo "白盒网元 (WhiteBox NE) - Docker 启动"
echo "======================================"

# 打印系统信息
echo "系统信息:"
echo "  主机名: $(hostname)"
echo "  内核: $(uname -r)"
echo "  时间: $(date)"
echo ""

# 检查网络接口
echo "网络接口:"
ip addr show | grep -E "^[0-9]+:|inet " || echo "  暂无网络接口"
echo ""

# 启用 IPv6 转发（SRv6 需要）
sysctl -w net.ipv6.conf.all.forwarding=1
sysctl -w net.ipv6.conf.default.forwarding=1
echo "IPv已启用转发"
echo ""

# 如果有 eth0 和 eth1，配置基础 IP（可选）
if ip link show eth0 &>/dev/null; then
    if ! ip addr show eth0 | grep -q "inet "; then
        echo "配置 eth0 接口 IP: 192.168.10.1/24"
        ip addr add 192.168.10.1/24 dev dev eth0 || true
        ip link set eth0 up || true
    fi
fi

if ip link show eth1 &>/dev/null; then
    if ! ip addr show eth1 | grep -q "inet "; then
        echo "配置 eth1 接口 IP: 192.168.20.1/24"
        ip addr add 192.168.20.1/24 dev eth1 || true
        ip link set eth1 up || true
    fi
fi

# 启动 SNMP 服务
echo "启动 SNMP 服务..."
service snmpd start

# 等待 SNMP 完全启动
sleep 2

# 启动 FRR 服务
echo "启动 FRR 服务..."
service frr start

# 等待 FRR 完全启动
sleep 3

# 检查服务状态
echo ""
echo "======================================"
echo "服务状态检查"
echo "======================================"

if systemctl is-active --quiet frr; then
    echo "✅ FRR 服务: 运行中"
else
    echo "❌ FRR 服务: 未启动"
    exit 1
fi

if systemctl is-active --quiet snmpd; then
    echo "✅ SNMP 服务: 运行中"
else
    echo "❌ SNMP 服务: 未启动"
fi

echo ""
echo "======================================"
echo "FRR 守护进程状态"
echo "======================================"
vtysh -c "show daemons" 2>/dev/null || echo "无法获取守护进程状态"

echo ""
echo "======================================"
echo "访问方式"
echo "======================================"
echo "命令行界面: docker exec -it <container_name> vtysh"
echo "查看日志: docker exec -it <container_name> cat /var/log/frr/frr.log"
echo "重启服务: docker exec -it <container_name> service frr restart"
echo ""
echo "SNMP 测试: snmpwalk -v2c -c public <container_ip> .1.3.6.1.2.1.1"
echo ""

# 检查是否在交互模式
if [ -t 0 ]; then
    echo "进入交互模式..."
    echo "输入 'exit' 退出容器，输入 'vtysh' 进入 FRR 命令行"
    echo ""
    exec /bin/bash
else
    echo "容器运行中..."
    # 保持容器运行
    tail -f /var/log/frr/frr.log /var/log/syslog 2>/dev/null &
    wait
fi
