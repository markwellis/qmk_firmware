# Mistel MD770 RGB BT

![MD770 RGB BT right PCB](https://raw.githubusercontent.com/markwellis/md770-qmk-firmware-stuff/main/docs/mistel-md770-photos/right-pcb.jpg)

A wired port for the US-layout MD770 RGB BT. Bluetooth and the vendor's on-keyboard macro editor are not implemented.

* Keyboard Maintainer: [Mark Ellis](https://github.com/markwellis)
* Hardware Supported: MD770BT-M, US 85-key layout, HT32F52352
* Hardware Availability: [Mistel](https://www.mistelkeyboard.com/)

Both halves have passed USB installation, basic typing and split reconnect tests. The RGB-enabled build also passed paired typing, Caps Lock and profile indicators, animated lighting and lighting on/off across both halves. Per-key colours, settings persistence, reset controls, full key coverage, remaining DIP combinations and suspend/wake still need hardware validation. The current ChibiOS dependency needs an HT32 reset-controller correction before building. See the separate [development and recovery notes](https://github.com/markwellis/md770-qmk-firmware-stuff/blob/main/docs/qmk-upstream-readiness.md) for that prerequisite.

Build each half from the QMK root:

    make mistel/md770rgb_bt/right:default
    make mistel/md770rgb_bt/left:default

The right half is the USB master. The left half can also report its local keys over USB when used independently. Both halves use the keyboard's UART protocol; operation with mixed QMK and vendor firmware has not been tested.

QMK settings on the right use external SPI flash at `0x0000`–`0x1fff`, replacing the vendor global-settings sector and first profile sector. Back up any vendor settings you need before using the port. Clear QMK settings when changing the layer or storage layout. Standalone left-half settings are transient.

The default keymap keeps the factory Fn media/navigation shortcuts and Pn lighting controls. Pn+Escape selects a colour, Pn+F5 advances the effect, Pn+0 turns lighting off, Pn+Up/Down changes brightness and Pn+Left/Right changes speed. Pn+F6/F7/F8 adjusts red/green/blue. Lighting defaults off; effects use QMK RGB Matrix.

Pn+Quote cycles QWERTY, Colemak and Dvorak. Pn+Left GUI toggles GUI lock. Pn+M selects the default profile; Pn+comma, Pn+dot and Pn+slash select three programmable QMK profile layers. The selected base layout and profile persist independently. The layer indicator is off, red, green or blue respectively. Fn+R held for one second resets the selected profile's dynamic key assignments after all keys are released. Hold both physical Alt keys for one second to reset all QMK settings and reboot.

Pn+F9 enters per-key colour editing and lights the layer indicator white. Select a colour with Pn+Escape or Pn+F6/F7/F8, then tap ordinary keys to paint them; tapping with the same colour again clears a key. Ordinary typing is suppressed while editing; Fn and Pn remain controls. Pn+F9 saves and exits; Pn+F10 shows the saved colours. Pn+F5 also selects native QMK effects, including a WASD/arrow-key effect. Lighting sleeps with the USB host. The Caps Lock indicator follows the host independently of RGB Matrix.

The right-half DIP switches retain their factory functions:

| Switch | On |
| --- | --- |
| 1 | Mac function keys and Alt/GUI ordering |
| 2 | Swap Caps Lock and left Ctrl |
| 3 | Swap left Alt and GUI outside Mac mode |
| 4 | Swap Caps Lock and Fn, unless switch 2 is on |

## Bootloader

* Hold Delete on the right half, or Escape on the left half, while connecting USB to clear QMK settings and enter the stock updater.
* Pn+Insert enters the stock updater from running QMK firmware.

These methods need functioning firmware. Independent ROM recovery and verified stock restoration are documented for the [right half](https://github.com/markwellis/md770-qmk-firmware-stuff/blob/main/docs/right-recovery-package.md) and [left half](https://github.com/markwellis/md770-qmk-firmware-stuff/blob/main/docs/left-recovery-package.md).

Normal installation uses the [stock USB updater](https://github.com/markwellis/md770-qmk-firmware-stuff/blob/main/docs/usb-flashing.md), preserving the bootloader and protection settings. No PCB bridge is required for that workflow. The stock bootloader uses a custom update protocol; no automatic QMK flashing target is provided.
