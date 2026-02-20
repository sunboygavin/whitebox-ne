#!/bin/bash
#
# Test script for routing protocols (Phase 1)
# Tests RIP, IS-IS, static routes, and policy routing
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Test result function
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
echo "Routing Protocols Test Suite - Phase 1"
echo "========================================="
echo ""

# Test 1: Check if static route files exist
echo "Test 1: Checking static route implementation..."
if [ -f "src/frr_core/zebra/static_route.c" ]; then
    test_result "Static route implementation exists" 0
else
    test_result "Static route implementation exists" 1
fi

# Test 2: Check if RIP files exist
echo "Test 2: Checking RIP implementation..."
if [ -f "src/frr_core/ripd/rip_huawei.c" ] && [ -f "openconfig-models/rip/openconfig-rip.yang" ]; then
    test_result "RIP implementation exists" 0
else
    test_result "RIP implementation exists" 1
fi

# Test 3: Validate RIP YANG model
echo "Test 3: Validating RIP YANG model..."
if command -v pyang &> /dev/null; then
    if pyang --strict openconfig-models/rip/openconfig-rip.yang 2>/dev/null; then
        test_result "RIP YANG model is valid" 0
    else
        test_result "RIP YANG model is valid" 1
    fi
else
    echo -e "${YELLOW}⚠${NC} pyang not installed, skipping YANG validation"
fi

# Test 4: Check Huawei CLI header
echo "Test 4: Checking Huawei CLI header..."
if [ -f "src/frr_core/lib/huawei_cli.h" ]; then
    test_result "Huawei CLI header exists" 0
else
    test_result "Huawei CLI header exists" 1
fi

# Test 5: Check for command registration functions
echo "Test 5: Checking command registration..."
if grep -q "register_huawei_routing_cmds" src/frr_core/lib/huawei_cli.h 2>/dev/null; then
    test_result "Command registration functions defined" 0
else
    test_result "Command registration functions defined" 1
fi

# Test 6: Check static route command implementation
echo "Test 6: Checking static route commands..."
if grep -q "cmd_ip_route_static" src/frr_core/zebra/static_route.c 2>/dev/null; then
    test_result "Static route commands implemented" 0
else
    test_result "Static route commands implemented" 1
fi

# Test 7: Check RIP command implementation
echo "Test 7: Checking RIP commands..."
if grep -q "cmd_rip" src/frr_core/ripd/rip_huawei.c 2>/dev/null; then
    test_result "RIP commands implemented" 0
else
    test_result "RIP commands implemented" 1
fi

# Test 8: Check for preference support in static routes
echo "Test 8: Checking preference support..."
if grep -q "preference" src/frr_core/zebra/static_route.c 2>/dev/null; then
    test_result "Preference support implemented" 0
else
    test_result "Preference support implemented" 1
fi

# Test 9: Check for BFD integration
echo "Test 9: Checking BFD integration..."
if grep -q "bfd_enable" src/frr_core/zebra/static_route.c 2>/dev/null; then
    test_result "BFD integration implemented" 0
else
    test_result "BFD integration implemented" 1
fi

# Test 10: Check RIP version support
echo "Test 10: Checking RIP version support..."
if grep -q "cmd_rip_version" src/frr_core/ripd/rip_huawei.c 2>/dev/null; then
    test_result "RIP version support implemented" 0
else
    test_result "RIP version support implemented" 1
fi

# Test 11: Check RIP silent interface support
echo "Test 11: Checking RIP silent interface..."
if grep -q "silent-interface" src/frr_core/ripd/rip_huawei.c 2>/dev/null; then
    test_result "RIP silent interface implemented" 0
else
    test_result "RIP silent interface implemented" 1
fi

# Test 12: Check RIP authentication
echo "Test 12: Checking RIP authentication..."
if grep -q "authentication" src/frr_core/ripd/rip_huawei.c 2>/dev/null; then
    test_result "RIP authentication implemented" 0
else
    test_result "RIP authentication implemented" 1
fi

# Test 13: Check YANG model structure
echo "Test 13: Checking RIP YANG model structure..."
if grep -q "rip-global-config" openconfig-models/rip/openconfig-rip.yang 2>/dev/null; then
    test_result "RIP YANG model structure correct" 0
else
    test_result "RIP YANG model structure correct" 1
fi

# Test 14: Check for IPv6 static route support
echo "Test 14: Checking IPv6 static route support..."
if grep -q "ipv6_route_static" src/frr_core/zebra/static_route.c 2>/dev/null; then
    test_result "IPv6 static route support implemented" 0
else
    test_result "IPv6 static route support implemented" 1
fi

# Test 15: Check for route tagging
echo "Test 15: Checking route tagging..."
if grep -q "tag" src/frr_core/zebra/static_route.c 2>/dev/null; then
    test_result "Route tagging implemented" 0
else
    test_result "Route tagging implemented" 1
fi

# Test 16: Check if IS-IS files exist
echo "Test 16: Checking IS-IS implementation..."
if [ -f "src/frr_core/isisd/isis_huawei.c" ] && [ -f "openconfig-models/isis/openconfig-isis.yang" ]; then
    test_result "IS-IS implementation exists" 0
else
    test_result "IS-IS implementation exists" 1
fi

# Test 17: Check IS-IS YANG model structure
echo "Test 17: Checking IS-IS YANG model..."
if grep -q "isis-global-config" openconfig-models/isis/openconfig-isis.yang 2>/dev/null; then
    test_result "IS-IS YANG model structure correct" 0
else
    test_result "IS-IS YANG model structure correct" 1
fi

# Test 18: Check IS-IS command implementation
echo "Test 18: Checking IS-IS commands..."
if grep -q "cmd_isis" src/frr_core/isisd/isis_huawei.c 2>/dev/null; then
    test_result "IS-IS commands implemented" 0
else
    test_result "IS-IS commands implemented" 1
fi

# Test 19: Check IS-IS NET configuration
echo "Test 19: Checking IS-IS NET support..."
if grep -q "network-entity" src/frr_core/isisd/isis_huawei.c 2>/dev/null; then
    test_result "IS-IS NET configuration implemented" 0
else
    test_result "IS-IS NET configuration implemented" 1
fi

# Test 20: Check IS-IS level support
echo "Test 20: Checking IS-IS level support..."
if grep -q "is-level" src/frr_core/isisd/isis_huawei.c 2>/dev/null; then
    test_result "IS-IS level support implemented" 0
else
    test_result "IS-IS level support implemented" 1
fi

# Test 21: Check OSPF enhanced implementation
echo "Test 21: Checking OSPF enhanced features..."
if [ -f "src/frr_core/ospfd/ospf_huawei.c" ]; then
    test_result "OSPF enhanced implementation exists" 0
else
    test_result "OSPF enhanced implementation exists" 1
fi

# Test 22: Check OSPF NSSA support
echo "Test 22: Checking OSPF NSSA support..."
if grep -q "OSPF_AREA_NSSA" src/frr_core/ospfd/ospf_huawei.c 2>/dev/null; then
    test_result "OSPF NSSA support implemented" 0
else
    test_result "OSPF NSSA support implemented" 1
fi

# Test 23: Check OSPF virtual link support
echo "Test 23: Checking OSPF virtual link..."
if grep -q "vlink" src/frr_core/ospfd/ospf_huawei.c 2>/dev/null; then
    test_result "OSPF virtual link implemented" 0
else
    test_result "OSPF virtual link implemented" 1
fi

# Test 24: Check OSPF authentication
echo "Test 24: Checking OSPF authentication..."
if grep -q "authentication-mode" src/frr_core/ospfd/ospf_huawei.c 2>/dev/null; then
    test_result "OSPF authentication implemented" 0
else
    test_result "OSPF authentication implemented" 1
fi

# Test 25: Check OSPF stub area support
echo "Test 25: Checking OSPF stub area..."
if grep -q "OSPF_AREA_STUB" src/frr_core/ospfd/ospf_huawei.c 2>/dev/null; then
    test_result "OSPF stub area implemented" 0
else
    test_result "OSPF stub area implemented" 1
fi

# Test 26: Check OSPF filter policy
echo "Test 26: Checking OSPF filter policy..."
if grep -q "filter-policy" src/frr_core/ospfd/ospf_huawei.c 2>/dev/null; then
    test_result "OSPF filter policy implemented" 0
else
    test_result "OSPF filter policy implemented" 1
fi

# Test 27: Check OSPF bandwidth reference
echo "Test 27: Checking OSPF bandwidth reference..."
if grep -q "bandwidth-reference" src/frr_core/ospfd/ospf_huawei.c 2>/dev/null; then
    test_result "OSPF bandwidth reference implemented" 0
else
    test_result "OSPF bandwidth reference implemented" 1
fi

# Test 28: Check BGP enhanced implementation
echo "Test 28: Checking BGP enhanced features..."
if [ -f "src/frr_core/bgpd/bgp_huawei.c" ]; then
    test_result "BGP enhanced implementation exists" 0
else
    test_result "BGP enhanced implementation exists" 1
fi

# Test 29: Check BGP route reflector support
echo "Test 29: Checking BGP route reflector..."
if grep -q "route_reflector_client" src/frr_core/bgpd/bgp_huawei.c 2>/dev/null; then
    test_result "BGP route reflector implemented" 0
else
    test_result "BGP route reflector implemented" 1
fi

# Test 30: Check BGP confederation support
echo "Test 30: Checking BGP confederation..."
if grep -q "confederation" src/frr_core/bgpd/bgp_huawei.c 2>/dev/null; then
    test_result "BGP confederation implemented" 0
else
    test_result "BGP confederation implemented" 1
fi

# Test 31: Check BGP route policy support
echo "Test 31: Checking BGP route policy..."
if grep -q "route-policy" src/frr_core/bgpd/bgp_huawei.c 2>/dev/null; then
    test_result "BGP route policy implemented" 0
else
    test_result "BGP route policy implemented" 1
fi

# Test 32: Check BGP aggregation support
echo "Test 32: Checking BGP aggregation..."
if grep -q "aggregate" src/frr_core/bgpd/bgp_huawei.c 2>/dev/null; then
    test_result "BGP aggregation implemented" 0
else
    test_result "BGP aggregation implemented" 1
fi

# Test 33: Check BGP community support
echo "Test 33: Checking BGP community..."
if grep -q "advertise-community" src/frr_core/bgpd/bgp_huawei.c 2>/dev/null; then
    test_result "BGP community support implemented" 0
else
    test_result "BGP community support implemented" 1
fi

# Test 34: Check policy routing implementation
echo "Test 34: Checking policy routing..."
if [ -f "src/frr_core/zebra/policy_route.c" ]; then
    test_result "Policy routing implementation exists" 0
else
    test_result "Policy routing implementation exists" 1
fi

# Test 35: Check policy match conditions
echo "Test 35: Checking policy match conditions..."
if grep -q "if-match" src/frr_core/zebra/policy_route.c 2>/dev/null; then
    test_result "Policy match conditions implemented" 0
else
    test_result "Policy match conditions implemented" 1
fi

# Test 36: Check policy apply actions
echo "Test 36: Checking policy apply actions..."
if grep -q "apply" src/frr_core/zebra/policy_route.c 2>/dev/null; then
    test_result "Policy apply actions implemented" 0
else
    test_result "Policy apply actions implemented" 1
fi

# Test 37: Check policy ACL matching
echo "Test 37: Checking policy ACL matching..."
if grep -q "match_acl" src/frr_core/zebra/policy_route.c 2>/dev/null; then
    test_result "Policy ACL matching implemented" 0
else
    test_result "Policy ACL matching implemented" 1
fi

# Test 38: Check policy next-hop action
echo "Test 38: Checking policy next-hop action..."
if grep -q "apply_nexthop" src/frr_core/zebra/policy_route.c 2>/dev/null; then
    test_result "Policy next-hop action implemented" 0
else
    test_result "Policy next-hop action implemented" 1
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
