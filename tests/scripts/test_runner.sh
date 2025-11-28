#!/bin/bash
#
# ThunderOS Test Runner
# A visually appealing test runner with progress tracking
#
# Usage: ./test_runner.sh [OPTIONS]
#   --quick    Run only boot test (faster, ~5s)
#   --help     Show this help message
#
# Output is saved to tests/outputs/test_results.log
#

set -e

# Ensure TERM is set for tput commands (needed for CI environments)
export TERM="${TERM:-dumb}"

# Detect CI/non-interactive environment
if [ -z "$TERM" ] || [ "$TERM" = "dumb" ] || [ ! -t 1 ]; then
    CI_MODE=1
else
    CI_MODE=0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../.."
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/../outputs"
LOG_FILE="${OUTPUT_DIR}/test_results.log"
QEMU_TIMEOUT=12

# Options
QUICK_MODE=0

# Parse arguments
for arg in "$@"; do
    case $arg in
        --quick)
            QUICK_MODE=1
            ;;
        --help)
            echo "ThunderOS Test Runner"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --quick    Run only boot test (faster, ~5s)"
            echo "  --help     Show this help message"
            echo ""
            echo "Output is saved to: tests/outputs/test_results.log"
            exit 0
            ;;
    esac
done

# QEMU detection
if command -v qemu-system-riscv64 >/dev/null 2>&1; then
    QEMU_BIN="qemu-system-riscv64"
elif [ -x /tmp/qemu-10.1.2/build/qemu-system-riscv64 ]; then
    QEMU_BIN="/tmp/qemu-10.1.2/build/qemu-system-riscv64"
else
    echo "ERROR: qemu-system-riscv64 not found"
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"

# Colors and formatting
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
WHITE='\033[1;37m'
DIM='\033[2m'
BOLD='\033[1m'
NC='\033[0m'

# Unicode symbols
CHECK="✓"
CROSS="✗"
BULLET="○"
ARROW="▶"
SPINNER=('⠋' '⠙' '⠹' '⠸' '⠼' '⠴' '⠦' '⠧' '⠇' '⠏')

# Test stages configuration
declare -a STAGES
declare -A STAGE_STATUS
declare -A STAGE_PATTERN

if [ $QUICK_MODE -eq 1 ]; then
    STAGES=(
        "Kernel Boot"
        "UART Initialization"
        "Interrupt System"
        "Memory Management"
        "Virtual Memory"
    )
    STAGE_PATTERN["Kernel Boot"]="ThunderOS.*RISC-V\|Kernel loaded"
    STAGE_PATTERN["UART Initialization"]="\[OK\] UART initialized"
    STAGE_PATTERN["Interrupt System"]="\[OK\] Interrupt subsystem initialized"
    STAGE_PATTERN["Memory Management"]="\[OK\] Memory management initialized"
    STAGE_PATTERN["Virtual Memory"]="\[OK\] Virtual memory initialized"
    QEMU_TIMEOUT=6
else
    STAGES=(
        "Kernel Boot"
        "UART Initialization"
        "Interrupt System"
        "Trap Handler"
        "Memory Management"
        "Virtual Memory (Sv39)"
        "DMA Allocator"
        "Memory Tests"
        "ELF Loader Tests"
        "Process Management"
        "Scheduler"
        "Pipe Subsystem"
        "VirtIO Block Device"
        "ext2 Filesystem"
        "VFS Mount"
        "System Stability"
    )
    STAGE_PATTERN["Kernel Boot"]="ThunderOS.*RISC-V\|Kernel loaded"
    STAGE_PATTERN["UART Initialization"]="\[OK\] UART initialized"
    STAGE_PATTERN["Interrupt System"]="\[OK\] Interrupt subsystem initialized"
    STAGE_PATTERN["Trap Handler"]="\[OK\] Trap handler initialized"
    STAGE_PATTERN["Memory Management"]="\[OK\] Memory management initialized"
    STAGE_PATTERN["Virtual Memory (Sv39)"]="\[OK\] Virtual memory initialized"
    STAGE_PATTERN["DMA Allocator"]="\[OK\] DMA allocator initialized"
    STAGE_PATTERN["Memory Tests"]="Status: ALL TESTS PASSED"
    STAGE_PATTERN["ELF Loader Tests"]="\*\*\* ALL TESTS PASSED \*\*\*"
    STAGE_PATTERN["Process Management"]="\[OK\] Process management initialized"
    STAGE_PATTERN["Scheduler"]="\[OK\] Scheduler initialized"
    STAGE_PATTERN["Pipe Subsystem"]="\[OK\] Pipe subsystem initialized"
    STAGE_PATTERN["VirtIO Block Device"]="\[OK\] VirtIO block device initialized"
    STAGE_PATTERN["ext2 Filesystem"]="\[OK\] ext2 filesystem mounted"
    STAGE_PATTERN["VFS Mount"]="\[OK\] VFS root filesystem mounted"
    STAGE_PATTERN["System Stability"]="Test Mode - Halting\|All kernel tests completed"
fi

# Initialize all stages as pending
for stage in "${STAGES[@]}"; do
    STAGE_STATUS["$stage"]="pending"
done

# Terminal control (disabled in CI mode)
hide_cursor() { 
    if [ $CI_MODE -eq 0 ]; then
        printf '\033[?25l'
    fi
}
show_cursor() { 
    if [ $CI_MODE -eq 0 ]; then
        printf '\033[?25h'
    fi
}

# Cleanup on exit
cleanup() {
    show_cursor
    if [ $CI_MODE -eq 0 ]; then
        tput cnorm 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Draw the progress display
draw_progress() {
    local current_stage="$1"
    local total=${#STAGES[@]}
    local passed=0
    local failed=0
    
    for stage in "${STAGES[@]}"; do
        case "${STAGE_STATUS[$stage]}" in
            passed) passed=$((passed + 1)) ;;
            failed) failed=$((failed + 1)) ;;
        esac
    done
    
    local percent=0
    if [ $total -gt 0 ]; then
        percent=$((passed * 100 / total))
    fi
    
    # In CI mode, use simple output without screen clearing
    if [ $CI_MODE -eq 1 ]; then
        return
    fi
    
    # Clear screen and draw (interactive mode only)
    clear
    
    echo ""
    echo -e "  ${BOLD}${CYAN}╔══════════════════════════════════════════════════════════╗${NC}"
    echo -e "  ${BOLD}${CYAN}║${NC}           ${BOLD}${WHITE}⚡ ThunderOS Test Suite ⚡${NC}              ${BOLD}${CYAN}║${NC}"
    echo -e "  ${BOLD}${CYAN}╚══════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    # Progress bar
    local bar_width=44
    local filled=$((percent * bar_width / 100))
    local empty=$((bar_width - filled))
    
    printf "  ${DIM}Progress${NC} ["
    printf "${GREEN}"
    for ((i=0; i<filled; i++)); do printf "█"; done
    printf "${DIM}"
    for ((i=0; i<empty; i++)); do printf "░"; done
    printf "${NC}] ${BOLD}%3d%%${NC}\n" "$percent"
    
    echo ""
    echo -e "  ${BOLD}${CYAN}┌──────────────────────────────────────────────────────────┐${NC}"
    echo -e "  ${BOLD}${CYAN}│${NC}  ${DIM}Test Stage${NC}                                   ${DIM}Status${NC}  ${BOLD}${CYAN}│${NC}"
    echo -e "  ${BOLD}${CYAN}├──────────────────────────────────────────────────────────┤${NC}"
    
    # Stages list
    for stage in "${STAGES[@]}"; do
        local status="${STAGE_STATUS[$stage]}"
        local padding=$((45 - ${#stage}))
        local spaces=""
        for ((i=0; i<padding; i++)); do spaces+=" "; done
        
        case "$status" in
            passed)
                echo -e "  ${BOLD}${CYAN}│${NC}   ${GREEN}${CHECK}${NC} ${stage}${spaces}${GREEN}PASS${NC}   ${BOLD}${CYAN}│${NC}"
                ;;
            failed)
                echo -e "  ${BOLD}${CYAN}│${NC}   ${RED}${CROSS}${NC} ${stage}${spaces}${RED}FAIL${NC}   ${BOLD}${CYAN}│${NC}"
                ;;
            running)
                echo -e "  ${BOLD}${CYAN}│${NC}   ${YELLOW}${ARROW}${NC} ${YELLOW}${stage}${NC}${spaces}${DIM}....${NC}   ${BOLD}${CYAN}│${NC}"
                ;;
            *)
                echo -e "  ${BOLD}${CYAN}│${NC}   ${DIM}${BULLET} ${stage}${spaces}    ${NC}   ${BOLD}${CYAN}│${NC}"
                ;;
        esac
    done
    
    echo -e "  ${BOLD}${CYAN}└──────────────────────────────────────────────────────────┘${NC}"
    echo ""
    
    # Status footer
    if [ $failed -gt 0 ]; then
        echo -e "  ${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        echo -e "  ${RED}${CROSS}${NC} ${RED}${failed} failed${NC}  ${GREEN}${CHECK}${NC} ${GREEN}${passed} passed${NC}  ${DIM}of ${total} tests${NC}"
        echo -e "  ${RED}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    elif [ $passed -eq $total ]; then
        echo -e "  ${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        echo -e "  ${GREEN}${CHECK}${NC} ${BOLD}${GREEN}All ${total} tests passed successfully!${NC}"
        echo -e "  ${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    else
        echo -e "  ${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        if [ -n "$current_stage" ]; then
            echo -e "  ${YELLOW}${ARROW}${NC} Running: ${current_stage}..."
        else
            echo -e "  ${DIM}Preparing tests...${NC}"
        fi
        echo -e "  ${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    fi
}

# Update stage status from log file
update_stages() {
    for stage in "${STAGES[@]}"; do
        if [ "${STAGE_STATUS[$stage]}" = "pending" ] || [ "${STAGE_STATUS[$stage]}" = "running" ]; then
            local pattern="${STAGE_PATTERN[$stage]}"
            if grep -q "$pattern" "${LOG_FILE}" 2>/dev/null; then
                STAGE_STATUS["$stage"]="passed"
            fi
        fi
    done
}

# Get current running stage
get_current_stage() {
    for stage in "${STAGES[@]}"; do
        if [ "${STAGE_STATUS[$stage]}" = "running" ]; then
            echo "$stage"
            return
        fi
    done
    echo ""
}

# Build phase
build_kernel() {
    cd "${ROOT_DIR}"
    make clean >/dev/null 2>&1
    make ENABLE_TESTS=1 TEST_MODE=1 >/dev/null 2>&1
}

build_userland() {
    cd "${ROOT_DIR}"
    make userland >/dev/null 2>&1 || true
}

create_filesystem() {
    local disk_image="${BUILD_DIR}/test_fs.img"
    rm -rf "${BUILD_DIR}/test_fs_contents"
    mkdir -p "${BUILD_DIR}/test_fs_contents/bin"
    
    echo "Test file for ThunderOS" > "${BUILD_DIR}/test_fs_contents/test.txt"
    echo "Hello from ext2!" > "${BUILD_DIR}/test_fs_contents/hello.txt"
    
    for prog in hello cat ls pwd mkdir rmdir clear ush; do
        if [ -f "${ROOT_DIR}/userland/build/$prog" ]; then
            cp "${ROOT_DIR}/userland/build/$prog" "${BUILD_DIR}/test_fs_contents/bin/"
        fi
    done
    
    mkfs.ext2 -F -q -d "${BUILD_DIR}/test_fs_contents" "${disk_image}" 10M >/dev/null 2>&1
    rm -rf "${BUILD_DIR}/test_fs_contents"
}

run_qemu() {
    local disk_image="${BUILD_DIR}/test_fs.img"
    
    if [ $QUICK_MODE -eq 1 ]; then
        # Quick mode: no disk, just boot test
        timeout $((QEMU_TIMEOUT + 2)) "${QEMU_BIN}" \
            -machine virt \
            -m 128M \
            -nographic \
            -serial mon:stdio \
            -bios none \
            -kernel "${BUILD_DIR}/thunderos.elf" \
            </dev/null 2>&1 > "${LOG_FILE}" || true
    else
        # Full mode: with VirtIO disk
        timeout $((QEMU_TIMEOUT + 2)) "${QEMU_BIN}" \
            -machine virt \
            -m 128M \
            -nographic \
            -serial mon:stdio \
            -bios none \
            -kernel "${BUILD_DIR}/thunderos.elf" \
            -global virtio-mmio.force-legacy=false \
            -drive file="${disk_image}",if=none,format=raw,id=hd0 \
            -device virtio-blk-device,drive=hd0 \
            </dev/null 2>&1 > "${LOG_FILE}" || true
    fi
}

# Main execution
main() {
    # CI mode header
    if [ $CI_MODE -eq 1 ]; then
        echo "╔══════════════════════════════════════════════════════════╗"
        echo "║           ⚡ ThunderOS Test Suite ⚡                     ║"
        echo "╚══════════════════════════════════════════════════════════╝"
        echo ""
        if [ $QUICK_MODE -eq 1 ]; then
            echo "[*] Running quick boot test..."
        else
            echo "[*] Running full test suite..."
        fi
        echo ""
    fi
    
    hide_cursor
    
    # Clear log file
    > "${LOG_FILE}"
    
    # Initial display
    draw_progress ""
    
    # Build phase
    STAGE_STATUS["Kernel Boot"]="running"
    if [ $CI_MODE -eq 1 ]; then
        echo "[*] Building kernel..."
    fi
    draw_progress "Building kernel"
    
    build_kernel
    
    if [ $QUICK_MODE -eq 0 ]; then
        if [ $CI_MODE -eq 1 ]; then
            echo "[*] Building userland..."
        fi
        build_userland
        if [ $CI_MODE -eq 1 ]; then
            echo "[*] Creating test filesystem..."
        fi
        create_filesystem
    fi
    
    if [ $CI_MODE -eq 1 ]; then
        echo "[*] Starting QEMU (timeout: ${QEMU_TIMEOUT}s)..."
    fi
    draw_progress "Starting QEMU"
    
    # Run QEMU in background
    run_qemu &
    local qemu_pid=$!
    
    # Monitor progress
    while kill -0 $qemu_pid 2>/dev/null; do
        update_stages
        
        # Check if all tests passed - exit early if so
        local all_passed=1
        for stage in "${STAGES[@]}"; do
            if [ "${STAGE_STATUS[$stage]}" != "passed" ]; then
                all_passed=0
                break
            fi
        done
        
        if [ $all_passed -eq 1 ]; then
            # All tests passed, kill QEMU and finish
            kill $qemu_pid 2>/dev/null || true
            wait $qemu_pid 2>/dev/null || true
            draw_progress ""
            break
        fi
        
        # Find first pending stage and mark as running
        local found_running=0
        for stage in "${STAGES[@]}"; do
            if [ "${STAGE_STATUS[$stage]}" = "running" ]; then
                found_running=1
                break
            fi
        done
        
        if [ $found_running -eq 0 ]; then
            for stage in "${STAGES[@]}"; do
                if [ "${STAGE_STATUS[$stage]}" = "pending" ]; then
                    STAGE_STATUS["$stage"]="running"
                    break
                fi
            done
        fi
        
        draw_progress "$(get_current_stage)"
        sleep 0.15
    done
    
    wait $qemu_pid 2>/dev/null || true
    
    # Final update
    update_stages
    
    # Mark any remaining pending/running stages as failed
    for stage in "${STAGES[@]}"; do
        if [ "${STAGE_STATUS[$stage]}" = "pending" ] || [ "${STAGE_STATUS[$stage]}" = "running" ]; then
            STAGE_STATUS["$stage"]="failed"
        fi
    done
    
    draw_progress ""
    show_cursor
    
    # Check final result and print CI-friendly output
    local failed=0
    local passed=0
    for stage in "${STAGES[@]}"; do
        if [ "${STAGE_STATUS[$stage]}" = "failed" ]; then
            failed=$((failed + 1))
        elif [ "${STAGE_STATUS[$stage]}" = "passed" ]; then
            passed=$((passed + 1))
        fi
    done
    
    # In CI mode, print detailed results
    if [ $CI_MODE -eq 1 ]; then
        echo ""
        echo "Test Results:"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        for stage in "${STAGES[@]}"; do
            if [ "${STAGE_STATUS[$stage]}" = "passed" ]; then
                echo "  ✓ PASS: $stage"
            else
                echo "  ✗ FAIL: $stage"
            fi
        done
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo ""
        if [ $failed -eq 0 ]; then
            echo "✓ All ${passed} tests passed!"
        else
            echo "✗ ${failed} tests failed, ${passed} passed"
        fi
        echo ""
    fi
    
    echo ""
    echo -e "  ${DIM}Log saved to: tests/outputs/test_results.log${NC}"
    echo ""
    
    if [ $failed -eq 0 ]; then
        exit 0
    else
        exit 1
    fi
}

main
