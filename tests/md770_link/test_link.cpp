// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gtest/gtest.h"
#include <array>
#include <vector>
extern "C" {
#include "quantum.h"
#include "test_matrix.h"
#include "connection.h"
#include "keyboards/mistel/md770rgb_bt/md770rgb_bt.h"
#include "keyboards/mistel/md770rgb_bt/protocol.h"
SerialDriver SD0;
int          USBD1;
}

static size_t                              space = MD770_FRAME_MAX;
static std::vector<std::array<uint8_t, 8>> snapshots;
extern "C" {
void  sdStart(SerialDriver *, const SerialConfig *) {}
void  sdStop(SerialDriver *) {}
void  usbConnectBus(void *) {}
void  usbDisconnectBus(void *) {}
void  connection_set_host_noeeprom(connection_host_t) {}
msg_t sdGetTimeout(SerialDriver *, unsigned) {
    return -1;
}
size_t oqGetEmptyI(unsigned *) {
    return space;
}
size_t sdWriteTimeout(SerialDriver *, const uint8_t *data, size_t length, unsigned) {
    md770_parser_t         parser{};
    std::array<uint8_t, 8> keys{};
    for (size_t i = 0; i < length; ++i) {
        if (md770_receive(&parser, data[i], keys.data(), keys.size())) snapshots.push_back(keys);
    }
    return length;
}
void md770_rgb_receive(const uint8_t *) {}
void md770_rgb_sleep(bool) {}
void md770_power_activity(bool) {}
}

static void period(matrix_row_t rows[MATRIX_ROWS]) {
    for (unsigned i = 0; i < 64; ++i) {
        wait_ms(1);
        md770_link_task(rows);
    }
}

class MD770Link : public testing::Test {
   protected:
    matrix_row_t rows[MATRIX_ROWS]{};

    void SetUp() override {
        clear_all_keys();
        snapshots.clear();
        space = MD770_FRAME_MAX;
        md770_link_init();
        for (unsigned i = 0; i < 16; ++i)
            md770_link_task(rows);
    }
};

TEST_F(MD770Link, ReportsShortTapsBetweenPeriodicSnapshots) {
    press_key(0, 8);
    md770_link_task(rows);
    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots.back()[0], 0xfe);
    wait_ms(20);
    release_key(0, 8);
    md770_link_task(rows);
    ASSERT_EQ(snapshots.size(), 2U);
    EXPECT_EQ(snapshots.back()[0], 0xff);
}

TEST_F(MD770Link, RetriesChangedStateAfterQueueBackpressure) {
    space = 0;
    press_key(1, 8);
    period(rows);
    EXPECT_TRUE(snapshots.empty());
    space = MD770_FRAME_MAX;
    md770_link_task(rows);
    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots.back()[0], 0xfd);
    space = 0;
    release_key(1, 8);
    md770_link_task(rows);
    EXPECT_EQ(snapshots.size(), 1U);
    space = MD770_FRAME_MAX;
    md770_link_task(rows);
    ASSERT_EQ(snapshots.size(), 2U);
    EXPECT_EQ(snapshots.back()[0], 0xff);
}

TEST_F(MD770Link, KeepsPeriodicSnapshotsWithoutFloodingUnchangedState) {
    for (unsigned i = 0; i < 100; ++i)
        md770_link_task(rows);
    EXPECT_TRUE(snapshots.empty());
    period(rows);
    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots.back()[0], 0xff);
    period(rows);
    ASSERT_EQ(snapshots.size(), 2U);
    EXPECT_EQ(snapshots.back()[0], 0xff);
}
