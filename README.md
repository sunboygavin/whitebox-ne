# WhiteBox Network Element (NE) Project - 深度开发与部署指南

这是一个**生产级、企业级**的白盒网元实施方案，基于 **FRRouting (FRR)** 协议栈构建，旨在提供高性能、标准化、可观测的路由和管理功能。

**🎉 v3.0 生产就绪版 (2026-06-27)** - 真实内核下发 + 完整功能实现

**v3.0 核心增强**:
- ✅ **ACL 真实下发** — 华为风格 ACL 命令直接同步到 iptables/nftables 内核规则
- ✅ **NAT 真实下发** — EasyIP (MASQUERADE)、NAT Server (DNAT)、Static NAT 均通过 iptables 实际生效
- ✅ **Zone Firewall 真实下发** — 安全区域 + 策略规则通过 iptables FORWARD 链 + ipset 实际生效
- ✅ **QoS 真实下发** — Traffic Classifier + Behavior + Policy 通过 Linux TC (HTB/tbf/u32 filter) 实际生效
- ✅ **Eth-Trunk 真实下发** — 链路聚合通过 Linux bonding 驱动 (802.3ad/LACP/round-robin/active-backup) 实际生效
- ✅ **OpenConfig 适配器** — Sysrepo 数据存储回调 + gNMI 接口存根 + YANG ↔ FRR 双向转换
- ✅ **运行时集成测试** — `./test-runtime-integration.sh` 验证协议、ACL、NAT、QoS、Bonding、Web 全链路
- ✅ **配置持久化** — ACL/NAT/Firewall/QoS/Eth-Trunk 配置支持 save / reload

**v2.0 优化版 (2026-02-26)** - 借鉴 VyOS 和 OpenConfig 最佳实践

**v2.0 特性**:
- ✅ **多阶段构建** - 镜像体积减少 25% (242 MB → 180 MB)
- ✅ **精细化权限管理** - 移除 `--privileged`，使用最小权限原则
- ✅ **Prometheus 监控** - 原生 Prometheus Exporter，完整指标导出
- ✅ **安全加固** - SNMPv3 支持、Netconf TLS、RBAC
- ✅ **结构化日志** - JSON 格式日志，支持 ELK/Loki 集成
- ✅ **systemd 支持** - 更好的服务管理和依赖控制
- ✅ **完整监控栈** - Prometheus + Grafana + Loki (一键部署)

本项目不仅提供了可直接部署的配置和脚本，更包含了**可修改的 FRR 核心 C 语言源代码**，以及详尽的修改逻辑和开发指南，助您实现深度定制。

## 🚀 快速开始

### 方式一：优化版 Docker 部署（推荐）⭐

**v3.0 优化版 - 包含完整监控栈 + 全功能内核下发**

```bash
# 1. 构建优化版镜像
./build-optimized.sh

# 2. 运行完整监控栈 (Prometheus + Grafana + Loki)
docker-compose -f docker-compose.optimized.yml up -d

# 3. 运行全链路运行时测试
./test-runtime-integration.sh

# 4. 进入 FRR 命令行
docker exec -it whitebox-ne-router vtysh

# 5. 访问 Web 管理界面
# http://localhost:8080

# 6. 访问 Grafana 仪表板 (admin/admin)
# http://localhost:3000
```

### 方式二：直接安装

```bash
sudo ./install_script.sh
sudo cp frr.conf /etc/frr/frr.conf
sudo chown frr:frr /etc/frr/frr.conf
sudo systemctl restart frr
sudo vtysh
```

---

## 核心功能概览

| 功能模块 | 核心组件 | 协议支持 | 接口类型 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **路由控制面** | FRRouting (FRR) | OSPF, BGP, IS-IS, RIP, VRRP, BFD | CLI (VTYSH) | 原生协议栈，华为风格命令映射 |
| **二层/接口** | iproute2 + Linux Kernel | VLAN, VLANIF, Eth-Trunk (LACP) | CLI / Netconf | v3.0: 真实 bonding 驱动 / VLAN 子接口创建 |
| **Web 管理界面** | Flask | HTTP/REST API | Web UI | 图形化配置管理，调用 vtysh 获取状态 |
| **管理接口** | Net-SNMP | SNMPv2c/v3 | SNMP AgentX | AgentX 扩展 FRR MIB + 自定义子代理 |
| **配置接口** | Sysrepo/Netopeer2 | Netconf/gNMI/YANG | SSH / gRPC | v3.0: Sysrepo 双向回调 + gNMI 存根 |
| **安全功能** | iptables + ipset | ACL, NAT44, Zone Firewall | CLI / Netconf | v3.0: 真实内核规则下发，配置持久化 |
| **QoS** | Linux TC (HTB/tbf/u32) | Classifier, CAR, Shaping, WRED, WRR | CLI / Netconf | v3.0: 真实 HTB class + filter 下发 |
| **监控** | Prometheus + Grafana + Loki | BGP/OSPF/接口/系统指标 | HTTP / Web | 完整监控栈一键部署 |
| **转发面** | Linux Kernel | IPv4/IPv6 转发 | - | 依赖内核转发能力，支持 DPDK 扩展 |

---

## 📁 项目代码结构

```
whitebox-ne/
├── README.md                       # 项目总览、安装、使用、开发与测试指南
├── DOCKER_DEPLOYMENT.md            # Docker 部署详细指南
├── install_script.sh               # 基础组件一键安装脚本
├── build_from_source.sh            # 从源码构建 FRR 脚本
├── build-docker.sh                 # Docker 镜像构建脚本
├── run-docker.sh                   # Docker 容器运行脚本
├── Dockerfile                      # Docker 镜像定义
├── docker-compose.yml              # Docker Compose 标准版
├── docker-compose.optimized.yml    # Docker Compose 优化版 (监控栈)
├── docker-entrypoint.sh            # Docker 容器启动脚本
├── frr.conf                        # FRR 核心路由配置模板
├── frr.docker.conf                 # Docker 版 FRR 配置
├── test-runtime-integration.sh     # v3.0 运行时集成测试 (全链路验证)
├── src/
│   ├── frr_core/                   # FRR 核心源码改造
│   │   ├── lib/
│   │   │   ├── command.c           # 华为风格 CLI 命令映射
│   │   │   └── huawei_cli.h        # CLI 扩展头文件
│   │   ├── zebra/
│   │   │   ├── srv6.c              # SRv6 处理逻辑
│   │   │   ├── interface_vlan.c    # VLAN 子接口 (真实 iproute2 下发)
│   │   │   └── eth_trunk.c         # Eth-Trunk 链路聚合 (Linux bonding 驱动)
│   │   ├── bgpd/
│   │   │   ├── bgp_flowspec.c      # BGP Flowspec 处理逻辑
│   │   │   └── bgp_huawei.c        # BGP 华为扩展命令
│   │   ├── ospfd/
│   │   │   └── ospf_huawei.c       # OSPF 华为扩展命令
│   │   ├── isisd/
│   │   │   └── isis_huawei.c       # IS-IS 华为扩展命令
│   │   └── ripd/
│   │       └── rip_huawei.c        # RIP 华为扩展命令
│   ├── frr_patch/                  # FRR 源码补丁 (备用)
│   ├── ip_services/                # IP 业务服务 (v3.0 真实下发)
│   │   ├── acl/
│   │   │   └── acl_huawei.c        # ACL (iptables/nftables 真实下发)
│   │   └── nat/
│   │       └── nat44.c             # NAT44 (iptables NAT 表真实下发)
│   ├── security/                   # 安全模块 (v3.0 真实下发)
│   │   ├── auth/
│   │   │   └── aaa.c               # AAA 认证框架
│   │   ├── firewall/
│   │   │   └── zone_firewall.c     # 区域防火墙 (iptables FORWARD + ipset)
│   │   └── vpn/
│   │       └── gre/
│   │           └── gre_tunnel.c    # GRE 隧道
│   ├── qos/                        # QoS 模块 (v3.0 重构，真实 TC 下发)
│   │   └── qos_all_in_one.c        # Classifier + Behavior + Policy + Queue (HTB/tbf/u32)
│   ├── high_availability/          # 高可用模块
│   │   ├── vrrp.c                  # VRRP v2/v3 + 认证 + Track
│   │   ├── bfd.c                   # BFD 双向转发检测
│   │   └── track.c                 # Track 联动
│   ├── openconfig_adapter/         # OpenConfig / gNMI / Netconf (v3.0 新增)
│   │   └── openconfig_adapter.c    # Sysrepo 回调 + gNMI 存根 + YANG↔FRR
│   ├── snmp_subagent/              # SNMP 子代理
│   ├── monitoring/                 # 监控可观测性
│   │   └── prometheus_exporter.py  # Prometheus 指标导出
│   └── web_management/             # Web 管理界面 (Flask)
│       ├── app.py
│       ├── templates/index.html
│       └── ...
```

---

## 源码修改与开发指南

### v3.0 真实内核下发架构

| 功能 | 源码文件 | 下发后端 | 配置持久化 |
|------|----------|----------|------------|
| **ACL** | `src/ip_services/acl/acl_huawei.c` | iptables/nftables chain + FORWARD/INPUT 跳转 | `/etc/whitebox-ne/acl.conf` |
| **NAT44** | `src/ip_services/nat/nat44.c` | iptables nat 表 (PREROUTING/POSTROUTING) | `/etc/whitebox-ne/nat.conf` |
| **Firewall** | `src/security/firewall/zone_firewall.c` | iptables FORWARD + ipset + zone chain | `/etc/whitebox-ne/firewall.conf` |
| **QoS** | `src/qos/qos_all_in_one.c` | Linux TC (HTB/tbf/u32 filter) | `/etc/whitebox-ne/qos.conf` |
| **Eth-Trunk** | `src/frr_core/zebra/eth_trunk.c` | Linux bonding 驱动 (bonding_masters) | `/etc/whitebox-ne/eth-trunk.conf` |
| **OpenConfig** | `src/openconfig_adapter/openconfig_adapter.c` | Sysrepo 双向回调 + gNMI 存根 | 依赖 FRR write memory |

---

## 测试

```bash
# 全链路运行时集成测试
sudo ./test-runtime-integration.sh

# 仅测试 Docker 环境
sudo ./test-runtime-integration.sh --with-docker
```

---

## 许可证

MIT
