# Audio 2 DJ for Apple Silicon

Experimental, unofficial macOS driver and control panel for **Native Instruments Audio 2 DJ Mark 1 (MK1)**, USB **17cc:041c**.

Two independent stereo Core Audio devices, **Audio 2 DJ — A** and **Audio 2 DJ — B**, share a userspace USB engine. No kernel extension or reduced macOS security setting is required.

## Status and requirements

Tested on one Apple Silicon Mac with **macOS 27.0.1**. FL Studio and Chrome playback have been confirmed by the device owner. Broader compatibility and long-duration testing remain open. Current binaries target **arm64 and macOS 27 or later**. Intel Macs and other interface models are not supported.

**Experimental local build, ad-hoc signed, without Apple notarization or Developer ID signing.** Start with low volume and validate your setup before relying on it for live performance.

This release targets **Mark 1 only**, not Audio 2 MK2 or other Native Instruments interfaces.

## Features

- Separate stereo A/B devices for simultaneous use by different applications.
- Independent 44.1/48 kHz application streams; shared 48 kHz USB output.
- Independent output gain and mute; post-volume L/R digital peak meters for each output.
- 32/64/128 ms software buffer reserve. These settings are **not measured end-to-end latency**.
- 5 ms gain smoothing, settings checked every 20 ms off USB callbacks, and haptic slider detents on supported trackpads.

Meters show digital signal sent to the interface, not measured analog output.

## Install

1. Save your projects and quit audio applications, including FL Studio.
2. Download the DMG from **Releases**, open it, and run the `.pkg` installer.
3. Open **Audio2DJ Control** from Applications.
4. Reopen audio applications and select **Audio 2 DJ — A** or **Audio 2 DJ — B**.

Administrator authorization is required. Installation restarts Core Audio. Existing volume/buffer settings are preserved. The USB service runs under the desktop account selected during installation; switching macOS user accounts is not yet supported.

## Build

Requires Xcode with the macOS SDK and command line tools; Python 3 is needed for packaging.

```sh
./build.sh
./test.sh
./package.sh
```

The payload is created under `build/root/`, and the DMG under `dist/`. Builds are ad-hoc signed for local use. Package scripts wait for the old service to stop and configure the installed service for the logged-in desktop user.

## Uninstall

Quit audio applications and run `Uninstall.command` from the DMG. It removes this project's drivers, USB service and control panel, then restarts Core Audio. User settings and installation backups are preserved.

## Troubleshooting

- **FL Studio cannot enable the device after an update:** save the project, quit FL Studio completely and reopen it. It may retain the old Core Audio connection.
- **Clicks or increasing underruns:** try 64/128 ms. Check both the application's buffer and the control-panel buffer; they are separate.
- **Meters move but no sound:** check the A/B selection, connection, physical output knob and software mute/gain.
- **Disconnected:** check USB and the `local.audio2dj.bridge` service.
- Counters may include playback stop/start transitions. Zero counters alone do not prove glitch-free analog output.

Issue reports should include Mac model, macOS version, USB ID, A/B output, application and sample rate, both buffer settings, and reproduction steps. Do not upload private projects or unrelated logs.

## Architecture

Two stereo HAL plug-ins own independent rate, clock and volume state. Versioned Mach messages carry PCM to one USB process with independent ring buffers and resamplers. The USB engine follows the interface's feedback packet cadence. Settings and telemetry file operations run outside USB callbacks.

## Credits and license

- Core Audio code derives from [Apple's NullAudio sample](https://github.com/russellporter/NullAudioDriver). Its MIT notice is retained in `licenses/Apple-NullAudio-MIT.txt`.
- USB protocol reference: Linux [snd-usb-caiaq, v6.12](https://github.com/torvalds/linux/tree/v6.12/sound/usb/caiaq), including work by Daniel Mack, Karsten Wiese and other Linux contributors.
- Bridge and project modifications: **GPL-2.0-or-later**, see `LICENSE` and `NOTICE.md`.
- The supplied Traktor icon is excluded from the source-code license. Native Instruments/Traktor names and artwork belong to their respective owners. This project is not affiliated with or endorsed by Native Instruments.
