#pragma once
#include <stdint.h>
#include <string.h>

/**
 * BMDevice wire protocol, mirroring COMMON_DEVICE_COMMANDS in
 * RNUmbrella/constants.ts and BMProtocol.swift on the watch. One command byte,
 * then a payload whose shape depends on the command:
 *
 *   - bools ride as a single byte
 *   - ints ride as int32 little-endian (the firmware memcpy's 4 bytes)
 *   - palette/effect ride as their ASCII id string. Never 1 char: the
 *     firmware reads a 2-byte write as a numeric id.
 *
 * The features characteristic is BLERead | BLEWrite - writes must go with
 * response or the firmware never sees them.
 */

constexpr uint8_t BM_CMD_POWER = 0x01;
constexpr uint8_t BM_CMD_REQUEST_STATUS = 0x02;
constexpr uint8_t BM_CMD_BRIGHTNESS = 0x04;   // percent 1-100
constexpr uint8_t BM_CMD_SPEED = 0x05;        // raw 5-200, lower = faster
constexpr uint8_t BM_CMD_DIRECTION = 0x06;
constexpr uint8_t BM_CMD_PALETTE = 0x08;
constexpr uint8_t BM_CMD_EFFECT = 0x0A;
constexpr uint8_t BM_CMD_COLOR = 0x19;        // [r][g][b]
constexpr uint8_t BM_CMD_SAVE_AS_DEFAULTS = 0x1C;
constexpr uint8_t BM_CMD_SET_SYNC_ENABLED = 0x24;
constexpr uint8_t BM_CMD_IDENTIFY = 0x7E;     // find-me strobe, 5 s
constexpr uint8_t BM_CMD_SET_MARQUEE_TEXT = 0x80;  // [ASCII], <= MARQUEE_MAX
constexpr uint8_t BM_CMD_MATRIX_TEST = 0x83;  // 30 s wiring-diagnostic rainbow
constexpr uint8_t BM_CMD_SET_TEXT_STYLE = 0x84;  // 0 normal, 1 bold
constexpr uint8_t BM_CMD_SET_TEXT_FILL = 0x85;   // 0 gradient, 1 fire, 2 rain, 3 plasma
// The matrix display overlay: 0 off (the running effect owns the panel),
// 1 marquee, 2 word zoom, 3 bitmap, 4 animation. Independent of BM_CMD_EFFECT
// - the rest of the rig keeps playing the selected mode while it shows.
constexpr uint8_t BM_CMD_SET_MATRIX_DISPLAY = 0x89;

constexpr size_t BM_WRITE_MAX = 72;  // longest is [0x80] + 63 chars of marquee

inline size_t bmEncodeBare(uint8_t* out, uint8_t code) {
  out[0] = code;
  return 1;
}

inline size_t bmEncodeBool(uint8_t* out, uint8_t code, bool v) {
  out[0] = code;
  out[1] = v ? 1 : 0;
  return 2;
}

inline size_t bmEncodeInt(uint8_t* out, uint8_t code, int32_t v) {
  out[0] = code;
  memcpy(out + 1, &v, 4);  // ESP32 is little-endian, same as the wire
  return 5;
}

inline size_t bmEncodeString(uint8_t* out, uint8_t code, const char* s) {
  out[0] = code;
  size_t len = strlen(s);
  if (len > BM_WRITE_MAX - 1) len = BM_WRITE_MAX - 1;
  memcpy(out + 1, s, len);
  return 1 + len;
}

inline size_t bmEncodeColor(uint8_t* out, uint8_t r, uint8_t g, uint8_t b) {
  out[0] = BM_CMD_COLOR;
  out[1] = r;
  out[2] = g;
  out[3] = b;
  return 4;
}
