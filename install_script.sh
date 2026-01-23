#!/bin/bash
# 安装 FRR 和 SNMP
sudo apt update
sudo apt install -y frr frr-pythontools snmp snmpd build-essential cmake libyang-dev libssh-dev libpcre3-dev libtool automake pkg-config libjson-c-dev libelf-dev libcap-dev libpam-dev python3-dev python3-pip

# 启用 FRR 守护进程
sudo systemctl stop frr
sudo sed -i 's/ospfd=no/ospfd=yes/' /etc/frr/daemons
sudo sed -i 's/bgpd=no/bgpd=yes/' /etc/frr/daemons
sudo sed -i 's/vrrpd=no/vrrpd=yes/' /etc/frr/daemons
sudo sed -i 's/zebra=no/zebra=yes/' /etc/frr/daemons
sudo systemctl start frr

# 配置 SNMP AgentX
sudo sed -i 's/#master agentx/master agentx/' /etc/snmp/snmpd.conf
sudo systemctl restart snmpd

echo "FRR and SNMP installed and configured."
