# Mistel MD770 RGB BT

![MD770 RGB BT right PCB](https://raw.githubusercontent.com/markwellis/md770-qmk-firmware-stuff/main/docs/mistel-md770-photos/right-pcb.jpg)

A wired port for the US-layout MD770 RGB BT. Bluetooth and the vendor's on-keyboard macro editor are not implemented.

* Keyboard Maintainer: [Mark Ellis](https://github.com/markwellis)
* Hardware Supported: MD770BT-M, US 85-key layout, HT32F52352
* Hardware Availability: [Mistel](https://www.mistelkeyboard.com/)

This development port has not been tested on hardware. The current ChibiOS dependency needs an HT32 reset-controller correction before building. See the separate [development and recovery notes](https://github.com/markwellis/md770-qmk-firmware-stuff/blob/main/docs/qmk-upstream-readiness.md) for that prerequisite.

Build each half from the QMK root:

    make mistel/md770rgb_bt/right:default
    make mistel/md770rgb_bt/left:default

The right half is the USB master. The left half can also report its local keys over USB when used independently. Both halves use the keyboard's UART protocol; operation with mixed QMK and vendor firmware has not been tested.

QMK settings use the external SPI flash and replace the vendor global-settings sector. Back up any vendor settings you need before using the port.

The default keymap keeps the factory Fn media/navigation shortcuts and Pn lighting controls. Pn+Escape selects a colour, Pn+F5 advances the effect, Pn+0 turns lighting off, Pn+Up/Down changes brightness and Pn+Left/Right changes speed. Pn+F6/F7/F8 adjusts red/green/blue. Lighting defaults off; effects use QMK RGB Matrix.

Pn+Quote cycles QWERTY, Colemak and Dvorak. Pn+Left GUI toggles GUI lock. Custom mappings and macros use QMK or VIA.

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

These methods need functioning firmware. Independent ROM recovery is documented in the [recovery procedure](https://github.com/markwellis/md770-qmk-firmware-stuff/blob/main/docs/right-recovery-package.md). Right-half stock restoration has been verified; left-half recovery remains untested.

The stock bootloader uses a custom update protocol. No automatic QMK flashing target is provided.
