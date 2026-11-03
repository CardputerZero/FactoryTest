# CardputerZero Factory Test Application

This repository holds the factory test application of CardputerZero.

## Get started:

+ Download the debian release

```
wget https://github.com/CardputerZero/FactoryTest/releases/download/1.0.1/FactoryTest_1.0.1_m5stack1_arm64.deb
```

+ Install the package

```
sudo apt install ./FactoryTest_1.0.1_m5stack1_arm64.deb
```

The packaged systemd service enables exclusive evdev input for the built-in `tca8418c` keyboard
with `FACTORY_TEST_EXCLUSIVE_INPUT=1`. Direct launches use shared input by default. Set the same
environment variable to `1`, `true`, `yes`, or `on` to request exclusive input outside systemd.
`FACTORY_TEST_EXCLUSIVE_INPUT_DEVICE` can override the target evdev device name. Other keyboards,
including USB keyboards, remain shared. Startup fails if the named device cannot be grabbed.


## Manually build

Choose one of the following build systems for a CardputerZero target build.

### SCons

The default SCons build uses `app` mode and starts the main application through
the Linux framebuffer:

```
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8
```

To build the main application in `product` mode with the DRM/KMS backend:

```
FACTORY_TEST_PRODUCT_MODE=1 CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8
```

### CMake

The CMake CardputerZero preset uses the CM0/aarch64 cross toolchain and keeps
the main application in framebuffer `app` mode by default:

```
cmake --workflow --preset cp0-cross-package
```

For a build without packaging:

```
cmake --preset cp0-cross
cmake --build --preset cp0-cross-rel
```

Pass `-DFACTORY_TEST_PRODUCT_MODE=ON` when configuring CMake if the main
application should use DRM/KMS instead. The standalone `factory_precheck`
target always uses DRM independently of the main application display mode.

The cross-built binaries must report `ELF 64-bit ... ARM aarch64`. The generated
package is `dist/FactoryTest_<version>_m5stack1_arm64.deb`. Do not run
`package` from a desktop build directory; desktop binaries are Mach-O and are
not valid for the target device.

In `FACTORY_TEST_PRODUCT_MODE`, the HDMI precheck service is enabled at boot
(`WantedBy=multi-user.target`) and is also started by the DRM udev hotplug rule
when an HDMI connector reports `status=connected`. The unit is gated by
`ExecCondition=/usr/libexec/cardputerzero-hdmi-check`
(the package installs this helper under `/usr/libexec`); the check only passes
when at least one DRM HDMI connector reports `connected`, so no HDMI means the
service is skipped rather than started. It connects to the PulseAudio user
socket at `/run/user/1000/pulse/native`, matching the main factory-test service.
If audio is unavailable, check that the user PulseAudio/PipeWire session is
running and inspect the service log for the reported `PULSE_SERVER` and
`XDG_RUNTIME_DIR`. Default audio routing is a system-level PipeWire/WirePlumber
concern and is not changed by the application package. Configure the board I2S
sink/source as the higher-priority default in the system WirePlumber profile so
HDMI hotplug does not replace it.

Both `factory_test` and `factory_precheck` are installed in `/usr/bin`. Do not
launch `factory_precheck` manually while `factory_precheck.service` is active;
the executable uses a shared instance lock and will refuse a second DRM owner.
Stop the service first when a manual run is needed.

The precheck service deliberately does **not** bind a TTY/VT (`tty1`): the
precheck UI is a DRM application, and a `TTYPath`/`VTDisallocate` combination
torn down on stop was found to wedge the kernel console and trip the board
watchdog, resetting the system. Logs go to the journal instead.


## Changelog

### 1.0.1 - 2026-11-03

- Highlight repeated presses on the Input Test page: the first press keeps the layer's original color, the second press switches to the warning color, and the third (or later) press to the error color, so ghost presses caused by mechanical tolerances are visible at a glance.
- Use blue instead of amber for the second FN-layer press, so the repeat highlight never matches the layer's own first-press color.
- Bump the CMake project version, the SCons version header, the vcpkg manifest, and the Debian package version and download instructions to 1.0.1.
- Build against the libcamera 0.7.2 headers (`std::span` ABI instead of `libcamera::Span`) with C++20, verify the sysroot headers are at least 0.7.2 when configuring, and require `libcamera0.7 (>= 0.7.2+rpt20261009)` at install time so `Camera::generateConfiguration` resolves on the device.

### 1.0.0 - 2026-09-18

- Mark the CardputerZero factory test application as the first formal release.

### 0.3.4 - 2026-09-18

- Enable and package systemd/DRM hotplug services only in product mode; app mode remains launch-on-demand.

### 0.3.3 - 2026-09-07

- Add controlled EXT 5V power-up, settling delay, failure reporting, and cleanup to the CC1101 and LoRa-1262 tests.
- Add Grove 5V power control for Grove I2C testing and disable both EXT and Grove 5V outputs during fixture cleanup.
- Use `/dev/dri/card1` as the default product DRM device and align the CMake preset, systemd dependency, and LCD smoke test with the device path.
- Simplify product DRM initialization to use the configured device directly, removing the previous retry and multi-device fallback behavior.

### 0.3.2 - 2026-09-03

- Add the "Fire in the Pi" stress-test page with CPU, memory, and storage workloads, thermal/battery telemetry, live charts, rotating cycles, and JSONL metrics.
- Expand CAP fixture validation from six to eight checks with Grove I2C/UART routing, protocol handshakes, fixed settling delays, USB hub recovery, and deterministic cleanup.
- Identify CAP USB devices by physical sysfs port topology instead of volatile device numbers, and add desktop coverage for port matching.
- Improve keyboard-layer accounting and delayed automatic progression, with timer cleanup when leaving the page.
- Make DRM startup more resilient with device-open retries and an immediate first frame; wait for the configured DRM alias in the systemd service.
- Enhance HDMI precheck with an eight-item CAP summary, physical USB-port checks, scrollable results, and updated controls/localization.

### 0.3.1 - 2026-08-13

- Add the standalone HDMI factory precheck executable and systemd service.
- Use a responsive LVGL Flex layout with Chinese-first status output.
- Add compile-time `app` and `product` display modes for framebuffer and DRM/KMS output.
- Add DRM-based HDMI hotplug precheck packaging, exclusive instance locking, and LCD/HDMI display smoke tests.
- Add SCons and CMake cross-build support with separate main-application and precheck display backends.
- Require confirmation before PY32 firmware upgrades, reboot automatically after success, and localize the upgrade warning.
- Improve USB factory-result upload, input routing, IR handling, UI audio setup, and runtime asset resolution.

### 0.3.0 - 2026-08-12

- Harden the packaged systemd startup order and runtime environment, and restart the factory test service automatically.
- Add optional exclusive evdev input for the built-in keyboard, with configurable device selection, clean grab release, and startup failure when exclusive input cannot be acquired.
- Shorten recovery and error dialog titles, and replace raw BlueZ failures with localized guidance to enable Bluetooth while retaining diagnostic details in logs.

- Add a versioned JSON configuration store for persistent theme, language, key-click, iperf, UART, logging, and factory-station settings.
- Add thread-safe file logging with UTC timestamps, bounded segment rotation, directory retention, graceful fallback, and desktop coverage.
- Upload completed test results from the result page over the `M5FACTORY` USB serial protocol, with device discovery, handshake and acknowledgement handling, progress feedback, and Chinese localization.
- Harden CAP fixture testing with software-reset retries, corrected `303a:4005` USB detection, GPIO ownership checks, deterministic USB/GPIO switching, and cleaner resource release.
- Improve UART lifecycle and USB serial discovery, persist manual UART and link settings, and refine audio channel handling and configurable key-click playback.
- Simplify target builds around the DRM backend and required hardware libraries, fail early on missing BSP dependencies, and add configuration, logger, UART, and session tests.
- Add compile-time `app` and `product` display modes. The default `app` build starts through the Linux framebuffer, while `product` keeps the DRM startup path.

### Display modes

The default build is `app` mode and uses the Linux framebuffer device (normally `/dev/fb0`). Select `product` mode at compile time with `FACTORY_TEST_PRODUCT_MODE=1`; this changes the compiled display backend to DRM/KMS and does not require both display drivers to be built:

```bash
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8
FACTORY_TEST_PRODUCT_MODE=1 CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8
```

For CMake builds, use `-DFACTORY_TEST_PRODUCT_MODE=ON` for `product` mode. Without this option, CMake builds `app` mode.

The framebuffer device can be overridden in `app` mode with the same environment variables used by APPLaunch; `LV_LINUX_FBDEV_DEVICE` takes precedence over `APPLAUNCH_LINUX_FBDEV_DEVICE`:

```bash
LV_LINUX_FBDEV_DEVICE=/dev/fb1 ./FactoryTest
```

The compile-time macro defaults to `0`, so a normal build uses `app` mode.

### 0.2.10 - 2026-07-30

- Replace the flat full-test report with atomic, per-run session and result files containing device and build metadata, UTC timestamps, attempt history, detailed subtest results, and typed test evidence.
- Recover the latest compatible unfinished full-test session after an interruption, with start-new and resume actions that continue from the first incomplete test.
- Capture evidence from input, display, audio, camera, fixture, connectivity, device, power, IMU, and performance tests, and flag completed tests that have no evidence on the result page.
- Standardize full-test outcomes on `PASS` and `FAIL`, remove the skip action, and add desktop CTest coverage for session persistence, recovery, result generation, and plan validation.
- Upgrade the packaged IOE1 firmware to target version `0x5A01`, rename the package and SHA-256 manifest, and validate both the archive and extracted firmware before programming.
- Read and display the combined IOE1 major/minor firmware version, show the installed and available versions on the upgrade page, and improve upgrade progress parsing and diagnostic logging.
- Embed the source commit in build metadata, bump the application and Debian download instructions to 0.2.10, and complete the Chinese localization for session recovery and the revised IOE1 workflow.

### 0.2.9 - 2026-07-28

- Add a PY32/IOE1 firmware upgrade page with packaged 0xF8 firmware, archive and extracted-file checksum verification, a preflight dry run, live progress reporting, and post-upgrade version validation.
- Package the PY32 upgrade assets and required `coreutils`, `python3`, and `tar` runtime dependencies, with complete Chinese localization for the upgrade workflow.
- Rework the libcamera preview around 640x480 YUV420 capture, CMA DMA-heap allocation with a libcamera allocator fallback, deterministic request cleanup, and reusable RGB565 conversion buffers.
- Route external-bus selection and shared binary sysfs controls through the GPIO service, including verified readback for EXT.IO and CAP power switching.
- Keep EXT.IO input GPIO requests alive across polls, retry only after read failures, and refresh only rows whose input state changed.
- Extend the process service with child-stdin input and bounded line-handler buffers, and release retained headphone and page-owned GPIO resources when their screens close or change modes.
- Add confirmed safe poweroff and reboot actions to the start menu, offer reboot after a successful PY32 upgrade, and package the required `systemctl` polkit authorization.

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
