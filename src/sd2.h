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

#ifndef SFCLOAD_SD2_H
#define SFCLOAD_SD2_H

#include <stdint.h>
#include <stdbool.h>

#include <libserialport.h> /* struct sp_port */

int sd2_check_cmd_response(packet_cmd_t *packet, uint8_t *result);
int sd2_search_fs(struct sp_port *port, const char *str, const char *dir, const bool is_file);
int sd2_mkdir(struct sp_port *port, const char *dir);
int sd2_boot_rom(struct sp_port *port, const char *path);
int sd2_upload_rom(struct sp_port *port, char *path, uint8_t *data, uint32_t size);
int sd2_send_safety_padding(struct sp_port *port);

#endif