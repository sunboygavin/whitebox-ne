#!/bin/bash
# 白盒网元 Web 管理界面快速启动脚本

set -e

echo "==================================="
echo "  白盒网元 Web 管理界面"
echo "  WhiteBox NE Web Management"
echo "==================================="
echo ""

# 检查是否在项目根目录
if [ ! -d "src/web_management" ]; then
    echo "错误: 请在项目根目录运行此脚本"
    exit 1
fi

# 进入 web_management 目录
cd src/web_management

# 执行启动脚本
./start.sh
