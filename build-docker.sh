#!/bin/bash
#
# 白盒网元 Docker 镜像构建脚本
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "========================================"
echo "白盒网元 Docker 镜像构建"
echo "========================================"

# 检查 Docker 是否运行
if ! docker info >/dev/null 2>&1; then
    echo "❌ Docker 未运行，请先启动 Docker"
    exit 1
fi

# 检查必需文件
if [ ! -f "Dockerfile" ]; then
    echo "❌ Dockerfile 不存在"
    exit 1
fi

if [ ! -f "frr.docker.conf" ]; then
    echo "❌ frr.docker.conf 不存在"
    exit 1
fi

# 使用 Docker 配置文件
cp frr.docker.conf frr.conf

# 构建镜像
echo ""
echo "开始构建镜像..."
echo "镜像名称: whitebox-ne"
echo "镜像标签: latest"
echo ""

docker build -t whitebox-ne:latest .

echo ""
echo "========================================"
echo "构建完成！"
echo "========================================"

# 显示镜像信息
echo ""
echo "镜像列表:"
docker images | grep whitebox-ne

echo ""
echo "下一步操作:"
echo "  1. 启动容器: ./run-docker.sh"
echo "  2. 或使用 docker-compose: docker-compose up -d"
echo ""
