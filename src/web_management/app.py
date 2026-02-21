#!/usr/bin/env python3
"""
WhiteBox Network Element Web Management Interface
基于 Flask 的白盒网元 Web 管理界面
"""

from flask import Flask, render_template, jsonify, request
import subprocess
import json
import re
import os
from datetime import datetime

# 加载配置
def load_config():
    """从环境变量或配置文件加载配置"""
    config = {
        'HOST': os.getenv('WEB_HOST', '0.0.0.0'),
        'PORT': int(os.getenv('WEB_PORT', '8080')),
        'DEBUG': os.getenv('WEB_DEBUG', 'false').lower() == 'true',
        'COMMAND_TIMEOUT': int(os.getenv('COMMAND_TIMEOUT', '30')),
        'ALLOW_CONFIG_COMMANDS': os.getenv('ALLOW_CONFIG_COMMANDS', 'true').lower() == 'true'
    }

    # 尝试从 config.env 文件加载
    config_file = os.path.join(os.path.dirname(__file__), 'config.env')
    if os.path.exists(config_file):
        with open(config_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#') and '=' in line:
                    key, value = line.split('=', 1)
                    key = key.strip()
                    value = value.strip()
                    if key == 'HOST':
                        config['HOST'] = value
                    elif key == 'PORT':
                        config['PORT'] = int(value)
                    elif key == 'DEBUG':
                        config['DEBUG'] = value.lower() == 'true'
                    elif key == 'COMMAND_TIMEOUT':
                        config['COMMAND_TIMEOUT'] = int(value)
                    elif key == 'ALLOW_CONFIG_COMMANDS':
                        config['ALLOW_CONFIG_COMMANDS'] = value.lower() == 'true'

    return config

CONFIG = load_config()
app = Flask(__name__)

class VtyshCommand:
    """执行 vtysh 命令的工具类"""

    @staticmethod
    def execute(command, timeout=None):
        """执行 vtysh 命令并返回结果"""
        if timeout is None:
            timeout = CONFIG['COMMAND_TIMEOUT']

        try:
            result = subprocess.run(
                ['vtysh', '-c', command],
                capture_output=True,
                text=True,
                timeout=timeout
            )
            return {
                'success': result.returncode == 0,
                'output': result.stdout,
                'error': result.stderr
            }
        except subprocess.TimeoutExpired:
            return {'success': False, 'error': '命令执行超时'}
        except Exception as e:
            return {'success': False, 'error': str(e)}

# 路由定义
@app.route('/')
def index():
    """主页"""
    return render_template('index.html')

@app.route('/api/system/info')
def system_info():
    """获取系统信息"""
    hostname_result = VtyshCommand.execute('show running-config | include hostname')
    version_result = VtyshCommand.execute('show version')

    return jsonify({
        'hostname': hostname_result.get('output', '').strip(),
        'version': version_result.get('output', '').strip(),
        'timestamp': datetime.now().isoformat()
    })

@app.route('/api/interfaces')
def get_interfaces():
    """获取接口信息"""
    result = VtyshCommand.execute('show interface brief')
    return jsonify({
        'success': result['success'],
        'data': result.get('output', ''),
        'error': result.get('error', '')
    })

@app.route('/api/routes')
def get_routes():
    """获取路由表"""
    result = VtyshCommand.execute('show ip route')
    return jsonify({
        'success': result['success'],
        'data': result.get('output', ''),
        'error': result.get('error', '')
    })

@app.route('/api/ospf/neighbors')
def get_ospf_neighbors():
    """获取 OSPF 邻居信息"""
    result = VtyshCommand.execute('show ip ospf neighbor')
    return jsonify({
        'success': result['success'],
        'data': result.get('output', ''),
        'error': result.get('error', '')
    })

@app.route('/api/bgp/summary')
def get_bgp_summary():
    """获取 BGP 摘要信息"""
    result = VtyshCommand.execute('show ip bgp summary')
    return jsonify({
        'success': result['success'],
        'data': result.get('output', ''),
        'error': result.get('error', '')
    })

@app.route('/api/vrrp/status')
def get_vrrp_status():
    """获取 VRRP 状态"""
    result = VtyshCommand.execute('show vrrp')
    return jsonify({
        'success': result['success'],
        'data': result.get('output', ''),
        'error': result.get('error', '')
    })

@app.route('/api/config/running')
def get_running_config():
    """获取运行配置"""
    result = VtyshCommand.execute('show running-config')
    return jsonify({
        'success': result['success'],
        'data': result.get('output', ''),
        'error': result.get('error', '')
    })

@app.route('/api/config/save', methods=['POST'])
def save_config():
    """保存配置"""
    result = VtyshCommand.execute('write memory')
    return jsonify({
        'success': result['success'],
        'message': '配置已保存' if result['success'] else '保存失败',
        'error': result.get('error', '')
    })

@app.route('/api/command', methods=['POST'])
def execute_command():
    """执行自定义命令"""
    data = request.get_json()
    command = data.get('command', '')

    if not command:
        return jsonify({'success': False, 'error': '命令不能为空'})

    # 安全检查：根据配置决定是否允许配置命令
    if not CONFIG['ALLOW_CONFIG_COMMANDS']:
        if not command.startswith(('show', 'display')):
            return jsonify({'success': False, 'error': '只允许执行查询命令（配置命令已禁用）'})

    result = VtyshCommand.execute(command)
    return jsonify(result)

@app.route('/api/config/interface', methods=['POST'])
def config_interface():
    """配置接口"""
    if not CONFIG['ALLOW_CONFIG_COMMANDS']:
        return jsonify({'success': False, 'error': '配置命令已禁用'})

    data = request.get_json()
    interface = data.get('interface', '')
    ip_address = data.get('ip_address', '')
    netmask = data.get('netmask', '')

    if not all([interface, ip_address, netmask]):
        return jsonify({'success': False, 'error': '参数不完整'})

    commands = [
        'configure terminal',
        f'interface {interface}',
        f'ip address {ip_address}/{netmask}',
        'no shutdown',
        'exit',
        'exit'
    ]

    result = VtyshCommand.execute('; '.join(commands))
    return jsonify(result)

@app.route('/api/config/ospf', methods=['POST'])
def config_ospf():
    """配置 OSPF"""
    if not CONFIG['ALLOW_CONFIG_COMMANDS']:
        return jsonify({'success': False, 'error': '配置命令已禁用'})

    data = request.get_json()
    network = data.get('network', '')
    area = data.get('area', '0.0.0.0')

    if not network:
        return jsonify({'success': False, 'error': '网络地址不能为空'})

    commands = [
        'configure terminal',
        'router ospf',
        f'network {network} area {area}',
        'exit',
        'exit'
    ]

    result = VtyshCommand.execute('; '.join(commands))
    return jsonify(result)

@app.route('/api/config/bgp', methods=['POST'])
def config_bgp():
    """配置 BGP"""
    if not CONFIG['ALLOW_CONFIG_COMMANDS']:
        return jsonify({'success': False, 'error': '配置命令已禁用'})

    data = request.get_json()
    asn = data.get('asn', '')
    neighbor = data.get('neighbor', '')
    remote_as = data.get('remote_as', '')

    if not all([asn, neighbor, remote_as]):
        return jsonify({'success': False, 'error': '参数不完整'})

    commands = [
        'configure terminal',
        f'router bgp {asn}',
        f'neighbor {neighbor} remote-as {remote_as}',
        'exit',
        'exit'
    ]

    result = VtyshCommand.execute('; '.join(commands))
    return jsonify(result)

@app.route('/api/config/static-route', methods=['POST'])
def config_static_route():
    """配置静态路由"""
    if not CONFIG['ALLOW_CONFIG_COMMANDS']:
        return jsonify({'success': False, 'error': '配置命令已禁用'})

    data = request.get_json()
    destination = data.get('destination', '')
    gateway = data.get('gateway', '')

    if not all([destination, gateway]):
        return jsonify({'success': False, 'error': '参数不完整'})

    commands = [
        'configure terminal',
        f'ip route {destination} {gateway}',
        'exit'
    ]

    result = VtyshCommand.execute('; '.join(commands))
    return jsonify(result)

@app.route('/api/config/delete-static-route', methods=['POST'])
def delete_static_route():
    """删除静态路由"""
    if not CONFIG['ALLOW_CONFIG_COMMANDS']:
        return jsonify({'success': False, 'error': '配置命令已禁用'})

    data = request.get_json()
    destination = data.get('destination', '')
    gateway = data.get('gateway', '')

    if not all([destination, gateway]):
        return jsonify({'success': False, 'error': '参数不完整'})

    commands = [
        'configure terminal',
        f'no ip route {destination} {gateway}',
        'exit'
    ]

    result = VtyshCommand.execute('; '.join(commands))
    return jsonify(result)

@app.route('/api/config/hostname', methods=['POST'])
def config_hostname():
    """配置主机名"""
    if not CONFIG['ALLOW_CONFIG_COMMANDS']:
        return jsonify({'success': False, 'error': '配置命令已禁用'})

    data = request.get_json()
    hostname = data.get('hostname', '')

    if not hostname:
        return jsonify({'success': False, 'error': '主机名不能为空'})

    commands = [
        'configure terminal',
        f'hostname {hostname}',
        'exit'
    ]

    result = VtyshCommand.execute('; '.join(commands))
    return jsonify(result)

@app.route('/api/config/settings')
def get_config_settings():
    """获取配置设置"""
    return jsonify({
        'allow_config_commands': CONFIG['ALLOW_CONFIG_COMMANDS'],
        'command_timeout': CONFIG['COMMAND_TIMEOUT'],
        'port': CONFIG['PORT']
    })

if __name__ == '__main__':
    print(f"启动 Web 管理界面...")
    print(f"监听地址: {CONFIG['HOST']}:{CONFIG['PORT']}")
    print(f"配置命令: {'允许' if CONFIG['ALLOW_CONFIG_COMMANDS'] else '禁止'}")
    print(f"访问地址: http://localhost:{CONFIG['PORT']}")
    app.run(host=CONFIG['HOST'], port=CONFIG['PORT'], debug=CONFIG['DEBUG'])
