# 允许 vtysh 远程访问的配置

# 1. 修改 /etc/frr/daemons 文件，将监听地址改为 0.0.0.0
# zebra_options="-M snmp -A 0.0.0.0 -s 90000000"
# bgpd_options="-M snmp -A 0.0.0.0"
# ospfd_options="-M snmp -A 0.0.0.1"

# 2. 重启 FRR 服务
# sudo systemctl restart frr

# 3. 允许防火墙通过（如果启用了防火墙）
# sudo ufw allow 2601/tcp  # vtysh 端口
# sudo ufw allow 179/tcp   # BGP 端口
# sudo ufw allow 89/tcp    # OSPF 端口

# 4. 远程连接
# telnet 服务器IP 2601
