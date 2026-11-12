// Copyright 2026 Mark Ellis (@markwellis)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol.h"
#include <string.h>

static uint16_t read_word(const uint8_t *data) {
    return data[0] | (uint16_t)data[1] << 8;
}

static void write_word(uint8_t *data, uint16_t word) {
    data[0] = word;
    data[1] = word >> 8;
}

uint16_t md770_crc(const uint8_t *data, size_t length) {
    uint16_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
        }
    }
    return crc;
}

size_t md770_encode(uint8_t *frame, const uint8_t *payload, uint8_t length) {
    if (length > MD770_FRAME_MAX - 6) {
        return 0;
    }
    write_word(frame + 4, 0xa800 | length);
    memcpy(frame + 6, payload, length);
    uint16_t crc = md770_crc(frame + 4, length + 2);
    write_word(frame, crc);
    write_word(frame + 2, crc ^ 0xffff);
    return length + 6;
}

bool md770_receive(md770_parser_t *parser, uint8_t byte, uint8_t *payload, uint8_t expected_length) {
    if (expected_length > MD770_FRAME_MAX - 6) {
        parser->length = 0;
        return false;
    }
    parser->buffer[parser->length++] = byte;
    while (parser->length >= 6) {
        uint16_t crc = read_word(parser->buffer);
        if ((uint16_t)(crc ^ read_word(parser->buffer + 2)) != 0xffff || read_word(parser->buffer + 4) != (0xa800 | expected_length)) {
            memmove(parser->buffer, parser->buffer + 1, --parser->length);
            continue;
        }
        if (parser->length < (size_t)expected_length + 6) {
            return false;
        }
        bool valid = md770_crc(parser->buffer + 4, expected_length + 2) == crc;
        if (valid) {
            memcpy(payload, parser->buffer + 6, expected_length);
        }
        parser->length = 0;
        return valid;
    }
    return false;
}
