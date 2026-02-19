# 白盒路由器安全加固指南

## 🔒 安全问题修复清单

### ✅ 已修复问题

1. **Git Token 泄露** - 已修改为 SSH URL
2. **Docker 过度权限** - 已移除 `privileged: true`

### ⚠️ 待修复问题

#### 1. SNMP 安全加固

**当前问题**: 使用默认 community string "public"

**修复方案**:
```bash
# 创建 SNMPv3 用户配置
cat > /etc/snmp/snmpd.conf << 'EOF'
# 禁用 SNMPv1/v2c
rocommunity public localhost
# 启用 SNMPv3
createUser snmpadmin SHA "StrongAuthPass123!" AES "StrongPrivPass123!"
rouser snmpadmin priv
# 限制访问
agentAddress udp:127.0.0.1:161
master agentx
EOF
```

#### 2. Netconf TLS 加密

**当前问题**: Netconf 未启用 TLS

**修复方案**:
```bash
# 生成 TLS 证书
openssl req -x509 -newkey rsa:4096 -keyout /etc/netopeer2/server.key \
  -out /etc/netopeer2/server.crt -days 365 -nodes \
  -subj "/CN=whitebox-router"

# 配置 Netopeer2 使用 TLS
netopeer2-cli
> tls --cert /etc/netopeer2/server.crt --key /etc/netopeer2/server.key
```

#### 3. 输入验证增强

**当前问题**: 配置适配器缺少输入验证

**修复方案**: 在 `src/openconfig_adapter/` 中添加验证函数
```c
// 添加到 frr_to_yang.c 和 yang_to_frr.c
#include <regex.h>

static bool is_valid_interface_name(const char *ifname) {
    regex_t regex;
    int ret;

    // 接口名称格式: eth0, lo, vlan100 等
    ret = regcomp(&regex, "^[a-zA-Z][a-zA-Z0-9.:-]{0,15}$", REG_EXTENDED);
    if (ret) return false;

    ret = regexec(&regex, ifname, 0, NULL, 0);
    regfree(&regex);

    return (ret == 0);
}

static bool is_valid_ipv4(const char *ip) {
    regex_t regex;
    int ret;

    ret = regcomp(&regex,
        "^([0-9]{1,3}\\.){3}[0-9]{1,3}$",
        REG_EXTENDED);
    if (ret) return false;

    ret = regexec(&regex, ip, 0, NULL, 0);
    regfree(&regex);

    return (ret == 0);
}
```

#### 4. 日志脱敏

**修复方案**: 创建日志包装函数
```c
// 添加到适配器代码
static void log_safe(const char *format, ...) {
    va_list args;
    va_start(args, format);

    // 脱敏处理: 隐藏 IP 地址最后一段
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);

    // 替换敏感信息
    // 192.168.1.100 -> 192.168.1.***
    // TODO: 实现脱敏逻辑

    fprintf(stdout, "%s", buffer);
    va_end(args);
}
```

---

## 🛡️ 安全最佳实践

### 1. 访问控制

```yaml
# 限制容器网络访问
services:
  whitebox-ne:
    networks:
      management:
        ipv4_address: 172.20.0.10
    # 仅暴露必要端口到管理网络
    ports: []  # 不暴露到主机

networks:
  management:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.0.0/24
    internal: true  # 隔离外部网络
```

### 2. 资源限制

```yaml
services:
  whitebox-ne:
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
          pids: 200
        reservations:
          cpus: '1'
          memory: 1G
```

### 3. 只读文件系统

```yaml
services:
  whitebox-ne:
    read_only: true
    tmpfs:
      - /tmp
      - /run
      - /var/log/frr
```

### 4. 用户权限

```dockerfile
# 在 Dockerfile 中添加
RUN useradd -r -s /bin/false frr-user
USER frr-user
```

---

## 🔐 认证和授权

### 1. SSH 密钥管理

```bash
# 生成 SSH 密钥对
ssh-keygen -t ed25519 -C "whitebox-ne-deploy" -f ~/.ssh/whitebox-ne

# 添加到 GitHub
cat ~/.ssh/whitebox-ne.pub
# 在 GitHub Settings > SSH Keys 中添加

# 配置 Git 使用 SSH
git config --global url."git@github.com:".insteadOf "https://github.com/"
```

### 2. RBAC 实现

创建基于角色的访问控制：

```yaml
# roles.yaml
roles:
  - name: admin
    permissions:
      - read:*
      - write:*
      - execute:*

  - name: operator
    permissions:
      - read:*
      - write:interfaces
      - write:routing

  - name: viewer
    permissions:
      - read:*
```

---

## 📋 安全检查清单

### 部署前检查

- [ ] 修改所有默认密码和 community strings
- [ ] 启用 TLS/SSL 加密
- [ ] 配置防火墙规则
- [ ] 限制容器权限
- [ ] 启用审计日志
- [ ] 配置资源限制
- [ ] 验证输入数据
- [ ] 实施最小权限原则

### 运行时监控

- [ ] 监控异常登录尝试
- [ ] 检测配置变更
- [ ] 监控资源使用
- [ ] 审计 API 调用
- [ ] 检查日志异常

### 定期维护

- [ ] 更新依赖包
- [ ] 轮转日志文件
- [ ] 备份配置
- [ ] 审查访问权限
- [ ] 扫描安全漏洞

---

## 🔍 安全扫描工具

### 1. 容器镜像扫描

```bash
# 使用 Trivy 扫描镜像
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  aquasec/trivy image whitebox-ne:latest

# 使用 Clair 扫描
clairctl analyze whitebox-ne:latest
```

### 2. 代码安全扫描

```bash
# 使用 Bandit 扫描 Python 代码
bandit -r src/

# 使用 Flawfinder 扫描 C 代码
flawfinder src/
```

### 3. 依赖漏洞扫描

```bash
# 扫描系统包
apt list --installed | grep frr
apt-cache policy frr

# 检查 CVE
curl -s https://cve.mitre.org/cgi-bin/cvekey.cgi?keyword=frrouting
```

---

## 📞 安全事件响应

### 发现安全问题时

1. **立即隔离**: 断开受影响系统的网络连接
2. **评估影响**: 确定受影响的范围和数据
3. **收集证据**: 保存日志和系统状态
4. **修复漏洞**: 应用补丁或临时缓解措施
5. **恢复服务**: 验证修复后恢复正常运行
6. **事后分析**: 总结经验，改进流程

### 联系方式

- **项目维护者**: sunboygavin
- **GitHub Issues**: https://github.com/sunboygavin/whitebox-ne/issues
- **安全报告**: 请通过私密方式报告安全漏洞

---

**最后更新**: 2026-02-20
**版本**: 1.0
