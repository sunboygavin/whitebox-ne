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
├── netconf_guide.md                # Netconf/YANG 集成与编译指南
├── DEVELOPMENT.md                  # 本开发指南
└── src/
    ├── frr_core/                   # 经过改造的 FRR 核心源码目录
    │   └── lib/                    # 存放 FRR 的 lib 库源码
    │       └── command.c           # 华为风格 CLI 原生支持的 command.c 示例
    ├── frr_patch/                  # FRR 源码修改补丁存放目录 (备用)
    │   └── 0001-huawei-cli-native-support.patch # 华为风格 CLI 原生支持补丁 (作为参考)
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
*   **`netconf_guide.md`**: 提供了关于如何集成 Netconf/YANG 的详细指南，包括 Sysrepo 和 Netopeer2 的编译安装步骤。
*   **`DEVELOPMENT.md`**: 您正在阅读的这份文档，专注于源码级别的开发和扩展。
*   **`src/frr_core/`**: **此目录存放了经过我们改造的 FRR 核心源码示例。与 `frr_patch` 不同，这里是直接修改后的 C 语言文件，方便您直接查看和修改。**
*   **`src/frr_patch/`**: 存放对 FRR 源码进行修改的补丁文件。这些补丁是作为 `src/frr_core` 目录中代码修改的参考，如果您选择从 FRR 官方源码开始，可以应用这些补丁。
*   **`src/snmp_subagent/`**: 存放自定义 SNMP 子代理的源码。您可以在此处添加自己的 MIB 扩展。

## 2. FRR 核心源码修改详解 (`src/frr_core/lib/command.c`)

我们提供的 `src/frr_core/lib/command.c` 文件是 FRR 源码中负责命令行解析的核心文件的一个**模拟版本**，它直接集成了华为风格的 CLI 命令。这意味着这些命令在编译后将成为 FRR `vtysh` 的原生命令，无需通过 `alias` 机制。

**修改内容概览：**
该文件通过在 `cmd_elements` 数组中添加新的 `cmd_element` 结构体，将华为风格的命令（如 `system-view`, `display`, `save`）直接注册为 FRR 的内部命令，并将其功能指向对应的 FRR/Cisco 风格命令的实现函数。这样，这些华为风格的命令就成为了 FRR `vtysh` 的“一等公民”。

**具体修改点 (`src/frr_core/lib/command.c`)：**

1.  **`system-view`**: 映射到 `cmd_configure_terminal` 函数，实现进入配置模式。
    ```c
    // ... (在 `cmd_elements` 数组中添加)
    {
      .name = "system-view",
      .func = cmd_configure_terminal,
      .alias = "configure terminal", /* 仅用于帮助信息或兼容性 */
      .help = "Enter configuration mode (Huawei VRP style)",
    },
    // ...
    ```
    *   **如何修改**：如果您想修改 `system-view` 的行为，可以直接修改 `cmd_configure_terminal` 函数的实现。如果您想添加其他进入配置模式的华为风格命令，可以在 `cmd_elements` 数组中添加新的 `cmd_element`，并将其 `.func` 指向 `cmd_configure_terminal`。

2.  **`display`**: 映射到 `cmd_show` 函数，实现查看系统信息。
    ```c
    // ...
    {
      .name = "display",
      .func = cmd_show,
      .alias = "show", /* 仅用于帮助信息或兼容性 */
      .help = "Show running system information (Huawei VRP style)",
    },
    // ...
    ```
    *   **如何修改**：`cmd_show` 函数是一个非常重要的扩展点。在实际的 FRR 源码中，`cmd_show` 会根据后续参数调用不同的子函数来显示路由表、接口状态、BGP 邻居等信息。您可以在 `cmd_show` 函数内部，根据 `args->argv` 的内容，调用 FRR 内部的 API 来获取并格式化输出您希望的华为风格 `display` 命令结果。例如，要实现 `display ip routing-table`，您需要在 `cmd_show` 中解析 `ip routing-table` 参数，然后调用 FRR 内部获取路由表的函数。

3.  **`save`**: 映射到 `cmd_write` 函数，实现保存配置。
    ```c
    // ...
    {
      .name = "save",
      .func = cmd_write,
      .alias = "write", /* 仅用于帮助信息或兼容性 */
      .help = "Write running configuration to startup configuration (Huawei VRP style)",
    },
    // ...
    ```
    *   **如何修改**：`cmd_write` 函数通常会调用 FRR 内部的配置保存机制。如果您需要修改保存配置的行为（例如，保存到不同的位置或以不同的格式保存），可以修改 `cmd_write` 函数的实现。

**如何使用此源码：**
`build_from_source.sh` 脚本将不再应用补丁，而是直接将 `src/frr_core/` 目录下的文件替换到 FRR 源码的相应位置，然后进行编译。这样，您就可以直接在 `src/frr_core/` 目录下修改代码，然后运行 `build_from_source.sh` 来构建您的定制化 FRR。

### 2.3. 扩展 FRR 源码的建议

如果您需要进一步扩展 FRR 的功能，例如：
*   **添加新的 CLI 命令**：除了在 `command.c` 中注册命令外，您还需要在 FRR 源码中找到或创建对应的处理函数。例如，如果您想添加一个 `display srv6 policy` 命令，您需要在 `bgpd/` 或 `zebra/` 中找到处理 SRv6 策略的逻辑，并编写一个 C 函数来获取并格式化这些信息，然后将 `display srv6 policy` 命令的 `.func` 指向这个函数。
*   **修改协议行为**：深入到 `bgpd/`、`ospfd/` 等目录下的 C 源码，修改协议报文处理、路由计算逻辑等。例如，如果您需要修改 BGP SRv6 的特定行为，可以查找 `bgpd/bgp_srv6.c` 等相关文件。
*   **集成硬件驱动**：对于特定的白盒硬件（如基于 ASIC 的交换机），您可能需要在 `zebra/` 或单独的守护进程中添加与硬件 SDK 交互的代码。

**开发流程：**
1.  **克隆本项目**：`git clone https://github.com/sunboygavin/whitebox-ne.git`。
2.  **修改 `src/frr_core/` 下的源码**：直接编辑您希望修改的 C 文件。
3.  **运行 `build_from_source.sh`**：脚本会自动将您的修改集成到 FRR 的编译过程中。
4.  **测试**：编译并测试您的修改。

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
3.  **应用 FRR 源码替换**：**将 `src/frr_core/` 目录下的文件复制到 FRR 源码的相应位置，覆盖原始文件。**
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
2.  **克隆本项目**：`git clone https://github.com/sunboygavin/whitebox-ne.git`。
3.  **修改 `src/frr_core/` 下的源码**：直接编辑您希望修改的 C 文件。
4.  **运行 `build_from_source.sh`**：脚本会自动将您的修改集成到 FRR 的编译过程中。
5.  **更新文档**：同步更新 `README.md` 和 `DEVELOPMENT.md`，说明您的修改和新增功能。
6.  **提交 Pull Request**：将您的修改提交到本项目的 `master` 分支。

**请注意：** 如果您选择从 FRR 官方源码开始，并希望应用 `src/frr_patch/` 中的补丁，请确保您了解补丁的应用方式。直接修改 `src/frr_core/` 中的文件是更直接的开发方式。
