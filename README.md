# WhiteBox Network Element (NE) Project

这是一个纯命令行的白盒网元实施方案，基于 **FRRouting (FRR)** 构建，支持核心路由协议和标准管理接口。

## 核心功能
- **路由协议**: OSPF, BGP (支持 SRv6, Flowspec), VRRP
- **管理接口**: 
  - **CLI**: 行业标准的 VTYSH 命令行界面
  - **SNMP**: 通过 Net-SNMP AgentX 提供 MIB 支持
  - **Netconf**: 支持 Sysrepo/Netopeer2 集成（详见部署指南）

## 目录结构
- `install_script.sh`: 一键安装脚本（支持 Ubuntu 22.04）
- `frr.conf`: FRR 核心配置文件模板
- `snmpd.conf`: Net-SNMP 配置文件模板
- `netconf_guide.md`: Netconf/YANG 集成与编译指南

## 快速开始
1. 运行 `sudo ./install_script.sh` 安装基础组件。
2. 将 `frr.conf` 拷贝至 `/etc/frr/frr.conf`。
3. 重启服务：`sudo systemctl restart frr snmpd`。
4. 使用 `vtysh` 进入命令行界面。

