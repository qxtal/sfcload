/*
	SFCLoad: A simple ROM loader for SD2SNES/FXPak Pro flash cartridges.
	Copyright (C) 2026 qxtal (www.xtal.net)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.

	SPDX-License-Identifier: GPL-3.0-or-later
*/

#ifndef SFCLOAD_COMMON_H
#define SFCLOAD_COMMON_H

#define PROGRAM	"SFCLoad"
#define VERSION	"1.0.0"
#define YEAR	"2026"

#include <stdint.h>
#include <stdbool.h>

extern bool g_verbose;
extern bool g_debug;
extern bool g_is_uploading;

#define OK	 0
#define ERR	-1

#define MAX_FILE_SIZE			16777216 /* 16 MiB */

#define ROM_TYPE_LOROM			0
#define ROM_TYPE_HIROM			1
#define ROM_TYPE_EXHIROM		2

#define HEADER_ADDR_LOROM		0x00007FC0
#define HEADER_ADDR_HIROM		0x0000FFC0
#define HEADER_ADDR_EXHIROM		0x0040FFC0

#define SERIAL_BAUD_RATE		9600
#define SERIAL_BITS				8
#define SERIAL_PARITY			0
#define SERIAL_STOP_BITS		1
#define SERIAL_FLOW_CONTROL		0
#define SERIAL_TIMEOUT			1000

#define PACKET_SIZE				512

/* Most opcodes are not gonna get used, but they're here for convenience. */
#define OPCODE_GET			0x00
#define OPCODE_PUT			0x01
#define OPCODE_VGET			0x02
#define OPCODE_VPUT			0x03
#define OPCODE_LS			0x04
#define OPCODE_MKDIR		0x05
#define OPCODE_RM			0x06
#define OPCODE_MV			0x07
#define OPCODE_RESET		0x08
#define OPCODE_BOOT			0x09
#define OPCODE_POWER_CYCLE	0x0a
#define OPCODE_INFO			0x0b
#define OPCODE_MENU_RESET	0x0c
#define OPCODE_STREAM		0x0d
#define OPCODE_TIME			0x0e
#define OPCODE_RESPONSE		0x0f

#define SPACE_FILE			0x00
#define SPACE_SNES			0x01
#define SPACE_MSU			0x02
#define SPACE_CMD			0x03
#define SPACE_CONFIG		0x04

#define FLAGS_NONE			0x00 /* 00000000*/
#define FLAGS_SKIPRESET		0x01 /* 00000001*/
#define FLAGS_ONLYRESET		0x02 /* 00000010*/
#define FLAGS_CLRX			0x04 /* 00000100*/
#define FLAGS_SETX			0x08 /* 00001000*/
#define FLAGS_STREAM_BURST	0x10 /* 00010000*/
#define FLAGS_NORESP		0x40 /* 01000000*/
#define FLAGS_DATA64B		0x80 /* 10000000*/

typedef struct __attribute__((packed)) {
	uint8_t magic[4];
	uint8_t opcode;
	uint8_t space;
	uint8_t flags;
	uint8_t _pad1[25];
	uint8_t vectors[32];
	uint8_t _pad2[188];
	uint8_t size[4];
	uint8_t payload[256];
} packet_cmd_t;

typedef struct __attribute__((packed)) {
	uint8_t data[PACKET_SIZE];
} packet_raw_t;

/* https://stackoverflow.com/a/1644898 */
#define print_vrb(...) \
	do { if (g_verbose) fprintf(stderr, __VA_ARGS__); } while (0)
#define print_dbg(fmt, ...) \
	do { if (g_debug) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
		__LINE__, __func__, __VA_ARGS__); } while (0)

#endif
