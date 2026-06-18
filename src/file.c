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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "common.h"
#include "file.h"

/**
 * @brief Load contents of binary file into a newly allocated work buffer.
 * 
 * It takes a file name/path, which is loaded from the file system into a
 * new heap-allocated buffer in the exact size of the file.
 * 
 * @note The caller takes ownership of the newly allocated buffer.
 *       Remember to free the buffer after use!
 * 
 * @param[in]  f_name   Input file path.
 * @param[out] f_len    Pointer to store the file length in bytes.
 * @param[out] buffer   Pointer to a uint8_t pointer that receives the address
 *                      of the newly allocated buffer. Set to NULL on failure.
 * 
 * @return OK on success, ERR on failure
 * 
 */
int load_file_to_work_buffer(char *f_name, uint32_t *f_len, uint8_t **buffer) {
	print_dbg("Opening file: %s\n", f_name);

	FILE *fp = fopen(f_name, "rb");
	if (!fp) {
		fprintf(stderr, "Error: Failed to open file: %s\n", f_name);
		return ERR;
	}

	print_dbg("%s\n", "Determining file size...");
	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	print_dbg("%s %ld\n", "File size:", len);

	if (len < 0) {
		fprintf(stderr, "Error: Could not determine file size.\n");
		fclose(fp);
		return ERR;
	}
	if (len == 0) {
		fprintf(stderr, "Error: File is empty.\n");
		fclose(fp);
		return ERR;
	}
	if (len > MAX_FILE_SIZE) {
		fprintf(stderr, "Error: File too big.\n");
		fclose(fp);
		return ERR;
	}
	*f_len = len;

	print_dbg("%s\n", "Allocating ROM data buffer in heap...");
	*buffer = malloc(*f_len);
	if (!*buffer) {
		fprintf(stderr, "Error: Could not allocate data buffer in memory.\n");
		fclose(fp);
		return ERR;
	}

	print_dbg("%s\n", "Reading file into data buffer...");
	if (fread(*buffer, *f_len, 1, fp) != 1) {
		fprintf(stderr, "Error: Could not read file.\n");

		free(*buffer);
		*buffer = NULL;

		fclose(fp);
		return ERR;
	}

	fclose(fp);
	return OK;
}

/**
 * @brief Checks if a given address in a data buffer is a valid ROM header.
 * 
 * This checks the header validity using some minimal heuristics. First by
 * checking if the first 21 bytes are valid ASCII, and then checking if the 
 * checksum complement and the checksum results is 0xFFFF when added together.
 * If they all do, then we're most likely sure this is a valid ROM header.
 * 
 * @param[in] buf		Pointer to data buffer (entire ROM).
 * @param[in] len		Size of data buffer (entire ROM).
 * @param[in] h_addr	The start address for the header.
 * 
 * @return OK if the header is found at the location, ERR if not.
 */
static int check_rom_header(uint8_t *buf, size_t len, uint32_t h_addr)
{
	if (h_addr + 0x20 > len) {
		return ERR; /* out of bounds */
	}

	/* Check if the first 21 bytes are valid ASCII */
	for (int i = 0; i < 21; i++) {
		uint8_t c = buf[h_addr + i];
		if (c < 0x20 || c > 0x7E)
			return ERR;
	}

	/* Check if checksum complement and checksum sums up to 0xFFFF */
	uint16_t cmp = buf[h_addr + 0x1C] | (buf[h_addr + 0x1D] << 8);
	uint16_t chk = buf[h_addr + 0x1E] | (buf[h_addr + 0x1F] << 8);
	if ((cmp + chk) != 0xFFFF) {
		return ERR;
	}

	return OK;
}

/**
 * @brief Checks the ROM type, based on the location of the ROM header.
 * 
 * This checks one by one if it's a LoROM, HiROM or ExHiROM, using the different
 * known address offsets of each respective ROM headers. Most ROMs tend to be 
 * LoROM, so we check for those first, then HiROM and at last ExHiROM
 * 
 * @param[in]  buf	Poiner to data buffer
 * @param[in]  len	Size of data buffer
 * @param[out] ptr	Pointer to a pointer which will store the location of
 *                  the ROM header's first byte
 * 
 * @return ROM_TYPE_LOROM if LoROM, ROM_TYPE_HIROM if HiROM, 
 *         ROM_TYPE_EXHIROM if ExHiROM, or ERR if no valid header found.
 */
int check_rom_type(uint8_t *buf, size_t len, uint8_t **ptr)
{
	/*
	 * Clean SNES ROMs are usually divisible by 1024 KiB, so if the remainder
	 * is 512, there's most likely an SMC copier header (judged by heuristics).
	 * If it has a copier header, offset the address with 512. We don't need it.
	 */
	size_t off = (len % 1024 == 512) ? 512 : 0;

	if (check_rom_header(buf, len, off + HEADER_ADDR_LOROM) == OK) {
		*ptr = buf + off + HEADER_ADDR_LOROM;
		return ROM_TYPE_LOROM;
	} else if (check_rom_header(buf, len, off + HEADER_ADDR_HIROM) == OK) {
		*ptr = buf + off + HEADER_ADDR_HIROM;
		return ROM_TYPE_HIROM;
	} else if (check_rom_header(buf, len, off + HEADER_ADDR_EXHIROM) == OK) {
		*ptr = buf + off + HEADER_ADDR_EXHIROM;
		return ROM_TYPE_EXHIROM;
	}

	*ptr = NULL;
	return ERR;
}
