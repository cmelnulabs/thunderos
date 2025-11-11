#!/bin/bash
# Automated test for program execution from disk

cd /workspaces/thunderos

echo "==================================="
echo "Testing Program Execution from Disk"
echo "==================================="
echo ""

# Create expect script to automate QEMU interaction
cat > /tmp/test_exec.exp << 'EOF'
#!/usr/bin/expect -f

set timeout 30

# Start QEMU
spawn make qemu-ext2

# Wait for shell prompt
expect "ThunderOS> " {
    send "ls /bin\r"
}

expect "ThunderOS> " {
    send "hello\r"
}

expect "ThunderOS> " {
    send "echo Test complete\r"
}

expect "ThunderOS> " {
    send "\x01x"
}

expect eof
EOF

chmod +x /tmp/test_exec.exp

# Check if expect is available
if command -v expect &> /dev/null; then
    /tmp/test_exec.exp
else
    echo "expect not installed, running manual test instead..."
    echo ""
    echo "Please test manually with these commands:"
    echo "  1. ls /bin"
    echo "  2. hello"
    echo "  3. Ctrl-A then X to exit"
    echo ""
    make qemu-ext2
fi
