@echo off
REM BeamBet DApp Build Script for Windows
REM This script builds the React UI and packages everything as a .dapp file
REM
REM DApp Package Structure (based on Beam DApp template):
REM   beambet.dapp (zip file)
REM   ├── manifest.json     (at root level)
REM   └── app/              (UI folder named "app" not "html")
REM       ├── index.html
REM       ├── icon.svg
REM       ├── app.wasm      (app shader - goes alongside HTML)
REM       ├── static/
REM       │   ├── css/
REM       │   └── js/
REM       └── ...

echo === BeamBet DApp Build Script ===
echo.

REM Configuration
set APP_NAME=beambet
set VERSION=1.0.0
set DIST_DIR=dist
set PACKAGE_DIR=package

REM Step 1: Build React App
echo Step 1: Building React app...
cd ui
if not exist "node_modules" (
    echo Installing dependencies...
    call yarn install
)
call yarn build
if errorlevel 1 (
    echo ERROR: React build failed!
    cd ..
    pause
    exit /b 1
)
cd ..
echo React build complete!
echo.

REM Step 2: Create package directory structure
echo Step 2: Creating package directory...
if exist %PACKAGE_DIR% rmdir /s /q %PACKAGE_DIR%
mkdir %PACKAGE_DIR%
mkdir %PACKAGE_DIR%\app
mkdir %PACKAGE_DIR%\app\static
echo Package directory created!
echo.

REM Step 3: Copy UI files to app folder (NOT html folder!)
echo Step 3: Copying UI files to app folder...
xcopy /s /e /y ui\build\* %PACKAGE_DIR%\app\
echo UI files copied!
echo.

REM Step 4: Copy app shader to app folder (same location as index.html)
echo Step 4: Copying app shader...
if exist "shaders\beambet\app.wasm" (
    copy shaders\beambet\app.wasm %PACKAGE_DIR%\app\
    echo app.wasm copied to app folder
) else (
    echo WARNING: shaders\beambet\app.wasm not found!
    echo The DApp will not work without the app shader!
)
echo.
echo NOTE: contract.wasm is NOT included in the DApp package.
echo It must be deployed separately to the blockchain.
echo.

REM Step 5: Copy manifest to package root
echo Step 5: Copying manifest...
copy manifest.json %PACKAGE_DIR%\
echo Manifest copied!
echo.

REM Step 6: List package contents for verification
echo Step 6: Package contents:
dir /s /b %PACKAGE_DIR%
echo.

echo Step 7: Creating .dapp package (7-Zip, forward-slash paths)...
if not exist %DIST_DIR% mkdir %DIST_DIR%
if exist %DIST_DIR%\%APP_NAME%-%VERSION%.dapp del %DIST_DIR%\%APP_NAME%-%VERSION%.dapp

REM Use 7-Zip to create ZIP with proper forward-slash paths (ZIP spec standard)
REM Compress-Archive on Windows uses backslash paths which can cause wallet issues
"C:\Program Files\7-Zip\7z.exe" a -tzip %DIST_DIR%\%APP_NAME%-%VERSION%.dapp .\%PACKAGE_DIR%\*
if errorlevel 1 (
    echo ERROR: 7-Zip failed! Is 7-Zip installed at C:\Program Files\7-Zip\?
    echo Fallback: trying PowerShell Compress-Archive...
    powershell -Command "Compress-Archive -Path %PACKAGE_DIR%\* -DestinationPath %DIST_DIR%\%APP_NAME%-%VERSION%.zip -Force"
    move /y %DIST_DIR%\%APP_NAME%-%VERSION%.zip %DIST_DIR%\%APP_NAME%-%VERSION%.dapp
)

echo .dapp package created!
echo.

echo === Build Complete ===
echo.
echo Output: %DIST_DIR%\%APP_NAME%-%VERSION%.dapp
echo.
echo Package structure:
echo   manifest.json (at root)
echo   app/
echo     index.html
echo     icon.svg
echo     app.wasm (app shader)
echo     static/css/, static/js/
echo.
echo To install in Beam Wallet:
echo 1. Open Beam Wallet (DAPPNET)
echo 2. Go to DApps tab
echo 3. Click "Add DApp" or drag and drop the .dapp file
echo.
echo IMPORTANT: The contract shader must be deployed separately!
echo Use: beam-wallet-dappnet shader --shader_app_file shaders\beambet\app.wasm
echo.
pause