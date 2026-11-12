// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gtest/gtest.h"
#include <cstring>
#include <vector>
extern "C" {
#include "drivers/flash/flash_spi.h"
#include "drivers/spi_master.h"
}

static std::vector<std::vector<uint8_t>> transactions;
static std::vector<uint8_t>              current;

extern "C" {
void spi_init(void) {}
bool spi_start(pin_t, bool, uint8_t, uint16_t) {
    current.clear();
    return true;
}
spi_status_t spi_write(uint8_t byte) {
    current.push_back(byte);
    return SPI_STATUS_SUCCESS;
}
spi_status_t spi_read(void) {
    return 0;
}
spi_status_t spi_transmit(const uint8_t *data, uint16_t length) {
    current.insert(current.end(), data, data + length);
    return SPI_STATUS_SUCCESS;
}
spi_status_t spi_receive(uint8_t *data, uint16_t length) {
    std::memset(data, 0xff, length);
    return SPI_STATUS_SUCCESS;
}
void spi_stop(void) {
    transactions.push_back(current);
}
}

TEST(SPIFlash, ConfiguredEraseGeometrySelectsPhysicalCommand) {
    transactions.clear();
    ASSERT_EQ(flash_erase_block(0), FLASH_STATUS_SUCCESS);
#if EXTERNAL_FLASH_BLOCK_SIZE == EXTERNAL_FLASH_SECTOR_SIZE
    const std::vector<uint8_t> erase = {0x20, 0, 0, 0};
#else
    const std::vector<uint8_t> erase = {0xd8, 0, 0, 0};
#endif
    EXPECT_EQ(transactions, (std::vector<std::vector<uint8_t>>{{0x05}, {0x06}, erase, {0x05}}));
}

TEST(SPIFlash, EraseAddressUsesBigEndianWireOrder) {
    transactions.clear();
    ASSERT_EQ(flash_erase_block(EXTERNAL_FLASH_BLOCK_SIZE), FLASH_STATUS_SUCCESS);
    ASSERT_EQ(transactions.size(), 4U);
    EXPECT_EQ(transactions[2][1], (EXTERNAL_FLASH_BLOCK_SIZE >> 16) & 0xff);
    EXPECT_EQ(transactions[2][2], (EXTERNAL_FLASH_BLOCK_SIZE >> 8) & 0xff);
    EXPECT_EQ(transactions[2][3], EXTERNAL_FLASH_BLOCK_SIZE & 0xff);
}

TEST(SPIFlash, RejectsUnalignedAndOutOfRangeEraseBeforeSPI) {
    transactions.clear();
    EXPECT_EQ(flash_erase_block(1), FLASH_STATUS_ERROR);
    EXPECT_EQ(flash_erase_block(EXTERNAL_FLASH_SIZE), FLASH_STATUS_ERROR);
    EXPECT_TRUE(transactions.empty());
}
