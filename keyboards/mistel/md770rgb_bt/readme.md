# Mistel MD770 RGB BT

![MD770 RGB BT right PCB](https://raw.githubusercontent.com/markwellis/mistel-md770-qmk/main/docs/mistel-md770-photos/right-pcb.jpg)

A QMK port for the US-layout MD770 RGB BT, including its resident Bluetooth module, stock charging indicators, onboard assignments, persistent macros and Fn/Pn relocation.

* Keyboard Maintainer: [Mark Ellis](https://github.com/markwellis)
* Hardware Supported: MD770BT-M, US 85-key layout, HT32F52352
* Hardware Availability: [Mistel](https://www.mistelkeyboard.com/)

Build each half from the QMK root:

    make mistel/md770rgb_bt/right:default
    make mistel/md770rgb_bt/left:default

Install [pok3rtool](https://github.com/pok3r-custom/pok3rtool) with MD770 RGB BT
support. Until that support is merged, use the
[MD770 device-ID branch](https://github.com/markwellis/pok3rtool/tree/mistel-md770).
The tool's existing CYKB protocol handles both the initial stock-to-QMK
installation and subsequent updates through the retained Mistel bootloader.
On Linux, install the current
[QMK udev rules](https://docs.qmk.fm/faq_build#linux-udev-rules)
with MD770 application/updater support for USB access without root.

Flash each half separately, using its normal host USB port with the
inter-half cable disconnected:

    make mistel/md770rgb_bt/right:default:flash
    make mistel/md770rgb_bt/left:default:flash

Equivalent QMK CLI commands are `qmk flash -kb mistel/md770rgb_bt/right -km default`
and `qmk flash -kb mistel/md770rgb_bt/left -km default`. Stock firmware enters
its retained USB updater automatically. From QMK, use one of the bootloader
shortcuts below before flashing. Connect only the half being flashed and
check that the image belongs to that half.

The flash target invokes the external tool as follows:

    pok3rtool flash QMK mistel_md770rgb_bt_right_default.bin --no-boot

Use the left binary for the left half. The tool verifies application checksums
and writes its version metadata. It leaves the half in the updater after a
successful update: disconnect host USB at the keyboard for ten seconds, then
reconnect without holding keys to start QMK. Keep the inter-half cable
disconnected during this power cycle. Automatic return checking expects
Mistel's vendor interface, which QMK does not expose.
Normal flashing preserves the factory bootloader and protection settings
and needs no PCB bridge. Images must be word-aligned and no larger than
115 KiB (`0x1cc00`); the port's normal builds satisfy these requirements.

The right half is the USB master. The left half can also report its local keys over USB when used independently. Both halves use the keyboard's UART protocol; operation with mixed QMK and vendor firmware has not been tested.

QMK reserves the first 64 KiB of the right half's 512 KiB external SPI flash (`0x0000`–`0xffff`), with a 12,800-byte backing store and 6,400-byte logical EEPROM. Settings resets and journal consolidation erase the whole reserved block, including vendor settings and macro data within it. Save any stock settings or macros you need before using the port. Clear QMK settings when changing the layer or storage layout. Standalone left-half settings are transient.

The `default` keymap provides QWERTY, Fn media/navigation shortcuts and standard QMK lighting and connection controls. The DIP-switch mappings and the factory-style features below belong to the optional `default_mistel` keymap. Build it with `make mistel/md770rgb_bt/right:default_mistel` and `make mistel/md770rgb_bt/left:default_mistel`.

The `default_mistel` keymap keeps the factory Fn media/navigation shortcuts and Pn lighting controls. Pn+Escape selects a colour, Pn+F5 advances the effect, Pn+0 turns lighting off, Pn+Up/Down changes brightness and Pn+Left/Right changes speed. Pn+F6/F7/F8 adjusts red/green/blue. Lighting defaults off; effects use QMK RGB Matrix.

Pn+1 selects Bluetooth output; Pn+2 selects USB. Startup restores the last selected output and Bluetooth slot. Tap Pn+F1/F2/F3 to select Bluetooth slots 1/2/3, or hold for three seconds to pair that slot. USB uses QMK rollover settings; Bluetooth uses six-key reports. Saved dynamic keymaps retain older bindings, so restore or assign these controls when upgrading. A full QMK reset installs the compiled defaults and clears saved assignments, macros and lighting.

LED2 blue follows the stock charging input: steady when high, or three short flashes followed by a three-second gap when low and charging indication is eligible. Low battery uses the same burst on red and disables RGB. Caps Lock is steady red over USB and briefly flashes over Bluetooth; blue charging and red Caps indications can coexist. Both halves run at a fixed 48 MHz while active and use native QMK delays and kernel ticks. Battery idle is ten minutes on the right with local right-hand wake, and 300 ms on the left. Idle retains the board's deep-sleep sequence and uses native 2 ms waits during manual matrix settling, with the scan timer stopped. Physical wake behaviour and battery runtime remain unverified.

USB and power-bank Bluetooth typing, automatic reconnection, RGB effects and USB suspend/wake passed hardware checks. The battery was absent during implementation; battery sleep/wake, runtime, charger current and termination were not tested. The stock application provides no firmware charge-current algorithm to reproduce.

Pn+Quote cycles QWERTY, Colemak and Dvorak. Pn+Left GUI toggles GUI lock. Pn+M selects the default profile; Pn+comma, Pn+dot and Pn+slash select three programmable QMK profile layers. The selected base layout and profile persist independently. The layer indicator is off, red, green or blue respectively. Fn+R held for one second resets the selected profile's assignments after all keys are released when dynamic keymaps are enabled. Hold both physical Alt keys for one second to reset all QMK settings and reboot.

The right `default_mistel` build enables QMK dynamic keymaps without requiring host software. In any profile, press Fn+Right Ctrl, tap a destination, type its replacement key or macro, then press Pn to save. Hold Fn or Pn while choosing a destination to edit its function bank. Release all keys to apply the assignment. Press Fn+Right Ctrl again to exit. White indicates the editor; blinking white indicates recording; solid red after saving indicates a capacity error. Single-key assignments retain normal held-key behaviour. Recorded macro sequences persist across normal reboots. Recording reads original layout assignments and uses a 40 ms interval rather than measuring typing speed. Fn+T/G/B during recording add 15/100/500 ms per complete tap before the next recorded event. The first explicit interval starts from zero; subsequent explicit intervals start from 40 ms. Half-millisecond intervals round up to a millisecond. Recorded delays keep keyboard tasks running; playback stops when another key is pressed. Imported native tap commands retain QMK tap timing. Held ordinary keys prevent playback from starting; held modifiers are allowed.

Hold Fn+Left Shift or Pn+Left Shift for three seconds, release them, then tap a new position to relocate that control within the selected profile. Both Shift keys and Right Ctrl are excluded. Fn/Pn assignments remain reserved even when DIP4 temporarily changes their roles. The original Fn and Pn positions restore Right GUI and Application respectively. Explicit Caps assignments or Fn relocation take precedence over DIP4's Fn swap.

Profile reset and full reset flash the profile indicator white five times. The recorder defaults to 20 shared macro slots and 200 events in 800 bytes of RAM. `DYNAMIC_KEYMAP_MACRO_COUNT` and `MD770_MACRO_RECORD_EVENTS` change these limits; recordings share the remaining logical EEPROM space after 19 keymap layers and other settings. Each profile, including the default, has independent ordinary, Fn and Pn assignments. Profile reset restores all three banks; later recordings reclaim macro slots without remaining assignments. `DYNAMIC_KEYMAP_ENABLE=no` removes the editor from a custom build. An interrupted macro-store update disables playback until a full settings reset. Clear QMK settings when upgrading from the earlier layer order: profiles 1–3 occupy layers 5–7 above the Mac and DIP4 overlays; layers 10–15 hold their function banks and layers 16–18 hold default-profile edits.

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
[hardware and recovery reference](https://github.com/markwellis/mistel-md770-qmk).

Personal notification code can be added to `default_mistel` through QMK userspace with `USER_NAME=<name>`. Its `housekeeping_task_keymap()` hook runs from the factory keymap's housekeeping task.
