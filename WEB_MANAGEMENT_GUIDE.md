# 白盒网元 Web 管理界面使用指南

## 概述

白盒网元 Web 管理界面是一个基于 Flask 的图形化配置管理工具，提供直观的界面来监控和管理网络设备。

## 快速开始

### 本地部署

```bash
# 进入项目根目录
cd /path/to/whitebox-ne

# 启动 Web 界面
./start-web.sh

# 或者直接进入 web_management 目录
cd src/web_management
./start.sh
```

访问 http://localhost:8080

### Docker 部署

```bash
# 使用 docker-compose 启动（推荐）
docker-compose up -d

# 访问 Web 界面
# http://localhost:8080
```

## 功能模块

### 1. 仪表盘

显示系统基本信息：
- 主机名
- 系统版本
- 更新时间

### 2. 接口管理

查看所有网络接口的状态：
- 接口名称
- IP 地址
- 状态（up/down）
- MTU 等信息

**操作步骤：**
1. 点击"接口管理"标签
2. 点击"刷新"按钮加载最新数据

### 3. 路由管理

查看系统路由表：
- 目标网络
- 网关
- 接口
- 协议类型

**操作步骤：**
1. 点击"路由表"标签
2. 点击"刷新"按钮查看路由信息

### 4. OSPF 监控

查看 OSPF 协议状态：
- OSPF 邻居列表
- 邻居状态
- 区域信息

**操作步骤：**
1. 点击"OSPF"标签
2. 点击"刷新"按钮查看 OSPF 邻居

### 5. BGP 监控

查看 BGP 协议状态：
- BGP 邻居摘要
- 会话状态
- 路由数量

**操作步骤：**
1. 点击"BGP"标签
2. 点击"刷新"按钮查看 BGP 摘要

### 6. VRRP 状态

查看 VRRP 高可用状态：
- VRRP 组信息
- 主备状态
- 虚拟 IP

**操作步骤：**
1. 点击"VRRP"标签
2. 点击"刷新"按钮查看 VRRP 状态

### 7. 配置管理

查看和保存设备配置：
- 查看运行配置
- 保存配置到启动配置

**操作步骤：**
1. 点击"配置管理"标签
2. 点击"查看配置"查看当前运行配置
3. 点击"保存配置"将配置写入文件

### 8. 命令行

在 Web 界面执行 vtysh 命令：
- 支持所有查询命令（show/display）
- 实时显示命令输出

**操作步骤：**
1. 点击"命令行"标签
2. 在输入框中输入命令（例如：show ip route）
3. 点击"执行"按钮或按回车键
4. 查看命令输出

**注意：** 出于安全考虑，只支持查询命令，不支持配置修改命令。

## 安全建议

### 生产环境部署

1. **修改默认端口**

编辑 `src/web_management/app.py`：
```python
app.run(host='0.0.0.0', port=8443, debug=False)
```

2. **添加身份认证**

安装 Flask-HTTPAuth：
```bash
pip3 install Flask-HTTPAuth
```

在 `app.py` 中添加：
```python
from flask_httpauth import HTTPBasicAuth

auth = HTTPBasicAuth()

users = {
    "admin": "your_password_here"
}

@auth.verify_password
def verify_password(username, password):
    if username in users and users[username] == password:
        return username

@app.route('/api/system/info')
@auth.login_required
def system_info():
    # ... 现有代码
```

3. **使用 HTTPS**

使用 Nginx 作为反向代理：
```nginx
server {
    listen 443 ssl;
    server_name your-domain.com;

    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;

    location / {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

4. **配置防火墙**

```bash
# 只允许特定 IP 访问
sudo ufw allow from 192.168.1.0/24 to any port 8080

# 或使用 iptables
sudo iptables -A INPUT -p tcp --dport 8080 -s 192.168.1.0/24 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 8080 -j DROP
```

5. **使用生产级 WSGI 服务器**

安装 gunicorn：
```bash
pip3 install gunicorn
```

启动服务：
```bash
gunicorn -w 4 -b 0.0.0.0:8080 app:app
```

## 故障排除

### 问题 1: 无法访问 Web 界面

**症状：** 浏览器无法打开 http://localhost:8080

**解决方案：**
1. 检查服务是否启动：
   ```bash
   ps aux | grep python3
   ```

2. 检查端口是否被占用：
   ```bash
   netstat -tuln | grep 8080
   ```

3. 检查防火墙规则：
   ```bash
   sudo ufw status
   ```

### 问题 2: 命令执行失败

**症状：** 执行命令时显示错误

**解决方案：**
1. 确认 FRR 服务正在运行：
   ```bash
   sudo systemctl status frr
   ```

2. 检查 vtysh 权限：
   ```bash
   sudo vtysh -c "show version"
   ```

3. 确保 Web 服务以 root 权限运行（或用户在 frrvty 组中）

### 问题 3: 数据不更新

**症状：** 点击刷新按钮后数据没有变化

**解决方案：**
1. 打开浏览器开发者工具（F12）查看网络请求
2. 检查 API 响应是否正常
3. 清除浏览器缓存后重试

### 问题 4: Docker 容器中无法访问

**症状：** Docker 容器启动后无法访问 Web 界面

**解决方案：**
1. 检查端口映射：
   ```bash
   docker ps
   ```

2. 确认容器内服务正在运行：
   ```bash
   docker exec -it whitebox-ne-router ps aux | grep python
   ```

3. 查看容器日志：
   ```bash
   docker logs whitebox-ne-router
   ```

## 性能优化

### 1. 启用缓存

对于不经常变化的数据，可以添加缓存：

```python
from functools import lru_cache
import time

@lru_cache(maxsize=128)
def cached_vtysh_command(command, timestamp):
    return VtyshCommand.execute(command)

@app.route('/api/system/info')
def system_info():
    # 缓存 30 秒
    timestamp = int(time.time() / 30)
    result = cached_vtysh_command('show version', timestamp)
    return jsonify(result)
```

### 2. 异步处理

对于耗时操作，使用异步处理：

```python
from concurrent.futures import ThreadPoolExecutor

executor = ThreadPoolExecutor(max_workers=4)

@app.route('/api/routes')
def get_routes():
    future = executor.submit(VtyshCommand.execute, 'show ip route')
    result = future.result(timeout=10)
    return jsonify(result)
```

### 3. 压缩响应

启用 gzip 压缩：

```python
from flask_compress import Compress

app = Flask(__name__)
Compress(app)
```

## API 参考

### 系统信息

**GET** `/api/system/info`

返回系统基本信息。

**响应示例：**
```json
{
  "hostname": "whitebox-router",
  "version": "FRRouting 8.1",
  "timestamp": "2024-02-21T10:30:00"
}
```

### 接口列表

**GET** `/api/interfaces`

返回所有网络接口信息。

**响应示例：**
```json
{
  "success": true,
  "data": "Interface eth0 is up...",
  "error": ""
}
```

### 执行命令

**POST** `/api/command`

执行 vtysh 命令。

**请求体：**
```json
{
  "command": "show ip route"
}
```

**响应示例：**
```json
{
  "success": true,
  "output": "Codes: K - kernel route...",
  "error": ""
}
```

## 扩展开发

### 添加新的 API 端点

1. 在 `app.py` 中添加路由：

```python
@app.route('/api/custom/feature')
def custom_feature():
    result = VtyshCommand.execute('your custom command')
    return jsonify(result)
```

2. 在 `index.html` 中添加前端调用：

```javascript
async function loadCustomFeature() {
    const data = await apiCall('/api/custom/feature');
    // 处理数据
}
```

### 自定义界面样式

编辑 `templates/index.html` 中的 CSS：

```css
.custom-style {
    background: #your-color;
    /* 其他样式 */
}
```

## 许可证

MIT License

## 支持

如有问题，请查看：
- [项目主 README](../../README.md)
- [GitHub Issues](https://github.com/sunboygavin/whitebox-ne/issues)
