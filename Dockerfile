# 白盒网元 Docker 镜像
# 基于 FRRouting + Net-SNMP 构建
# 用于网络路由、BGP、OSPF、VRRP、SRv6、Flowspec 等功能

FROM ubuntu:22.04

# 设置环境变量
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# 安装基础工具
RUN apt-get update && apt-get install -y \
    curl \
    wget \
    vim \
    net-tools \
    iputils-ping \
    tcpdump \
    telnet \
    netcat \
    && rm -rf /var/lib/apt/lists/*

# 安装 FRRouting 和 Net-SNMP
RUN apt-get update && apt-get install -y \
    frr \
    frr-snmp \
    snmp \
    snmpd \
    && rm -rf /var/lib/apt/lists/*

# 配置 FRR 守护进程
RUN sed -i 's/bgpd=no/bgpd=yes/' /etc/frr/daemons && \
    sed -i 's/ospfd=no/ospfd=yes/' /etc/frr/daemons && \
    sed -i 's/vrrpd=no/vrrpd=yes/' /etc/frr/daemons && \
    sed -i 's/zebra=no/zebra=yes/' /etc/frr/daemons

# 为守护进程启用 SNMP 模块
RUN sed -i 's/zebra_options="/zebra_options="-M snmp /' /etc/frr/daemons && \
    sed -i 's/bgpd_options="/bgpd_options="-M snmp /' /etc/frr/daemons && \
    sed -i 's/ospfd_options="/ospfd_options="-M snmp /' /etc/frr/daemons

# 配置 Net-SNMP AgentX
RUN echo "master agentx" >> /etc/snmp/snmpd.conf && \
    echo "rocommunity public" >> /etc/snmp/snmpd.conf

# 创建目录
RUN mkdir -p /var/log/frr /etc/frr

# 复制配置文件
COPY frr.conf /etc/frr/frr.conf

# 设置权限
RUN chown frr:frr /etc/frr/frr.conf && \
    chmod 640 /etc/frr/daemons

# 暴露端口
# SNMP: 161 (UDP)
# BGP: 179 (TCP)
# OSPF: 89 (TCP/UDP)
# VRRP: 112 (VIP, 不需要暴露)
EXPOSE 161/udp 179/tcp 89/tcp

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=10s --retries=3 \
    CMD vtysh -c "show version" > /dev/null 2>&1 || exit 1

# 启动脚本
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

# 设置工作目录
WORKDIR /root/whitebox-ne

# 启动命令
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
