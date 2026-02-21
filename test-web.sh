#!/bin/bash
# Web 管理界面测试脚本

echo "=== 白盒网元 Web 管理界面测试 ==="
echo ""

# 测试 1: 检查文件是否存在
echo "1. 检查文件完整性..."
files=(
    "src/web_management/app.py"
    "src/web_management/templates/index.html"
    "src/web_management/requirements.txt"
    "src/web_management/start.sh"
)

for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        echo "  ✓ $file"
    else
        echo "  ✗ $file (缺失)"
        exit 1
    fi
done

# 测试 2: 检查 Python3
echo ""
echo "2. 检查 Python3..."
if command -v python3 &> /dev/null; then
    echo "  ✓ Python3: $(python3 --version)"
else
    echo "  ✗ Python3 未安装"
    exit 1
fi

# 测试 3: 检查 pip3
echo ""
echo "3. 检查 pip3..."
if command -v pip3 &> /dev/null; then
    echo "  ✓ pip3: $(pip3 --version)"
else
    echo "  ✗ pip3 未安装"
    exit 1
fi

# 测试 4: 检查 vtysh
echo ""
echo "4. 检查 vtysh..."
if command -v vtysh &> /dev/null; then
    echo "  ✓ vtysh 已安装"
else
    echo "  ⚠ vtysh 未安装（Web 界面功能将受限）"
fi

# 测试 5: 检查端口 8080
echo ""
echo "5. 检查端口 8080..."
if netstat -tuln 2>/dev/null | grep -q ":8080 "; then
    echo "  ⚠ 端口 8080 已被占用"
else
    echo "  ✓ 端口 8080 可用"
fi

echo ""
echo "=== 测试完成 ==="
echo ""
echo "启动 Web 管理界面："
echo "  ./start-web.sh"
echo ""
echo "或者："
echo "  cd src/web_management && ./start.sh"
