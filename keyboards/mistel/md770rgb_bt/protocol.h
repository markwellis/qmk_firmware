// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MD770_FRAME_MAX 132

typedef struct {
    uint8_t buffer[MD770_FRAME_MAX];
    size_t  length;
} md770_parser_t;

uint16_t md770_crc(const uint8_t *data, size_t length);
size_t   md770_encode(uint8_t *frame, const uint8_t *payload, uint8_t length);
bool     md770_receive(md770_parser_t *parser, uint8_t byte, uint8_t *payload, uint8_t expected_length);
