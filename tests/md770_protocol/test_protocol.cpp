// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gtest/gtest.h"
#include <array>
#include <cstring>
extern "C" {
#include "keyboards/mistel/md770rgb_bt/protocol.h"
}

TEST(MD770Protocol, XmodemReference) {
    const uint8_t input[] = "123456789";
    EXPECT_EQ(md770_crc(input, 9), 0x31c3);
}

TEST(MD770Protocol, KeySnapshotGoldenFrame) {
    const uint8_t keys[8]    = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    const uint8_t golden[14] = {0x1b, 0xdd, 0xe4, 0x22, 0x08, 0xa8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t       frame[MD770_FRAME_MAX];
    EXPECT_EQ(md770_encode(frame, keys, 8), sizeof(golden));
    EXPECT_EQ(std::memcmp(frame, golden, sizeof(golden)), 0);
}

TEST(MD770Protocol, RGBSnapshotAndNoiseRecovery) {
    std::array<uint8_t, 126> rgb{};
    for (size_t i = 0; i < rgb.size(); ++i) {
        rgb[i] = i;
    }
    uint8_t frame[MD770_FRAME_MAX];
    ASSERT_EQ(md770_encode(frame, rgb.data(), rgb.size()), sizeof(frame));
    md770_parser_t           parser{};
    std::array<uint8_t, 126> received{};
    for (uint8_t noise : {0x18, 0x9c, 0xe7, 0x00, 0x08, 0xa8}) {
        EXPECT_FALSE(md770_receive(&parser, noise, received.data(), received.size()));
    }
    for (size_t i = 0; i < sizeof(frame); ++i) {
        EXPECT_EQ(md770_receive(&parser, frame[i], received.data(), received.size()), i + 1 == sizeof(frame));
    }
    EXPECT_EQ(received, rgb);
}

TEST(MD770Protocol, RejectsCorruptionAndRecoversOnNextSnapshot) {
    uint8_t keys[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe};
    uint8_t frame[MD770_FRAME_MAX];
    size_t  length = md770_encode(frame, keys, 8);
    uint8_t received[8];
    std::memset(received, 0x55, sizeof(received));
    md770_parser_t parser{};
    frame[10] ^= 1;
    for (size_t i = 0; i < length; ++i) {
        EXPECT_FALSE(md770_receive(&parser, frame[i], received, 8));
    }
    EXPECT_EQ(received[0], 0x55);
    frame[10] ^= 1;
    for (size_t i = 0; i < length; ++i) {
        EXPECT_EQ(md770_receive(&parser, frame[i], received, 8), i + 1 == length);
    }
    EXPECT_EQ(std::memcmp(received, keys, 8), 0);
}

TEST(MD770Protocol, RejectsOversizedAndWrongDirectionFrames) {
    uint8_t payload[127] = {};
    uint8_t frame[MD770_FRAME_MAX];
    EXPECT_EQ(md770_encode(frame, payload, 127), 0U);
    size_t         length = md770_encode(frame, payload, 126);
    md770_parser_t parser{};
    uint8_t        received[8];
    for (size_t i = 0; i < length; ++i) {
        EXPECT_FALSE(md770_receive(&parser, frame[i], received, 8));
    }
}
