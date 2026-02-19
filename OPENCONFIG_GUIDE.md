# OpenConfig 集成指南

本文档说明如何在白盒路由器上使用 OpenConfig 进行标准化配置管理。

---

## 📖 概述

OpenConfig 是一个由运营商驱动的开源项目，旨在为网络设备提供标准化的配置和操作数据模型。通过集成 OpenConfig，白盒路由器可以实现：

- **标准化配置接口**：基于 YANG 模型的统一配置
- **Netconf 支持**：通过 Netconf 协议进行配置管理
- **gNMI 支持**：基于 gRPC 的配置和状态查询
- **多厂商兼容**：使用标准化的数据模型

---

## 🚀 快速开始

### 1. 启动 OpenConfig 服务

```bash
cd /root/.openclaw/workspace/whitebox-ne

# 赋予执行权限
chmod +x setup-openconfig.sh test-openconfig.sh

# 运行安装脚本
sudo ./setup-openconfig.sh
```

安装脚本会询问安装模式：

- **选项 1**: 完整安装（从源码编译 Sysrepo 和 Netopeer2）
- **选项 2**: 快速安装（使用预编译包）
- **选项 3**: 仅安装 YANG 模型

**推荐**：对于生产环境，选择选项 1（完整安装）。

### 2. 验证安装

```bash
# 检查 Netopeer2 服务状态
sudo systemctl status netopeer2

# 查看已安装的 YANG 模型
sysrepoctl --list

# 测试 Netopeer2 端口
nc -z localhost 830 && echo "Netopeer2 is listening on port 830"
```

### 3. 测试功能

```bash
# 运行测试套件
sudo ./test-openconfig.sh
```

---

## 📋 支持的 OpenConfig 模型

本项目支持以下 OpenConfig 标准模型：

| 模块名称 | 描述 | 功能范围 |
|---------|------|---------|
| **openconfig-interfaces** | 接口配置 | 接口名称、描述、状态、计数器 |
| **openconfig-bgp** | BGP 配置 | 全局参数、邻居配置、AFI-SAFI |
| **openconfig-network-instance** | 网络实例 | VRF、协议实例 |
| **openconfig-system** | 系统配置 | 主机名、NTP、时间等 |
| **openconfig-acl** | ACL 配置 | IPv4/IPv6 访问控制列表 |

---

## 🔧 使用 Netconf

### 连接到 Netconf 服务器

```bash
# 使用 netconf-tool（推荐）
netconf-tool --host localhost --port 830

# 使用 NETCONF 客户端库（Python）
from ncclient import manager

with manager.connect(
    host="localhost",
    port=830,
    username="admin",
    password="admin",
    hostkey_verify=False
) as m:
    print(m.connected)
```

### 配置接口示例

```xml
<?xml version="="1.0" encoding="UTF-8"?>
<rpc message-id="101" xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
  <edit-config>
    <target>
      <candidate/>
    </target>
    <config>
      <interfaces xmlns="http://openconfig.net/yang/interfaces">
        <interface>
          <name>eth0</name>
          <config>
            <description>Management Interface</description>
            <enabled>true</enabled>
          </config>
        </interface>
      </interfaces>
    </config>
  </edit-config>
</rpc>
```

### 配置 BGP 示例

```xml
<?xml version="1.0" encoding="UTF-8"?>
<rpc message-id="102" xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
  <edit-config>
    <target>
      <candidate/>
    </target>
    <config>
      <bg xmlns="http://openconfig.net/yang/bgp">
        <global>
          <config>
            <as>65001</as>
            <router-id>192.168.1.1</router-id>
          </config>
        </global>
        <neighbors>
          <neighbor>
            <neighbor-address>192.168.1.2</neighbor-address>
            <config>
              <peer-as>65002</peer-as>
              <enabled>true</enabled>
            </config>
          </neighbor>
        </neighbors>
      </bg>
    </config>
  </edit-config>
</rpc>
```

### 提交配置

```xml
<?xml version="1.0" encoding="UTF-8"?>
<rpc message-id="103" xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
  <commit/>
</rpc>
```

---

## 🔍 使用 gNMI（可选）

gNMI (gRPC Network Management Interface) 是基于 gRPC 的配置协议。

### 安装 gNMI 参考实现

```bash
# 克隆 OpenConfig 参考实现
cd /tmp
git clone https://github.com/openconfig/gnmi.git
cd gnmi

# 编译（需要 Go 环境）
go build -o gnmi-server ./cmd/gnmi_server

# 启动 gNMI 服务器
./gnmi-server -address localhost:9339
```

### 使用 gNMI 客户端

```bash
# 获取接口状态
gnmi-get -target localhost:9339 \
    -xpath /interfaces/interface[name=eth0]/state/oper-status

# 设置接口配置
gnmi-set -target localhost:9339 \
    -xpath /interfaces/interface[name=eth0]/config/enabled \
    -val true

# 订阅状态更新
gnmi-subscribe -target localhost:9339 \
    -xpath /interfaces/interface/state/oper-status
```

---

## 🔄 配置适配器

项目提供了两个配置适配器，用于在 FRR 配置和 YANG 模型之间进行转换。

### FRR → YANG 适配器

从 FRR 读取配置并转换为 YANG 格式：

```bash
# 手动触发配置同步
sudo /usr/local/bin/frr_to_yang_adapter

# 查看转换输出
sudo /usr/local/bin/frr_to_yang_adapter -v
```

### YANG → FRR 适配器

从 YANG 数据生成 FRR 配置：

```bash
# 手动触发配置应用
sudo /usr/local/bin/yang_to_frr_adapter

# 查看转换输出
sudo /usr/local/bin/yang_to_frr_adapter -v
```

---

## 🐳 Docker 部署

### 更新 Dockerfile

Dockerfile 已更新以包含 OpenConfig 支持：

```dockerfile
# 安装 OpenConfig 依赖
RUN apt-get update && apt-get install -y \
    libsysrepo-dev \
    libnetopeer2-dev \
    sysrepo \
    netopeer2

# 复制 OpenConfig 模型和适配器
COPY openconfig-models/ /opt/whitebox-ne/openconfig-models/
COPY src/openconfig_adapter/ /opt/whitebox-ne/src/openconfig_adapter/

# 构建适配器
RUN cd /opt/whitebox-ne/src/openconfig_adapter && make install

# 安装 YANG 模型
RUN sysrepoctl --install --yang=openconfig-interfaces.yang \
    --search-dir=/opt/whitebox-ne/openconfig-models/interfaces/

# 启动 Netopeer2 服务
CMD ["sh", "-c", "netopeer2-server -d -p 830 & frr start"]
```

### 构建和运行

```bash
# 重新构建镜像
cd /root/.openclaw/workspace/whitebox-ne
sudo docker build -t whitebox-ne:openconfig .

# 运行容器
sudo docker run -d --name whitebox-ne-openconfig \
            --privileged \
            -p 830:830 \
            whitebox-ne:openconfig

# 测试 Netconf 连接
netconf-tool --host localhost --port 830
```

---

## 🧪 故障排除

### Netopeer2 无法启动

**问题**: Netopeer2 服务启动失败

**解决方案**:

```bash
# 查看日志
sudo journalctl -u netopeer2 -n 50

# 检查端口占用
sudo netstat -tulpn | grep 830

# 手动启动
sudo netopeer2-server -d -p 830

# 检查进程
ps aux | grep netopeer2
```

### YANG 模型安装失败

**问题**: `sysrepoctl --install` 失败

**解决方案**:

```bash
# 检查 YANG 文件语法
pyang --lint openconfig-interfaces.yang

# 检查模型依赖
sysrepoctl --list

# 重新初始化 Sysrepo
sudo sysrepoctl --init
```

### 配置同步问题

**问题**: FRR 配置与 YANG 模型不同步

**解决方案**:

```bash
# 手动触发 FRR → YANG 同步
sudo /usr/local/bin/frr_to_yang_adapter

# 手动触发 YANG → FRR 同步
sudo /usr/local/bin/yang_to_frr_adapter

# 重启 Netopeer2 服务
sudo systemctl restart netopeer2
```

---

## 📚 参考资源

- [OpenConfig 官网](https://openconfig.net/)
- [OpenConfig GitHub](https://github.com/openconfig)
- [Sysrepo 文档](https://github.com/sysrepo/sysrepo)
- [Netopeer2 文档](https://github.com/sysrepo/netopeer2)
- [YANG 模型库](https://github.com/YangModels/yang)

---

## 🤝 贡献

欢迎为本项目贡献 OpenConfig 相关的改进：

1. Fork 项目
2. 创建功能分支 (`git checkout -b feature/your-feature`)
3. 提交更改 (`git commit -m 'Add your feature'`)
4. 推送到分支 (`git push origin feature/your-feature`)
5. 创建 Pull Request

---

## 📄 许可证

MIT License

---

**文档版本**: 1.0  
**最后更新**: 2024-02-20  
**作者**: WhiteBox NE Team
