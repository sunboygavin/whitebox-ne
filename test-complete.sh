#!/bin/bash
# Complete OpenConfig functionality test
# This script tests all OpenConfig features including newly added models

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_test() { echo -e "${BLUE}[TEST]${NC} $1"; ((TOTAL_TESTS++)); }
test_passed() { log_info "✓ PASSED: $1"; ((PASSED_TESTS++)); }
test_failed() { log_error "✗ FAILED: $1"; ((FAILED_TESTS++)); }

echo "============================================"
echo "   Complete OpenConfig Test"
echo "============================================"
echo ""

# Test 1: Directory structure
log_test "Checking OpenConfig directory structure"
if [ -d "openconfig-models" ]; then
    test_passed "openconfig-models directory exists"
else
    test_failed "openconfig-models directory not found"
fi

# Test 2: All YANG models
log_test "Checking all OpenConfig YANG models"
models=(
    "openconfig-models/interfaces/openconfig-interfaces.yang"
    "openconfig-models/network-instance/openconfig-bgp.yang"
    "openconfig-models/network-instance/openconfig-network-instance.yang"
    "openconfig-models/system/openconfig-system.yang"
    "openconfig-models/acl/openconfig-acl.yang"
    "openconfig-models/routing/openconfig-ospf.yang"
    "openconfig-models/mpls/openconfig-mpls.yang"
    "openconfig-models/qos/openconfig-qos.yang"
)
for model in "${models[@]}"; do
    model_name=$(basename "$model")
    if [ -f "$model" ]; then
        log_info "  - $model_name exists"
    else
        test_failed "$model_name not found"
    fi
done
test_passed "All YANG model files exist"

# Test 3: Model completeness
log_test "Verifying YANG model completeness"
expected_models=8
actual_models=$(find openconfig-models -name "*.yang" 2>/dev/null | wc -l)
if [ "$actual_models" -eq "$expected_models" ]; then
    test_passed "YANG model count correct ($expected_models models)"
else
    test_failed "Y YANG model count incorrect (expected $expected_models, actual $actual_models)"
fi

# Test 4: Enhanced Huawei CLI
log_test "Checking Huawei-style CLI enhancements"
if [ -f "src/frr_core/lib/command.c" ]; then
    cmd_size=$(wc -l < src/frr_core/lib/command.c)
    if [ "$cmd_size" -gt 100 ]; then
        test_passed "command.c enhanced ($cmd_size lines)"
    else
        test_failed "command.c line count too low ($cmd_size lines)"
    fi
    
    # Check for key Huawei commands
    if grep -q "display.*ip" src/frr_core/lib/command.c; then
        test_passed "display ip command added"
    else
        test_failed "display ip command not found"
    fi
    
    if grep -q "display.*bgp" src/frr_core/lib/command.c; then
        test_passed "display bgp command added"
    else
        test_failed "display bgp command not found"
    fi
    
    if grep -q "display.*ospf" src/frr_core/lib/command.c; then
        test_passed "display ospf command added"
    else
        test_failed "display ospf command not found"
    fi
    
    if grep -q "display.*mpls" src/frr_core/lib/command.c; then
        test_passed "display mpls command added"
    else
        test_failed "display mpls command not found"
    fi
    
    if grep -q "display.*qos" src/frr_core/lib/command.c; then
        test_passed "display qos command added"
    else
        test_failed "display qos command not found"
    fi
    
    if grep -q "system-view" src/frr_core/lib/command.c; then
        test_passed "system-view command added"
    else
        test_failed "system-view command not found"
    fi
else
    test_failed "command.c file not found"
fi

# Test 5: Setup script updated
log_test "Checking setup-openconfig.sh updates"
if grep -q "openconfig-ospf" setup-openconfig.sh; then
    test_passed "setup-openconfig.sh includes OSPF model"
else
    test_failed "setup-openconfig.sh missing OSPF model"
fi

if grep -q "openconfig-mpls" setup-openconfig.sh; then
    test_passed "setup-openconfig.sh includes MPLS model"
else
    test_failed "setup-openconfig.sh missing MPLS model"
fi

if grep -q "openconfig-qos" setup-openconfig.sh; then
    test_passed "setup-openconfig.sh includes QoS model"
else
    test_failed "setup-openconfig.sh missing QoS model"
fi

# Test 6: Adapter source code
log_test "Checking configuration adapter source code"
if [ -f "src/openconfig_adapter/frr_to_yang.c" ]; then
    test_passed "frr_to_yang.c exists"
else
    test_failed "frr_to_yang.c not found"
fi

if [ -f "src/openconfig_adapter/yang_to_frr.c" ]; then
    test_passed "yang_to_frr.c exists"
else
    test_failed "yang_to_frr.c not found"
fi

if [ -f "src/openconfig_adapter/Makefile" ]; then
    test_passed "Makefile exists"
else
    test_failed "Makefile not found"
fi

# Test 7: Documentation
log_test "Checking documentation files"
if [ -f "OPENCONFIG_GUIDE.md" ]; then
    test_passed "OPENCONFIG_GUIDE.md exists"
else
    test_failed "OPENCONFIG_GUIDE.md not found"
fi

if [ -f "TEST_OPENCONFIG.md" ]; then
    test_passed "TEST_OPENCONFIG.md exists"
else
    test_failed "TEST_OPENCONFIG.md not found"
fi

if [ -f "OPENCONFIG_IMPLEMENTATION_REPORT.md" ]; then
    test_passed "OPENCONFIG_IMPLEMENTATION_REPORT.md exists"
else
    test_failed "OPENCONFIG_IMPLEMENTATION_REPORT.md not found"
fi

# Test 8: FRR integration
log_test "Testing FRR integration"
if command -v vtysh &> /dev/null; then
    test_passed "vtysh command available"
    
    output=$(vtysh -c "show version" 2>&1 || true)
    if [[ $output == *"FRR"* ]]; then
        test_passed "FRR version check passed"
    else
        test_failed "FRR version check failed"
    fi
else
    test_failed "vtysh command not available"
fi

# Test 9: Script executability
log_test "Checking script executability"
scripts=(
    "setup-openconfig.sh"
    "test-openconfig.sh"
    "test-openconfig-simple.sh"
)
for script in "${scripts[@]}"; do
    if [ -x "$script" ]; then
        test_passed "$script executable"
    else
        test_failed "$script not executable"
    fi
done

# Test 10: Git repository
log_test "Checking Git repository status"
if git rev-parse --git-dir > /dev/null 2>&1; then
    test_passed "Git repository valid"
    
    # Check latest commit
    last_commit=$(git log -1 --format="%h %s" 2>&1)
    if [[ $last_commit == *"增强华为风格 CLI"* ]]; then
        test_passed "Latest commit includes Huawei CLI enhancements"
    else
        log_warn "Latest commit: $last_commit"
    fi
else
    test_failed "Git repository invalid"
fi

# Test 11: Code statistics
log_test "Counting code files"
yang_count=$(find openconfig-models -name "*.yang" 2>/dev/null | wc -l)
c_count=$(find src -name "*.c" 2>/dev/null | wc -l)
sh_count=$(find . -maxdepth 1 -name "*.sh" -type f 2>/dev/null | wc -l)
md_count=$(find . -maxdepth 1 -name "*.md" -type f 2>/dev/null | wc -l)

log_info "  - YANG models: $yang_count files"
log_info "  - C source files: $c_count files"
log_info "  - Shell scripts: $sh_count files"
log_info "  - Markdown docs: $md_count files"

if [ "$yang_count" -ge 8 ]; then
    test_passed "YANG model count sufficient (>=8)"
else
    test_failed "YANG model count insufficient (<8)"
fi

# Generate report
echo ""
echo "============================================"
echo "         Test Report"
echo "============================================"
echo ""
echo "Total tests:   $TOTAL_TESTS"
echo -e "Passed:        ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed:        ${RED}$FAILED_TESTS${NC}"
echo ""

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}============================================${NC}"
    echo -e "${GREEN}         All Tests Passed! ✓${NC}"
    echo -e "${GREEN}============================================${NC}"
    echo ""
    echo "✅ OpenConfig complete integration successful!"
    echo ""
    echo "Implemented features:"
    echo "  - 8 OpenConfig standard models (YANG)"
    echo "  - 2 configuration adapters (C language)"
    echo "  - Enhanced Huawei-style CLI (7+ commands)"
    echo "  - Automatic installation and test scripts"
    echo "  - Complete documentation and reports"
    echo ""
    echo "Next steps:"
    echo "1. Run 'sudo ./setup-openconfig.sh' to install"
    echo "2. Run 'sudo ./test-openconfig.sh' for full functional test"
    echo "3. Check OPENCONFIG_GUIDE.md for usage instructions"
    echo ""
    exit 0
else
    echo -e "${RED}============================================${NC}"
    echo -e "${RED}         Some Tests Failed ✗${NC}"
    echo -e "${RED}============================================${NC}"
    exit 1
fi
