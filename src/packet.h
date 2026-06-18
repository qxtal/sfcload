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

#ifndef SFCLOAD_PACKET_H
#define SFCLOAD_PACKET_H

#include "common.h"

void packet_cmd_init(packet_cmd_t *packet, uint8_t opcode, uint8_t space, uint8_t flags);
void packet_cmd_set_size(packet_cmd_t *packet, uint32_t size);
void packet_cmd_set_payload(packet_cmd_t *packet, const char *payload);

#endif