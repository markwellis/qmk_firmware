// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "flash_spi.h"
#include "md770rgb_bt.h"
#include "spi_master.h"

static bool asleep = true;

static void flash_pins(void) {
    gpio_write_pin_high(A9);
    gpio_set_pin_output(A9);
    palSetLineMode(C2, SPI_SCK_FLAGS);
    palSetLineMode(C3, SPI_MOSI_FLAGS);
    palSetLineMode(B6, SPI_MISO_FLAGS);
}

static bool select_flash(void) {
    return spi_start(A9, false, 0, EXTERNAL_FLASH_SPI_CLOCK_DIVISOR);
}

bool md770_storage_wake(void) {
    if (asleep) {
        flash_pins();
        bool identified = false;
        for (uint16_t attempt = 0; attempt < 1024; ++attempt) {
            if (!select_flash()) return false;
            spi_status_t status = spi_write(0xab);
            spi_stop();
            if (status < 0) return false;
            wait_us(10);
            if (!select_flash()) return false;
            uint8_t identity[3];
            status = spi_write(0x9f);
            if (status >= 0) status = spi_receive(identity, sizeof(identity));
            spi_stop();
            if (status < 0) return false;
            if (identity[0] == 0xc8 && identity[1] == 0x40) {
                identified = true;
                break;
            }
        }
        if (!identified || !select_flash()) return false;
        spi_status_t status = spi_write(0x06);
        spi_stop();
        if (status < 0) return false;
        if (!select_flash()) return false;
        const uint8_t configuration[] = {0x01, 0x02, 0x00};
        status                        = spi_transmit(configuration, sizeof(configuration));
        spi_stop();
        if (status < 0) return false;
        for (uint16_t attempt = 0; attempt < 600; ++attempt) {
            if (!select_flash()) return false;
            status = spi_write(0x05);
            if (status >= 0) status = spi_read();
            spi_stop();
            if (status < 0) return false;
            if (!(status & 1)) {
                asleep = false;
                return true;
            }
            wait_us(100);
        }
        return false;
    }
    return true;
}

void spi_init(void) {
    if (!md770_storage_wake()) chSysHalt("flash");
}

bool md770_storage_sleep(void) {
    if (asleep) return true;
    if (flash_is_busy() != FLASH_STATUS_SUCCESS) return false;
    if (!select_flash()) return false;
    spi_status_t status = spi_write(0xb9);
    spi_stop();
    if (status < 0) return false;
    wait_us(10);
    asleep = true;
    palSetLineMode(A9, PAL_MODE_HT32_AF(0));
    return true;
}
