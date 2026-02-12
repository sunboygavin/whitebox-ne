#!/bin/bash
#
# 白盒网元 Docker 容器运行脚本
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 配置变量
IMAGE_NAME="whitebox-ne:latest"
CONTAINER_NAME="whitebox-ne-router"
NETWORK_NAME="whitebox-network"

echo "========================================"
echo "白盒网元 Docker 容器运行"
echo "========================================"

# 检查镜像是否存在
if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "❌ 镜像 $IMAGE_NAME 不存在"
    echo "请先运行 ./build-docker.sh 构建镜像"
    exit 1
fi

# 检查容器是否已存在
if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "⚠️  容器 $CONTAINER_NAME 已存在"
    read -p "是否删除并重新创建？(y/N): " confirm
    if [[ "$confirm" =~ ^[Yy]$ ]]; then
        echo "删除旧容器..."
        docker stop "$CONTAINER_NAME" >/dev/null 2>&1 || true
        docker rm "$CONTAINER_NAME" >/dev/null 2>&1 || true
    else
        echo "❌ 操作取消"
        exit 1
    fi
fi

# 创建网络（如果不存在）
if ! docker network inspect "$NETWORK_NAME" >/dev/null 2>&1; then
    echo "创建网络: $NETWORK_NAME"
    docker network create "$NETWORK_NAME"
fi

# 创建并启动容器
echo ""
echo "创建并启动容器..."
echo "容器名称: $CONTAINER_NAME"
echo "网络: $NETWORK_NAME"
echo ""

docker run -d \
    --name "$CONTAINER_NAME" \
    --hostname whitebox-ne \
    --network "$NETWORK_NAME" \
    --privileged \
    --cap-add=NET_ADMIN \
    --cap-add=NET_RAW \
    --cap-add=SYS_ADMIN \
    -v "$SCRIPT_DIR/frr.docker.conf:/etc/frr/frr.conf:ro" \
    -v "$SCRIPT_DIR/logs:/var/log/frr" \
    -p 8161:161/udp \
    -p 8179:179/tcp \
    -p 8089:89/tcp \
    -p 8089:89/udp \
    -e TZ=Asia/Shanghai \
    --restart unless-stopped \
    "$IMAGE_NAME"

# 等待容器启动
echo ""
echo "等待容器启动..."
sleep 5

# 显示容器状态
echo ""
echo "========================================"
echo "容器状态"
echo "========================================"
docker ps
echo ""

# 显示容器 IP 地址
echo "容器 IP 地址:"
docker exec "$CONTAINER_NAME" ip addr show eth0 2>/dev/null | grep "inet " || echo "  暂无 IP"

echo ""
echo "========================================"
echo "访问方式"
echo "========================================"
echo "进入容器: docker exec -it $CONTAINER_NAME bash"
echo "进入 VTYSH: docker exec -it $CONTAINER_NAME vtysh"
echo "查看日志: docker exec -it $CONTAINER_NAME cat /var/log/frr/frr.log"
echo "重启容器: docker restart $CONTAINER_NAME"
echo ""
echo "SNMP 测试: snmpwalk -v2c -c public \$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' $CONTAINER_NAME) .1.3.6.1.2.1.1"
echo ""

# 检查服务健康状态
echo "服务健康检查:"
if docker exec "$CONTAINER_NAME" systemctl is-active --quiet frr; then
    echo "  ✅ FRR 服务: 运行中"
else
    echo "  ⚠️  FRR 服务: 未启动"
fi

if docker exec "$CONTAINER_NAME" systemctl is-active --quiet snmpd; then
    echo "  ✅ SNMP 服务: 运行中"
else
    echo "  ⚠️  SNMP 服务: 未启动"
fi

echo ""
echo "========================================"
echo "运行完成！"
echo "========================================"
