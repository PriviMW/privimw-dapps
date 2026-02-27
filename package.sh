#!/bin/bash

# BeamBet DApp Build Script
# Builds the React UI and packages everything as a .dapp file
#
# DApp Package Structure (matches official Beam DApps):
#   beambet.dapp (zip file)
#   ├── manifest.json     (at root level)
#   └── app/              (all build output in "app/" subdirectory)
#       ├── index.html
#       ├── icon.svg
#       ├── app.wasm      (app shader)
#       ├── static/
#       │   ├── css/
#       │   └── js/
#       └── ...

set -e

echo "=== BeamBet DApp Build Script ==="
echo ""

# Configuration
APP_NAME="beambet"
VERSION="1.0.0"
DIST_DIR="dist"
PACKAGE_DIR="package"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Step 1: Build React App
echo -e "${YELLOW}Step 1: Building React app...${NC}"
cd ui
if [ ! -d "node_modules" ]; then
    echo "Installing dependencies..."
    yarn install
fi
yarn build
cd ..
echo -e "${GREEN}React build complete!${NC}"
echo ""

# Step 2: Create package directory structure
echo -e "${YELLOW}Step 2: Creating package directory...${NC}"
rm -rf $PACKAGE_DIR
mkdir -p $PACKAGE_DIR/app
echo -e "${GREEN}Package directory created!${NC}"
echo ""

# Step 3: Copy UI files to app/ subfolder
echo -e "${YELLOW}Step 3: Copying UI files to app/ folder...${NC}"
cp -r ui/build/* $PACKAGE_DIR/app/
echo -e "${GREEN}UI files copied!${NC}"
echo ""

# Step 4: Copy app shader to app/ folder (alongside index.html)
echo -e "${YELLOW}Step 4: Copying app shader...${NC}"
if [ -f "shaders/beambet/app.wasm" ]; then
    cp shaders/beambet/app.wasm $PACKAGE_DIR/app/
    echo -e "${GREEN}app.wasm copied to app/ folder${NC}"
else
    echo -e "${RED}WARNING: shaders/beambet/app.wasm not found!${NC}"
    echo -e "${RED}The DApp will not work without the app shader!${NC}"
fi
echo ""
echo "NOTE: contract.wasm is NOT included — it must be deployed separately on-chain."
echo ""

# Step 5: Copy manifest to package root
echo -e "${YELLOW}Step 5: Copying manifest...${NC}"
cp manifest.json $PACKAGE_DIR/
echo -e "${GREEN}Manifest copied!${NC}"
echo ""

# Step 6: Create .dapp package (zip archive)
echo -e "${YELLOW}Step 6: Creating .dapp package...${NC}"
mkdir -p $DIST_DIR
cd $PACKAGE_DIR
zip -r ../$DIST_DIR/${APP_NAME}-${VERSION}.dapp .
cd ..
echo -e "${GREEN}.dapp package created!${NC}"
echo ""

# Done!
echo "=== Build Complete ==="
echo ""
echo "Output: $DIST_DIR/${APP_NAME}-${VERSION}.dapp"
echo ""
echo "Package structure:"
echo "  manifest.json (at root)"
echo "  app/"
echo "    index.html"
echo "    icon.svg"
echo "    app.wasm (app shader)"
echo "    static/css/, static/js/"
echo ""
echo -e "${GREEN}Ready to test in Beam Wallet!${NC}"
echo "Load the .dapp file in your Beam Wallet DAPPNET instance."
echo ""
echo "IMPORTANT: The contract shader must be deployed separately!"
