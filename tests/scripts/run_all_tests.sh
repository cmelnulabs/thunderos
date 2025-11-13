#!/bin/bash
#
# Run All ThunderOS Tests
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo ""
echo "========================================"
echo "  ThunderOS Complete Test Suite"
echo "========================================"
echo ""

TOTAL_FAILED=0

# Test 1: Boot test
echo -e "${BLUE}[1/2]${NC} Running boot test..."
if bash "${SCRIPT_DIR}/test_boot.sh"; then
    echo -e "${GREEN}✓ Boot test passed${NC}"
else
    echo -e "${RED}✗ Boot test failed${NC}"
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
