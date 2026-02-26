#!/bin/bash
#
# 白盒网元优化版本构建脚本
# 构建优化后的 Docker 镜像
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "======================================"
echo "白盒网元优化版构建脚本"
echo "======================================"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查 Docker
if ! command -v docker &> /dev/null; then
    echo -e "${RED}错误: Docker 未安装${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Docker 已安装${NC}"
echo ""

# 构建参数
IMAGE_NAME="${1:-whitebox-ne:optimized}"
DOCKERFILE="${2:-Dockerfile.optimized}"

echo "构建配置:"
echo "  镜像名称: $IMAGE_NAME"
echo "  Dockerfile: $DOCKERFILE"
echo ""

# 开始构建
echo -e "${YELLOW}开始构建 Docker 镜像...${NC}"
echo ""

BUILD_START=$(date +%s)

if docker build \
  -f "$DOCKERFILE" \
  -t "$IMAGE_NAME" \
  --progress plain \
  . 2>&1 | tee build.log; then
    
    BUILD_END=$(date +%s)
    BUILD_TIME=$((BUILD_END - BUILD_START))
    
    echo ""
    echo -e "${GREEN}======================================${NC}"
    echo -e "${GREEN}✓ 构建成功！${NC}"
    echo -e "${GREEN}======================================${NC}"
    echo ""
    
    # 显示镜像信息
    echo "镜像信息:"
    docker images | grep "$IMAGE_NAME" | head -1
    echo ""
    
    # 计算镜像大小
    IMAGE_SIZE=$(docker images "$IMAGE_NAME" --format "{{.Size}}")
    echo -e "${GREEN}镜像大小: $IMAGE_SIZE${NC}"
    echo -e "${GREEN}构建时间: ${BUILD_TIME} 秒${NC}"
    echo ""
    
    # 显示优化效果
    echo "优化效果 (对比原版):"
    echo "  原版大小: ~242 MB"
    echo "  优化版大小: $IMAGE_SIZE"
    echo "  预期减少: ~25% (↓60 MB)"
    echo "  构建时间预期减少: ~40% (↓2 min)"
    echo ""
    
    # 运行安全扫描 (可选)
    if command -v trivy &> /dev/null; then
        echo -e "${YELLOW}运行安全扫描...${NC}"
        echo ""
        
        if trivy image --severity HIGH,CRITICAL --no-progress "$IMAGE_NAME"; then
            echo -e "${GREEN}✓ 安全扫描完成${NC}"
        else
            echo -e "${RED}⚠ 发现安全漏洞，请查看报告${NC}"
        fi
        echo ""
    fi
    
    echo "使用方法:"
    echo "  运行容器: docker run -d --name whitebox-router $IMAGE_NAME"
    echo "  使用 Docker Compose: docker-compose -f docker-compose.optimized.yml up -d"
    echo "  进入命令行: docker exec -it whitebox-router vtysh"
    echo ""
    
    echo -e "${GREEN}构建完成！${NC}"
    
else
    echo ""
    echo -e "${RED}======================================${NC}"
    echo -e "${RED}✗ 构建失败${NC}"
    echo -e "${RED}======================================${NC}"
    echo ""
    echo "请查看构建日志: build.log"
    exit 1
fi
