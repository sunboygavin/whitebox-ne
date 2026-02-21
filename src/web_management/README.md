# WhiteBox Network Element - Web Management Interface

基于 Flask 的白盒网元 Web 管理界面，提供直观的图形化配置和监控功能。

## 功能特性

- **仪表盘**: 系统信息概览
- **接口管理**: 查看网络接口状态
- **路由管理**: 查看和管理路由表
- **OSPF 监控**: 查看 OSPF 邻居和状态
- **BGP 监控**: 查看 BGP 会话和路由
- **VRRP 状态**: 查看 VRRP 高可用状态
- **配置管理**: 查看和保存运行配置
- **命令行**: 执行 vtysh 查询命令

## 快速启动

### 方式一：使用启动脚本（推荐）

```bash
cd src/web_management
./start.sh
```

访问 http://localhost:8080

### 方式二：手动启动

```bash
cd src/web_management

# 安装依赖
pip3 install -r requirements.txt

# 启动服务
python3 app.py
```

### 方式三：系统服务（生产环境）

```bash
# 复制服务文件
sudo cp whitebox-web.service /etc/systemd/system/

# 重载 systemd
sudo systemctl daemon-reload

# 启动服务
sudo systemctl start whitebox-web

# 设置开机自启
sudo systemctl enable whitebox-web

# 查看状态
sudo systemctl status whitebox-web
```

## Docker 部署

在 Docker 容器中使用时，需要在 Dockerfile 中添加：

```dockerfile
# 安装 Python 和依赖
RUN apt-get update && apt-get install -y python3 python3-pip
COPY src/web_management /opt/whitebox-web
RUN pip3 install -r /opt/whitebox-web/requirements.txt

# 暴露端口
EXPOSE 8080

# 启动 Web 服务（在 entrypoint 中添加）
CMD ["python3", "/opt/whitebox-web/app.py"]
```

## 安全注意事项

1. **生产环境部署**：
   - 修改默认端口
   - 添加身份认证
   - 使用 HTTPS
   - 配置防火墙规则

2. **命令执行限制**：
   - 当前只允许执行查询命令（show/display）
   - 不支持配置修改命令
   - 建议在生产环境中进一步限制

3. **访问控制**：
   - 默认监听 0.0.0.0:8080
   - 建议配置反向代理（Nginx）
   - 添加 IP 白名单

## 配置选项

编辑 `app.py` 修改配置：

```python
# 修改监听地址和端口
app.run(host='0.0.0.0', port=8080, debug=False)

# 生产环境建议使用 gunicorn
# gunicorn -w 4 -b 0.0.0.0:8080 app:app
```

## 故障排除

### 问题：无法连接到 vtysh

**原因**：FRR 未安装或未启动

**解决**：
```bash
sudo systemctl status frr
sudo systemctl start frr
```

### 问题：权限不足

**原因**：当前用户无权执行 vtysh

**解决**：
```bash
# 将用户添加到 frrvty 组
sudo usermod -a -G frrvty $USER

# 或使用 root 权限运行
sudo python3 app.py
```

### 问题：端口被占用

**原因**：8080 端口已被其他服务使用

**解决**：修改 app.py 中的端口号

## API 接口文档

### 系统信息
- `GET /api/system/info` - 获取系统信息

### 网络接口
- `GET /api/interfaces` - 获取接口列表

### 路由
- `GET /api/routes` - 获取路由表

### OSPF
- `GET /api/ospf/neighbors` - 获取 OSPF 邻居

### BGP
- `GET /api/bgp/summary` - 获取 BGP 摘要

### VRRP
- `GET /api/vrrp/status` - 获取 VRRP 状态

### 配置管理
- `GET /api/config/running` - 获取运行配置
- `POST /api/config/save` - 保存配置

### 命令执行
- `POST /api/command` - 执行自定义命令
  ```json
  {
    "command": "show ip route"
  }
  ```

## 扩展开发

### 添加新功能

1. 在 `app.py` 中添加新的路由：

```python
@app.route('/api/custom/feature')
def custom_feature():
    result = VtyshCommand.execute('your command')
    return jsonify(result)
```

2. 在 `templates/index.html` 中添加对应的前端界面

### 添加身份认证

使用 Flask-Login 或 Flask-HTTPAuth：

```python
from flask_httpauth import HTTPBasicAuth

auth = HTTPBasicAuth()

@auth.verify_password
def verify_password(username, password):
    # 实现密码验证逻辑
    return username == 'admin' and password == 'password'

@app.route('/api/protected')
@auth.login_required
def protected():
    return jsonify({'message': 'Authenticated'})
```

## 技术栈

- **后端**: Python 3 + Flask
- **前端**: HTML5 + CSS3 + JavaScript (Vanilla)
- **通信**: RESTful API
- **命令执行**: subprocess + vtysh

## 许可证

MIT License
