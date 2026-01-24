# WhiteBox Network Element (NE) Project - 详细安装与使用手册

这是一个纯命令行的白盒网元实施方案，基于 **FRRouting (FRR)** 协议栈构建，旨在提供高性能、标准化的路由和管理功能。

## 核心功能概览

| 功能模块 | 核心组件 | 协议支持 | 接口类型 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **路由控制面** | FRRouting (FRR) | OSPF, BGP, VRRP | CLI (VTYSH) | BGP 支持 SRv6 和 Flowspec 配置逻辑 (已集成华为风格别名) |
| **管理接口** | Net-SNMP | SNMPv2c/v3 | SNMP AgentX | 通过 AgentX 扩展 FRR MIB |
| **配置接口** | Sysrepo/Netopeer2 | Netconf/YANG | SSH (Netconf) | 需手动编译安装，详见 `netconf_guide.md` |
| **转发面** | Linux Kernel | IPv4/IPv6 | - | 依赖 Linux 内核转发能力 |

## 1. 环境要求

*   **操作系统**: Ubuntu 22.04 LTS (推荐) 或其他基于 Debian 的发行版。
*   **硬件**: 至少 2 vCPU，4GB RAM。
*   **网络**: 至少两个网络接口（例如 `eth0`, `eth1`）用于模拟路由器端口。

## 2. 快速安装

使用项目根目录下的 `install_script.sh` 脚本可以一键安装 FRR 和 SNMP 基础组件。

### 2.1. 执行安装脚本

```bash
# 切换到项目目录
cd /path/to/whitebox-ne-project

# 赋予执行权限并运行
sudo chmod +x install_script.sh
sudo ./install_script.sh
```

**`install_script.sh` 执行内容：**
1.  更新系统软件包列表。
2.  安装 `frr`, `frr-snmp`, `snmp`, `snmpd` 等核心软件包。
3.  修改 `/etc/frr/daemons` 文件，启用 `zebra`, `bgpd`, `ospfd`, `vrrpd` 守护进程。
4.  修改 `/etc/frr/daemons` 文件，为 `zebra`, `bgpd`, `ospfd` 启用 SNMP 模块加载。
5.  配置 `/etc/snmp/snmpd.conf` 启用 `master agentx`。
6.  重启 `frr` 和 `snmpd` 服务。

### 2.2. 应用 FRR 配置文件

安装完成后，需要应用路由配置。

```bash
# 拷贝配置文件模板
sudo cp frr.conf /etc/frr/frr.conf

# 确保文件权限正确
sudo chown frr:frr /etc/frr/frr.conf

# 重启 FRR 服务以加载新配置
sudo systemctl restart frr
```

## 3. 核心功能验证与使用

### 3.1. 命令行界面 (CLI)

使用 `vtysh` 命令进入 FRR 的集成命令行界面。

```bash
sudo vtysh
```

**常用命令示例：**
| 功能 | 命令 |
| :--- | :--- |
| 查看所有路由协议状态 | `show running-config` |
| 查看 OSPF 邻居 | `show ip ospf neighbor` |
| 查看 BGP 摘要 | `show ip bgp summary` |
| 查看 VRRP 状态 | `show vrrp` |
| 配置模式 | `configure terminal` |

### 3.2. SNMP 管理

SNMP 服务默认监听 `127.0.0.1:161`，社区字符串为 `public`。

```bash
# 验证 SNMP AgentX 是否正常工作（应能获取到系统信息）
snmpwalk -v2c -c public localhost .1.3.6.1.2.1.1.1.0

# 验证 BGP MIB (OID: .1.3.6.1.2.1.15)
# 如果 FRR 模块加载成功，此处应能获取到 BGP 状态信息
snmpwalk -v2c -c public localhost .1.3.6.1.2.1.15
```

### 3.3. Netconf/YANG (高级)

Netconf/YANG 的集成需要手动编译 Sysrepo 和 Netopeer2。详细的编译步骤请参考项目根目录下的 `netconf_guide.md` 文件。

## 4. 配置文件说明

### `frr.conf` 模板关键配置

| 配置项 | 目的 | 备注 |
| :--- | :--- | :--- |
| `interface eth0` | 接口 IP 配置 | 需根据实际环境修改 IP 地址 |
| `vrrp 1 ip 192.168.10.254` | VRRP 虚拟 IP | 模拟高可用网关 |
| `router ospf` | OSPF 路由协议 | 启用 OSPF，配置区域 0 |
| `router bgp 65001` | BGP 路由协议 | 配置 AS 号和邻居 |
| `address-family ipv6 unicast` | BGP SRv6 支持 | 启用 IPv6 地址族，并配置 `send-label` |
| `address-family ipv4 flowspec` | BGP Flowspec 支持 | 启用 Flowspec 地址族 |

### `snmpd.conf` 关键配置

| 配置项 | 目的 | 备注 |
| :--- | :--- | :--- |
| `agentaddress 127.0.0.1,[::1]` | 监听地址 | 默认只监听本地回环，如需远程访问需修改 |
| `master agentx` | 启用 AgentX | 允许 FRR 等子代理注册 MIB |
| `rocommunity public` | 社区字符串 | 默认只读社区 |

## 5. 故障排除

| 问题描述 | 常见原因 | 解决方案 |
| :--- | :--- | :--- |
| FRR 守护进程未启动 | `/etc/frr/daemons` 配置错误 | 检查 `daemons` 文件，确保 `bgpd=yes` 等已正确设置，并重启 `frr` 服务。 |
| `vtysh` 无法进入配置模式 | 配置文件权限错误 | 确保 `/etc/frr/frr.conf` 属于 `frr:frr` 且权限正确。 |
| SNMP 无法获取 FRR MIB | AgentX 连接失败 | 确保 `snmpd` 和 `frr` 服务都已启动，且 `snmpd.conf` 中有 `master agentx`。 |
| BGP/OSPF 邻居无法建立 | 接口 IP 或防火墙问题 | 检查接口 IP 配置是否正确，并确保没有防火墙规则阻挡协议端口（OSPF: 89, BGP: 179）。 |
| SRv6/Flowspec 不工作 | 内核或 FRR 版本不支持 | 确保 Linux 内核版本支持 SRv6，且 FRR 版本在 8.0 以上。 |

## 6. 测试报告

详细的沙盒环境测试报告请参阅 `TEST_REPORT.md`。

### 3.1.1. 华为风格 CLI 使用

安装脚本会自动将 `huawei_cli_alias.conf` 中的别名配置加载到 `vtysh` 中。您现在可以使用常用的华为风格命令：

| 华为命令 (VRP) | 对应 FRR 命令 | 示例 |
| :--- | :--- | :--- |
| `system-view` | `configure terminal` | `system-view` |
| `display ip routing-table` | `show ip route` | `display ip routing-table` |
| `display interface` | `show interface` | `display interface eth0` |
| `display bgp peer` | `show ip bgp summary` | `display bgp peer` |
| `save` | `write` | `save` |
