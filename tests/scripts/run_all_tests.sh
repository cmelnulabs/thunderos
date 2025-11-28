#!/bin/bash
#
# Run All ThunderOS Tests
#
# This script runs all non-interactive tests:
#   - Kernel functionality test (comprehensive)
#   - Boot test (quick sanity check)
#   - Integration test (VirtIO, ext2, programs)
#
# Usage: ./run_all_tests.sh [--quick]
#   --quick: Run only boot test (faster, for CI)
#

set -e

# Ensure TERM is set for tput commands (needed for CI environments)
export TERM="${TERM:-dumb}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo ""
echo "========================================"
echo "  ThunderOS Complete Test Suite"
echo "========================================"
echo ""

TOTAL_FAILED=0
QUICK_MODE=0

# Parse arguments
for arg in "$@"; do
    case $arg in
        --quick)
            QUICK_MODE=1
            ;;
    esac
done

if [ $QUICK_MODE -eq 1 ]; then
    echo -e "${YELLOW}Running in quick mode (boot test only)${NC}"
    echo ""
    
    # Quick mode: just boot test
    echo -e "${BLUE}[1/1]${NC} Running boot test..."
    if bash "${SCRIPT_DIR}/test_boot.sh"; then
        echo -e "${GREEN}✓ Boot test passed${NC}"
    else
        echo -e "${RED}✗ Boot test failed${NC}"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
    fi
else
    # Full test suite
    
    # Test 1: Kernel functionality test (comprehensive)
    echo -e "${BLUE}[1/2]${NC} Running kernel functionality test..."
    if bash "${SCRIPT_DIR}/test_kernel.sh"; then
        echo -e "${GREEN}✓ Kernel functionality test passed${NC}"
    else
        echo -e "${RED}✗ Kernel functionality test failed${NC}"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
    fi

    echo ""

    # Test 2: Integration test
    echo -e "${BLUE}[2/2]${NC} Running integration test..."
    if bash "${SCRIPT_DIR}/test_integration.sh"; then
        echo -e "${GREEN}✓ Integration test passed${NC}"
    else
        echo -e "${RED}✗ Integration test failed${NC}"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
    fi
fi

# Summary
echo ""
echo "========================================"
echo "  Complete Test Suite Summary"
echo "========================================"
echo ""

if [ $TOTAL_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ ALL TESTS PASSED${NC}"
    exit 0
else
    echo -e "${RED}✗ $TOTAL_FAILED test suite(s) failed${NC}"
    exit 1
fi
