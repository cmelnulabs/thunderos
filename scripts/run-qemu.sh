#!/usr/bin/env bash
set -euo pipefail

QEMU_OUT="qemu_output.log"
QEMU_PID_FILE="qemu.pid"
TIMEOUT_SECONDS=${TIMEOUT_SECONDS:-60}
QEMU_FILE_WAIT_ATTEMPTS=5

wait_for_qemu() {
  local pattern="$1"
  local timeout=${2:-$TIMEOUT_SECONDS}
  echo "Waiting up to ${timeout}s for pattern: ${pattern}"
  
  # Wait for the output file to be created (give QEMU a moment to create it)
  local wait_file=0
  while [ ! -f "$QEMU_OUT" ] && [ "$wait_file" -lt "$QEMU_FILE_WAIT_ATTEMPTS" ]; do
    sleep 0.5
    wait_file=$((wait_file + 1))
  done
  
  # Use tail -f with timeout for proper synchronization
  # This avoids race conditions by continuously monitoring the file as it's written
  if timeout "$timeout" tail -f "$QEMU_OUT" 2>/dev/null | grep -q -m 1 "$pattern"; then
    echo "Found pattern: $pattern"
    return 0
  fi
  
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
