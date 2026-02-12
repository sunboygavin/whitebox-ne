#!/bin/bash
#
# WhiteBox Network Element (NE) Source Build Script
#
# 此脚本用于从源码编译安装经过“华为化”改造的 FRR 协议栈和自定义 SNMP 子代理。
#
# 依赖环境: Ubuntu 22.04 LTS

set -e

echo "--- 1. 安装编译依赖 ---"
sudo apt update
sudo apt install -y git autoconf automake libtool make gawk libreadline-dev \
     libelf-dev libjson-c-dev libpcre3-dev libcap-dev libpam0g-dev \
     python3-dev python3-pytest bison flex libnss3-dev libssl-dev \
     libsnmp-dev pkg-config libyang-dev

echo "--- 2. 下载 FRR 源码 (v8.1) ---"
cd /home/ubuntu
if [ ! -d "frr" ]; then
    git clone --branch frr-8.1 https://github.com/frrouting/frr.git
fi
cd frr

echo "--- 3. 应用华为风格 CLI 补丁 ---"
# 假设补丁文件在项目 src 目录下
PATCH_FILE="/home/ubuntu/whitebox-ne-project/src/frr_patch/0001-huawei-cli-native-support.patch"
if [ -f "$PATCH_FILE" ]; then
    git apply "$PATCH_FILE"
    echo "补丁应用成功！"
else
    echo "警告: 未找到补丁文件 $PATCH_FILE，跳过源码修改。"
fi

echo "--- 4. 编译并安装 FRR ---"
./bootstrap.sh
./configure \
    --prefix=/usr \
    --includedir=\${prefix}/include \
    --mandir=\${prefix}/share/man \
    --infodir=\${prefix}/share/info \
    --sysconfdir=/etc/frr \
    --localstatedir=/var/run/frr \
    --sbindir=/usr/lib/frr \
    --enable-snmp \
    --enable-user=frr \
    --enable-group=frr \
    --enable-vty-group=frrvty \
    --enable-configfile-mask=0640 \
    --enable-logfile-mask=0640
make -j$(nproc)
sudo make install

echo "--- 5. 编译自定义 SNMP 子代理 ---"
cd /home/ubuntu/whitebox-ne-project/src/snmp_subagent
make
echo "自定义 SNMP 子代理编译完成: $(pwd)/custom_subagent"

echo "--- 6. 后续配置 ---"
# 确保 FRR 用户和组存在
sudo groupadd -r -f frr
sudo groupadd -r -f frrvty
sudo useradd -g frr -G frrvty -r -d /var/run/frr -s /sbin/nologin -c "FRR routing suite" frr || true

# 创建配置目录
sudo mkdir -p /etc/frr
sudo chown -R frr:frr /etc/frr

echo "--- 构建完成！ ---"
echo "您可以运行以下命令启动自定义 SNMP 子代理："
echo "  sudo ./custom_subagent"
