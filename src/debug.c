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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "debug.h"

/**
 * @brief Prints out a hex dump of a packet. (For debugging only)
 * 
 * Nothing too fancy. Although please keep the columns a power of two, 
 * otherwise things might get a bit weird.
 * 
 * @param[in] packet Input packet 
 *                   (either of a #packet_cmd_t or a #packet_raw_t type.)
 */
void dbg_print_packet_bytes(const void *packet)
{
	const uint8_t columns = 16;
	const uint8_t *bytes = packet;

	for (uint16_t i = 0; i < PACKET_SIZE; i += columns) {
		uint8_t j;

		/* row indicators */
		printf("%03x:%03x | ", i, i + columns - 1);

		/* hex printout */
		for (j = 0; j < columns; j++) {
			printf("%02x ", bytes[i + j]);
		}
		printf("| ");

		/* ascii printout */
		for (j = 0; j < columns; j++) {
			char c = bytes[i + j];
			if (isprint((unsigned char)c)) {
				putchar(c);
			} else {
				putchar('.'); /* non-printable */
			}
		}
		putchar('\n');
	}
	putchar('\n');
}