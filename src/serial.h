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

#ifndef SFCLOAD_SERIAL_H
#define SFCLOAD_SERIAL_H

#include <libserialport.h>
#include "common.h"

int serial_scan_device(char **port_name);
int serial_write_packet(struct sp_port *port, void *input_packet);
int serial_read_packet(struct sp_port *port, void *output_packet);
int serial_ping(struct sp_port *port);
int serial_packet_drop_recovery(struct sp_port *port, bool silent);

#endif