#!/usr/bin/env bash
set -e

echo "╔══════════════════════════════════════════╗"
echo "║       FLUENT Language Installer           ║"
echo "╚══════════════════════════════════════════╝"
echo ""

# Check dependencies
command -v cmake  >/dev/null 2>&1 || { echo "Installing cmake..."; sudo apt-get install -y cmake 2>/dev/null || sudo yum install -y cmake 2>/dev/null || brew install cmake; }
command -v g++    >/dev/null 2>&1 || { echo "Installing g++...";   sudo apt-get install -y g++ 2>/dev/null || sudo yum install -y gcc-c++ 2>/dev/null || brew install gcc; }
command -v make   >/dev/null 2>&1 || { echo "Installing make...";  sudo apt-get install -y make 2>/dev/null || sudo yum install -y make 2>/dev/null; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "[1/3] Configuring build..."
mkdir -p "$BUILD_DIR"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local

echo "[2/3] Compiling FLUENT..."
cmake --build "$BUILD_DIR" --config Release -j$(nproc 2>/dev/null || echo 4)

echo "[3/3] Installing FLUENT..."
sudo cmake --install "$BUILD_DIR"

echo ""
echo "✓ FLUENT installed successfully!"
echo "  Run your scripts with:  fluent script.fluent"
echo ""

# Optional: add shell wrapper for running .fluent directly
WRAPPER=/usr/local/bin/fluent-run
sudo tee "$WRAPPER" > /dev/null <<'WRAPPER_EOF'
#!/usr/bin/env bash
exec fluent "$@"
WRAPPER_EOF
sudo chmod +x "$WRAPPER"
