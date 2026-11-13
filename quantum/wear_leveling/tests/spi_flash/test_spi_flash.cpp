// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <vector>
extern "C" {
#include "wear_leveling_flash_spi_config.h"
#include "wear_leveling_internal.h"
}

static std::array<uint8_t, EXTERNAL_FLASH_SIZE> flash;
static std::vector<uint32_t>                    addresses;
static std::vector<bool>                        blocks;
static bool                                     fail_erase;

static flash_status_t erase(uint32_t address, uint32_t size, bool block) {
    addresses.push_back(address);
    blocks.push_back(block);
    if (fail_erase || address % size || address + size >= EXTERNAL_FLASH_SIZE) {
        return FLASH_STATUS_ERROR;
    }
    std::fill_n(flash.begin() + address, size, 0xff);
    return FLASH_STATUS_SUCCESS;
}

extern "C" {
void           flash_init(void) {}
flash_status_t flash_erase_block(uint32_t address) {
    return erase(address, EXTERNAL_FLASH_BLOCK_SIZE, true);
}
flash_status_t flash_erase_sector(uint32_t address) {
    return erase(address, EXTERNAL_FLASH_SECTOR_SIZE, false);
}
flash_status_t flash_read_range(uint32_t, void *, size_t) {
    return FLASH_STATUS_ERROR;
}
flash_status_t flash_write_range(uint32_t, const void *, size_t) {
    return FLASH_STATUS_ERROR;
}
}

class SPIBacking : public testing::Test {
   protected:
    void SetUp() override {
        flash.fill(0xa5);
        addresses.clear();
        blocks.clear();
        fail_erase = false;
    }
};

TEST_F(SPIBacking, ErasesCoveringSectorsAndPreservesSurroundingData) {
    ASSERT_TRUE(backing_store_erase());
    constexpr uint32_t start  = WEAR_LEVELING_EXTERNAL_FLASH_BLOCK_OFFSET * EXTERNAL_FLASH_BLOCK_SIZE;
    constexpr uint32_t extent = ((WEAR_LEVELING_BACKING_SIZE + EXTERNAL_FLASH_SECTOR_SIZE - 1) / EXTERNAL_FLASH_SECTOR_SIZE) * EXTERNAL_FLASH_SECTOR_SIZE;
    ASSERT_FALSE(addresses.empty());
    EXPECT_EQ(addresses.front(), start);
    EXPECT_EQ(std::count(blocks.begin(), blocks.end(), true), WEAR_LEVELING_BACKING_SIZE / EXTERNAL_FLASH_BLOCK_SIZE);
    EXPECT_EQ(std::count(blocks.begin(), blocks.end(), false), ((WEAR_LEVELING_BACKING_SIZE % EXTERNAL_FLASH_BLOCK_SIZE) + EXTERNAL_FLASH_SECTOR_SIZE - 1) / EXTERNAL_FLASH_SECTOR_SIZE);
    for (uint32_t i = 0; i < flash.size(); ++i) {
        ASSERT_EQ(flash[i], i >= start && i < start + extent ? 0xff : 0xa5) << "address " << i;
    }
}

TEST_F(SPIBacking, StopsOnFirstEraseFailure) {
    fail_erase = true;
    EXPECT_FALSE(backing_store_erase());
    ASSERT_EQ(addresses.size(), 1U);
    EXPECT_TRUE(std::all_of(flash.begin(), flash.end(), [](uint8_t value) { return value == 0xa5; }));
}
