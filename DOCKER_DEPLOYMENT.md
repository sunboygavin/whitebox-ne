# 白盒网元 Docker 部署指南

本文档提供了使用 Docker 部署白盒网元（WhiteBox Network Element）的完整指南。

## 📋 目录

- [快速开始](#快速开始)
- [方式一：使用构建脚本](#方式一使用构建脚本)
- [方式二：使用 Docker Compose](#方式二使用-docker-compose)
- [手动构建和运行](#手动构建和运行)
- [验证部署](#验证部署)
- [配置说明](#配置说明)
- [网络拓扑示例](#网络拓扑示例)
- [故障排除](#故障排除)

---

## 快速开始

### 前提条件

- Docker 已安装并运行
- Docker Compose 已安装（可选）

### 一键部署

```bash
# 1. 构建镜像
./build-docker.sh

# 2. 运行容器
./run-docker.sh

# 3. 进入 FRR 命令行
docker exec -it whitebox-ne-router vtysh
```

---

## 方式一：使用构建脚本

### 1. 构建镜像

```bash
cd /root/.openclaw/workspace/whitebox-ne
chmod +x build-docker.sh
./build-docker.sh
```

### 2. 运行容器

```bash
chmod +x run-docker.sh
./run-docker.sh
```

### 3. 访问容器

```bash
# 进入容器 Shell
docker exec -it whitebox-ne-router bash

# 进入 FRR 命令行
docker exec -it whitebox-ne-router vtysh

# 查看日志
docker exec -it whitebox-ne-router cat /var/log/frr/frr.log

# 重启服务
docker exec -it whitebox-ne-router service frr restart
```

---

## 方式二：使用 Docker Compose

### 1. 构建并启动

```bash
cd /root/.openclaw/workspace/whitebox-ne

# 构建镜像并启动容器
docker-compose up -d

# 查看日志
docker-compose logs -f

# 查看容器状态
docker-compose ps
```

### 2. 启动对等网元（可选）

```bash
# 启动第二个网元（用于测试网络拓扑）
docker-compose --profile peer up -d

# 查看两个网元
docker-compose ps
```

### 3. 停止和清理

```bash
# 停止容器
docker-compose stop

# 停止并删除容器
docker-compose down

# 停止并删除容器、网络和卷
docker-compose down -v
```

---

## 手动构建和运行

### 1. 构建 Docker 镜像

```bash
cd /root/.openclaw/workspace/whitebox-ne

docker build -t whitebox-ne:latest .
```

### 2. 创建网络

```bash
docker network create whitebox-network
```

### 3. 运行容器

```bash
docker run -d \
    --name whitebox-ne-router \
    --hostname whitebox-ne \
    --network whitebox-network \
    --privileged \
    --cap-add=NET_ADMIN \
    --cap-add=NET_RAW \
    --cap-add=SYS_ADMIN \
    -v ./frr.docker.conf:/etc/frr/frr.conf:ro \
    -v ./logs:/var/log/frr \
    -p 161:161/udp \
    -p 179:179/tcp \
    -p 89:89/tcp \
    -p 89:89/udp \
    -e TZ=Asia/Shanghai \
    --restart unless-stopped \
    whitebox-ne:latest
```

### 4. 进入容器

```bash
# 交互式进入容器
docker exec -it whitebox-ne-router bash

# 或者直接进入 VTYSH
docker exec -it whitebox-ne-router vtysh
```

---

## 验证部署

### 1. 检查容器状态

```bash
docker ps | grep whitebox-ne
```

### 2. 检查服务状态

```bash
# FRR 服务
docker exec -it whitebox-ne-router systemctl status frr

# SNMP 服务
docker exec -it whitebox-ne-router systemctl status snmpd
```

### 3. 检查网络接口

```bash
docker exec -it whitebox-ne-router ip addr show
```

### 4. 测试 FRR 功能

```bash
# 进入 VTYSH
docker exec -it whitebox-ne-router vtysh

# 查看运行配置
show running-config

# 查看 OSPF 状态
show ip ospf interface

# 查看 BGP 状态
show ip bgp summary

# 查看 VRRP 状态
show vrrp

# 退出
exit
```

### 5. 测试 SNMP

```bash
# 获取容器 IP
CONTAINER_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' whitebox-ne-router)

# 测试 SNMP
snmpwalk -v2c -c public $CONTAINER_IP .1.3.6.1.2.1.1.1.0
```

---

## 配置说明

### 端口映射

| 端口 | 协议 | 说明 |
|------|------|------|
| 161 | UDP | SNMP (网络管理) |
| 179 | TCP | BGP (边界网关协议) |
| 89 | TCP/UDP | OSPF (开放最短路径优先) |

### 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| TZ | Asia/Shanghai | 时区设置 |

### 卷挂载

| 路径 | 说明 |
|------|------|
| `/etc/frr/frr.conf` | FRR 配置文件（只读） |
| `/var/log/frr` | FRR 日志目录 |

### 容器权限

容器需要以下权限才能正常运行路由功能：

- `privileged`: 特权模式（某些路由功能需要）
- `NET_ADMIN`: 网络管理权限
- `NET_RAW`: 原始套接字权限
- `SYS_ADMIN`: 系统管理权限

---

## 网络拓扑示例

### 单网元测试

最简单的部署是单个网元：

```bash
docker-compose up -d
```

### 双网元拓扑（测试 BGP/OSPF）

启动两个网元进行互连测试：

```bash
# 启动主网元和对等网元
docker-compose --profile peer up -d

# 查看两个网元
docker-compose ps
```

### 自定义网络拓扑

使用 Docker 网络创建自定义拓扑：

```bash
# 创建网络
docker network create net1
docker network create net2

# 启动网元并连接到多个网络
docker run -d --name router1 --network net1 whitebox-ne:latest
docker run -d --name router2 --network net1 whitebox-ne:latest
docker run -d --name router3 --network net2 whitebox-ne:latest

# 连接网络
docker network connect net2 router1
docker network connect net2 router2
```

---

## 故障排除

### 容器无法启动

```bash
# 查看容器日志
docker logs whitebox-ne-router

# 查看详细日志
docker logs --tail 100 whitebox-ne-router
```

### FRR 服务未运行

```bash
# 进入容器检查
docker exec -it whitebox-ne-router bash

# 查看 FRR 状态
systemctl status frr

# 查看 FRR 日志
cat /var/log/frr/frr.log

# 手动启动 FRR
service frr start
```

### 网络接口问题

```bash
# 查看网络接口
docker exec -it whitebox-ne-router ip addr show

# 检查容器网络
docker inspect whitebox-ne-router | grep -A 10 NetworkSettings
```

### 权限问题

确保容器以足够权限运行：

```bash
# 检查容器能力
docker inspect whitebox-ne-router | grep CapAdd

# 如果缺少权限，重新创建容器
docker stop whitebox-ne-router
docker rm whitebox-ne-router
./run-docker.sh
```

### 配置文件问题

```bash
# 验证配置文件语法
docker run --rm -v ./frr.docker.conf:/etc/frr/frr.conf:ro whitebox-ne:latest \
    vtysh -c "show running-config"
```

---

## 进阶使用

### 自定义配置

1. 修改 `frr.docker.conf` 文件
2. 重启容器以应用新配置：

```bash
docker restart whitebox-ne-router
```

### 构建带华为风格 CLI 的镜像

如果需要华为风格 CLI（`system-view`、`display` 命令）：

```bash
# 1. 修改 Dockerfile 使用构建脚本
# 2. 在 Dockerfile 中添加：
#    COPY build_from_source.sh /tmp/
#    RUN bash /tmp/build_from_source.sh
# 3. 重新构建镜像
```

### 持久化数据

使用 Docker 卷持久化配置和日志：

```bash
# 创建卷
docker volume create whitebox-config
docker volume create whitebox-logs

# 运行容器
docker run -d \
    --name whitebox-ne-router \
    -v whitebox-config:/etc/frr \
    -v whitebox-logs:/var/log/frr \
    whitebox-ne:latest
```

---

## 参考资源

- [FRRouting 文档](https://docs.frr.org/)
- [Docker 文档](https://docs.docker.com/)
- [Net-SNMP 文档](http://www.net-snmp.org/)
- 项目主仓库: https://github.com/sunboygavin/whitebox-ne.git

---

**部署完成！** 🎉

如有问题，请查看日志或联系支持。
