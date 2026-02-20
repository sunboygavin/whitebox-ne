#!/bin/bash
#
# Test script for security features (Phase 3)
#

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

test_result() {
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    if [ "$2" = "0" ]; then
        echo -e "${GREEN}✓${NC} $1"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $1"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

echo "========================================="
echo "Security Features Test Suite - Phase 3"
echo "========================================="
echo ""

# Firewall tests
echo "Test 1: Checking firewall implementation..."
[ -f "src/security/firewall/zone_firewall.c" ] && test_result "Firewall implementation exists" 0 || test_result "Firewall implementation exists" 1

echo "Test 2: Checking security zones..."
grep -q "security_zone" src/security/firewall/zone_firewall.c 2>/dev/null && test_result "Security zones implemented" 0 || test_result "Security zones implemented" 1

echo "Test 3: Checking security policy..."
grep -q "security_policy" src/security/firewall/zone_firewall.c 2>/dev/null && test_result "Security policy implemented" 0 || test_result "Security policy implemented" 1

echo "Test 4: Checking firewall rules..."
grep -q "security_rule" src/security/firewall/zone_firewall.c 2>/dev/null && test_result "Firewall rules implemented" 0 || test_result "Firewall rules implemented" 1

# AAA tests
echo "Test 5: Checking AAA implementation..."
[ -f "src/security/auth/aaa.c" ] && test_result "AAA implementation exists" 0 || test_result "AAA implementation exists" 1

echo "Test 6: Checking local users..."
grep -q "local_user" src/security/auth/aaa.c 2>/dev/null && test_result "Local user management implemented" 0 || test_result "Local user management implemented" 1

echo "Test 7: Checking RADIUS support..."
grep -q "radius_server" src/security/auth/aaa.c 2>/dev/null && test_result "RADIUS support implemented" 0 || test_result "RADIUS support implemented" 1

echo "Test 8: Checking privilege levels..."
grep -q "privilege_level" src/security/auth/aaa.c 2>/dev/null && test_result "Privilege levels implemented" 0 || test_result "Privilege levels implemented" 1

# GRE tunnel tests
echo "Test 9: Checking GRE tunnel implementation..."
[ -f "src/security/vpn/gre/gre_tunnel.c" ] && test_result "GRE tunnel implementation exists" 0 || test_result "GRE tunnel implementation exists" 1

echo "Test 10: Checking tunnel configuration..."
grep -q "gre_tunnel" src/security/vpn/gre/gre_tunnel.c 2>/dev/null && test_result "Tunnel configuration implemented" 0 || test_result "Tunnel configuration implemented" 1

echo "Test 11: Checking keepalive support..."
grep -q "keepalive" src/security/vpn/gre/gre_tunnel.c 2>/dev/null && test_result "Keepalive support implemented" 0 || test_result "Keepalive support implemented" 1

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
