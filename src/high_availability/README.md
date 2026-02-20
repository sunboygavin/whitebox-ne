# 高可用性 (High Availability) 模块

## 概述

本模块实现了华为 VRP 风格的高可用性功能，包括 VRRP、BFD 和 Track 机制。

## 功能特性

### 1. VRRP (Virtual Router Redundancy Protocol)

VRRP 提供网关冗余，确保网络的高可用性。

**支持的功能**:
- VRRP v2/v3 协议
- 虚拟 IP 地址配置（最多 8 个）
- 优先级配置 (1-254)
- 抢占模式和延迟
- 认证（简单密码、MD5）
- 通告间隔配置
- Track 接口/BFD 集成
- 优先级动态调整

**命令示例**:
```
# 配置 VRRP
interface GigabitEthernet0/0/1
  vrrp vrid 1
    version 2
    priority 120
    virtual-ip 192.168.1.1
    timer advertise 100
    preempt-mode timer delay 10
    authentication-mode md5 mypassword
    track interface GigabitEthernet0/0/2 reduced 20
    track bfd-session bfd-to-peer reduced 30

# 查看 VRRP 状态
display vrrp
display vrrp brief
display vrrp interface GigabitEthernet0/0/1
```

### 2. BFD (Bidirectional Forwarding Detection)

BFD 提供快速故障检测，检测时间可达毫秒级。

**支持的功能**:
- 单跳和多跳 BFD
- 可配置的检测定时器（3-20000 ms）
- 检测倍数配置 (3-50)
- Echo 模式支持
- 与静态路由、OSPF、BGP、IS-IS 集成
- 会话状态管理
- 详细的统计信息

**命令示例**:
```
# 配置 BFD 会话
bfd session-to-peer bind peer-ip 10.1.1.2 interface GigabitEthernet0/0/1 source-ip 10.1.1.1
  discriminator local 100
  min-tx-interval 100
  min-rx-interval 100
  detect-multiplier 3
  echo-mode interval 50
  session-type single-hop

# 为静态路由启用 BFD
bfd static-route 192.168.2.0/24 10.1.1.2 session-to-peer

# 查看 BFD 状态
display bfd session all
display bfd session session-to-peer
display bfd statistics
```

### 3. Track 机制

Track 机制监控网络对象状态，用于动态调整路由和 VRRP 优先级。

**支持的跟踪对象**:
- 接口状态（协议/链路）
- IP 路由可达性
- BFD 会话状态
- NQA 测试结果
- 布尔组合（AND/OR）

**命令示例**:
```
# 跟踪接口
track 1 name track-uplink
  interface GigabitEthernet0/0/1 protocol
  delay up 5 down 10

# 跟踪 IP 路由
track 2 name track-route
  ip route 192.168.1.0 255.255.255.0 reachability

# 跟踪 BFD 会话
track 3 name track-bfd
  bfd-session session-to-peer

# 跟踪 NQA 测试
track 4 name track-nqa
  nqa admin test1 threshold 100

# 布尔组合
track 10 name track-combined
  boolean and 1 2 3

# 查看 Track 状态
display track
display track 1
display track brief
```

## 配置流程

### 场景 1: VRRP 网关冗余

```
# 主路由器配置
interface GigabitEthernet0/0/1
  ip address 192.168.1.2 255.255.255.0
  vrrp vrid 1
    version 2
    priority 120
    virtual-ip 192.168.1.1
    preempt-mode timer delay 10
    authentication-mode md5 vrrp-secret

# 备份路由器配置
interface GigabitEthernet0/0/1
  ip address 192.168.1.3 255.255.255.0
  vrrp vrid 1
    version 2
    priority 100
    virtual-ip 192.168.1.1
    preempt-mode timer delay 10
    authentication-mode md5 vrrp-secret
```

### 场景 2: BFD 快速故障检测

```
# 配置 BFD 会话
bfd bfd-to-peer bind peer-ip 10.1.1.2 interface GigabitEthernet0/0/1
  min-tx-interval 50
  min-rx-interval 50
  detect-multiplier 3

# 静态路由使用 BFD
ip route-static 192.168.2.0 255.255.255.0 10.1.1.2 bfd-session bfd-to-peer

# OSPF 使用 BFD
ospf 1
  area 0
    bfd all-interfaces enable
```

### 场景 3: Track 动态调整 VRRP

```
# 配置 Track 监控上行接口
track 1 name track-uplink
  interface GigabitEthernet0/0/2 protocol
  delay up 5 down 10

# VRRP 使用 Track
interface GigabitEthernet0/0/1
  vrrp vrid 1
    priority 120
    virtual-ip 192.168.1.1
    track interface GigabitEthernet0/0/2 reduced 30

# 当上行接口故障时，VRRP 优先级降低 30
# 从 120 降到 90，触发主备切换
```

### 场景 4: 综合高可用方案

```
# 1. 配置 BFD 会话
bfd bfd-to-isp bind peer-ip 1.1.1.1 interface GigabitEthernet0/0/2
  min-tx-interval 100
  min-rx-interval 100
  detect-multiplier 3

# 2. 配置 Track 监控 BFD
track 1 name track-isp-link
  bfd-session bfd-to-isp
  delay up 5 down 5

# 3. VRRP 使用 Track
interface GigabitEthernet0/0/1
  vrrp vrid 1
    priority 120
    virtual-ip 192.168.1.1
    track bfd-session bfd-to-isp reduced 40
    preempt-mode timer delay 30

# 当 ISP 链路故障时：
# - BFD 在 300ms 内检测到故障
# - Track 在 5 秒延迟后变为 Negative
# - VRRP 优先级从 120 降到 80
# - 备份路由器（优先级 100）成为 Master
# - 30 秒抢占延迟避免频繁切换
```

## 命令参考

### VRRP 命令

| 命令 | 说明 |
|------|------|
| `vrrp vrid <vrid>` | 创建/进入 VRRP 实例 (1-255) |
| `version {2\|3}` | 设置 VRRP 版本 |
| `priority <priority>` | 设置优先级 (1-254) |
| `virtual-ip <ip>` | 配置虚拟 IP 地址 |
| `timer advertise <interval>` | 设置通告间隔（厘秒）|
| `preempt-mode [timer delay <delay>]` | 启用抢占模式 |
| `undo preempt-mode` | 禁用抢占模式 |
| `authentication-mode {simple\|md5} <key>` | 配置认证 |
| `track interface <if> [reduced <pri>]` | 跟踪接口 |
| `track bfd-session <name> [reduced <pri>]` | 跟踪 BFD 会话 |
| `display vrrp [brief] [interface <if>]` | 显示 VRRP 信息 |

### BFD 命令

| 命令 | 说明 |
|------|------|
| `bfd <name> bind peer-ip <ip> [interface <if>] [source-ip <ip>]` | 创建 BFD 会话 |
| `discriminator local <value>` | 设置本地鉴别符 |
| `min-tx-interval <interval>` | 设置最小发送间隔 (3-20000 ms) |
| `min-rx-interval <interval>` | 设置最小接收间隔 (3-20000 ms) |
| `detect-multiplier <multiplier>` | 设置检测倍数 (3-50) |
| `echo-mode [interval <interval>]` | 启用 Echo 模式 |
| `session-type {single-hop\|multi-hop}` | 设置会话类型 |
| `bfd static-route <dest> <nexthop> <session>` | 静态路由启用 BFD |
| `display bfd session [all\|<name>]` | 显示 BFD 会话 |
| `display bfd statistics` | 显示 BFD 统计 |

### Track 命令

| 命令 | 说明 |
|------|------|
| `track <id> [name <name>]` | 创建 Track 条目 (1-1024) |
| `interface <if> [protocol\|line]` | 跟踪接口状态 |
| `ip route <dest> <mask> [reachability]` | 跟踪 IP 路由 |
| `bfd-session <name>` | 跟踪 BFD 会话 |
| `nqa <admin> <test> [threshold <value>]` | 跟踪 NQA 测试 |
| `boolean {and\|or} <id> [<id> ...]` | 布尔组合 |
| `delay up <sec> down <sec>` | 配置延迟定时器 |
| `display track [<id>] [brief]` | 显示 Track 信息 |

## 技术细节

### VRRP 状态机

- **Initialize**: 初始状态
- **Backup**: 备份状态，监听 Master 通告
- **Master**: 主状态，发送通告报文

### BFD 检测时间计算

```
检测时间 = min-rx-interval × detect-multiplier

例如：
min-rx-interval = 100 ms
detect-multiplier = 3
检测时间 = 100 × 3 = 300 ms
```

### Track 状态

- **Positive**: 跟踪对象正常
- **Negative**: 跟踪对象故障
- **Not Ready**: 跟踪对象未就绪

### 布尔逻辑

- **AND**: 所有跟踪对象都为 Positive 时，结果为 Positive
- **OR**: 任一跟踪对象为 Positive 时，结果为 Positive

## 统计信息

### VRRP 统计
- Master 转换次数
- 发送/接收的通告报文数
- 优先级为 0 的报文数

### BFD 统计
- 发送/接收的报文数
- Up 时间
- Down 次数
- 最后状态变化时间

### Track 统计
- 状态变化次数
- Positive/Negative 持续时间
- 最后变化时间

## 文件结构

```
src/high_availability/
├── vrrp.c       # VRRP 实现 (450+ 行)
├── bfd.c        # BFD 实现 (400+ 行)
├── track.c      # Track 机制实现 (400+ 行)
├── ha_init.c    # 高可用性模块初始化
└── README.md    # 本文档
```

## 兼容性

本模块实现了华为 VRP 系统的核心高可用性命令，与以下设备兼容：
- 华为 AR 系列路由器
- 华为 NE 系列路由器
- 华为 S 系列交换机

## 限制

当前实现的限制：
- 最多 256 个 VRRP 实例
- 每个 VRRP 实例最多 8 个虚拟 IP
- 每个 VRRP 实例最多 16 个 Track 对象
- 最多 512 个 BFD 会话
- 最多 256 个 Track 条目
- 每个布尔 Track 最多 8 个子 Track

## 最佳实践

1. **VRRP 优先级规划**: 主路由器优先级建议 120，备份路由器 100，预留 20 的调整空间
2. **BFD 定时器**: 生产环境建议 min-tx/rx-interval ≥ 100ms，detect-multiplier = 3
3. **Track 延迟**: 配置适当的 delay 避免状态抖动
4. **抢占延迟**: VRRP 抢占延迟建议 ≥ 30 秒，避免频繁切换
5. **认证**: 生产环境建议使用 MD5 认证

## 故障排查

### VRRP 无法成为 Master
1. 检查优先级配置
2. 检查虚拟 IP 配置
3. 检查认证配置是否一致
4. 检查 Track 对象状态

### BFD 会话无法 Up
1. 检查对端 IP 可达性
2. 检查定时器配置
3. 检查接口状态
4. 检查防火墙规则（UDP 3784/3785）

### Track 状态异常
1. 检查被跟踪对象状态
2. 检查延迟定时器配置
3. 检查布尔逻辑配置

---

**版本**: 1.0
**日期**: 2026-02-20
**作者**: WhiteBox NE Team
