# 白盒网元代码和镜像上传指南

本文档提供将白盒网元代码和 Docker 镜像上传到 GitHub 的完整方案。

---

## 📋 上传清单

### ✅ 已完成

- [x] 本地 git 提交
- [x] 代码更新完成
- [x] .gitignore 配置（排除大文件）
文件
- [x] Docker 镜像导出 (whitebox-ne-latest.tar)

### ⏳ 待完成

- [ ] 推送代码到 GitHub
- [ ] 上传 Docker 镜像到 Docker Hub（可选）

---

## 🚀 方案一：推送代码到 GitHub

### 1. 检查 git 状态

```bash
cd /root/.openclaw/workspace/whitebox-ne

# 查看当前分支
git branch

# 查看提交历史
git log --oneline -5

# 查看远程仓库
git remote -v
```

### 2. 推送到 GitHub

**正常推送**:
```bash
cd /root/.openclaw/workspace/whitebox-ne
git push origin master
```

**如果推送失败，尝试以下方法**:

#### 方法 A: 使用 SSH

```bash
# 检查 SSH 密钥
ls -la ~/.ssh/id_rsa*

# 测试 SSH 连接
ssh -T git@github.com

# 强制使用 SSH 推送
git remote set-url origin git@github.com:sunboygavin/whitebox-ne.git
git push origin master
```

#### 方法 B: 使用 VPN

```bash
# 启动 VPN
cd /root/.openclaw/workspace/Anycast
./start-vpn.sh

# 设置 git 代理
git config --global http.proxy http://127.0.0.1:1087
git config --global https.proxy http://127.0.0.1:1087

# 推送
cd /root/.openclaw/workspace/whitebox-ne
git push origin master

# 推送完成后取消代理
git config --global --unset http.proxy
git config --global --unset https.proxy
```

#### 方法 C: 使用 Personal Access Token

```bash
# 设置远程 URL（包含 token）
git remote set-url origin https://<YOUR_TOKEN>@github.com/sunboygavin/whitebox-ne.git

# 推送
git push origin master
```

### 3. 验证推送成功

```bash
# 查看远程分支
git branch -r

# 或在浏览器访问
# https://github.com/sunboygavin/whitebox-ne
```

---

## 🐳 方案二：上传 Docker 镜像

Docker 镜像文件 (239 MB) 不适合直接上传到 git。有两种方案：

### 方案 A: 使用 Docker Hub（推荐）

#### 1. 注册 Docker Hub 账号

访问 https://hub.docker.com/ 并注册账号。

#### 2. 登录 Docker Hub

```bash
docker login
# 输入用户名和密码
```

#### 3. 重新标记镜像

```bash
# 重命名为 Docker Hub 格式
docker tag whitebox-ne:latest <your-dockerhub-username>/whitebox-ne:latest
```

#### 4. 推送镜像到 Docker Hub

```bash
# 推送镜像
docker push <your-dockerhub-username>/whitebox-ne:latest

# 推送完成后，在 README.md 中添加使用说明：
# docker pull <your-dockerhub-username>/whitebox-ne:latest
```

#### 5. 验证镜像

```bash
# 在其他机器上拉取
docker pull <your-dockerhub-username>/whitebox-ne:latest

# 运行
docker run -d --name whitebox-ne <your-dockerhub-username>/whitebox-ne:latest
```

### 方案 B: 使用 GitHub Packages（推荐）

#### 1. 登录 GitHub Container Registry

```bash
# 使用 Personal Access Token
# Token 权限需要包含: write:packages

echo <YOUR_GITHUB_TOKEN> | docker login ghcr.io -u <your-username> --password-stdin
```

#### 2. 重新标记镜像

```bash
# 重命名为 GitHub Packages 格式
docker tag whitebox-ne:latest ghcr.io/sunboygavin/whitebox-ne:latest
```

#### 3. 推送镜像到 GitHub Packages

```bash
# 推送镜像
docker push ghcr.io/sunboygavin/whitebox-ne:latest
```

#### 4. 更新 README.md

在 README.md 中添加拉取说明：

```markdown
## 使用 Docker 镜像

### 从 GitHub Packages 拉取

```bash
# 拉取镜像
docker pull ghcr.io/sunboygavin/whitebox-ne:latest

# 运行容器
docker run -d --name whitebox-ne-router ghcr.io/sunboygavin/whitebox-ne:latest
```

### 进入容器

```bash
docker exec -it whitebox-ne-router vtysh
```
```

#### 5. 验证镜像

访问: https://github.com/sunboygavin/whitebox-ne/packages

### 方案 C: 使用文件传输（手动）

如果上述方案不可用，可以使用文件传输：

#### 1. 传输 tar 文件

```bash
# 使用 scp 传输到目标服务器
scp /root/.openclaw/workspace/whitebox-ne/whitebox-ne-latest.tar user@server:/path/

# 或使用 rsync
rsync -avz /root/.openclaw/workspace/whitebox-ne/whitebox-ne-latest.tar user@server:/path/
```

#### 2. 在目标服务器加载

```bash
# 加载镜像
docker load -i whitebox-ne-latest.tar

# 验证
docker images | grep whitebox-ne

# 运行
docker run -d --name whitebox-ne-router whitebox-ne:latest
```

---

## 📝 更新 GitHub 仓库说明

推送成功后，在 GitHub 仓库页面更新 README：

### 添加的标签

- `docker` - Docker 支持
- `frrouting` - FRR 路由协议栈
- `networking` - 网络设备
- `router` - 路由器
- `bgp` - BGP 协议
- `ospf` - OSPF 协议
- `snmp` - 网络管理

### Topics

在 GitHub 仓库设置中添加以下 Topics：
- docker
- frrouting
- bgp
- ospf
- vrrp
- snmp
- networking
- router
- whitebox
- network-element

---

## 🔧 自动化方案（可选）

### GitHub Actions 自动构建

创建 `.github/workflows/docker-build.yml`：

```yaml
name: Build and Push Docker Image

on:
  push:
    branches: [ master ]
    tags:
      - 'v*'
  pull_request:
    branches: [ master ]

jobs:
  build-and-push:
    runs-on: ubuntu-latest

    steps:
    - name: Checkout code
      uses: actions/checkout@v3

    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v2

    - name: Login to Docker Hub
      uses: docker/login-action@v2
      with:
        username: ${{ secrets.DOCKER_USERNAME }}
        password: ${{ secrets.DOCKER_PASSWORD }}

    - name: Build and push
      uses: docker/build-push-action@v4
      with:
        context: .
        push: true
        tags: |
          sunboygavin/whitebox-ne:latest
          sunboygavin/whitebox-ne:${{ github.sha }}
        cache-from: type=registry,ref=sunboygavin/whitebox-ne:buildcache
        cache-to: type=registry,ref=sunboygavin/whitebox-ne:buildcache,mode=max
```

### 设置 Secrets

在 GitHub 仓库设置中添加以下 Secrets：

1. `DOCKER_USERNAME` - Docker Hub 用户名
2. `DOCKER_PASSWORD` - Docker Hub 密码或 Access Token

### 推送触发构建

```bash
# 添加标签并推送
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

---

## 📊 上传状态检查

### 1. 检查 Git 推送状态

```bash
cd /root/.openclaw/workspace/whitebox-ne

# 查看本地和远程的差异
git diff origin/master master

# 查看推送状态
git log origin/master..master
```

### 2. 检查 Docker 镜像

```bash
# 查看本地镜像
docker images | grep whitebox-ne

# 查看镜像历史
docker history whitebox-ne:latest
```

### 3. 检查文件大小

```bash
# 查看导出的 tar 文件
ls -lh /root/.openclaw/workspace/whitebox-ne/whitebox-ne-latest.tar
```

---

## 🎯 推荐方案

### 最简单方案

1. **推送代码到 GitHub**（使用 SSH 或 Personal Access Token）
2. **在 README.md 中提供构建说明**（不上传镜像）

```markdown
## Docker 部署

```bash
# 克隆仓库
git clone https://github.com/sunboygavin/whitebox-ne.git
cd whitebox-ne

# 构建镜像
./build-docker.sh

# 运行容器
./run-docker.sh
```
```

### 完整方案（推荐）

1. **推送代码到 GitHub**
2. **上传镜像到 GitHub Packages**
3. **在 README.md 中提供拉取说明**

---

## 📞 帮助资源

- [GitHub 文档](https://docs.github.com/)
- [Docker Hub 文档](https://docs.docker.com/docker-hub/)
- [GitHub Packages 文档](https://docs.github.com/en/packages)
- [Git 文档](https://git-scm.com/doc)

---

## 📝 总结

| 任务 | 方法 | 状态 |
|------|------|------|
| 推送代码 | git push origin master | ⏳ 待完成 |
| 上传镜像 | GitHub Packages/ Docker Hub | ⏳ 可选 |
| 更新文档 | GitHub Web UI | ⏳ 可选 |

---

**下一步**: 选择适合您的方法完成上传。

如有问题，请参考上述文档或联系支持。
