#!/bin/zsh
set -euo pipefail
cd -- "${0:A:h}"
version=0.3.3
./build.sh
mkdir -p dist build/image
pkgbuild --analyze --root build/root build/components.plist
python3 - <<'PY'
import plistlib
p='build/components.plist'
with open(p,'rb') as f: items=plistlib.load(f)
for item in items:item['BundleIsRelocatable']=False;item['BundleIsVersionChecked']=False
with open(p,'wb') as f:plistlib.dump(items,f)
PY
pkgbuild --root build/root --component-plist build/components.plist --scripts packaging/scripts --identifier local.audio2dj.package --version "$version" --install-location / --ownership recommended "build/image/Audio2DJ-$version.pkg"
cp packaging/Uninstall.command build/image/
cp README.md build/image/Read-Me.md
zip -qr build/image/Source.zip src resources packaging licenses build.sh test.sh package.sh README.md LICENSE NOTICE.md .gitignore
hdiutil create -volname 'Audio 2 DJ' -srcfolder build/image -format UDZO -ov "dist/Audio2DJ-$version-AppleSilicon.dmg"
hdiutil verify "dist/Audio2DJ-$version-AppleSilicon.dmg"
