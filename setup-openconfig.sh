#!/bin/bash
# OpenConfig Setup Script for WhiteBox NE
# 
# This script installs and configures Sysrepo, Netopeer2,
# and loads OpenConfig YANG models.
# 
# Author: WhiteBox NE Team
# Date: 2024-02-20

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Log function
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "This script must be run as root"
        exit 1
    fi
}

# Install dependencies
install_dependencies() {
    log_info "Installing dependencies..."
    
    apt update
    apt install -y \
        build-essential \
        git \
        cmake \
        libyang-dev \
        libpcre2-dev \
        libev-dev \
        libprotobuf-c-dev \
        protobuf-c-compiler \
        libssl-dev \
        zlib1g-dev \
        libnetopeer2-dev \
        libsysrepo-dev

    log_info "Dependencies installed successfully"
}

# Clone and build Sysrepo
build_sysrepo() {
    log_info "Building Sysrepo..."
    
    cd /tmp
    if [ -d "sysrepo" ]; then
        log_info "Sysrepo already exists, removing..."
        rm -rf sysrepo
    fi
    
    git clone https://github.com/sysrepo/sysrepo.git
    cd sysrepo
    mkdir build && cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr ..
    make -j$(nproc)
    make install
    ldconfig
    
    log_info "Sysrepo built and installed successfully"
}

# Clone and build Netopeer2
build_netopeer2() {
    log_info "Building Netopeer2..."
    
    cd /tmp
    if [ -d "netopeer2" ]; then
        log_info "Netopeer2 already exists, removing..."
        rm -rf netopeer2
    fi
    
    git clone https://github.com/sysrepo/netopeer2.git
    cd netopeer2
    mkdir build && cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr ..
    make -j$(nproc)
    make install
    ldconfig
    
    log_info "Netopeer2 built and installed successfully"
}

# Install OpenConfig YANG models
install_yang_models() {
    log_info "Installing OpenConfig YANG models..."
    
    MODEL_DIR="/root/.openclaw/workspace/whitebox-ne/openconfig-models"
    
    # Install interface model
    if [ -f "$MODEL_DIR/interfaces/openconfig-interfaces.yang" ]; then
        sysrepoctl --install --yang=openconfig-interfaces.yang \
            --search-dir="$MODEL_DIR/interfaces/"
        log_info "Installed openconfig-interfaces.yang"
    else
        log_error "openconfig-interfaces.yang not found"
        exit 1
    fi
    
    # Install BGP model
    if [ -f "$MODEL_DIR/network-instance/openconfig-bgp.yang" ]; then
        sysrepoctl --install --yang=openconfig-bgp.yang \
            --search-dir="$MODEL_DIR/network-instance/"
        log_info "Installed openconfig-bgp.yang"
    else
        log_error "openconfig-bgp.yang not found"
        exit 1
    fi
    
    # Install network instance model
    if [ -f "$MODEL_DIR/network-instance/openconfig-network-instance.yang" ]; then
        sysrepoctl --install --yang=openconfig-network-instance.yang \
            --search-dir="$MODEL_DIR/network-instance/"
        log_info "Installed openconfig-network-instance.yang"
    else
        log_error "openconfig-network-instance.yang not found"
        exit 1
    fi
    
    # Install system model
    if [ -f "$MODEL_DIR/system/openconfig-system.yang" ]; then
        sysrepoctl --install --yang=openconfig-system.yang \
            --search-dir="$MODEL_DIR/system/"
        log_info "Installed openconfig-system.yang"
    else
        log_error "openconfig-system.yang not found"
        exit 1
    fi
    
    # Install ACL model
    if [ -f "$MODEL_DIR/acl/openconfig-acl.yang" ]; then
        sysrepoctl --install --yang=openconfig-acl.yang \
            --search-dir="$MODEL_DIR/acl/"
        log_info "Installed openconfig-acl.yang"
    else
        log_error "openconfig-acl.yang not found"
        exit 1
    fi
    
    # Install OSPF model
    if [ -f "$MODEL_DIR/routing/openconfig-ospf.yang" ]; then
        sysrepoctl --install --yang=openconfig-ospf.yang \
            --search-dir="$MODEL_DIR/routing/"
        log_info "Installed openconfig-ospf.yang"
    else
        log_error "openconfig-ospf.yang not found"
        exit 1
    fi
    
    # Install MPLS model
    if [ -f "$MODEL_DIR/mpls/openconfig-mpls.yang" ]; then
        sysrepoctl --install --yang=openconfig-mpls.yang \
            --search-dir="$MODEL_DIR/mpls/"
        log_info "Installed openconfig-mpls.yang"
    else
        log_error "openconfig-mpls.yang not found"
        exit 1
    fi
    
    # Install QoS model
    if [ -f "$MODEL_DIR/qos/openconfig-qos.yang" ]; then
        sysrepoctl --install --yang=openconfig-qos.yang \
            --search-dir="$MODEL_DIR/qos/"
        log_info "Installed openconfig-qos.yang"
    else
        log_error "openconfig-qos.yang not found"
        exit 1
    fi
    
    log_info "OpenConfig YANG models installed successfully"
}

# Initialize Sysrepo datastores
init_sysrepo() {
    log_info "Initializing Sysrepo datastores..."
    
    sysrepoctl --init
    
    log_info "Sysrepo datastores initialized"
}

# Build OpenConfig adapter
build_adapter() {
    log_info "Building OpenConfig adapter..."
    
    ADAPTER_DIR="/root/.openclaw/workspace/whitebox-ne/src/openconfig_adapter"
    
    cd "$ADAPTER_DIR"
    make clean
    make
    make install
    
    log_info "OpenConfig adapter built and installed"
}

# Start Netopeer2 server
start_netopeer2() {
    log_info "Starting Netopeer2 server..."
    
    # Stop any existing Netopeer2 server
    if pgrep -x "netopeer2-server" > /dev/null; then
        log_warn "Netopeer2 server already running, stopping..."
        pkill -x "netopeer2-server"
        sleep 2
    fi
    
    # Start Netopeer2 server in background
    netopeer2-server -d -p 830 &
    
    sleep 3
    
    if pgrep -x "netopeer2-server" > /dev/null; then
        log_info "Netopeer2 server started successfully (port 830)"
    else
        log_error "Failed to start Netopeer2 server"
        exit 1
    fi
}

# Configure firewall for Netopeer2
configure_firewall() {
    log_info "Configuring firewall for Netopeer2..."
    
    # Allow Netopeer2 port (830)
    if command -v ufw &> /dev/null; then
        ufw allow 830/tcp
        log_info "Firewall configured: allow port 830/tcp"
    fi
}

# Create systemd service for Netopeer2
create_netopeer2_service() {
    log_info "Creating systemd service for Netopeer2..."
    
    cat > /etc/systemd/system/netopeer2.service <<EOF
[Unit]
Description=Netopeer2 Netconf Server
After=network.target

[Service]
Type=simple
ExecStart=/usr/bin/netopeer2-server -d -p 830
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    systemctl enable netopeer2.service
    
    log_info "Netopeer2 systemd service created"
}

# Main function
main() {
    log_info "OpenConfig Setup for WhiteBox NE"
    log_info "================================="
    
    check_root
    
    # Ask user for installation type
    echo ""
    echo "Select installation mode:"
    echo "1) Full installation (build Sysrepo and Netopeer2 from source)"
    echo "2) Quick installation (use pre-built packages)"
    echo "3) Install YANG models only"
    echo "4) Exit"
    echo ""
    read -p "Enter your choice [1-4]: " choice
    
    case $choice in
        1)
            log_info "Starting full installation..."
            install_dependencies
            build_sysrepo
            build_netopeer2
            install_yang_models
            init_sysrepo
            build_adapter
            configure_firewall
            create_netopeer2_service
            start_netopeer2
            ;;
        2)
            log_info "Starting quick installation..."
            install_dependencies
            install_yang_models
            init_sysrepo
            build_adapter
            configure_firewall
            create_netopeer2_service
            start_netopeer2
            ;;
        3)
            log_info "Installing YANG models only..."
            install_yang_models
            init_sysrepo
            ;;
        4)
            log_info "Exiting..."
            exit 0
            ;;
        *)
            log_error "Invalid choice"
            exit 1
            ;;
    esac
    
    log_info "================================="
    log_info "OpenConfig setup completed successfully!"
    log_info ""
    log_info "Netopeer2 server is running on port 830"
    log_info "Use the following command to test Netconf:"
    log_info "  netconf-tool --host localhost --port 830"
    log_info ""
    log_info "OpenConfig YANG models installed:"
    log_info "  - openconfig-interfaces"
    log_info "  - openconfig-bgp"
    log_info "  - openconfig-network-instance"
    log_info "  - openconfig-system"
    log_info "  - openconfig-acl"
}

# Run main function
main "$@"
