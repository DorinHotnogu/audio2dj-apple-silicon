#!/bin/zsh
set -euo pipefail
cd -- "${0:A:h}"
mkdir -p build/root/Library/Audio/Plug-Ins/HAL build/root/Library/Audio2DJ build/root/Library/LaunchDaemons build/root/Applications
for pair in A B; do
 bundle="build/root/Library/Audio/Plug-Ins/HAL/Audio2DJ${pair}.driver/Contents"
 mkdir -p "$bundle/MacOS" "$bundle/Resources/en.lproj"
 xcrun clang -std=c11 -O2 -Wno-deprecated-declarations -bundle "src/Driver${pair}.c" -framework CoreAudio -framework CoreFoundation -o "$bundle/MacOS/Audio2DJ"
 cp "resources/Driver${pair}-Info.plist" "$bundle/Info.plist"
 print -r -- "DeviceName=\"Audio 2 DJ — $pair\"; OutputStreamName=\"Stereo $pair\"; ManufacturerName=\"Audio2DJ Experimental\"; LeftElementName=\"Left\"; RightElementName=\"Right\"; MasterElementName=\"Master\";" > "$bundle/Resources/en.lproj/Localizable.strings"
 codesign --force --sign - "${bundle:h}"
done
xcrun clang++ -std=c++17 -O2 -Wno-deprecated-declarations src/bridge.cpp -framework IOKit -framework CoreFoundation -lbsm -o build/root/Library/Audio2DJ/audio2-bridge
codesign --force --sign - build/root/Library/Audio2DJ/audio2-bridge
app='build/root/Applications/Audio2DJ Control.app/Contents'
mkdir -p "$app/MacOS" "$app/Resources"
xcrun swiftc -parse-as-library src/Control.swift -framework SwiftUI -o "$app/MacOS/Audio2DJControl"
cp resources/Control-Info.plist "$app/Info.plist"
cp resources/AppIcon.icns "$app/Resources/"
codesign --force --sign - "${app:h}"
cp packaging/local.audio2dj.bridge.plist build/root/Library/LaunchDaemons/
print 'Build complete: build/root'
