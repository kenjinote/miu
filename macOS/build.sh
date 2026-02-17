#!/bin/bash

# アプリ名設定
APP_NAME="miu"
SOURCE="miu.mm"
ICON_SOURCE="icon.png"

# ディレクトリ構成の作成
echo "Creating app bundle structure..."
if [ -d "$APP_NAME.app" ]; then
    rm -rf "$APP_NAME.app"
fi
mkdir -p "$APP_NAME.app/Contents/MacOS"
mkdir -p "$APP_NAME.app/Contents/Resources"

# 1. コンパイル
echo "Compiling..."
clang++ -o "$APP_NAME.app/Contents/MacOS/$APP_NAME" "$SOURCE" -std=c++17 -framework Cocoa -framework CoreText -framework CoreFoundation -O2

if [ $? -ne 0 ]; then
    echo "Compilation failed."
    exit 1
fi

# 2. アイコンの生成 (icon.pngがある場合のみ)
if [ -f "$ICON_SOURCE" ]; then
    echo "Generating .icns from $ICON_SOURCE..."
    mkdir "$APP_NAME.iconset"
    
    # 各サイズの画像を生成
    sips -z 16 16     "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_16x16.png" > /dev/null
    sips -z 32 32     "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_16x16@2x.png" > /dev/null
    sips -z 32 32     "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_32x32.png" > /dev/null
    sips -z 64 64     "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_32x32@2x.png" > /dev/null
    sips -z 128 128   "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_128x128.png" > /dev/null
    sips -z 256 256   "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_128x128@2x.png" > /dev/null
    sips -z 256 256   "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_256x256.png" > /dev/null
    sips -z 512 512   "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_256x256@2x.png" > /dev/null
    sips -z 512 512   "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_512x512.png" > /dev/null
    sips -z 1024 1024 "$ICON_SOURCE" --out "$APP_NAME.iconset/icon_512x512@2x.png" > /dev/null

    # .icnsに変換して配置
    iconutil -c icns "$APP_NAME.iconset"
    mv "$APP_NAME.icns" "$APP_NAME.app/Contents/Resources/$APP_NAME.icns"
    rm -rf "$APP_NAME.iconset"
else
    echo "Warning: icon.png not found. App will satisfy with default icon."
fi

# 3. Info.plist の作成
echo "Creating Info.plist..."
cat > "$APP_NAME.app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>com.kenji.$APP_NAME</string>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundleIconFile</key>
    <string>$APP_NAME</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.13</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

echo "Done! You can run ./$APP_NAME.app"
