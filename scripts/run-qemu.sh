#!/usr/bin/env bash
set -euo pipefail

QEMU_OUT="qemu_output.log"
QEMU_PID_FILE="qemu.pid"
TIMEOUT_SECONDS=${TIMEOUT_SECONDS:-60}

wait_for_qemu() {
  local pattern="$1"
  local timeout=${2:-$TIMEOUT_SECONDS}
  local interval=1
  local elapsed=0
  echo "Waiting up to ${timeout}s for pattern: ${pattern}"
  while [ "$elapsed" -lt "$timeout" ]; do
    if grep -q "$pattern" "$QEMU_OUT"; then
      echo "Found pattern: $pattern"
      return 0
    fi
    sleep $interval
    elapsed=$((elapsed + interval))
  done
  echo "Timed out waiting for pattern: $pattern"
  echo "----- ${QEMU_OUT} -----"
  cat "$QEMU_OUT" || true
  echo "----------------------"
  return 1
}

run_qemu() {
  rm -f "$QEMU_OUT" "$QEMU_PID_FILE"
  # Example qemu args; preserve any existing args passed to the script
  qemu-system-riscv64 "$@" -serial file:"$QEMU_OUT" -display none -no-reboot &
  QEMU_PID=$!
  echo "$QEMU_PID" > "$QEMU_PID_FILE"
  echo "Started qemu pid=$QEMU_PID (saved to $QEMU_PID_FILE)"
  # Wait for boot string
  wait_for_qemu "ThunderOS" || {
    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
    rm -f "$QEMU_PID_FILE"
    return 1
  }
  # Keep QEMU running for further tests; consumer can kill it
  return 0
}

# If called directly, pass all args to qemu
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  run_qemu "$@"
fi
