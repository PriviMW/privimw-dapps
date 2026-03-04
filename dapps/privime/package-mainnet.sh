#!/bin/bash
# PriviMe Mainnet .dapp Package Script
# Run from: dapps/privime/
#
# Prerequisites:
#   - Shaders compiled (shaders/app.wasm)
#   - Node.js installed (npm run build:mainnet)
#   - 7-Zip installed at C:\Program Files\7-Zip\7z.exe

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== PriviMe Mainnet Package Script ==="

# Step 0: Build UI from source with PRODUCTION CID
echo "Building UI from src/ (mainnet CID) ..."
npm run build:mainnet

# Step 1: Refresh releases/mainnet/ staging area
echo "Refreshing releases/mainnet/app/ ..."
mkdir -p releases/mainnet/app
cp ui/index.html releases/mainnet/app/
cp ui/appicon.svg releases/mainnet/app/
cp shaders/app.wasm releases/mainnet/app/

echo "Mainnet staging ready:"
ls -la releases/mainnet/app/

# Step 2: Package with 7-Zip
echo ""
echo "Creating releases/PriviMe-mainnet.dapp ..."
rm -f releases/PriviMe-mainnet.dapp
cd releases/mainnet
"C:/Program Files/7-Zip/7z.exe" a -tzip ../PriviMe-mainnet.dapp ./*
cd ../..

echo ""
echo "=== Done ==="
ls -la releases/PriviMe-mainnet.dapp
echo ""
echo "Install: Drag PriviMe-mainnet.dapp into Beam Wallet (Mainnet) DApps tab"
