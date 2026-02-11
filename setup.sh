#!/bin/bash
# ═══════════════════════════════════════════════════════
# God Simulation - Phase 0 Setup Script
# ═══════════════════════════════════════════════════════
# This script sets up the project in your cloned GitHub repo.
#
# Usage:
#   1. Clone your repo:  git clone https://github.com/1sitm3n/God-Simulation.git
#   2. Extract this archive next to it (or wherever you downloaded it)
#   3. Run:  bash setup.sh /path/to/God-Simulation
# ═══════════════════════════════════════════════════════

set -e

REPO_DIR="${1:-.}"

if [ ! -d "$REPO_DIR/.git" ]; then
    echo "Error: $REPO_DIR is not a git repository."
    echo "Usage: bash setup.sh /path/to/God-Simulation"
    exit 1
fi

echo "═══ Setting up God Simulation Phase 0 ═══"
echo "Target: $REPO_DIR"

# Copy all project files
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Copying project files..."
cp -r "$SCRIPT_DIR/src" "$REPO_DIR/"
cp -r "$SCRIPT_DIR/tests" "$REPO_DIR/"
cp -r "$SCRIPT_DIR/docs" "$REPO_DIR/"
cp "$SCRIPT_DIR/CMakeLists.txt" "$REPO_DIR/"
cp "$SCRIPT_DIR/.gitignore" "$REPO_DIR/"
cp "$SCRIPT_DIR/.clang-format" "$REPO_DIR/"
cp "$SCRIPT_DIR/README.md" "$REPO_DIR/"

echo ""
echo "═══ Project files copied! ═══"
echo ""
echo "Next steps:"
echo "  cd $REPO_DIR"
echo "  cmake -B build -DCMAKE_BUILD_TYPE=Debug"
echo "  cmake --build build -j\$(nproc)"
echo "  ./build/godsim"
echo "  cd build && ctest --output-on-failure"
echo ""
echo "Then commit and push:"
echo "  git add -A"
echo "  git commit -m \"Phase 0: Foundation - ECS, Events, Ticks, RNG, Serialisation\""
echo "  git push origin main"
