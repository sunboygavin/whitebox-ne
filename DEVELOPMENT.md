# 白盒网元项目开发指南

本指南详细说明了白盒网元项目的代码结构、核心源码修改内容以及如何进行二次开发和扩展。

## 1. 项目代码结构

```
whitebox-ne/
├── README.md                       # 项目总览、安装与使用手册
├── TEST_REPORT.md                  # 沙盒环境测试报告
├── install_script.sh               # 基础组件（FRR, SNMP）一键安装脚本
├── build_from_source.sh            # 从源码构建 FRR 和自定义子代理的脚本
├── frr.conf                        # FRR 核心路由配置模板
├── snmpd.conf                      # Net-SNMP 配置模板
├── huawei_cli_alias.conf           # 华为风格 CLI 别名配置（已废弃，现已原生支持）
├── netconf_guide.md                # Netconf/YANG 集成与编译指南
├── DEVELOPMENT.md                  # 本开发指南
└── src/
    ├── frr_patch/                  # FRR 源码修改补丁存放目录
    │   └── 0001-huawei-cli-native-support.patch # 华为风格 CLI 原生支持补丁
    └── snmp_subagent/              # 自定义 SNMP 子代理源码目录
        ├── custom_subagent.c       # 自定义 SNMP 子代理 C 语言源码
        └── Makefile                # custom_subagent 的编译文件
```

### 目录与文件说明：
*   **`README.md`**: 项目的入口文档，包含快速开始、安装、基本使用和故障排除等信息。
*   **`TEST_REPORT.md`**: 记录了在沙盒环境中对各项功能的测试结果和验证过程。
*   **`install_script.sh`**: 用于在 Ubuntu 22.04 上快速安装 FRR 和 Net-SNMP 软件包，并进行基础配置。此脚本适用于快速部署和测试，不涉及 FRR 源码编译。
*   **`build_from_source.sh`**: 这是一个更高级的安装脚本，用于从 FRR 官方源码下载、应用自定义补丁、编译并安装 FRR。如果您需要应用源码级别的修改，应使用此脚本。
*   **`frr.conf`**: FRR 路由器的核心配置文件模板，包含了 OSPF、BGP (SRv6, Flowspec)、VRRP 等协议的基本配置。
*   **`snmpd.conf`**: Net-SNMP 代理的配置文件模板，配置了 AgentX 以允许 FRR 和自定义子代理注册 MIB。
*   **`huawei_cli_alias.conf`**: 这是一个历史文件，最初用于通过 FRR 的 `alias` 功能模拟华为 CLI。**在应用 `0001-huawei-cli-native-support.patch` 后，此文件中的别名已不再需要，因为相关命令已原生支持。**
*   **`netconf_guide.md`**: 提供了关于如何集成 Netconf/YANG 的详细指南，包括 Sysrepo 和 Netopeer2 的编译安装步骤。
*   **`DEVELOPMENT.md`**: 您正在阅读的这份文档，专注于源码级别的开发和扩展。
*   **`src/frr_patch/`**: 存放对 FRR 源码进行修改的补丁文件。每个补丁文件都应有清晰的命名和描述。
*   **`src/snmp_subagent/`**: 存放自定义 SNMP 子代理的源码。您可以在此处添加自己的 MIB 扩展。

## 2. FRR 源码修改详解

### 2.1. `0001-huawei-cli-native-support.patch` 补丁说明

这个补丁文件 (`src/frr_patch/0001-huawei-cli-native-support.patch`) 的目的是在 FRR 的 `vtysh` 命令行界面中，**原生支持**一些常用的华为风格命令，而不是仅仅通过 `alias` 进行映射。它主要修改了 FRR 源码中的 `lib/command.c` 文件。

**修改内容概览：**
该补丁通过在 `command.c` 中添加新的 `cmd_element` 结构体，将华为风格的命令（如 `system-view`, `display`, `save`）直接注册为 FRR 的内部命令，并将其功能指向对应的 FRR/Cisco 风格命令的实现函数。这样，这些华为风格的命令就成为了 FRR `vtysh` 的“一等公民”。

**具体修改点：**
*   **`system-view`**: 映射到 `cmd_configure_terminal` 函数，实现进入配置模式。
    ```c
    // ... (在 `cmd_element` 数组中添加)
    {
      .name = "system-view",
      .func = cmd_configure_terminal,
      .alias = "configure terminal",
      .help = "Enter configuration mode (Huawei VRP style)",
    },
    // ...
    ```
*   **`display`**: 映射到 `cmd_show` 函数，实现查看系统信息。
    ```c
    // ...
    {
      .name = "display",
      .func = cmd_show,
      .alias = "show",
      .help = "Show running system information (Huawei VRP style)",
    },
    // ...
    ```
*   **`save`**: 映射到 `cmd_write` 函数，实现保存配置。
    ```c
    // ...
    {
      .name = "save",
      .func = cmd_write,
      .alias = "write",
      .help = "Write running configuration to startup configuration (Huawei VRP style)",
    },
    // ...
    ```

**如何应用此补丁：**
在执行 `build_from_source.sh` 脚本时，它会自动下载 FRR 源码并应用此补丁。如果您手动编译 FRR，可以在下载源码后，进入 FRR 源码根目录，然后执行 `git apply /path/to/0001-huawei-cli-native-support.patch`。

### 2.2. 扩展 FRR 源码的建议

如果您需要进一步扩展 FRR 的功能，例如：
*   **添加新的 CLI 命令**：您可以在 `lib/command.c` 或其他模块的 `command.c` 文件中，按照现有模式添加新的 `cmd_element` 结构体，并实现对应的处理函数。
*   **修改协议行为**：深入到 `bgpd/`、`ospfd/` 等目录下的 C 源码，修改协议报文处理、路由计算逻辑等。例如，如果您需要修改 BGP SRv6 的特定行为，可以查找 `bgpd/bgp_srv6.c` 等相关文件。
*   **集成硬件驱动**：对于特定的白盒硬件（如基于 ASIC 的交换机），您可能需要在 `zebra/` 或单独的守护进程中添加与硬件 SDK 交互的代码。

**开发流程：**
1.  **获取 FRR 源码**：`git clone https://github.com/frrouting/frr.git`。
2.  **创建新分支**：`git checkout -b my-feature-branch`。
3.  **修改源码**：在相应的 C 文件中进行修改。
4.  **生成补丁**：`git diff master > my-new-feature.patch`。
5.  **测试**：编译并测试您的修改。
6.  **集成**：将您的补丁添加到 `src/frr_patch/` 目录，并更新 `build_from_source.sh` 以应用它。

## 3. 自定义 SNMP 子代理详解

### 3.1. `src/snmp_subagent/custom_subagent.c` 说明

这个 C 语言源文件 (`custom_subagent.c`) 是一个简单的 Net-SNMP AgentX 子代理示例。它演示了如何创建一个独立的进程，通过 AgentX 协议向主 SNMP 代理 (`snmpd`) 注册自定义的 MIB 节点，并响应 SNMP 请求。

**核心功能：**
*   **注册自定义 MIB OID**：`1.3.6.1.4.1.9999.1` (iso.org.dod.internet.private.enterprise.9999.1)。
*   **暴露两个标量对象**：
    *   `CustomUptime` (`.1.3.6.1.4.1.9999.1.1.0`)：模拟一个自定义的启动时间。
    *   `CustomFlowCount` (`.1.3.6.1.4.1.9999.1.2.0`)：模拟一个 Flowspec 匹配的流量计数。
*   **`handle_custom_mib` 函数**：负责处理对这些自定义 OID 的 GET 请求，并返回模拟数据。
*   **`main` 函数**：初始化 AgentX 子代理，注册 MIB，并进入主循环等待 SNMP 请求。

**如何编译和运行：**
1.  进入 `src/snmp_subagent/` 目录。
2.  执行 `make` 编译。
3.  运行 `./custom_subagent` 启动子代理。

**如何验证：**
在 `snmpd` 和 `custom_subagent` 都运行的情况下，您可以使用 `snmpwalk` 命令查询自定义 MIB：
```bash
snmpwalk -v2c -c public localhost .1.3.6.1.4.1.9999.1
```

### 3.2. 扩展自定义 SNMP 子代理的建议

*   **添加更多自定义 MIB 节点**：您可以根据您的白盒网元需要暴露的特定状态、统计数据或配置参数，在 `custom_subagent.c` 中添加更多的 `netsnmp_create_handler_registration` 和 `netsnmp_register_table` 调用。
*   **获取真实数据**：将 `handle_custom_mib` 中的模拟数据替换为从 FRR 内部 API、Linux 内核接口（如 `/proc` 或 `sysfs`）、或者其他硬件驱动中获取的真实数据。
*   **实现 SET 操作**：如果需要通过 SNMP 修改网元配置，您需要在 `handle_custom_mib` 中实现 `MODE_SET_RESERVE1`、`MODE_SET_ACTION` 等模式的处理逻辑。

## 4. 自动化构建与部署

### 4.1. `build_from_source.sh` 脚本说明

这个脚本 (`build_from_source.sh`) 旨在提供一个从零开始构建白盒网元的自动化流程。它执行以下关键步骤：
1.  **安装编译依赖**：确保系统具备编译 FRR 和 Net-SNMP 子代理所需的所有工具和库。
2.  **下载 FRR 源码**：从 GitHub 克隆指定版本的 FRR 源码。
3.  **应用 FRR 补丁**：自动应用 `src/frr_patch/` 目录下的所有补丁文件。
4.  **编译和安装 FRR**：配置 FRR 的编译选项（包括启用 SNMP 支持），然后进行编译和安装。
5.  **编译自定义 SNMP 子代理**：编译 `src/snmp_subagent/` 目录下的子代理程序。

**使用此脚本的优势：**
*   **一致性**：确保每次构建的环境和结果都是一致的。
*   **自动化**：减少手动操作，降低出错率。
*   **可追溯性**：通过脚本可以清晰地了解构建过程中的每一步。

### 4.2. 部署流程

1.  **准备目标机器**：确保目标白盒机器安装了 Ubuntu 22.04 LTS，并具备网络连接。
2.  **克隆项目**：`git clone https://github.com/sunboygavin/whitebox-ne.git`。
3.  **执行构建脚本**：`cd whitebox-ne && sudo chmod +x build_from_source.sh && sudo ./build_from_source.sh`。
4.  **配置 FRR**：将 `frr.conf` 拷贝到 `/etc/frr/frr.conf` 并重启 FRR 服务。
5.  **启动自定义子代理**：`sudo /home/ubuntu/whitebox-ne/src/snmp_subagent/custom_subagent`。
6.  **验证**：使用 `vtysh` 和 `snmpwalk` 验证功能。

## 5. 二次开发与贡献

我们欢迎您对本项目进行二次开发和贡献。如果您有新的功能需求或改进建议，请遵循以下流程：
1.  **Fork 项目**：在 GitHub 上 Fork 本项目。
2.  **创建新分支**：从 `master` 分支创建新的功能分支。
3.  **进行修改**：在您的分支上进行代码修改和功能实现。
4.  **生成补丁**：如果是对 FRR 源码的修改，请生成相应的 `.patch` 文件并放置在 `src/frr_patch/` 目录下。
5.  **更新文档**：同步更新 `README.md` 和 `DEVELOPMENT.md`，说明您的修改和新增功能。
6.  **提交 Pull Request**：将您的修改提交到本项目的 `master` 分支。

**请注意：** 任何对 FRR 源码的修改都应尽可能以补丁 (`.patch`) 的形式提供，以便于管理和维护。
