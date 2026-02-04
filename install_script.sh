#!/bin/bash
#
# WhiteBox Network Element (NE) Installation Script
#
# This script installs FRRouting (FRR), Net-SNMP, and configures them for a
# pure command-line network element.
#
# Tested on: Ubuntu 22.04 LTS

set -e

echo "--- 1. 更新系统软件包列表 ---"
sudo apt update

echo "--- 2. 安装 FRRouting 和 SNMP 核心组件 ---"
# frr: 核心路由协议栈
# frr-snmp: FRR 的 SNMP MIB 扩展模块
# snmp snmpd: Net-SNMP 代理和客户端工具
sudo apt install -y frr frr-snmp snmp snmpd

echo "--- 3. 配置 FRR 守护进程 ---"
# 停止 FRR 服务以便修改配置
sudo systemctl stop frr

# 启用所需的守护进程：zebra (核心), bgpd (BGP), ospfd (OSPF), vrrpd (VRRP)
sudo sed -i 's/bgpd=no/bgpd=yes/' /etc/frr/daemons
sudo sed -i 's/ospfd=no/ospfd=yes/' /etc/frr/daemons
sudo sed -i 's/vrrpd=no/vrrpd=yes/' /etc/frr/daemons
sudo sed -i 's/zebra=no/zebra=yes/' /etc/frr/daemons

# 为守护进程启用 SNMP 模块加载
# 确保只在第一次运行时添加 -M snmp
if ! grep -q -- "-M snmp" /etc/frr/daemons; then
    sudo sed -i 's/zebra_options="/zebra_options="-M snmp /' /etc/frr/daemons
    sudo sed -i 's/bgpd_options="/bgpd_options="-M snmp /' /etc/frr/daemons
    sudo sed -i 's/ospfd_options="/ospfd_options="-M snmp /' /etc/frr/daemons
fi

echo "--- 4. 配置 Net-SNMP AgentX ---"
# 启用 AgentX 协议，允许 FRR 等子代理注册 MIB
if ! grep -q "master agentx" /etc/snmp/snmpd.conf; then
    echo "master agentx" | sudo tee -a /etc/snmp/snmpd.conf
fi

echo "--- 5. 启动服务 ---"
# 启动 SNMP 服务
sudo systemctl restart snmpd

# 启动 FRR 服务
sudo systemctl restart frr

echo "--- 安装和配置完成 ---"
echo "请将 frr.conf 模板拷贝至 /etc/frr/frr.conf 并再次重启 frr 服务以应用配置。"
echo "  sudo cp frr.conf /etc/frr/frr.conf"
echo "  sudo chown frr:frr /etc/frr/frr.conf"
echo "  sudo systemctl restart frr"
echo "使用 'sudo vtysh' 进入命令行界面。"
