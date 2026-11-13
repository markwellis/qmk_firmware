// Copyright 2022 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdbool.h>
#include <hal.h>
#include "util.h"
#include "timer.h"
#include "wear_leveling.h"
#include "wear_leveling_flash_spi_config.h"
#include "wear_leveling_internal.h"

#define ERASE_START ((uint64_t)(WEAR_LEVELING_EXTERNAL_FLASH_BLOCK_OFFSET) * (EXTERNAL_FLASH_BLOCK_SIZE))
#define ERASE_SIZE ((((uint64_t)(WEAR_LEVELING_BACKING_SIZE) + (EXTERNAL_FLASH_SECTOR_SIZE) - 1) / (EXTERNAL_FLASH_SECTOR_SIZE)) * (EXTERNAL_FLASH_SECTOR_SIZE))

STATIC_ASSERT(EXTERNAL_FLASH_SECTOR_SIZE > 0, "Flash sectors must have a positive size");
STATIC_ASSERT(EXTERNAL_FLASH_BLOCK_SIZE > 0, "Flash blocks must have a positive size");
STATIC_ASSERT(EXTERNAL_FLASH_BLOCK_SIZE % EXTERNAL_FLASH_SECTOR_SIZE == 0, "Flash blocks must contain whole sectors");
STATIC_ASSERT(WEAR_LEVELING_EXTERNAL_FLASH_BLOCK_OFFSET >= 0, "Flash block offset cannot be negative");
STATIC_ASSERT(WEAR_LEVELING_EXTERNAL_FLASH_BLOCK_COUNT > 0, "At least one flash block must be allocated");
STATIC_ASSERT(WEAR_LEVELING_BACKING_SIZE > 0, "Flash backing store must have a positive size");
STATIC_ASSERT(ERASE_SIZE <= (uint64_t)(WEAR_LEVELING_EXTERNAL_FLASH_BLOCK_COUNT) * (EXTERNAL_FLASH_BLOCK_SIZE), "Backing store exceeds its flash block allocation");
STATIC_ASSERT(ERASE_START + ERASE_SIZE <= EXTERNAL_FLASH_SIZE, "Backing store exceeds the external flash");

#ifndef WEAR_LEVELING_EXTERNAL_FLASH_BULK_COUNT
#    define WEAR_LEVELING_EXTERNAL_FLASH_BULK_COUNT 32
#endif // WEAR_LEVELING_EXTERNAL_FLASH_BULK_COUNT

bool backing_store_init(void) {
    bs_dprintf("Init\n");
    flash_init();
    return true;
}

bool backing_store_unlock(void) {
    bs_dprintf("Unlock\n");
    // No-op -- handled by the flash driver as it is.
    return true;
}

bool backing_store_erase(void) {
#ifdef WEAR_LEVELING_DEBUG_OUTPUT
    uint32_t start = timer_read32();
#endif

    bool     ret     = true;
    uint32_t address = ERASE_START;
    uint32_t end     = ERASE_START + WEAR_LEVELING_BACKING_SIZE;
    while (address < end) {
        // A backing store can use less than its allocated flash blocks.
        bool           whole_block = address % EXTERNAL_FLASH_BLOCK_SIZE == 0 && end - address >= EXTERNAL_FLASH_BLOCK_SIZE;
        flash_status_t status      = whole_block ? flash_erase_block(address) : flash_erase_sector(address);
        if (status != FLASH_STATUS_SUCCESS) {
            ret = false;
            break;
        }
        address += whole_block ? EXTERNAL_FLASH_BLOCK_SIZE : EXTERNAL_FLASH_SECTOR_SIZE;
    }

    bs_dprintf("Backing store erase took %ldms to complete\n", ((long)(timer_read32() - start)));
    return ret;
}

bool backing_store_write(uint32_t address, backing_store_int_t value) {
    return backing_store_write_bulk(address, &value, 1);
}

bool backing_store_lock(void) {
    bs_dprintf("Lock  \n");
    // No-op -- handled by the flash driver as it is.
    return true;
}

bool backing_store_read(uint32_t address, backing_store_int_t *value) {
    return backing_store_read_bulk(address, value, 1);
}

bool backing_store_read_bulk(uint32_t address, backing_store_int_t *values, size_t item_count) {
    bs_dprintf("Read  ");
    uint32_t       offset = (WEAR_LEVELING_EXTERNAL_FLASH_BLOCK_OFFSET) * (EXTERNAL_FLASH_BLOCK_SIZE) + address;
    flash_status_t status = flash_read_range(offset, values, sizeof(backing_store_int_t) * item_count);
    if (status == FLASH_STATUS_SUCCESS) {
        for (size_t i = 0; i < item_count; ++i) {
            values[i] = ~values[i];
        }
        wl_dump(offset, values, sizeof(backing_store_int_t) * item_count);
    }
    return status == FLASH_STATUS_SUCCESS;
}

bool backing_store_write_bulk(uint32_t address, backing_store_int_t *values, size_t item_count) {
    uint32_t            offset = (WEAR_LEVELING_EXTERNAL_FLASH_BLOCK_OFFSET) * (EXTERNAL_FLASH_BLOCK_SIZE) + address;
    size_t              index  = 0;
    backing_store_int_t temp[WEAR_LEVELING_EXTERNAL_FLASH_BULK_COUNT];
    do {
        // Copy out the block of data we want to transmit first
        size_t this_loop = MIN(item_count, WEAR_LEVELING_EXTERNAL_FLASH_BULK_COUNT);
        for (size_t i = 0; i < this_loop; ++i) {
            temp[i] = values[index + i];
        }

        bs_dprintf("Write ");
        wl_dump(offset, temp, sizeof(backing_store_int_t) * this_loop);

        // Take the complement instead
        for (size_t i = 0; i < this_loop; ++i) {
            temp[i] = ~temp[i];
        }

        // Write out the block
        if (flash_write_range(offset, temp, sizeof(backing_store_int_t) * this_loop) != FLASH_STATUS_SUCCESS) {
            return false;
        }

        offset += this_loop * sizeof(backing_store_int_t);
        index += this_loop;
        item_count -= this_loop;
    } while (item_count > 0);

    return true;
}
