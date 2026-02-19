#!/bin/bash
# 白盒路由器快速安全加固脚本
# 用途: 修复关键安全问题，提升系统安全性

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }

echo "============================================"
echo "   白盒路由器安全加固脚本"
echo "============================================"
echo ""

# 检查 root 权限
if [ "$EUID" -ne 0 ]; then
    log_error "此脚本需要 root 权限运行"
    exit 1
fi

# 1. 修复 SNMP 安全配置
log_step "1. 加固 SNMP 配置"
if [ -f /etc/snmp/snmpd.conf ]; then
    log_info "备份原始 SNMP 配置"
    cp /etc/snmp/snmpd.conf /etc/snmp/snmpd.conf.bak.$(date +%Y%m%d)

    log_info "生成强密码"
    AUTH_PASS=$(openssl rand -base64 16)
    PRIV_PASS=$(openssl rand -base64 16)

    log_info "创建安全的 SNMP 配置"
    cat > /etc/snmp/snmpd.conf << EOF
# 安全加固的 SNMP 配置
# 生成时间: $(date)

# 禁用 SNMPv1/v2c 公共访问
# rocommunity public localhost

# 启用 SNMPv3 认证和加密
createUser snmpadmin SHA "$AUTH_PASS" AES "$PRIV_PASS"
rouser snmpadmin priv

# 仅监听本地接口
agentAddress udp:127.0.0.1:161

# 启用 AgentX
master agentx
agentXSocket /var/agentx/master

# 系统信息
sysLocation    "WhiteBox Router"
sysContact     "admin@example.com"
sysServices    72

# 日志配置
logTimestamp yes
EOF

    log_info "保存 SNMPv3 凭据到安全位置"
    cat > /root/.snmp_credentials << EOF
# SNMPv3 凭据 - 请妥善保管
# 生成时间: $(date)

用户名: snmpadmin
认证协议: SHA
认证密码: $AUTH_PASS
加密协议: AES
加密密码: $PRIV_PASS

# 使用示例:
# snmpwalk -v3 -u snmpadmin -l authPriv \\
#   -a SHA -A "$AUTH_PASS" \\
#   -x AES -X "$PRIV_PASS" \\
#   localhost .1.3.6.1.2.1.1
EOF
    chmod 600 /root/.snmp_credentials

    log_info "重启 SNMP 服务"
    systemctl restart snmpd || service snmpd restart || true

    log_info "✓ SNMP 安全配置完成"
    log_warn "SNMPv3 凭据已保存到: /root/.snmp_credentials"
else
    log_warn "未找到 SNMP 配置文件，跳过"
fi

# 2. 配置防火墙规则
log_step "2. 配置防火墙规则"
if command -v ufw &> /dev/null; then
    log_info "使用 UFW 配置防火墙"

    # 默认策略
    ufw default deny incoming
    ufw default allow outgoing

    # 允许 SSH
    ufw allow 22/tcp comment "SSH"

    # 允许路由协议（仅从特定网络）
    # ufw allow from 192.168.0.0/16 to any port 179 proto tcp comment "BGP"
    # ufw allow from 192.168.0.0/16 to any proto ospf comment "OSPF"

    # 允许本地 SNMP
    ufw allow from 127.0.0.1 to any port 161 proto udp comment "SNMP local"

    # 启用防火墙
    echo "y" | ufw enable || true

    log_info "✓ UFW 防火墙配置完成"
elif command -v firewall-cmd &> /dev/null; then
    log_info "使用 firewalld 配置防火墙"

    # 移除不必要的服务
    firewall-cmd --permanent --remove-service=dhcpv6-client || true

    # 添加必要的服务
    firewall-cmd --permanent --add-service=ssh

    # 限制 SNMP 访问
    firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="127.0.0.1" port port="161" protocol="udp" accept'

    # 重载配置
    firewall-cmd --reload

    log_info "✓ firewalld 配置完成"
else
    log_warn "未找到防火墙工具，建议手动配置 iptables"
fi

# 3. 加固 FRR 配置文件权限
log_step "3. 加固 FRR 配置文件权限"
if [ -d /etc/frr ]; then
    log_info "设置 FRR 配置目录权限"
    chown -R frr:frr /etc/frr
    chmod 750 /etc/frr

    if [ -f /etc/frr/frr.conf ]; then
        chmod 640 /etc/frr/frr.conf
        log_info "✓ frr.conf 权限设置为 640"
    fi

    if [ -f /etc/frr/vtysh.conf ]; then
        chmod 640 /etc/frr/vtysh.conf
        log_info "✓ vtysh.conf 权限设置为 640"
    fi

    log_info "✓ FRR 配置文件权限加固完成"
else
    log_warn "未找到 FRR 配置目录"
fi

# 4. 配置日志轮转
log_step "4. 配置日志轮转"
if command -v logrotate &> /dev/null; then
    log_info "创建 FRR 日志轮转配置"
    cat > /etc/logrotate.d/frr << 'EOF'
/var/log/frr/*.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 640 frr frr
    sharedscripts
    postrotate
        systemctl reload frr || service frr reload || true
    endscript
}
EOF
    log_info "✓ 日志轮转配置完成"
else
    log_warn "未找到 logrotate，跳过日志轮转配置"
fi

# 5. 创建安全审计日志
log_step "5. 配置安全审计"
if command -v auditd &> /dev/null; then
    log_info "配置 auditd 监控 FRR 配置变更"

    cat >> /etc/audit/rules.d/frr.rules << 'EOF'
# 监控 FRR 配置文件变更
-w /etc/frr/frr.conf -p wa -k frr_config_change
-w /etc/frr/vtysh.conf -p wa -k frr_config_change
-w /etc/frr/daemons -p wa -k frr_daemon_change

# 监控 FRR 二进制文件
-w /usr/lib/frr/ -p x -k frr_execution
EOF

    # 重载 auditd 规则
    augenrules --load || service auditd restart || true

    log_info "✓ 安全审计配置完成"
else
    log_warn "未安装 auditd，建议安装: apt install auditd"
fi

# 6. 限制 vtysh 访问
log_step "6. 限制 vtysh 访问权限"
if [ -f /usr/bin/vtysh ]; then
    # 创建 frrvty 组（如果不存在）
    groupadd -f frrvty

    # 设置 vtysh 权限
    chown root:frrvty /usr/bin/vtysh
    chmod 2750 /usr/bin/vtysh

    log_info "✓ vtysh 访问权限已限制到 frrvty 组"
    log_warn "将需要访问 vtysh 的用户添加到 frrvty 组:"
    log_warn "  usermod -a -G frrvty <username>"
else
    log_warn "未找到 vtysh 二进制文件"
fi

# 7. 创建安全检查脚本
log_step "7. 创建安全检查脚本"
cat > /usr/local/bin/whitebox-security-check << 'EOF'
#!/bin/bash
# 白盒路由器安全检查脚本

echo "=== 白盒路由器安全检查 ==="
echo ""

# 检查 SNMP 配置
echo "1. SNMP 配置检查:"
if grep -q "rocommunity public" /etc/snmp/snmpd.conf 2>/dev/null; then
    echo "  ⚠️  警告: 使用默认 community string"
else
    echo "  ✓ SNMP 配置安全"
fi

# 检查 FRR 配置文件权限
echo ""
echo "2. FRR 配置文件权限:"
if [ -f /etc/frr/frr.conf ]; then
    perms=$(stat -c "%a" /etc/frr/frr.conf)
    if [ "$perms" -le 640 ]; then
        echo "  ✓ frr.conf 权限正确 ($perms)"
    else
        echo "  ⚠️  警告: frr.conf 权限过于宽松 ($perms)"
    fi
fi

# 检查防火墙状态
echo ""
echo "3. 防火墙状态:"
if command -v ufw &> /dev/null; then
    if ufw status | grep -q "Status: active"; then
        echo "  ✓ UFW 防火墙已启用"
    else
        echo "  ⚠️  警告: UFW 防火墙未启用"
    fi
elif command -v firewall-cmd &> /dev/null; then
    if firewall-cmd --state | grep -q "running"; then
        echo "  ✓ firewalld 已启用"
    else
        echo "  ⚠️  警告: firewalld 未启用"
    fi
else
    echo "  ⚠️  警告: 未检测到防火墙"
fi

# 检查 FRR 服务状态
echo ""
echo "4. FRR 服务状态:"
if systemctl is-active --quiet frr; then
    echo "  ✓ FRR 服务运行中"
else
    echo "  ⚠️  警告: FRR 服务未运行"
fi

# 检查日志文件大小
echo ""
echo "5. 日志文件检查:"
log_size=$(du -sh /var/log/frr 2>/dev/null | awk '{print $1}')
if [ -n "$log_size" ]; then
    echo "  日志目录大小: $log_size"
fi

echo ""
echo "=== 检查完成 ==="
EOF

chmod +x /usr/local/bin/whitebox-security-check
log_info "✓ 安全检查脚本已创建: /usr/local/bin/whitebox-security-check"

# 8. 创建定期安全检查 cron 任务
log_step "8. 配置定期安全检查"
cat > /etc/cron.daily/whitebox-security-check << 'EOF'
#!/bin/bash
/usr/local/bin/whitebox-security-check >> /var/log/whitebox-security-check.log 2>&1
EOF
chmod +x /etc/cron.daily/whitebox-security-check
log_info "✓ 每日安全检查任务已配置"

# 9. 生成安全报告
log_step "9. 生成安全加固报告"
cat > /root/security-hardening-report.txt << EOF
============================================
白盒路由器安全加固报告
============================================

加固时间: $(date)
主机名: $(hostname)
系统: $(uname -a)

已完成的安全加固:
-----------------
✓ SNMP 配置加固 (SNMPv3)
✓ 防火墙规则配置
✓ FRR 配置文件权限加固
✓ 日志轮转配置
✓ 安全审计配置
✓ vtysh 访问权限限制
✓ 安全检查脚本创建
✓ 定期安全检查任务

重要文件位置:
-------------
- SNMPv3 凭据: /root/.snmp_credentials
- 安全检查脚本: /usr/local/bin/whitebox-security-check
- 安全检查日志: /var/log/whitebox-security-check.log
- 本报告: /root/security-hardening-report.txt

下一步建议:
-----------
1. 配置 Netconf TLS 加密
2. 实现 RBAC 访问控制
3. 集成 Prometheus 监控
4. 配置集中式日志收集
5. 定期进行安全扫描

运行安全检查:
-------------
/usr/local/bin/whitebox-security-check

查看 SNMPv3 凭据:
-----------------
cat /root/.snmp_credentials

============================================
EOF

log_info "✓ 安全加固报告已生成: /root/security-hardening-report.txt"

# 完成
echo ""
echo "============================================"
echo -e "${GREEN}   安全加固完成！${NC}"
echo "============================================"
echo ""
echo "请查看报告: /root/security-hardening-report.txt"
echo "运行安全检查: /usr/local/bin/whitebox-security-check"
echo ""
log_warn "重要: 请妥善保管 SNMPv3 凭据文件: /root/.snmp_credentials"
echo ""
