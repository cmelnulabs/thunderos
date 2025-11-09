#!/usr/bin/env bash
#
# Build ThunderOS documentation
#
# Generates HTML documentation using Sphinx.
# Output: docs/build/html/index.html

set -euo pipefail

readonly DOCS_DIR="docs"
readonly BUILD_DIR="${DOCS_DIR}/build/html"

main() {
    echo "Building ThunderOS documentation..."
    
    cd "${DOCS_DIR}"
    make html
    
    echo ""
    echo "✓ Documentation built successfully"
    echo "  Open: ${BUILD_DIR}/index.html"
}

main "$@"
