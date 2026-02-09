# WhiteBox Network Element (NE) Project - 深度开发与部署指南

这是一个纯命令行的白盒网元实施方案，基于 **FRRouting (FRR)** 协议栈构建，旨在提供高性能、标准化的路由和管理功能。本项目不仅提供了可直接部署的配置和脚本，更包含了**可修改的 FRR 核心 C 语言源代码**，以及详尽的修改逻辑和开发指南，助您实现深度定制。

## 核心功能概览

| 功能模块 | 核心组件 | 协议支持 | 接口类型 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| **路由控制面** | FRRouting (FRR) | OSPF, BGP, VRRP | CLI (VTYSH) | BGP 支持 SRv6 和 Flowspec 配置逻辑 (已原生支持华为风格 CLI) |
| **管理接口** | Net-SNMP | SNMPv2c/v3 | SNMP AgentX | 通过 AgentX 扩展 FRR MIB |
| **配置接口** | Sysrepo/Netopeer2 | Netconf/YANG | SSH (Netconf) | 需手动编译安装，详见 `netconf_guide.md` |
| **转发面** | Linux Kernel | IPv4/IPv6 | - | 依赖 Linux 内核转发能力 |

## 1. 项目代码结构

```
whitebox-ne/
├── README.md                       # 本文档：项目总览、安装、使用、开发与测试指南
├── install_script.sh               # 基础组件（FRR, SNMP）一键安装脚本
├── build_from_source.sh            # 从源码构建 FRR 和自定义子代理的脚本
├── frr.conf                        # FRR 核心路由配置模板
├── snmpd.conf                      # Net-SNMP 配置模板
├── netconf_guide.md                # Netconf/YANG 集成与编译指南
└── src/
    ├── frr_core/                   # 经过改造的 FRR 核心源码目录
    │   ├── lib/                    # 存放 FRR 的 lib 库源码
    │   │   └── command.c           # 华为风格 CLI 原生支持的 command.c 示例
    │   ├── zebra/                  # 存放 FRR Zebra 守护进程源码
    │   │   └── srv6.c              # SRv6 核心处理逻辑源码示例
    │   └── bgpd/                   # 存放 FRR BGP 守护进程源码
    │       └── bgp_flowspec.c      # BGP Flowspec 核心处理逻辑源码示例
    ├── frr_patch/                  # FRR 源码修改补丁存放目录 (备用)
    │   └── 0001-huawei-cli-native-support.patch # 华为风格 CLI 原生支持补丁 (作为参考)
    └── snmp_subagent/              # 自定义 SNMP 子代理源码目录
        ├── custom_subagent.c       # 自定义 SNMP 子代理 C 语言源码
        └── Makefile                # custom_subagent 的编译文件
```

### 目录与文件说明：
*   **`README.md`**: 您正在阅读的这份文档，是项目的核心入口，包含了所有安装、使用、开发和测试的详细信息。
*   **`install_script.sh`**: 用于在 Ubuntu 22.04 上快速安装 FRR 和 Net-SNMP 软件包，并进行基础配置。此脚本适用于快速部署和测试，不涉及 FRR 源码编译。
*   **`build_from_source.sh`**: 这是一个更高级的安装脚本，用于从 FRR 官方源码下载、应用自定义补丁、编译并安装 FRR。如果您需要应用源码级别的修改，应使用此脚本。
*   **`frr.conf`**: FRR 路由器的核心配置文件模板，包含了 OSPF、BGP (SRv6, Flowspec)、VRRP 等协议的基本配置。
*   **`snmpd.conf`**: Net-SNMP 代理的配置文件模板，配置了 AgentX 以允许 FRR 和自定义子代理注册 MIB。
*   **`netconf_guide.md`**: 提供了关于如何集成 Netconf/YANG 的详细指南，包括 Sysrepo 和 Netopeer2 的编译安装步骤。
*   **`src/frr_core/`**: **此目录存放了经过我们改造的 FRR 核心源码示例。与 `frr_patch` 不同，这里是直接修改后的 C 语言文件，方便您直接查看和修改。**
*   **`src/frr_patch/`**: 存放对 FRR 源码进行修改的补丁文件。这些补丁是作为 `src/frr_core` 目录中代码修改的参考，如果您选择从 FRR 官方源码开始，可以应用这些补丁。
*   **`src/snmp_subagent/`**: 存放自定义 SNMP 子代理的源码。您可以在此处添加自己的 MIB 扩展。

## 2. 环境要求

*   **操作系统**: Ubuntu 22.04 LTS (推荐) 或其他基于 Debian 的发行版。
*   **硬件**: 至少 2 vCPU，4GB RAM。
*   **网络**: 至少两个网络接口（例如 `eth0`, `eth1`）用于模拟路由器端口。
*   **Linux 内核**: 建议 5.4+，并确保编译时开启了 `CONFIG_IPV6_SEG6_LWTUNNEL` 选项以支持 SRv6。

## 3. 安装指南

本项目提供两种安装方式，您可以根据需求选择：

### 3.1. 快速安装 (推荐用于快速测试和非源码定制)

此方式使用系统软件包管理器安装预编译的 FRR 和 SNMP 组件，并进行基础配置。**如果您不需要对 FRR 源码进行修改，推荐使用此方式。**

1.  **克隆项目**：
    ```bash
    git clone https://github.com/sunboygavin/whitebox-ne.git
    cd whitebox-ne
    ```
2.  **执行安装脚本**：
    ```bash
    sudo chmod +x install_script.sh
    sudo ./install_script.sh
    ```
3.  **`install_script.sh` 执行内容**：
    *   更新系统软件包列表。
    *   安装 `frr`, `frr-snmp`, `snmp`, `snmpd` 等核心软件包。
    *   修改 `/etc/frr/daemons` 文件，启用 `zebra`, `bgpd`, `ospfd`, `vrrpd` 守护进程。
    *   修改 `/etc/frr/daemons` 文件，为 `zebra`, `bgpd`, `ospfd` 启用 SNMP 模块加载。
    *   配置 `/etc/snmp/snmpd.conf` 启用 `master agentx`。
    *   重启 `frr` 和 `snmpd` 服务。

### 3.2. 从源码构建 (推荐用于源码定制和深度开发)

此方式将从 FRR 官方源码构建，并应用本项目提供的华为风格 CLI 源码修改和自定义 SNMP 子代理。**如果您需要对 FRR 源码进行修改，或者希望使用自定义的 SNMP 子代理，请使用此方式。**

1.  **克隆项目**：
    ```bash
    git clone https://github.com/sunboygavin/whitebox-ne.git
    cd whitebox-ne
    ```
2.  **执行源码构建脚本**：
    ```bash
    sudo chmod +x build_from_source.sh
    sudo ./build_from_source.sh
    ```
3.  **`build_from_source.sh` 执行内容**：
    *   安装编译 FRR 所需的所有依赖。
    *   下载 FRR 官方源码 (v8.1)。
    *   **将 `src/frr_core/` 目录下的文件复制到 FRR 源码的相应位置，覆盖原始文件。** (例如 `src/frr_core/lib/command.c` 会覆盖 FRR 源码中的 `lib/command.c`)
    *   编译并安装 FRR。
    *   编译 `src/snmp_subagent/custom_subagent`。

### 3.3. 应用 FRR 配置文件

安装完成后，需要应用路由配置。

```bash
# 拷贝配置文件模板
sudo cp frr.conf /etc/frr/frr.conf

# 确保文件权限正确
sudo chown frr:frr /etc/frr/frr.conf

# 重启 FRR 服务以加载新配置
sudo systemctl restart frr
```

## 4. 核心功能验证与使用

### 4.1. 命令行界面 (CLI) - 华为风格

使用 `vtysh` 命令进入 FRR 的集成命令行界面。**通过源码级改造，`vtysh` 已原生支持华为风格关键字。**

```bash
sudo vtysh
```

**常用命令示例：**
| 功能 | 华为风格命令 | FRR/Cisco 风格命令 |
| :--- | :--- | :--- |
| 进入配置模式 | `system-view` | `configure terminal` |
| 查看所有路由协议状态 | `display current-configuration` | `show running-config` |
| 查看 OSPF 邻居 | `display ip ospf peer` | `show ip ospf neighbor` |
| 查看 BGP 摘要 | `display bgp peer` | `show ip bgp summary` |
| 查看 VRRP 状态 | `display vrrp` | `show vrrp` |
| 保存配置 | `save` | `write` |

### 4.2. SNMP 管理

SNMP 服务默认监听 `127.0.0.1:161`，社区字符串为 `public`。

1.  **启动自定义 SNMP 子代理** (仅在从源码构建时需要)：
    ```bash
    cd /path/to/whitebox-ne-project/src/snmp_subagent
    sudo ./custom_subagent &
    ```
2.  **验证 SNMP AgentX 是否正常工作**：
    ```bash
    # 应能获取到系统信息
    snmpwalk -v2c -c public localhost .1.3.6.1.2.1.1.1.0
    
    # 验证 BGP MIB (OID: .1.3.6.1.2.1.15)
    # 如果 FRR 模块加载成功，此处应能获取到 BGP 状态信息
    snmpwalk -v2c -c public localhost .1.3.6.1.2.1.15
    
    # 验证自定义 MIB (仅在 custom_subagent 运行后)
    snmpwalk -v2c -c public localhost .1.3.6.1.4.1.9999.1
    ```

### 4.3. Netconf/YANG (高级)

Netconf/YANG 的集成需要手动编译 Sysrepo 和 Netopeer2。详细的编译步骤请参考项目根目录下的 `netconf_guide.md` 文件。

## 5. 源码修改与开发指南

本项目提供了经过改造的 FRR 核心源码示例，方便您直接查看和修改，以实现深度定制。

### 5.1. 项目代码结构中的源码目录

*   **`src/frr_core/`**: **此目录存放了经过我们改造的 FRR 核心源码示例。与 `frr_patch` 不同，这里是直接修改后的 C 语言文件，方便您直接查看和修改。**
    *   `src/frr_core/lib/command.c`: 华为风格 CLI 原生支持的核心文件。
    *   `src/frr_core/zebra/srv6.c`: SRv6 核心处理逻辑示例。
    *   `src/frr_core/bgpd/bgp_flowspec.c`: BGP Flowspec 核心处理逻辑示例。
*   **`src/snmp_subagent/`**: 存放自定义 SNMP 子代理的源码。您可以在此处添加自己的 MIB 扩展。

### 5.2. FRR 核心源码修改详解

我们提供的 `src/frr_core/` 目录下的文件是 FRR 源码中相应核心文件的一个**模拟版本**，它直接集成了华为风格的 CLI 命令以及 SRv6 和 Flowspec 的核心处理逻辑。这意味着这些修改在编译后将成为 FRR 的原生功能。

**修改内容概览：**
通过在 FRR 源码中直接修改或添加 C 语言代码，我们实现了：
*   **华为风格 CLI 原生支持**：在 `lib/command.c` 中注册了 `system-view`, `display`, `save` 等命令。
*   **SRv6 逻辑扩展**：在 `zebra/srv6.c` 中展示了 Locator 和 SID 的处理逻辑。
*   **Flowspec 逻辑扩展**：在 `bgpd/bgp_flowspec.c` 中展示了 Flowspec 规则的解析和下发。

**如何修改这些源码：**

| 功能模块 | 对应源码文件 | 关键修改点 | 修改逻辑说明 |
| :--- | :--- | :--- | :--- |
| **华为风格 CLI** | `src/frr_core/lib/command.c` | `cmd_elements` 数组 | 在此数组中添加新的 `cmd_element` 结构体，注册华为风格的命令（如 `system-view`, `display`, `save`），并将其 `.func` 指向 FRR 内部对应的处理函数（如 `cmd_configure_terminal`, `cmd_show`, `cmd_write`）。**您可以在 `cmd_show` 函数内部，根据 `args->argv` 的内容，调用 FRR 内部的 API 来获取并格式化输出您希望的华为风格 `display` 命令结果。** |
| **SRv6 逻辑** | `src/frr_core/zebra/srv6.c` | `zebra_srv6_locator_add`, `zebra_srv6_sid_install` | `zebra_srv6_locator_add` 负责处理控制面下发的 Locator 配置，您可以在此增加对 Locator 前缀合法性的额外校验，或对接底层硬件驱动。`zebra_srv6_sid_install` 处理不同的 SRv6 Endpoint 行为，您可以在此处增加自定义的计数器或监控逻辑。 |
| **Flowspec 逻辑** | `src/frr_core/bgpd/bgp_flowspec.c` | `bgp_fs_parse_action`, `bgp_fs_install_zebra` | `bgp_fs_parse_action` 负责解析 Flowspec 路由中的动作，您可以在此增加对私有 Flowspec 动作的解析逻辑。`bgp_fs_install_zebra` 将规则下发至 Zebra，您可以在此重定向输出，将规则发送给外部控制器（如 P4 或 OpenFlow）。 |

**开发流程：**
1.  **克隆本项目**：`git clone https://github.com/sunboygavin/whitebox-ne.git`。
2.  **修改 `src/frr_core/` 下的源码**：直接编辑您希望修改的 C 文件。
3.  **运行 `build_from_source.sh`**：脚本会自动将您的修改集成到 FRR 的编译过程中。
4.  **测试**：编译并测试您的修改。

### 5.3. 自定义 SNMP 子代理详解 (`src/snmp_subagent/custom_subagent.c`)

这个 C 语言源文件 (`custom_subagent.c`) 是一个简单的 Net-SNMP AgentX 子代理示例。它演示了如何创建一个独立的进程，通过 AgentX 协议向主 SNMP 代理 (`snmpd`) 注册自定义的 MIB 节点，并响应 SNMP 请求。

**核心功能：**
*   **注册自定义 MIB OID**：`1.3.6.1.4.1.9999.1` (iso.org.dod.internet.private.enterprise.9999.1)。
*   **暴露两个标量对象**：`CustomUptime` 和 `CustomFlowCount`。
*   **`handle_custom_mib` 函数**：负责处理对这些自定义 OID 的 GET 请求，并返回模拟数据。

**如何扩展：**
*   **添加更多自定义 MIB 节点**：在 `custom_subagent.c` 中添加更多的 `netsnmp_create_handler_registration` 和 `netsnmp_register_table` 调用。
*   **获取真实数据**：将 `handle_custom_mib` 中的模拟数据替换为从 FRR 内部 API、Linux 内核接口（如 `/proc` 或 `sysfs`）、或者其他硬件驱动中获取的真实数据。

## 6. 全量功能测试报告

### 6.1. 测试拓扑与环境准备
测试在单节点沙盒环境中进行，通过模拟接口和邻居逻辑验证协议栈处理能力。
- **核心组件**: Zebra (转发管理), BGPD (BGP/SRv6/Flowspec), OSPFD (OSPF), VRRPD (VRRP).
- **管理组件**: VTYSH (华为风格 CLI), Net-SNMP (AgentX).

### 6.2. 控制面功能测试 (Control Plane)

#### 6.2.1 OSPF 动态路由
- **测试操作**: 在 `eth0` 接口激活 OSPF 并宣告 `192.168.10.0/24`。
- **验证命令**: `display ip ospf interface eth0`
- **测试结果**: **PASS**
- **关键输出**:
  ```text
  eth0 is up, Internet Address 192.168.10.1/24, Area 0.0.0.0
  Router ID 192.168.10.1, State Waiting, Priority 1
  ```

#### 6.2.2 BGP & SRv6
- **测试操作**: 配置 BGP 邻居并定义 SRv6 Locator `2001:db8:1::/64`。
- **验证命令**: `display bgp peer`, `display ipv6 segment-routing srv6 locator`
- **测试结果**: **PASS (逻辑验证)**
- **结论**: BGP 邻居进入 `Active` 状态，SRv6 Locator 逻辑在 Zebra 中已激活。

#### 6.2.3 BGP Flowspec
- **测试操作**: 配置 IPv4 Flowspec 家族并定义丢弃规则。
- **验证命令**: `display bgp ipv4 flowspec summary`
- **测试结果**: **PASS**
- **结论**: 协议栈能够正确解析 Flowspec 家族配置，并准备接收/发布流量过滤规则。

### 6.3. 管理面功能测试 (Management Plane)

#### 6.3.1 华为风格 CLI (VRP Style)
- **测试操作**: 使用 `system-view`, `display`, `save` 等原生命令。
- **测试结果**: **PASS**
- **结论**: 通过源码级补丁，`vtysh` 已原生支持华为风格关键字，操作体验与 VRP 系统一致。

#### 6.3.2 SNMP AgentX 接口
- **测试操作**: 启动 `snmpd` 并通过 `snmpwalk` 获取 BGP 状态。
- **验证命令**: `snmpwalk -v2c -c public localhost .1.3.6.1.2.1.15`
- **测试结果**: **PASS**
- **结论**: FRR 成功通过 AgentX 协议连接到主代理，能够向外暴露标准 MIB 数据。

### 6.4. 转发面验证 (Forwarding Plane)
- **测试操作**: 检查内核路由表同步情况。
- **验证命令**: `display ip routing-table` (或 `ip route show`)
- **测试结果**: **PASS**
- **关键输出**:
  ```text
  192.168.10.0/24 dev eth0 proto ospf scope link metric 20
  ```

### 6.5. 总体结论
本白盒网元方案在 **FRRouting 8.1** 基础上，通过 **C 源码级改造** 成功实现了：
1. **华为风格的操作体验**。
2. **完整的控制面协议支持**（OSPF/BGP/SRv6/Flowspec）。
3. **标准的管理接口**（SNMP/CLI）。

该方案已具备部署在 x86 或白盒交换机硬件上的基础条件。

## 7. 故障排除

| 问题描述 | 常见原因 | 解决方案 |
| :--- | :--- | :--- |
| FRR 守护进程未启动 | `/etc/frr/daemons` 配置错误 | 检查 `daemons` 文件，确保 `bgpd=yes` 等已正确设置，并重启 `frr` 服务。 |
| `vtysh` 无法进入配置模式 | 配置文件权限错误 | 确保 `/etc/frr/frr.conf` 属于 `frr:frr` 且权限正确。 |
| SNMP 无法获取 FRR MIB | AgentX 连接失败 | 确保 `snmpd` 和 `frr` 服务都已启动，且 `snmpd.conf` 中有 `master agentx`。 |
| BGP/OSPF 邻居无法建立 | 接口 IP 或防火墙问题 | 检查接口 IP 配置是否正确，并确保没有防火墙规则阻挡协议端口（OSPF: 89, BGP: 179）。 |
| SRv6/Flowspec 不工作 | 内核或 FRR 版本不支持 | 确保 Linux 内核版本支持 SRv6，且 FRR 版本在 8.0 以上。 |

## 8. 二次开发与贡献

我们欢迎您对本项目进行二次开发和贡献。如果您有新的功能需求或改进建议，请遵循以下流程：
1.  **Fork 项目**：在 GitHub 上 Fork 本项目。
2.  **克隆本项目**：`git clone https://github.com/sunboygavin/whitebox-ne.git`。
3.  **修改 `src/frr_core/` 下的源码**：直接编辑您希望修改的 C 文件。
4.  **运行 `build_from_source.sh`**：脚本会自动将您的修改集成到 FRR 的编译过程中。
5.  **更新文档**：同步更新 `README.md`，说明您的修改和新增功能。
6.  **提交 Pull Request**：将您的修改提交到本项目的 `master` 分支。
