# Mistel MD770 RGB BT

A wired QMK port for the US-layout MD770 RGB BT. Bluetooth is not implemented.

* Keyboard Maintainer: [Mark Ellis](https://github.com/markwellis)
* Hardware Supported: MD770BT-M, inferred HT32F52352 controllers, US 85-key layout
* Hardware Availability: [Mistel](https://www.mistelkeyboard.com/)

**Development build: never flashed or tested on hardware. Establish recovery before flashing either half.** The inspected right board is `MD770BR-RIGHT V06 1223BJ`, with an `HT50F52352` controller. Its measured SEL2 BOOT/GND pads enter the Holtek ROM loader, and two matching stock-IAP backups have been saved. Complete restoration and left-half recovery remain untested. Build success does not establish correct electrical operation.

Build each half from the repository root:

    make mistel/md770rgb_bt/right:default
    make mistel/md770rgb_bt/left:default

Optional persistent remapping on the right uses QMK's VIA support:

    make mistel/md770rgb_bt/right:default VIA_ENABLE=yes TARGET=mistel_md770rgb_bt_right_via

On NixOS, a temporary build environment is available with:

    nix-shell -p qmk gcc-arm-embedded gnumake --run 'make mistel/md770rgb_bt/right:default'

The stock application bootloader is retained. Vectors start at `0x3200`; each half has its own compatibility record at `0x3bf8`. SRAM ends at `0x20003fe0`, reserving the stock boot flag at `0x20003ffc`. Application flash ends at `0x1fe00`, before the chip’s 512-byte option page. The default keymap has Fn+Escape mapped to `QK_BOOT`. That mechanism requires working firmware and is not independent recovery. No automatic flashing target is provided.

The right image is the USB master. Both images use the recovered stock UART framing, so keeping a stock left half is a development option; mixed firmware has not been tested. The left image also exposes its local keys over USB when used independently. Stock DIP-switch remapping, macros and Bluetooth are not reproduced. The default keymap uses Fn for lighting controls: Space toggles RGB, Up/Down change brightness, and Left/Right change effects. Lighting defaults off.

Matrix sensing and RGB multiplexing share PA pins. `scan.c` owns their timing through BFTM0, with BFTM1 reserved for QMK's microsecond delays. `rgb.c` reproduces the stock serial/latch sequence and uses QMK's RGB Matrix renderer. The right releases remote keys after 100 ms without a valid split snapshot.

## Persistent settings

The right uses QMK's existing SPI-flash wear-levelling backend. Stock SPI1 pins are PC2 clock, PC3 MOSI, PB6 MISO, and PA9 chip select. Initialisation wakes the flash from deep power-down. The QMK backing store occupies external flash `0x0000`–`0x0fff`; this replaces the stock global-settings sector. Back up any stock settings you want to retain before first boot. Stock profile sectors begin at `0x1000`, `0x2000`, `0x3000` and `0x4000`. The configured address limit covers those five known sectors; it is not a measured chip capacity. The left uses transient EEPROM.

## Lighting notifications

The default build uses QMK's standard 32-byte Raw HID interface. Send this payload, padded with zeros to 32 bytes:

| Bytes | Meaning |
| --- | --- |
| 0–3 | ASCII `MD77` |
| 4 | `1` starts an alert; `0` cancels it |
| 5–7 | Red, green, blue, each clamped to the configured brightness ceiling |
| 8–9 | On time in milliseconds, little-endian |
| 10–11 | Off time in milliseconds, little-endian |
| 12–13 | Cycle count, little-endian |

Both phase times and the cycle count must be non-zero. Total duration is limited to 60 seconds. For five 1 Hz pulses, use 500 ms on, 500 ms off and five cycles. The device times the alert and resumes the ordinary RGB framebuffer on completion or cancellation. The left half follows approximately 32 ms colour snapshots, so pulse edges there have that resolution and very short phases may disappear. Half-second phases are the intended notification setting. A VIA build accepts the same payload after the two-byte VIA custom-set-value prefix `07 00`. These are QMK commands, not the vendor updater's 64-byte protocol.

## Dependency correction

The pinned ChibiOS-Contrib commit `5a9ad82b6ba4f649cdb800e8c2bbc9be2d3ce767` has the wrong HT32 reset-controller base. Apply [ht32-reset.patch](ht32-reset.patch) from the QMK root before building:

    git -C lib/chibios-contrib apply ../../keyboards/mistel/md770rgb_bt/ht32-reset.patch

This corrects `RSTCU_BASE` from `0x40088000` to `0x40088100`, matching the [Holtek user manual](https://www.holtek.com/webapi/187531/HT32F52342_52_UserManual_v160.pdf). Otherwise peripheral reset writes can reach clock registers. The QMK SPI-flash driver also selects sector erase (`0x20`) when its configured block and sector sizes are both 4 KiB. Its previous hard-coded block erase (`0xd8`) could erase beyond the intended settings area. Both geometries have host-side command tests.

The ChibiOS dependency correction needs upstreaming separately, followed by a QMK submodule update; remove the local patch when the pinned dependency includes it.

The pinned platform also declares eight NVIC priority bits, while [Holtek's device header](https://github.com/RT-Thread/rt-thread/blob/master/bsp/ht32/libraries/HT32_STD_5xxxx_FWLib/library/Device/Holtek/HT32F5xxxx/Include/ht32f5xxxx_01.h) declares two for this part. Board priorities use the upper two bits so USB and UART can pre-empt the RGB scan with either declaration. The manual's contradictory four-bit statement does not justify changing the shared platform to four bits. Remove this board encoding when the pinned platform's declarations and driver defaults are corrected together.

## Evidence and checks

Pin maps, packet formats, lookup tables and application headers were recovered from the owner's Windows updaters without executing them:

| US application | SHA-256 |
| --- | --- |
| Right V1.07.02 | `8fafdca096756f23626802d5b586b270b0bd512fe5d1c0327db4eafcd4f689b2` |
| Left V4.01 | `febc84d9dc1fd23152218232839279ecb59f255615a6d6c5b4d951a848c85064` |

Related [TEX Shinobi](https://github.com/cyclistmass/tex-qmk/tree/main/shinobi) and [MK Typist](https://github.com/jpsutton/mk-typist-research) ports established useful HT32 startup and loader precedents. Their firmware and recovery claims do not establish MD770 compatibility.

Host-side packet tests use QMK's test runner:

    make test:md770_protocol
    make test:flash_spi/block_4k
    make test:flash_spi/block_64k

Run strict keyboard lint for each half:

    qmk lint -kb mistel/md770rgb_bt/right --strict
    qmk lint -kb mistel/md770rgb_bt/left --strict

See QMK's [build environment setup](https://docs.qmk.fm/getting_started_build_tools) and [make instructions](https://docs.qmk.fm/getting_started_make_guide). No flashing procedure is supplied until independent recovery is verified.
