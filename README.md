# CardputerZero Factory Test Application

This repository holds the factory test application of CardputerZero.

## Get started:

+ Download the debian release

```
wget https://github.com/CardputerZero/FactoryTest/releases/download/0.2.4/FactoryTest_0.2.4_m5stack1_arm64.deb
```

+ Install the package

```
sudo apt install ./FactoryTest_0.2.4_m5stack1_arm64.deb
```


## Manually build

TODO

## Changelog

### 0.2.4 - 2026-07-10

- Use the miniaudio PulseAudio backend so Audio Test and key-click playback share the system-managed audio path.
- Use the system default playback and capture devices instead of opening the ES8389 I2S card directly.
- Play Audio Test recording results at 80% volume with stereo output.
- Reduce miniaudio logs to selected devices and initialization details.

### 0.2.3 - 2026-07-09

- Refactor Audio Test recording and playback to use miniaudio.
- Remove ALSA/PipeWire build and package dependencies.
- Simplify Audio Test status UI and Enter-only recording flow.
- Pin miniaudio Audio Test capture/playback to the selected I2S device.

### 0.2.2 - 2026-07-08

- Update Input test FN layer layout for media, brightness, volume, navigation, and PrintScreen keys.
- Add keypad-style extra font glyphs for play/pause, rewind, fast-forward, and question icons.
- Add key input mappings for media keys and PrintScreen.

### 0.2.1 - 2026-07-07

- Restore UART page-owned long-press handling for P/T shortcuts.
- Release EXT.IO GPIO output requests before switching direction.
- Use /tmp for memory stress temporary files.
- Flatten IR send/receive into separate top-level test pages.

### 0.2.0 - 2026-07-06

- Add Chinese localization with dynamic translation and CJK font loading.
- Add Language settings entry and Chinese translations for pages, dialogs, status text, and navigation.
- Split test confirmation into a dedicated dialog and fix modal ESC/cancel key handling.
- Improve config dialog layout for link, UART, and EXT.IO tests.
- Package i18n assets and bump release version to 0.2.0.

### 0.1.1 - 2026-07-03

- Fix input test final tile navigation so the Hold 8 check action remains available after all key layers pass.
- Add focused test-result dialog button navigation and update hold-confirm flows for long-press actions.
- Expand LCD and connectivity test state handling, including structured result serialization and refreshed per-page views.
- Update Debian packaging dependencies for enabled runtime services, including LIRC, ALSA t64 compatibility, and pkexec ping fallback.
- Fix cross-build definitions and serialization linkage used by the factory test model.

### 0.1.0 - 2026-06-29

- 2026-06-26 17:05 +0800 - 2215c33 - feat: add categorized drawer navigation and performance test flow
- 2026-06-25 14:13 +0800 - 1ac24e1 - refactor: decouple the implementations in connectivity service, implement the UART service with libserialport
- 2026-06-24 09:22 +0800 - 0f89754 - feat: expand factory test hardware coverage and navigation actions
