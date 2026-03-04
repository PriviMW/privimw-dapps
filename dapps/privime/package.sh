#!/bin/bash
# PriviMe .dapp Package Script
# Run from: dapps/privime/
#
# Prerequisites:
#   - Shaders compiled (shaders/app.wasm)
#   - Node.js installed (npm run build)
#   - 7-Zip installed at C:\Program Files\7-Zip\7z.exe

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== PriviMe Package Script ==="

# Step 0: Build UI from source (staging CID injected automatically)
echo "Building UI from src/ ..."
npm run build

# Step 1: Refresh build/ staging area
echo "Refreshing build/app/ ..."
mkdir -p build/app
cp ui/index.html build/app/
cp ui/appicon.svg build/app/
cp shaders/app.wasm build/app/
cp manifest.json build/

echo "Build staging ready:"
ls -la build/app/

# Step 2: Package with 7-Zip
echo ""
echo "Creating PriviMe.dapp ..."
rm -f PriviMe.dapp
cd build
"C:/Program Files/7-Zip/7z.exe" a -tzip ../PriviMe.dapp ./*
cd ..

echo ""
echo "=== Done ==="
ls -la PriviMe.dapp
echo ""
echo "Install: Drag PriviMe.dapp into Beam Wallet DApps tab"
