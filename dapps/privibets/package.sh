#!/bin/bash
# PriviBets .dapp Package Script
# Run from: dapps/privibets/
#
# Prerequisites:
#   - Shaders compiled (shaders/app.wasm exists)
#   - UI updated (ui/index.html has correct CID)
#   - 7-Zip installed at C:\Program Files\7-Zip\7z.exe

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== PriviBets Package Script ==="

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
echo "Creating PriviBets.dapp ..."
rm -f PriviBets.dapp
cd build
"C:/Program Files/7-Zip/7z.exe" a -tzip ../PriviBets.dapp ./*
cd ..

echo ""
echo "=== Done ==="
ls -la PriviBets.dapp
echo ""
echo "Install: Drag PriviBets.dapp into Beam Wallet DApps tab"
