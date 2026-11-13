// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "quantum.h"
#include "spi_master.h"

void spi_init(void) {
    static bool initialised;
    if (initialised) {
        return;
    }
    initialised = true;
    gpio_write_pin_high(A9);
    gpio_set_pin_output(A9);
    palSetLineMode(C2, SPI_SCK_FLAGS);
    palSetLineMode(C3, SPI_MOSI_FLAGS);
    palSetLineMode(B6, SPI_MISO_FLAGS);
    // The stock application can leave the flash in deep power-down.
    if (spi_start(A9, false, 0, EXTERNAL_FLASH_SPI_CLOCK_DIVISOR)) {
        spi_write(0xab);
        spi_stop();
        wait_us(10);
    }
}
