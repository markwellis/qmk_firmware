// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gtest/gtest.h"
#include <array>
#include <vector>
extern "C" {
#include "quantum.h"
#include "test_matrix.h"
#include "keyboards/mistel/md770rgb_bt/md770rgb_bt.h"
#include "keyboards/mistel/md770rgb_bt/protocol.h"
SerialDriver SD0;
}

static size_t                              space = MD770_FRAME_MAX;
static std::vector<std::array<uint8_t, 8>> snapshots;
extern "C" {
void  sdStart(SerialDriver *, const SerialConfig *) {}
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
}

TEST(MD770Link, ShortTapsQueueBackpressureAndHeartbeat) {
    matrix_row_t rows[MATRIX_ROWS]{};
    md770_link_init();
    md770_link_task(rows);
    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots.back()[0], 0xff);
    press_key(0, 8);
    wait_ms(1);
    md770_link_task(rows);
    ASSERT_EQ(snapshots.size(), 2U);
    EXPECT_EQ(snapshots.back()[0], 0xfe);
    release_key(0, 8);
    wait_ms(1);
    md770_link_task(rows);
    ASSERT_EQ(snapshots.size(), 3U);
    EXPECT_EQ(snapshots.back()[0], 0xff);
    space = 0;
    press_key(1, 8);
    md770_link_task(rows);
    EXPECT_EQ(snapshots.size(), 3U);
    space = MD770_FRAME_MAX;
    md770_link_task(rows);
    ASSERT_EQ(snapshots.size(), 4U);
    EXPECT_EQ(snapshots.back()[0], 0xfd);
    wait_ms(31);
    md770_link_task(rows);
    EXPECT_EQ(snapshots.size(), 4U);
    wait_ms(1);
    md770_link_task(rows);
    EXPECT_EQ(snapshots.size(), 5U);
}
