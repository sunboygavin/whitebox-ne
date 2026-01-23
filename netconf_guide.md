# Netconf/YANG 集成指南

由于 Netconf 组件（libyang, sysrepo, netopeer2）依赖关系复杂，建议在生产环境中按照以下顺序编译安装：

## 1. 安装依赖
```bash
sudo apt install -y build-essential cmake libpcre2-dev libssh-dev libssl-dev pkg-config
```

## 2. 编译安装 libyang
```bash
git clone https://github.com/CESNET/libyang.git
cd libyang && mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc) && sudo make install
```

## 3. 编译安装 sysrepo
```bash
git clone https://github.com/sysrepo/sysrepo.git
cd sysrepo && mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DGEN_LANGUAGE_BINDINGS=OFF ..
make -j$(nproc) && sudo make install
```

## 4. 编译安装 Netopeer2
```bash
git clone https://github.com/CESNET/netopeer2.git
cd netopeer2 && mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc) && sudo make install
```

## 5. 验证
运行 `netopeer2-server -d` 启动服务，并使用 `netopeer2-cli` 进行连接测试。
