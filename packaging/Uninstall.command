#!/bin/zsh
set -euo pipefail
if [[ $EUID != 0 ]]; then exec sudo /bin/zsh "$0" "$@"; fi
print 'Removing Audio 2 DJ drivers and control panel. Audio playback will be interrupted.'
launchctl bootout system/local.audio2dj.bridge 2>/dev/null || true
for attempt in {1..75}; do
 if ! launchctl print system/local.audio2dj.bridge >/dev/null 2>&1; then break; fi
 sleep 0.2
done
if launchctl print system/local.audio2dj.bridge >/dev/null 2>&1; then print 'USB service is still stopping. Please retry.'; exit 1; fi
pkill -x Audio2DJControl 2>/dev/null || true
rm -rf /Library/Audio/Plug-Ins/HAL/Audio2DJA.driver /Library/Audio/Plug-Ins/HAL/Audio2DJB.driver /Library/Audio/Plug-Ins/HAL/Audio2DJ.driver '/Applications/Audio2DJ Control.app'
rm -f /Library/LaunchDaemons/local.audio2dj.bridge.plist /Library/Audio2DJ/audio2-bridge
pkgutil --forget local.audio2dj.package >/dev/null 2>&1 || true
killall coreaudiod 2>/dev/null || true
print 'Uninstalled. Saved volume settings and backups were preserved.'
read '?Press Enter to close.'
