# 华为 VRP 风格命令完整覆盖报告

**检查时间**: 2026-02-20 13:27  
**检查人员**: Junner AI Assistant  
**目标**: 确保华为 CLI 命令完整覆盖

---

## 📋 当前状态

### 原有命令统计

根据 `src/frr_core/lib/command.c` 统计：

```
系统视图命令：5 个
  - system-view
  - display
  - save
  - quit
  - exit

显示命令：7 个（子命令）
  - display ip
  - display interface
  - display bgp
  - display ospf
  - display mpls
  - display qos
  - display version

配置命令：4 个
  - interface
  - bgp
  - ospf
  - mpls

兼容命令：3 个（Cisco 风格）
  - configure
  - terminal
  - show
  - write

总计：16 个命令
```

### 华为 VRP 标准命令覆盖对比

**华为企业级路由器（AR 系列）** 应有：

| 类别 | 华为命令数 | 当前实现 | 缺失 |
|------|---------|---------|------|
| **系统视图** | 20+ | 5 | **15** |
| **显示命令** | 80+ | 7 | **73** |
| **路由协议** | 50+ | 4 | **46** |
| **接口配置** | 30+ | 1 | **29** |
| **MPLS 配置** | 20+ | 1 | **19** |
| **QoS 配置** | 40+ | 1 | **39** |
| **ACL 配置** | 20+ | 0 | **20** |
| **安全配置** | 30+ | 0 | **30** |
| **VLAN 配置** | 20+ | 0 | **20** |

**总缺失命令**: 291+ 个

---

## 🔧 建议的完整命令覆盖

### 1. 系统视图命令（20+ 个）

```
系统管理类：
  system-view          # 进入系统视图
  user-view            # 进入用户视图
  diagnose-view        # 进入诊断视图
  return (end)         # 返回上级视图
  quit (exit)         # 退出系统
  peek                # 查看下条命令
  undo                # 撤销命令
  redo                # 重做命令
  clear               # 清屏
  shell               # 执行 Shell 命令

信息查询类：
  display version       # 显示版本
  display cpu          # 显示 CPU 信息
  display memory        # 显示内存信息
  display device        # 显示设备信息
  display current-configuration # 显示当前配置
  display saved-configuration  # 显示已保存配置
  display startup-config   # 显示启动配置
  display clock         # 显示时钟
  display timezone      # 显示时区
  display users         # 显示在线用户
```

### 2. 显示命令（80+ 个）

```
路由信息类：
  display ip routing-table        # IP 路由表
  display ip routing-table all       # 所有路由表
  display ip fib                 # 转发信息库
  display ip interface            # 接口信息
  display ip interface brief       # 接口简略信息
  display ip interface statistics  # 接口统计
  display ip arp                 # ARP 表
  display ip arp all             # 所有 ARP 表
  display ip mac-address          # MAC 地址表
  display ip route-static          # 静态路由
  display ip route-summary        # 路由汇总

BGP 显示类：
  display bgp                    # BGP 汇总信息
  display bgp peer                 # BGP 对等体
  display bgp peer all            # 所有 BGP 对等体
  display bgp route                # BGP 路由
  display bgp route all            # 所有 BGP 路由
  display bgp statistics           # BGP 统计
  display bgp community           # BGP 团体
  display bgp dampening            # BGP 阻尼
  display bgp update-group         # BGP 更新组
  display bgp path                # BGP 路径

OSPF 显示类：
  display ospf                   # OSPF 总信息
  display ospf peer               # OSPF 邻居
  display ospf peer all           # 所有 OSPF 邻居
  display ospf interface          # OSPF 接口信息
  display ospf interface all       # 所有 OSPF 接口
  display ospf lsa                # OSPF 链接状态
  display ospf lsdb               # OSPF LSDB
  display ospf abr-summary         # OSPF ABR 汇总
  display ospf request             # OSPF 请求列表
  display ospf retransmit          # OSPF 重传列表
  display ospf statistics          # OSPF 统计

MPLS 显示类：
  display mpls                   # MPLS 总信息
  display mpls ldp                # MPLS LDP
  display mpls ldp global         # MPLS LDP 全局
  display mpls lsp                # MPLS LSP
  display mpls lsp all            # 所有 MPLS LSP
  display mpls lsp transit         # MPLS 转发 LSP
  display mpls lsp static          # MPLS 静态 LSP
  display mpls te                  # MPLS TE
  display mpls te tunnel          # MPLS TE 隧道
  display mpls l2vpn              # MPLS L2VPN
  display mpls static-lsp          # 静态 LSP

QoS 显示类：
  display qos                    # QoS 总信息
  display qos queue               # QoS 队列
  display qos queue all           # 所有 QoS 队列
  display qos scheduler            # QoS 调度器
  display qos car                  # QoS CAR
  display qos wred                 # QoS WRED
  display qos classifier           # QoS 分类器
  display qos behavior            # QoS 行为
  display qos acl                 # QoS ACL

RIP 显示类：
  display rip                    # RIP 信息
  display rip interface            # RIP 接口
  display rip route               # RIP 路由
  display rip statistics           # RIP 统计

IS-IS 显示类：
  display isis                   # IS-IS 信息
  display isis neighbor           # IS-IS 邻居
  display isis interface          # IS-IS 接口
  display isis database           # IS-IS 数据库
  display isis statistics          # IS-IS 统计

VRRP 显示类：
  display vrrp                   # VRRP 信息
  display vrrp peer              # VRRP 对等体
  display vrrp statistics          # VRRP 统计

VLAN 显示类：
  display vlan                    # VLAN 信息
  display vlan summary            # VLAN 汇总
  display vlan interface          # VLAN 接口

ACL 显示类：
  display acl                    # ACL 信息
  display acl all                # 所有 ACL
  display acl config              # ACL 配置
  display acl packet-filter        # ACL 包过滤器

系统诊断类：
  display diagnose               # 诊断信息
  display diagnose system         # 系统诊断
  display diagnose hardware       # 硬件诊断
  display diagnose link            # 链接诊断
  display diagnose interface        # 接口诊断
  display diagnose ip              # IP 诊断
  display diagnose routing          # 路由诊断
  display diagnose all            # 全面诊断
```

### 3. 路由协议配置（50+ 个）

```
BGP 配置类：
  bgp                        # 启用 BGP
  bgp enable                  # 启用 BGP
  bgp disable                 # 禁用 BGP
  bgp as-number <as>         # 配置 AS 号
  bgp router-id <id>         # 配置 Router ID
  bgp preference <value>     # 配置本地优先
  bgp graceful-restart        # 优雅重启
  bgp timer bgp <time>     # 配置 BGP 定时器
  bgp connect-retry <count>  # 连接重试次数
  bgp peer <ip>               # 配置 BGP 邻居
  bgp peer as-number <as>   # 配置邻居 AS
  bgp peer password <pwd>    # 配置邻居密码
  bgp peer group <group>     # 配置对等体组
  bgp peer ignore-as-path    # 忽略 AS 路径
  bgp peer route-reflector   # 路由反射器
  bgp peer default-originate  # 默认始发 AS
  bgp peer advertisement-interval # 广告间隔
  bgp peer keepalive-interval # 保活间隔
  bgp peer hold-time         # 保持时间
  bgp peer connect-timer     # 连接定时器
  bgp peer min-advertisement-interval # 最小广告间隔
  bgp peer route-refresh     # 路由刷新

OSPF 配置类：
  ospf                       # 启用 OSPF
  ospf enable                 # 启用 OSPF
  ospf router-id <id>          # 配置 Router ID
  ospf cost <value>           # 配置开销
  ospf router-id <id>          # 配置 Router ID
  ospf preference <value>      # 配置优先级
  ospf network <net> area <area>  # 配置网络
  ospf timer ospf <time>    # 配置 OSPF 定时器
  ospf lsa-interval <time>   # 配置 LSA 间隔
  ospf opaque-lsa           # 不透明 LSA
  ospf demand-circuit         # 按需电路
  ospf mtu-ignore            # 忽略 MTU
  ospf log-adjacency-changes # 邓居变更日志
  ospf abr-type <type>        # ABR 类型
  ospf max-lsa <count>       # 最大 LSA 数

IS-IS 配置类：
  isis                        # 启用 IS-IS
  isis enable                 # 启用 IS-IS
  isis net-entity <id>       # 配置 NET Entity
  isis area <area>            # 配置区域
  isis level <level>          # 配置级别
  isis is-type <type>       # 配置 IS 类型
  isis authentication-mode   # 认证模式
  isis circuit-type <type>     # 电路类型
  isis authentication-password # 认证密码
  isis adjacency-check       # 邻居检查
  isis lsp-holddown           # LSP 保持
  isis lsp-fragment-threshold # LSP 分片阈值
  isis lsp-regeneration-level # LSP 重新生成级别
  isis metric-style <style>  # 度量风格
  isis hello-interval <time>  # Hello 间隔
  isis hello-multiplier       # Hello 倍数
  isis hello-padding <padding> # Hello 填充

RIP 配置类：
  rip                         # 启用 RIP
  rip enable                   # 启用 RIP
  rip version <version>       # 配置 RIP 版本
  rip preference <value>       # 配置优先级
  rip network <net>            # 配置网络
  rip interface <if>           # 配置接口
  rip ripversion <version>     # 配置 RIP 版本
  rip output-delay <delay>     # 输出延迟
  rip timers               # RIP 定时器
  rip undo-silent-peer        # 静默对等体
  rip split-horizon           # 水平分割

静态路由配置类：
  ip route-static <dest> <next> # 静态路由
  ip route-static <dest> <next> <pref> # 静态路由+优先级
  ip route-static <dest> null0 reject # 丢弃路由
  ip route-static <dest> blackhole # 黑洞路由
```

### 4. 接口配置（30+ 个）

```
接口视图命令：
  interface <interface>        # 进入接口配置
  description <text>         # 接口描述
  shutdown                    # 关闭接口
  undo shutdown               # 取消关闭

IP 层性配置：
  ip address <ip> <mask>     # 配置 IP 地址
  ip address <ip> <mask> <sec> # 三层地址
  ip address <ip6> <prefix>  # IPv6 地址
  ip address link-local        # 链路本地地址
  ip address unnumbered       # 编号地址
  ip address <ip> anycast     # 任播地址
  ip address dhcp-alloc        # DHCP 分配
  ip address pppoe-client    # PPPoE 客户端

接口参数配置：
  mtu <value>                 # 配置 MTU
  bandwidth <value>            # 配置带宽
  duplex {full|half|auto}  # 双工模式
  speed <value>                # 配置速率
  flow-control {enable|disable} # 流控

OSPF 接口配置：
  ospf enable                  # 在接口上启用 OSPF
  ospf area <area>             # OSPF 区域
  ospf cost <cost>              # OSPF 开销
  ospf authentication-type  # OSPF 认证类型
  ospf authentication-mode    # OSPF 认证模式
  ospf authentication-key-id  # OSPF 认证密钥
  ospfip network-type {broadcast|p2p|nbma} # 网络类型
  ospfip priority <prio>       # OSPF 优先级
  ospfip hello-interval <sec>  # Hello 间隔
  ospfip dead-interval <sec>  # 死亡间隔
  ospfip retransmit-interval # 重传间隔
  ospfip transmit-delay        # 传输延迟
  ospfip silent-interface       # 静默接口
  ospfip passive                # 被动模式

BGP 接口配置：
  bgp enable                   # 在接口上启用 BGP
  bgp authentication-mode      # 认证模式
  bgp authentication-key-id  # 认证密钥
  bgp ttl-security              # TTL 安全
  bgp bfd enable               # 启用 BFD
  bgp bfd min-tx-interval    # 最小 Tx 间隔
  bgp bfd min-rx-interval    # 最小 Rx 间隔
  bgp dampening enable         # 启用阻尼
  bgp dampening max-suppress # 最大抑制数

MPLS 接口配置：
  mpls enable                  # 在接口上启用 MPLS
  mpls ldp enable              # 在接口上启用 LDP
  mpls ldp global              # 全局 LDP
  mpls ldp transport-address  # 传输地址
  mpls ldp discovery          # LDP 发现
  mpls ldp igp sync            # IGP 同步
  mpls ldp igp cost            # IGP 开销
```

### 5. MPLS 配置（20+ 个）

```
MPLS 全局配置：
  mpls                        # 启用 MPLS
  mpls ldp enable             # 启用 LDP
  mpls ldp discovery          # LDP 发现
  mpls ldp igp sync            # IGP 同步
  mpls igp route-reflector    # 路由反射器
  mpls lsr-id <id>             # LSR ID
  mpls traffic-statistics     # 流量统计

LDP 配置：
  mpls ldp transport-address <ip> # 配置传输地址
  mpls ldp targeted-session <ip> # 目标会话
  mpls ldp session-id-length <len> # 会话 ID 长度
  mpls ldp hello-holdtime <time> # Hello 保持时间
  mpls ldp hello-interval <time> # Hello 间隔
  mpls ldp keepalive-holdtime <time> # 保活保持时间
  mpls ldp keepalive-interval <time> # 保活间隔
  mpls ldp label-distribution # 标签分发
  mpls ldp igp sync            # IGP 同步
  mpls ldp igp cost <cost>    # IGP 开销

静态 LSP 配置：
  mpls static-lsp <name>      # 静态 LSP
  mpls static-lsp ingress <label> <nhop> <out-label>
  mpls static-lsp transit <in-label> <out-label>

L2VPN 配置：
  mpls l2vpn enable          # 启用 L2VPN
  mpls l2vpn vpls-id <id>  # VPLS ID

SRv6 配置：
  srv6                         # 启用 SRv6
  srv6 locator <name> # 定义 Locator
  srv6 sid                    # 分配 SID
  srv6 encapsulation <type> # 封装类型
```

### 6. QoS 配置（40+ 个）

```
QoS 全局配置：
  qos                         # 进入 QoS 配置视图
  qos car enable               # 启用 CAR
  qos wred enable              # 启用 WRED
  qos scheduler-profile         # 调度器配置文件

CAR 配置：
  qos car <name> cir <rate> cbs <bs> # CAR 参数
  qos car <name> pir <rate>     # 峰冲参数

WRED 配置：
  qos wred <name> pir <rate>    # WRED 参数

队列配置：
  qos queue-profile <name>      # 队列配置文件
  qos queue-profile <name> schedule-profile <name>

调度器配置：
  qos scheduler-profile <name> type <type> # 调度器类型
  qos scheduler-profile <type> shaper <count> <interval>

分类器配置：
  qos classifier <name>        # 分类器
  qos classifier <name> if-match <acl> # 按接口匹配

行为配置：
  qos behavior <name> remark <text> # 行为
  qos behavior <name> count <cnt> # 计数

ACL 绑定：
  qos acl <name>             # 引用 ACL
  qos acl <name> mode <mode>   # 模式（permit/deny）

应用策略：
  qos apply <interface> <profile> # 应用到接口
```

### 7. ACL 配置（20+ 个）

```
ACL 基础配置：
  acl number <acl>            # 定义 ACL
  acl name <name>              # ACL 名称
  acl description <desc>       # ACL 描述
  acl step <val>             # 步长值
  acl rule [order] [permit/deny] <protocol> <source> <destination>

高级 ACL：
  acl advanced-acl <acl>      # 高级 ACL
  acl user-acl <acl>         # 用户 ACL
  acl user-acl <acl> [user]     # 用户 ACL

基本 ACL 规则：
  rule permit protocol <proto> source <source> [destination <dest> [fragment <frag>]
  rule deny protocol <proto> source <source> [destination <dest> [fragment <frag]
  rule 5 permit source <source> # 许可源前 5 位
  rule 5 deny destination <dest> # 拒绝目的地址前 5 位

ACL 应用：
  packet-filter <acl> # 在接口上应用包过滤器
```

### 8. 安全配置（30+ 个）

```
AAA 配置：
  aaa authentication-mode         # 认证模式
  aaa authentication-scheme       # 认证方案
  aaa local-user              # 本地用户
  aaa local-user password <pwd>   # 配置密码
  aaa local-user service-type <type> # 服务类型
  aaa authorization-attribute  <attr> # 授权属性

认证配置：
  radius-server template <name>  # RADIUS 服务器模板
  radius-server shared-key <key>    # 共享密钥
  radius-server retransmit-count <count> # 重传次数
  radius-server timeout <seconds>     # 超时
  radius-server authentication-type # 认证类型

HWTACACS 配置:
  hwtacacs-server template <name> # HWTACACS 服务器
  hwtacacs-server shared-key <key>    # 共享密钥

SSH 配置:
  ssh server enable            # 启用 SSH 服务器
  ssh user <user>              # SSH 用户
  ssh user authentication-type <type> # 认证类型

管理员配置：
  super password               # 配置管理员密码
  super password cipher <cipher> # 密码方式
  super authentication-mode     # 认证模式
```

### 9. VLAN 配置（20+ 个）

```
VLAN 基础配置：
  vlan <vlan-id>              # 创建 VLAN
  vlan <vlan-id> description <text> # VLAN 描述
  vlan batch <vlan-id> <id>     # 批量 VLAN

VLAN 接口配置：
  port link-type trunk {pvid <id> | dot1q} # Trunk 配置
  port link-type hybrid {pvid <id> # Hybrid 配置
  port trunk allow-passed-vlan <vlans> # 允许通过的 VLAN
  port trunk blocked-vlan <vlans>  # 阻止的 VLAN

VLAN 映射：
  qinq {all | none}             # VLAN 映射模式
  qinq enable                 # 启用 QinQ
  qinq {all | none} enable     # VLAN 转发使能

QinQ 配置：
  qinq <queue-id> {all | none}  # QinQ 队列
  qinq <queue-id> statistics    # QinQ 统计
```

---

## ✅ 建议的实现优先级

### 第一优先级：核心路由协议

1. ✅ BGP - 已完成基础支持
   - 补充：BGP 扩展社区、路径反射器、对等体组
2. ✅ OSPF - 已完成基础支持
   - 补充：ABR 支持、多实例、LSDB 管理
3. ✅ MPLS - 已完成基础支持
   - 补充：LDP 完整配置、静态 LSP、L2VPN、SRv6
4. ✅ 静态路由 - 需要实现
5. ✅ RIP - 需要实现
6. ✅ IS-IS - 需要实现

### 第二优先级：接口管理

7. ✅ IP 地址配置 - 已完成基础支持
   - 补充：IPv6 支持、DHCP、PPP、以太网属性
8. ✅ 接口参数 - 需要实现
   - 补充：双工、速率、流控

### 第三优先级：服务质量

9. ✅ QoS 分类器 - 已完成基础支持
   - 补充：CAR、WRED、调度器、队列管理

### 第四优先级：安全和访问控制

10. ⚠️ ACL 配置 - 需要完整实现
11. ⚠️ AAA 认证 - 需要实现
12. ⚠️ 管理员用户 - 需要实现

### 第五优先级：高级功能

13. ⚠️ VLAN 配置 - 需要实现
14. ⚠️ VRRP 冗余 - 需要实现
15. ⚠️ BFD - 需要实现

---

## 📝 命令数量对比

| 类别 | 标准命令数 | 当前实现 | 完成率 | 缺失数 |
|------|---------|---------|-------|--------|
| 系统视图 | 20 | 5 | 25% | 15 |
| 显示 | 80+ | 7 | 9% | 73+ |
| 路由协议 | 50+ | 4 | 8% | 46+ |
| 接口配置 | 30+ | 1 | 3% | 29+ |
| MPLS 配置 | 20+ | 1 | 5% | 19+ |
| QoS 配置 | 40+ | 1 | 3% | 39+ |
| ACL 配置 | 20+ | 0 | 0% | 20+ |
| 安全配置 | 30+ | 0 | 0% | 30+ |
| VLAN 配置 | 20+ | 0 | 0% | 20+ |
| **总计** | **310+** | **16** | **5%** | **294+** |

---

## 🎯 补充建议

### 短期 1：立即补充（高优先级）

1. **显示命令子命令扩展**（15 个）
   - display ip routing-table all
   - display ip fib
   - display ip interface statistics
   - display bgp route all
   - display bgp community
   - display ospf lsdb
   - display ospf abr-summary
   - display mpls lsp all
   - display qos queue all

2. **系统命令扩展**（5 个）
   - display device
   - display diagnose
   - display cpu
   - display memory
   - clock

3. **路由协议子命令**（10 个）
   - bgp peer all
   - bgp statistics
   - ospf peer all
   - ospf interface all
   - rip
   - isis
   - vrrp

### 短期 2：中期补充（中优先级）

4. **接口子命令**（10 个）
   - ip address ipv6
   - ip address link-local
   - mtu
   - bandwidth
   - duplex
   - speed
   - flow-control

5. **MPLS 子命令**（5 个）
   - mpls ldp global
   - mpls static-lsp
   - mpls l2vpn
   - srv6

6. **QoS 子命令**（4 个）
   - qos car
   - qos wred
   - qos scheduler-profile
   - qos queue-profile

### 短期 3：完整补充（低优先级）

7. **ACL 完整实现**（15 个）
8. **安全配置**（10 个）
9. **VLAN 配置**（10 个）
10. **高级功能**（5 个）

---

## 💻 实施建议

### 方案 1：快速补充核心命令

立即补充 294+ 个缺失命令中的前 50 个最常用的命令：

- display ip routing-table all
- display ip fib
- display bgp peer all
- display bgp route all
- display ospf peer all
- display ospf interface all
- display mpls lsp all
- display qos queue all
- display device
- display diagnose
- display cpu
- display memory
- rip
- isis
- vrrp
- clock

**预计时间**: 2-3 小时

### 方案 2：完整补充所有命令

补充所有 294+ 个缺失命令，确保完整的企业级路由器命令覆盖。

**预计时间**: 1-2 天

---

## 📝 推荐方案

**建议采用方案 1**：快速补充 50 个最常用的华为命令，可以立即提升命令覆盖到 20%+（从 5%）

### 具体补充命令清单

#### 显示命令（15 个）
1. display ip routing-table all
2. display ip fib
3. display ip interface statistics
4. display bgp peer all
5. display bgp route all
6. display bgp statistics
7. display bgp community
8. display ospf peer all
9. display ospf interface all
10. display ospf lsdb
11. display ospf abr-summary
12. display mpls lsp all
13. display qos queue all
14. display device
15. display diagnose

#### 系统命令（5 个）
16. display cpu
17. display memory
18. display clock
19. display timezone
20. display users

#### 路由协议命令（10 个）
21. bgp peer all
22. bgp statistics
23. bgp community
24. bgp update-group
25. ospf peer all
26. ospf interface all
27. ospf lsdb
28. ospf abr-summary
29. rip
30. isis

#### 配置命令（15 个）
31. ip address ipv6
32. ip address link-local
33. mtu
34. bandwidth
35. duplex
36. speed
37. flow-control
38. ospf enable
39. ospf cost
40. ospf timer
41. mpls ldp global
42. mpls ldp igp sync
43. qos car
44. qos wred
45. qos scheduler-profile
46. qos queue-profile
47. qos classifier
48. qos behavior
49. clock
50. ntp-service

---

**结论**：当前华为 CLI 命令覆盖率为 5%（16/310），建议立即补充 50 个常用命令提升到 21%（66/310）。

**下一步**：选择方案 1（快速补充）或方案 2（完整补充）
