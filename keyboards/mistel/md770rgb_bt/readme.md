# Mistel MD770 RGB BT

![MD770 RGB BT right PCB](https://raw.githubusercontent.com/markwellis/md770-qmk-firmware-stuff/main/docs/mistel-md770-photos/right-pcb.jpg)

A wired port for the US-layout MD770 RGB BT. Bluetooth and the vendor's on-keyboard macro editor are not implemented.

* Keyboard Maintainer: [Mark Ellis](https://github.com/markwellis)
* Hardware Supported: MD770BT-M, US 85-key layout, HT32F52352
* Hardware Availability: [Mistel](https://www.mistelkeyboard.com/)

Build each half from the QMK root:

    make mistel/md770rgb_bt/right:default
    make mistel/md770rgb_bt/left:default

Flash each half separately, using its normal host USB port with the
inter-half cable disconnected:

    make mistel/md770rgb_bt/right:default:flash
    make mistel/md770rgb_bt/left:default:flash

Equivalent QMK CLI commands are `qmk flash -kb mistel/md770rgb_bt/right -km default`
and `qmk flash -kb mistel/md770rgb_bt/left -km default`. Stock firmware enters
its retained USB updater automatically. From QMK, use one of the bootloader
shortcuts below before flashing. Connect only the half being flashed.

The native Mistel flasher checks that the image matches the connected half,
saves updater metadata beside the copied binary in the QMK root as
`*.bin.iap-backup.json`,
then verifies the application and restored metadata. Keep that backup until
the keyboard works. If interrupted, reconnect in updater mode and retry the
same command with the same backup. Normal flashing preserves the factory
bootloader and protection settings and needs no PCB bridge.

The right half is the USB master. The left half can also report its local keys over USB when used independently. Both halves use the keyboard's UART protocol; operation with mixed QMK and vendor firmware has not been tested.

QMK settings on the right use external SPI flash at `0x0000`–`0x1fff`, replacing the vendor global-settings sector and first profile sector. Back up any vendor settings you need before using the port. Clear QMK settings when changing the layer or storage layout. Standalone left-half settings are transient.

The default keymap keeps the factory Fn media/navigation shortcuts and Pn lighting controls. Pn+Escape selects a colour, Pn+F5 advances the effect, Pn+0 turns lighting off, Pn+Up/Down changes brightness and Pn+Left/Right changes speed. Pn+F6/F7/F8 adjusts red/green/blue. Lighting defaults off; effects use QMK RGB Matrix.

Pn+Quote cycles QWERTY, Colemak and Dvorak. Pn+Left GUI toggles GUI lock. Pn+M selects the default profile; Pn+comma, Pn+dot and Pn+slash select three programmable QMK profile layers. The selected base layout and profile persist independently. The layer indicator is off, red, green or blue respectively. Fn+R held for one second resets the selected profile's assignments after all keys are released when dynamic keymaps are enabled. Hold both physical Alt keys for one second to reset all QMK settings and reboot.

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
* Pn+Insert flashes both status indicators three times, then enters the stock updater. The updater controls the indicators after entry.

These methods need functioning firmware. ROM recovery requires opening the
case: SEL2 on the right and SEL4 on the left select ROM recovery at power-on.
Do not bridge pads during normal flashing. Board photos and verified stock
recovery procedures are available in the optional
[hardware and recovery reference](https://github.com/markwellis/md770-qmk-firmware-stuff).
