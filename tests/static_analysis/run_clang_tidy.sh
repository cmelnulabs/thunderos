#!/bin/bash

# Generate compile_commands.json for clang-tidy
# This tells clang-tidy the correct compiler flags and include paths

INCLUDE_DIR="./include"
CFLAGS="-march=rv64gc -mabi=lp64d -mcmodel=medany -nostdlib -nostartfiles -ffreestanding -fno-common -O0 -g -Wall -Wextra -I$INCLUDE_DIR"

echo "Generating compile_commands.json..."

cat > compile_commands.json << 'EOF'
[]
EOF

echo "[" > compile_commands.json
first=true

for file in $(find kernel userland tests -name "*.c" -type f | sort); do
    if [ "$first" = false ]; then
        echo "," >> compile_commands.json
    fi
    first=false
    
    cat >> compile_commands.json << ENTRY
  {
    "directory": "$PWD",
    "command": "riscv64-unknown-elf-gcc $CFLAGS -c $file -o /dev/null",
    "file": "$file"
  }
ENTRY
done

echo "]" >> compile_commands.json

# Verify JSON is valid
if python3 -m json.tool compile_commands.json > /dev/null 2>&1; then
    echo "✅ compile_commands.json generated successfully"
else
    echo "❌ Invalid JSON in compile_commands.json"
    exit 1
fi

echo ""
echo "Running clang-tidy analysis..."
echo ""

# Run clang-tidy on all files
find kernel userland tests -name "*.c" -type f | sort | xargs clang-tidy --header-filter=".*" 2>&1 | tee clang_tidy_analysis.txt

# Extract summary
echo ""
echo "=== ANALYSIS SUMMARY ==="
echo ""
echo "Real errors (from source code):"
grep -E "^\/" clang_tidy_analysis.txt | grep "error:" | wc -l
echo ""
echo "Real warnings (from source code):"
grep -E "^\/" clang_tidy_analysis.txt | grep "warning:" | wc -l
echo ""
echo "Unique error types:"
grep -E "^\/" clang_tidy_analysis.txt | grep "error:" | sed 's/.*\[\(.*\)\].*/\1/' | sort | uniq -c | sort -rn | head -10
echo ""
echo "Unique warning types:"
grep -E "^\/" clang_tidy_analysis.txt | grep "warning:" | sed 's/.*\[\(.*\)\].*/\1/' | sort | uniq -c | sort -rn | head -10
