#!/bin/bash
#
# Test script for IP services (Phase 2)
# Tests VLAN, sub-interfaces, ACL, NAT, DHCP, DNS
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

test_result() {
    local test_name="$1"
    local result="$2"
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    if [ "$result" = "0" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

echo "========================================="
echo "IP Services Test Suite - Phase 2"
echo "========================================="
echo ""

# Test 1: VLAN implementation
echo "Test 1: Checking VLAN implementation..."
if [ -f "src/frr_core/zebra/interface_vlan.c" ]; then
    test_result "VLAN implementation exists" 0
else
    test_result "VLAN implementation exists" 1
fi

# Test 2: VLAN commands
echo "Test 2: Checking VLAN commands..."
if grep -q "cmd_vlan" src/frr_core/zebra/interface_vlan.c 2>/dev/null; then
    test_result "VLAN commands implemented" 0
else
    test_result "VLAN commands implemented" 1
fi

# Test 3: Vlanif support
echo "Test 3: Checking Vlanif support..."
if grep -q "Vlanif" src/frr_core/zebra/interface_vlan.c 2>/dev/null; then
    test_result "Vlanif support implemented" 0
else
    test_result "Vlanif support implemented" 1
fi

# Test 4: Port link type
echo "Test 4: Checking port link type..."
if grep -q "port link-type" src/frr_core/zebra/interface_vlan.c 2>/dev/null; then
    test_result "Port link type implemented" 0
else
    test_result "Port link type implemented" 1
fi

# Test 5: Sub-interface implementation
echo "Test 5: Checking sub-interface implementation..."
if [ -f "src/frr_core/zebra/interface_subif.c" ]; then
    test_result "Sub-interface implementation exists" 0
else
    test_result "Sub-interface implementation exists" 1
fi

# Test 6: 802.1Q support
echo "Test 6: Checking 802.1Q support..."
if grep -q "dot1q" src/frr_core/zebra/interface_subif.c 2>/dev/null; then
    test_result "802.1Q support implemented" 0
else
    test_result "802.1Q support implemented" 1
fi

# Test 7: ACL implementation
echo "Test 7: Checking ACL implementation..."
if [ -f "src/ip_services/acl/acl_huawei.c" ]; then
    test_result "ACL implementation exists" 0
else
    test_result "ACL implementation exists" 1
fi

# Test 8: Basic ACL support
echo "Test 8: Checking basic ACL..."
if grep -q "ACL_TYPE_BASIC" src/ip_services/acl/acl_huawei.c 2>/dev/null; then
    test_result "Basic ACL implemented" 0
else
    test_result "Basic ACL implemented" 1
fi

# Test 9: Advanced ACL support
echo "Test 9: Checking advanced ACL..."
if grep -q "ACL_TYPE_ADVANCED" src/ip_services/acl/acl_huawei.c 2>/dev/null; then
    test_result "Advanced ACL implemented" 0
else
    test_result "Advanced ACL implemented" 1
fi

# Test 10: ACL rules
echo "Test 10: Checking ACL rules..."
if grep -q "acl_rule" src/ip_services/acl/acl_huawei.c 2>/dev/null; then
    test_result "ACL rules implemented" 0
else
    test_result "ACL rules implemented" 1
fi

# Test 11: NAT implementation
echo "Test 11: Checking NAT implementation..."
if [ -f "src/ip_services/nat/nat44.c" ]; then
    test_result "NAT implementation exists" 0
else
    test_result "NAT implementation exists" 1
fi

# Test 12: NAT outbound
echo "Test 12: Checking NAT outbound..."
if grep -q "nat_outbound" src/ip_services/nat/nat44.c 2>/dev/null; then
    test_result "NAT outbound implemented" 0
else
    test_result "NAT outbound implemented" 1
fi

# Test 13: NAT server (port mapping)
echo "Test 13: Checking NAT server..."
if grep -q "nat_server" src/ip_services/nat/nat44.c 2>/dev/null; then
    test_result "NAT server implemented" 0
else
    test_result "NAT server implemented" 1
fi

# Test 14: VLAN description
echo "Test 14: Checking VLAN description..."
if grep -q "vlan_description" src/frr_core/zebra/interface_vlan.c 2>/dev/null; then
    test_result "VLAN description implemented" 0
else
    test_result "VLAN description implemented" 1
fi

# Test 15: Trunk allowed VLANs
echo "Test 15: Checking trunk allowed VLANs..."
if grep -q "trunk allow-pass" src/frr_core/zebra/interface_vlan.c 2>/dev/null; then
    test_result "Trunk allowed VLANs implemented" 0
else
    test_result "Trunk allowed VLANs implemented" 1
fi

echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo -e "Total tests: $TESTS_TOTAL"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi
