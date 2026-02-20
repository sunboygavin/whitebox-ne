# QoS (Quality of Service) 模块

## 概述

本模块实现了华为 VRP 风格的 QoS 功能，包括流量分类、流量行为、流量策略和队列管理。

## 功能特性

### 1. 流量分类器 (Traffic Classifier)

流量分类器用于识别和匹配特定的流量。

**支持的匹配条件**:
- ACL 匹配 (2000-3999)
- DSCP 值匹配 (0-63)
- IP 优先级匹配
- 源/目的 IP 地址匹配
- 源/目的端口匹配
- 协议匹配
- 接口匹配
- 包长度匹配

**命令示例**:
```
# 创建流量分类器
traffic classifier voice-traffic
  if-match dscp 46
  if-match acl 3000

# 查看分类器
display traffic classifier voice-traffic
```

### 2. 流量行为 (Traffic Behavior)

流量行为定义了对匹配流量执行的操作。

**支持的操作**:
- 重标记 DSCP/IP 优先级
- CAR (承诺访问速率) 限速
- 流量整形
- 优先级标记 (0-7)
- 拒绝流量
- 重定向

**命令示例**:
```
# 创建流量行为
traffic behavior voice-behavior
  remark dscp 46
  car cir 1000 cbs 125000
  priority 7

# 查看行为
display traffic behavior voice-behavior
```

### 3. 流量策略 (Traffic Policy)

流量策略将分类器和行为绑定在一起，并应用到接口。

**命令示例**:
```
# 创建流量策略
traffic policy qos-policy
  classifier voice-traffic behavior voice-behavior
  classifier data-traffic behavior data-behavior

# 应用到接口
interface GigabitEthernet0/0/1
  traffic-policy qos-policy inbound

# 查看策略
display traffic policy qos-policy
display qos statistics interface GigabitEthernet0/0/1
```

### 4. 队列管理 (Queue Management)

队列管理提供队列调度和拥塞避免功能。

**支持的调度算法**:
- PQ (Priority Queuing) - 优先级队列
- WFQ (Weighted Fair Queuing) - 加权公平队列
- CBWFQ (Class-Based WFQ) - 基于类的 WFQ

**支持的拥塞避免**:
- WRED (Weighted Random Early Detection)

**命令示例**:
```
# 配置队列
qos queue-profile GigabitEthernet0/0/1
  qos schedule cbwfq
  queue 0 bandwidth 2000 max-depth 64
  queue 1 bandwidth 1000 max-depth 32
  wred queue 1 min-threshold 16 max-threshold 48 drop-probability 10

# 查看队列
display qos queue interface GigabitEthernet0/0/1
```

## 配置流程

### 典型 QoS 配置步骤

1. **创建流量分类器**
   ```
   traffic classifier voice-traffic
     if-match dscp 46
   ```

2. **创建流量行为**
   ```
   traffic behavior voice-behavior
     priority 7
     car cir 2000
   ```

3. **创建流量策略并绑定**
   ```
   traffic policy qos-policy
     classifier voice-traffic behavior voice-behavior
   ```

4. **应用到接口**
   ```
   interface GigabitEthernet0/0/1
     traffic-policy qos-policy inbound
   ```

5. **配置队列调度**
   ```
   qos queue-profile GigabitEthernet0/0/1
     qos schedule cbwfq
     queue 0 bandwidth 3000
   ```

## 命令参考

### 流量分类器命令

| 命令 | 说明 |
|------|------|
| `traffic classifier <name>` | 创建/进入流量分类器 |
| `if-match acl <acl-number>` | 匹配 ACL |
| `if-match dscp <value>` | 匹配 DSCP 值 |
| `if-match ip-address source <ip>` | 匹配源 IP |
| `if-match ip-address destination <ip>` | 匹配目的 IP |
| `display traffic classifier [name]` | 显示分类器 |

### 流量行为命令

| 命令 | 说明 |
|------|------|
| `traffic behavior <name>` | 创建/进入流量行为 |
| `remark dscp <value>` | 重标记 DSCP |
| `car cir <rate> [cbs <size>]` | 配置 CAR 限速 |
| `priority <value>` | 设置优先级 (0-7) |
| `deny` | 拒绝流量 |
| `display traffic behavior [name]` | 显示行为 |

### 流量策略命令

| 命令 | 说明 |
|------|------|
| `traffic policy <name>` | 创建/进入流量策略 |
| `classifier <name> behavior <name>` | 绑定分类器和行为 |
| `traffic-policy <name> {inbound\|outbound}` | 应用策略到接口 |
| `display traffic policy [name]` | 显示策略 |
| `display qos statistics interface <if>` | 显示 QoS 统计 |

### 队列管理命令

| 命令 | 说明 |
|------|------|
| `qos queue-profile <interface>` | 配置队列配置文件 |
| `queue <id> [bandwidth <bw>] [max-depth <depth>]` | 配置队列 |
| `qos schedule {pq\|wfq\|cbwfq}` | 配置调度算法 |
| `wred queue <id> min-threshold <min> max-threshold <max> drop-probability <prob>` | 配置 WRED |
| `display qos queue interface <if>` | 显示队列配置 |

## 应用场景

### 场景 1: VoIP 流量优先

```
# 分类 VoIP 流量
traffic classifier voip
  if-match dscp 46

# 高优先级处理
traffic behavior voip-priority
  priority 7
  car cir 2000 cbs 250000

# 应用策略
traffic policy voice-first
  classifier voip behavior voip-priority

interface GigabitEthernet0/0/1
  traffic-policy voice-first inbound
```

### 场景 2: 带宽限制

```
# 分类下载流量
traffic classifier download
  if-match acl 3001

# 限制带宽
traffic behavior limit-download
  car cir 10000 cbs 1250000 pir 15000 pbs 1875000

# 应用策略
traffic policy bandwidth-control
  classifier download behavior limit-download

interface GigabitEthernet0/0/2
  traffic-policy bandwidth-control outbound
```

### 场景 3: 多级队列调度

```
# 配置 CBWFQ
qos queue-profile GigabitEthernet0/0/1
  qos schedule cbwfq
  queue 0 bandwidth 5000 max-depth 128    # 高优先级
  queue 1 bandwidth 3000 max-depth 64     # 中优先级
  queue 2 bandwidth 2000 max-depth 32     # 低优先级
  wred queue 2 min-threshold 16 max-threshold 28 drop-probability 20
```

## 技术细节

### CAR (Committed Access Rate)

CAR 使用令牌桶算法实现流量限速：
- **CIR** (Committed Information Rate): 承诺信息速率
- **CBS** (Committed Burst Size): 承诺突发大小
- **PIR** (Peak Information Rate): 峰值信息速率
- **PBS** (Peak Burst Size): 峰值突发大小

### WRED (Weighted Random Early Detection)

WRED 通过随机丢弃实现拥塞避免：
- **min-threshold**: 最小阈值，低于此值不丢包
- **max-threshold**: 最大阈值，高于此值全部丢包
- **drop-probability**: 在阈值之间的丢包概率

## 统计信息

QoS 模块收集以下统计信息：
- 匹配的数据包数
- 匹配的字节数
- 入队数据包数
- 出队数据包数
- 丢弃数据包数

使用 `display qos statistics` 命令查看详细统计。

## 文件结构

```
src/qos/
├── classifier.c    # 流量分类器实现
├── behavior.c      # 流量行为实现
├── policy.c        # 流量策略实现
├── queue.c         # 队列管理实现
├── qos_init.c      # QoS 模块初始化
└── README.md       # 本文档
```

## 兼容性

本模块实现了华为 VRP 系统的核心 QoS 命令，与以下设备兼容：
- 华为 AR 系列路由器
- 华为 NE 系列路由器
- 华为 S 系列交换机（部分命令）

## 限制

当前实现的限制：
- 最多 256 个分类器
- 最多 256 个行为
- 最多 128 个策略
- 每个分类器最多 16 个匹配条件
- 每个行为最多 8 个操作
- 每个策略最多 32 个规则
- 每个接口最多 8 个队列

## 未来增强

计划添加的功能：
- 流量监管 (Traffic Policing)
- 流量整形 (Traffic Shaping)
- 层次化 QoS (HQoS)
- 更多队列调度算法
- QoS 模板支持
- 动态 QoS 调整

---

**版本**: 1.0
**日期**: 2026-02-20
**作者**: WhiteBox NE Team
