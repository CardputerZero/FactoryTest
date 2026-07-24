# CardputerZero Factory Test Application

This repository holds the factory test application of CardputerZero.

## Get started:

+ Download the debian release

```
wget https://github.com/CardputerZero/FactoryTest/releases/download/0.2.8/FactoryTest_0.2.8_m5stack1_arm64.deb
```

+ Install the package

```
sudo apt install ./FactoryTest_0.2.8_m5stack1_arm64.deb
```


## Manually build

TODO


## Changelog

### 0.2.8 - 2026-07-24

- Add an IR fixture test to Full Test with randomized NEC32 transmissions and ordered fixture-response validation.
- Detect CardputerZero and CardputerZero Lite from enabled camera and IMU device-tree nodes, and skip unsupported Camera and IMU tests on Lite models.
- Improve hardware revision detection with the runtime ADC scale, cached IOE1 register reads, forced access for driver-owned I2C addresses, and an IOE1 version field.
- Make the default iperf server configurable at build time and pass it to desktop and systemd launches through `FACTORY_TEST_IPERF_HOST`.
- Release EXT.IO GPIO output requests when leaving the page so later fixture and manual tests can reacquire the lines.
- Complete Chinese translations for the IR fixture workflow and reduce routine font and input-device logs to debug level.

### 0.2.7 - 2026-07-22

- Add manual CAP LoRa-1262 diagnostics for SX1262 SPI communication and GPS IC/NMEA reception.
- Add read-only CAP-CC1101 diagnostics for CC1101 and ST25R3916, including GPIO22 software chip select support.
- Add the CAP diagnostics to the communications menu and Full Test flow, and require explicit Enter confirmation before fixture testing.
- Replace raw ICMP/liboping connectivity checks with HTTPS DNS-over-HTTPS fallbacks through Google DNS, AliDNS, and DNSPod.
- Improve key-click audio initialization and restore it after Audio Test playback.
- Stabilize hardware revision detection with median ADC sampling and release fixture GPIO requests after input setup.
- Complete Chinese translations for CAP and connectivity screens while removing obsolete localization entries.

### 0.2.6 - 2026-07-17

- Add the hidden CAP/HAT fixture test sequence for Full Test.
- Add fixed-delay I2C, power, UART, SPI, USB, and GPIO fixture validation.
- Add detailed fixture protocol logs and deterministic hardware cleanup states.
- Report individual CAP fixture checks in Full Test results and verify GPIO output readback.
- Keep the existing communication pages available for manual diagnostics.
- Make Debian post-install setup independent of the image-build user.
- Keep product-group setup failures non-blocking with explicit warnings.

### 0.2.5 - 2026-07-17

- Update I/O port controls and hardware status detection.
- Replace selected hardware reads with file-based interfaces.
- Refresh factory test UI and interaction behavior.
- Complete Debian packaging and installation configuration.

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
- Update Debian packaging dependencies for enabled runtime services, including LIRC and ALSA t64 compatibility.
- Fix cross-build definitions and serialization linkage used by the factory test model.

### 0.1.0 - 2026-06-29

- 2026-06-26 17:05 +0800 - 2215c33 - feat: add categorized drawer navigation and performance test flow
- 2026-06-25 14:13 +0800 - 1ac24e1 - refactor: decouple the implementations in connectivity service, implement the UART service with libserialport
- 2026-06-24 09:22 +0800 - 0f89754 - feat: expand factory test hardware coverage and navigation actions
