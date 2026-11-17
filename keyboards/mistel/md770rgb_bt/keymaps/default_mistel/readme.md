# Factory-style MD770 RGB BT keymap

This keymap provides the factory-style Fn/Pn shortcuts, DIP-switch mappings,
profiles, onboard reassignment/macros, Fn/Pn relocation and per-key colour
editor described in the keyboard readme. Both halves share this keymap;
persistent programming is enabled on the right.

It can be built with external QMK userspace using `USER_NAME=<name>`.
The optional `housekeeping_task_keymap()` callback lets that userspace add
housekeeping without replacing the factory task. Other standard QMK user
hooks are available where the factory keymap does not already implement them.
