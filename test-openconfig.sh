#!/bin/bash
# OpenConfig Test Script for WhiteBox NE
# 
# This script tests OpenConfig functionality including:
# - Netconf connectivity
# - YANG model validation
# - Interface configuration
# - BGP configuration
# - FRR synchronization
# 
# Author: WhiteBox NE Team
# Date: 2024-02-20

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Log functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
    ((TOTAL_TESTS++))
}

test_passed() {
    log_info "✓ PASSED: $1"
    ((PASSED_TESTS++))
}

test_failed() {
    log_error "✗ FAILED: $1"
    ((FAILED_TESTS++))
}

# Check if Netopeer2 is running
check_netopeer2() {
    log_test "Checking Netopeer2 server status"
    
    if pgrep -x "netopeer2-server" > /dev/null; then
        test_passed "Netopeer2 server is running"
        return 0
    else
        test_failed "Netopeer2 server is not running"
        return 1
    fi
}

# Test Netconf connectivity
test_netconf_connectivity() {
    log_test "Testing Netconf connectivity"
    
    # Use netconf-tool if available, otherwise use netcat
    if command -v netconf-tool &> /dev/null; then
        output=$(netconf-tool --host localhost --port 830 2>&1 || true)
        if [[ $output == *"hello"* ]] || [[ $output == *"capabilities"* ]]; then
            test_passed "Netconf connectivity successful"
            return 0
        else
            test_failed "Netconf connectivity failed"
            return 1
        fi
    else
        # Fallback to netcat
        if nc -z localhost 830 2>/dev/null; then
            test_passed "Netopeer2 port 830 is accessible"
            return 0
        else
            test_failed "Cannot connect to Netopeer2 port 830"
            return 1
        fi
    fi
}

# Test YANG model installation
test_yang_models() {
    log_test "Testing YANG model installation"
    
    models=(
        "openconfig-interfaces"
        "openconfig-bgp"
        "openconfig-network-instance"
        "openconfig-system"
        "openconfig-acl"
    )
    
    all_installed=true
    
    for model in "${models[@]}"; do
        if sysrepoctl --list | grep -q "$model"; then
            log_info "  - $model installed"
        else
            log_error "  - $model NOT installed"
            all_installed=false
        fi
    done
    
    if $all_installed; then
        test_passed "All YANG models installed"
        return 0
    else
        test_failed "Some YANG models missing"
        return 1
    fi
}

# Test interface configuration via OpenConfig
test_interface_config() {
    log_test "Testing interface configuration via OpenConfig"
    
    # Create test Netconf XML
    cat > /tmp/test_interface_config.xml <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<rpc message-id="101" xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
  <edit-config>
    <target>
      <candidate/>
    </target>
    <config>
      <interfaces xmlns="http://openconfig.net/yang/interfaces">
        <interface>
          <name>eth0</name>
          <config>
            <description>Test Interface</description>
            <enabled>true</enabled>
          </config>
        </interface>
      </interfaces>
    </config>
  </edit-config>
</rpc>
EOF

    # Send configuration via Netconf (using netconf-tool)
    if command -v netconf-tool &> /dev/null; then
        output=$(netconf-tool --host localhost --port 830 --file /tmp/test_interface_config.xml 2>&1 || true)
        if [[ $output == *"ok"* ]] || [[ $output == *"OK"* ]]; then
            test_passed "Interface configuration successful"
            
            # Verify FRR configuration
            log_info "Verifying FRR configuration..."
            frr_output=$(vtysh -c "show running-config interface eth0" 2>&1 || true)
            if [[ $frr_output == *"eth0"* ]]; then
                log_info "✓ FRR interface configuration verified"
            else
                log_warn "FRR interface configuration not found (may need sync)"
            fi
            
            return 0
        else
            test_failed "Interface configuration failed"
            return 1
        fi
    else
        log_warn "netconf-tool not available, skipping configuration test"
        return 0
    fi
}

# Test BGP configuration via OpenConfig
test_bgp_config() {
    log_test "Testing BGP configuration via OpenConfig"
    
    # Create test Netconf XML
    cat > /tmp/test_bgp_config.xml <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<rpc message-id="102" xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
  <edit-config>
    <target>
      <candidate/>
    </target>
    <config>
      <bg xmlns="http://openconfig.net/yang/bgp">
        <global>
          <config>
            <as>65001</as>
            <router-id>192.168.1.1</router-id>
          </config>
        </global>
      </bg>
    </config>
  </edit-config>
</rpc>
EOF

    if command -v netconf-tool &> /dev/null; then
        output=$(netconf-tool --host localhost --port 830 --file /tmp/test_bgp_config.xml 2>&1 || true)
        if [[ $output == *"ok"* ]] || [[ $output == *"OK"* ]]; then
            test_passed "BGP configuration successful"
            return 0
        else
            test_failed "BGP configuration failed"
            return 1
        fi
    else
        log_warn "netconf-tool not available, skipping BGP configuration test"
        return 0
    fi
}

# Test FRR to YANG adapter
test_frr_to_yang_adapter() {
    log_test "Testing FRR to YANG adapter"
    
    if [ -x "/usr/local/bin/frr_to_yang_adapter" ]; then
        output=$(/usr/local/bin/frr_to_yang_adapter 2>&1 || true)
        if [[ $output == *"successfully"* ]] || [[ $output == *"completed"* ]]; then
            test_passed "FRR to YANG adapter working"
            return 0
        else
            log_warn "FRR to YANG adapter output: $output"
            test_failed "FRR to YANG adapter failed"
            return 1
        fi
    else
        log_warn "FRR to YANG adapter not installed, skipping test"
        return 0
    fi
}

# Test YANG to FRR adapter
test_yang_to_frr_adapter() {
    log_test "Testing YANG to FRR adapter"
    
    if [ -x "/usr/local/bin/yang_to_frr_adapter" ]; then
        output=$(/usr/local/bin/yang_to_frr_adapter 2>&1 || true)
        if [[ $output == *"successfully"* ]] || [[ $output == *"completed"* ]]; then
            test_passed "YANG to FRR adapter working"
            return 0
        else
            log_warn "YANG to FRR adapter output: $output"
            test_failed "YANG to FRR adapter failed"
            return 1
        fi
    else
        log_warn "YANG to FRR adapter not installed, skipping test"
        return 0
    fi
}

# Test FRR service status
test_frr_service() {
    log_test "Testing FRR service status"
    
    if systemctl is-active --quiet frr; then
        test_passed "FRR service is running"
        return 0
    else
        test_failed "FRR service is not running"
        return 1
    fi
}

# Test FRR vtysh
test_frr_vtysh() {
    log_test "Testing FRR vtysh"
    
    output=$(vtysh -c "show version" 2>&1 || true)
    if [[ $output == *"FRR"* ]]; then
        test_passed "FRR vtysh working"
        return 0
    else
        test_failed "FRR vtysh not working"
        return 1
    fi
}

# Generate test report
generate_report() {
    echo ""
    echo "============================================"
    echo "         OpenConfig Test Report"
    echo "============================================"
    echo ""
    echo "Total Tests:  $TOTAL_TESTS"
    echo -e "Passed:       ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed:       ${RED}$FAILED_TESTS${NC}"
    echo ""
    
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}============================================${NC}"
        echo -e "${GREEN}         All Tests Passed! ✓${NC}"
        echo -e "${GREEN}============================================${NC}"
        return 0
    else
        echo -e "${RED}============================================${NC}"
        echo -e "${RED}         Some Tests Failed ✗${NC}"
        echo -e "${RED}============================================${NC}"
        return 1
    fi
}

# Main function
main() {
    echo "============================================"
    echo "   OpenConfig Test Suite for WhiteBox NE"
    echo "============================================"
    echo ""
    
    # Run all tests
    check_netopeer2
    test_netconf_connectivity
    test_yang_models
    test_interface_config
    test_bgp_config
    test_frr_to_yang_adapter
    test_yang_to_frr_adapter
    test_frr_service
    test_frr_vtysh
    
    # Generate report
    generate_report
}

# Run main function
main "$@"
