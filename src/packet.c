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

#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

#include <string.h>

#include "common.h"
#include "packet.h"

/**
 * @brief Initializes a basic command packet.
 * 
 * The packet is initialized with the magic number "USBA" set. All other fields
 * beside the \p opcode, \p space and \p flags fields are set to zero.
 * 
 * @param[in,out] packet    The command packet to initialize.
 * @param[in]     opcode    The command's opcode (if none, set to 0).
 * @param[in]     space     The command's space (if none, set to 0).
 * @param[in]     flags     The command's flags (if none, set to 0).
 */
void packet_cmd_init(packet_cmd_t *packet, uint8_t opcode,
					 uint8_t space, uint8_t flags)
{
	memset(packet, 0, sizeof(packet_cmd_t));

	/* magic number */
	packet->magic[0] = 'U';
	packet->magic[1] = 'S';
	packet->magic[2] = 'B';
	packet->magic[3] = 'A';

	packet->opcode  = opcode;
	packet->space   = space;
	packet->flags   = flags;
}

/**
 * @brief Sets the size field of a command packet.
 * 
 * The uint32_t gets converted to network byte ordre (big-endian) before
 * being stored in its respective field.
 * 
 * @param[in,out] packet    The command packet to modify.
 * @param[in]     size      The size value to be set.
 */
void packet_cmd_set_size(packet_cmd_t *packet, uint32_t size)
{
	packet->size[0] = (size >> 24) & 0xFF;
	packet->size[1] = (size >> 16) & 0xFF;
	packet->size[2] = (size >>  8) & 0xFF;
	packet->size[3] = (size >>  0) & 0xFF;
}

/**
 * @brief Sets the payload (latter 256-byte half) of a command packet.
 * Usually used to provide the path/filename string for a SPACE_FILE command.
 */
void packet_cmd_set_payload(packet_cmd_t *packet, const char *payload)
{
	strncpy((char *)packet->payload, payload, 255);
	packet->payload[255] = '\0';
}