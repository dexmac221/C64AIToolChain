#!/bin/bash
# Test loop for snake2 - builds, runs VICE, and monitors with ai_toolchain

echo "=== Snake2 Test Loop ==="
echo ""

# Step 1: Build
echo "1. Building snake2..."
cd /home/dexmac/work/cc65/projects/C64AIToolChain/snake2
./build.sh
if [ $? -ne 0 ]; then
    echo "✗ Build failed!"
    exit 1
fi
echo "✓ Build successful"
echo ""

# Step 2: Check if VICE is running
echo "2. Checking VICE status..."
if netstat -an 2>/dev/null | grep -q "6510.*LISTEN"; then
    echo "✓ VICE is already running"
    echo "  Reloading game..."
    python3 reload.py
else
    echo "  Starting VICE..."
    # Kill any existing VICE
    pkill -9 x64sc 2>/dev/null
    pkill -9 x64 2>/dev/null
    sleep 1

    # Start VICE in background
    ./run_vice.sh &
    echo "  Waiting for VICE to start..."
    sleep 5

    # Check if it started
    if netstat -an 2>/dev/null | grep -q "6510.*LISTEN"; then
        echo "✓ VICE started successfully"
    else
        echo "✗ VICE failed to start"
        echo "  Try manually: cd snake2 && ./run_vice.sh"
        exit 1
    fi
fi
echo ""

# Step 3: Monitor with ai_toolchain
echo "3. Monitoring game with ai_toolchain..."
echo "   Press Ctrl+C to stop monitoring"
echo ""
cd /home/dexmac/work/cc65/projects/C64AIToolChain
python3 ai_toolchain.py
