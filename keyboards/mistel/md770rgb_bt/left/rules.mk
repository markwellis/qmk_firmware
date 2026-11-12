OPT_DEFS += -DMD770_LEFT
EEPROM_DRIVER = transient
LDFLAGS += -Wl,--defsym=md770_left=1
