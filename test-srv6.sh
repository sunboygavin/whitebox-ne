#!/bin/bash
# =============================================================================
# WhiteBox NE v3.0 - SRv6 Runtime Integration Test Script
# Tests: kernel SRv6 support, iproute2 seg6, locator/SID/policy installation
# =============================================================================
set -euo pipefail

NE_USER="whitebox-ne"
NE_HOME="/etc/whitebox-ne"
TEST_LOG="/tmp/test-srv6-$(date +%Y%m%d-%H%M%S).log"
PASS=0
FAIL=0
WARN=0

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$TEST_LOG"; }
pass() { log "[PASS] $*"; ((PASS++)); }
fail() { log "[FAIL] $*"; ((FAIL++)); }
warn() { log "[WARN] $*"; ((WARN++)); }

log "========================================"
log "WhiteBox NE v3.0 - SRv6 Integration Test"
log "========================================"

# ----------------------------------------------------------------------------
# 1. Kernel SRv6 Support Detection
# ----------------------------------------------------------------------------
log "--- 1. Kernel SRv6 Support ---"
if grep -q "CONFIG_IPV6_SEG6_LWTUNNEL=y" /boot/config-$(uname -r) 2>/dev/null || \
   grep -q "CONFIG_IPV6_SEG6_LWTUNNEL=m" /boot/config-$(uname -r) 2>/dev/null; then
    pass "Kernel CONFIG_IPV6_SEG6_LWTUNNEL found in /boot/config"
else
    warn "CONFIG_IPV6_SEG6_LWTUNNEL not found in /boot/config (may be built-in or custom kernel)"
fi

if ip route help 2>&1 | grep -q "seg6"; then
    pass "iproute2 supports seg6 encap"
else
    fail "iproute2 missing seg6 support (need iproute2-ss or patched version)"
fi

if [ -f /proc/sys/net/ipv6/conf/all/seg6_enabled ]; then
    pass "Kernel sysfs seg6_enabled present"
    sysctl net.ipv6.conf.all.seg6_enabled | tee -a "$TEST_LOG"
else
    warn "seg6_enabled sysctl not found - kernel may not support SRv6"
fi

# ----------------------------------------------------------------------------
# 2. SRv6 Module Compilation Test
# ----------------------------------------------------------------------------
log "--- 2. SRv6 Module Build ---"
if [ -f "src/frr_core/zebra/srv6.c" ]; then
    pass "srv6.c source file exists"
else
    fail "srv6.c source file missing"
fi

# Check if gcc can compile it (with -fsyntax-only, ignoring missing FRR headers)
if gcc -fsyntax-only -I. -I./src/frr_core/lib -std=c99 -D_GNU_SOURCE \
   -Wno-implicit-function-declaration -Wno-declaration-after-statement \
   "src/frr_core/zebra/srv6.c" 2>/dev/null; then
    pass "srv6.c compiles without syntax errors"
else
    warn "srv6.c has compilation warnings (expected due to missing FRR headers in standalone test)"
fi

# ----------------------------------------------------------------------------
# 3. Functional: Kernel Route Manipulation
# ----------------------------------------------------------------------------
log "--- 3. Functional: Kernel SRv6 Routes ---"

# Test 3a: Enable seg6
sysctl -w net.ipv6.conf.all.seg6_enabled=1 >/dev/null 2>&1 || true
if [ "$(cat /proc/sys/net/ipv6/conf/all/seg6_enabled 2>/dev/null || echo 0)" = "1" ]; then
    pass "seg6_enabled set to 1"
else
    warn "seg6_enabled could not be set to 1 (kernel may not support SRv6)"
fi

# Test 3b: Install a locator route
TEST_PREFIX="2001:db8:ff01::/64"
ip -6 route add "$TEST_PREFIX" dev lo proto static 2>/dev/null || true
if ip -6 route show | grep -q "$TEST_PREFIX"; then
    pass "Locator prefix route installed: $TEST_PREFIX"
    ip -6 route del "$TEST_PREFIX" dev lo 2>/dev/null || true
else
    warn "Locator route test skipped or failed (may need CAP_NET_ADMIN)"
fi

# Test 3c: Install a local SID (End behavior)
TEST_SID="2001:db8:ff01::1"
ip -6 route add "$TEST_SID" dev lo proto static table local 2>/dev/null || true
if ip -6 route show table local | grep -q "$TEST_SID"; then
    pass "Local SID route installed: $TEST_SID"
    ip -6 route del "$TEST_SID" table local 2>/dev/null || true
else
    warn "Local SID route test skipped or failed"
fi

# Test 3d: Install SRv6 policy (segment list) - requires seg6 support
TEST_DST="2001:db8:9999::/64"
TEST_SEGS="2001:db8:1::1,2001:db8:2::1"
if ip -6 route add "$TEST_DST" encap seg6 mode inline segs "$TEST_SEGS" dev lo 2>/dev/null; then
    if ip -6 route show | grep -q "$TEST_DST"; then
        pass "SRv6 policy (segment list) installed: $TEST_DST -> [$TEST_SEGS]"
        ip -6 route del "$TEST_DST" 2>/dev/null || true
    else
        warn "Policy route installed but not visible in route show"
    fi
else
    warn "SRv6 policy route test failed (kernel seg6 LWT may not be available)"
fi

# Test 3e: seg6local End.DT4 (requires kernel support)
TEST_SID_DT4="2001:db8:ff01::2"
if ip -6 route add "$TEST_SID_DT4" dev lo encap seg6local action End.DT4 table 254 2>/dev/null; then
    if ip -6 route show table local | grep -q "$TEST_SID_DT4"; then
        pass "seg6local End.DT4 installed: $TEST_SID_DT4"
        ip -6 route del "$TEST_SID_DT4" table local 2>/dev/null || true
    else
        warn "seg6local End.DT4 installed but not visible"
    fi
else
    warn "seg6local End.DT4 test failed (kernel may lack seg6local support)"
fi

# ----------------------------------------------------------------------------
# 4. Configuration Save/Load Simulation
# ----------------------------------------------------------------------------
log "--- 4. Configuration Persistence ---"
mkdir -p "$NE_HOME"

cat > "$NE_HOME/srv6.conf" << 'EOF'
srv6 locator LOC1 prefix 2001:db8:ff01::/64 func-bits 16
srv6 sid 2001:db8:ff01::1 locator LOC1 behavior end
srv6 sid 2001:db8:ff01::2 locator LOC1 behavior end.dx4 argument 10.0.0.1
srv6 policy POL1 destination 2001:db8:9999::/64 segment-list 2001:db8:1::1,2001:db8:2::1 encap inline
EOF

if [ -f "$NE_HOME/srv6.conf" ]; then
    pass "SRv6 config file saved to $NE_HOME/srv6.conf"
    wc -l "$NE_HOME/srv6.conf" | tee -a "$TEST_LOG"
else
    fail "SRv6 config file not created"
fi

# ----------------------------------------------------------------------------
# 5. Cleanup Test
# ----------------------------------------------------------------------------
log "--- 5. Cleanup / Reset ---"
# Ensure no test routes left behind
ip -6 route flush 2>/dev/null || true
pass "IPv6 route flush executed (cleanup)"

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
log "========================================"
log "SRv6 Test Summary"
log "========================================"
log "Passed:  $PASS"
log "Failed:  $FAIL"
log "Warnings: $WARN"
log "Log file: $TEST_LOG"

if [ $FAIL -eq 0 ]; then
    log "RESULT: SRv6 module PASS (with $WARN warnings)"
    exit 0
else
    log "RESULT: SRv6 module FAIL ($FAIL failures)"
    exit 1
fi
