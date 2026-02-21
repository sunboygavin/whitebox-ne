#!/bin/bash
# Web 管理界面启动脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== 白盒网元 Web 管理界面启动脚本 ==="
echo ""

# 检查 Python3
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到 Python3，请先安装 Python3"
    exit 1
fi

# 检查 pip3
if ! command -v pip3 &> /dev/null; then
    echo "错误: 未找到 pip3，请先安装 pip3"
    exit 1
fi

# 安装依赖
echo "1. 检查并安装 Python 依赖..."
pip3 install -r requirements.txt --quiet

# 检查 vtysh 是否可用
if ! command -v vtysh &> /dev/null; then
    echo "警告: 未找到 vtysh 命令，请确保 FRR 已正确安装"
    echo "      Web 界面将启动，但功能可能受限"
fi

# 启动 Flask 应用
echo ""
echo "2. 启动 Web 服务..."
echo "   访问地址: http://localhost:8080"
echo "   按 Ctrl+C 停止服务"
echo ""

python3 app.py
