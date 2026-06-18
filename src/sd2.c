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

#define _GNU_SOURCE
#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> /* usleep() */

#include <libserialport.h> /* struct sp_port */

#include "common.h"
#include "debug.h"
#include "packet.h"
#include "serial.h"

#include "compat/memmem.h" /* for memmem() compatibility */

#include "sd2.h"

/**
 * @brief Check the response code of a RESPONSE packet.
 * 
 * @param[in]  packet   The response packet to read.
 * @param[out] result   The response code extracted from the packet.
 * 
 * @return OK on successful, ERR on faliure.
 */
int sd2_check_cmd_response(packet_cmd_t *packet, uint8_t *result)
{
	if (packet == NULL || result == NULL) {
		return ERR;
	}

	if (packet->magic[0] != 'U' || packet->magic[1] != 'S' ||
		packet->magic[2] != 'B' || packet->magic[3] != 'A') {
		return ERR;
	}

	if (packet->opcode != OPCODE_RESPONSE){
		return ERR;
	}

	/**
	 * NOTE: Name is misleading, but the response code is stored
	 * at index 5, which just happens to be in the 'space' field.
	 */
	*result = packet->space;

	return OK;
}

/**
 * @brief Search for a file or directory within a directory on device.
 * 
 * @param[in] port      The active serial port.
 * @param[in] str       The string to search for.
 * @param[in] dir       The directory to search within.
 * @param[in] is_file   If searching for a file, set this to 'true'.
 *                      Otherwise, for a directory, set this to 'false'.
 * 
 * @return Number of matching results found. If none, returns zero. Otherwise,
 *         returns ERR on error.
 */
int sd2_search_fs(struct sp_port *port, const char *str, const char *dir, const bool is_file)
{
	packet_cmd_t cmd_pkt;
	packet_cmd_t resp_pkt;

	packet_cmd_init(&cmd_pkt, OPCODE_LS, SPACE_FILE, FLAGS_NONE);
	packet_cmd_set_payload(&cmd_pkt, dir);

	serial_write_packet(port, &cmd_pkt);
	serial_read_packet(port, &resp_pkt);

	uint8_t resp_code;
	if (sd2_check_cmd_response(&resp_pkt, &resp_code) == ERR || resp_code != 0) {
		printf("\n");
		fprintf(stderr, "Error: Device failed to list internal file directory.\n");
		return ERR;
	}

	size_t buf_max_p = 32;
	size_t buf_max_b = PACKET_SIZE * buf_max_p;
	uint8_t *buf = malloc(buf_max_b);
	if (!buf) {
		fprintf(stderr, "Error: Out of memory.\n");
		return ERR;
	}

	size_t i = 0;
	for (;;) {
		int rx = serial_read_packet(port, &resp_pkt);

		if (rx == ERR) {
			fprintf(stderr, "Error: Could not receive data from device.\n");
			free(buf);
			return ERR;
		}

		if (i >= buf_max_p) {
			buf_max_p *= 2;
			uint8_t *tmp = realloc(buf, PACKET_SIZE * buf_max_p);
			if (!tmp) {
				free(buf);
				fprintf(stderr, "Error: Out of memory.\n");
				return ERR;
			}
			buf = tmp;
		}

		memcpy(buf + (i * PACKET_SIZE), &resp_pkt, PACKET_SIZE);
		i++;

		/*dbg_print_packet_bytes(&resp_pkt);*/

		if (memmem(&resp_pkt, sizeof(resp_pkt), "\xFF", 1) != NULL) {
			break;
		}
	}

	size_t buf_sz = i * PACKET_SIZE;
	buf = realloc(buf, buf_sz);

	uint8_t *s_ptr = memmem(buf, buf_sz, str, strlen(str) + 1); /* null term */

	if (s_ptr == NULL) {
		/* no results found */
		free(buf);
		return 0;
	}

	uint8_t s_type;
	if (s_ptr > buf) {
		/* Get the byte *before* the start of the string. */
		s_type = *(s_ptr - 1); /* 0 = directory, 1 = file */
	} else {
		/**
		 * There should always be a single byte *before* the matched string,
		 * so if there isn't for whatever reason, the buffer is probably
		 * malformed. We bail here to avoid an out-of-bounds read.
		 */
		fprintf(stderr, "Error: Malformed response from device.\n");
		free(buf);
		return ERR;
	}

	if ((is_file && s_type == 1) || (!is_file && s_type == 0)) {
		free(buf);
		return 1;
	}

	/* item found, but not of the type caller was looking for. */
	free(buf);
	return 0;
}


/**
 * @brief Makes a new directory on the device.
 * 
 * @param[in] port      The active serial port.
 * @param[in] dir       The directory to create.
 * 
 * @return OK if directory successfully created, otherwise ERR on failure.
 */
int sd2_mkdir(struct sp_port *port, const char *dir)
{
	packet_cmd_t cmd_pkt;
	packet_cmd_t resp_pkt;

	packet_cmd_init(&cmd_pkt, OPCODE_MKDIR, SPACE_FILE, FLAGS_NONE);
	packet_cmd_set_payload(&cmd_pkt, dir);

	serial_write_packet(port, &cmd_pkt);
	serial_read_packet(port, &resp_pkt);

	uint8_t resp_code;
	if (sd2_check_cmd_response(&resp_pkt, &resp_code) == ERR || resp_code != 0) {
		printf("\n");
		fprintf(stderr, "Error: Device failed to make directory.\n");
		return ERR;
	}

	return OK;
}

/**
 * @brief Boots up a given ROM on the device.
 * 
 * @param[in] port	The active serial port.
 * @param[in] path	The path to the ROM file on the device.
 * 
 * @return OK if successfully booted, ERR on failure.
 * 
 * @todo Add verification that the ROM has booted, using INFO cmd.
 * To be implemented in a future version.
 */
int sd2_boot_rom(struct sp_port *port, const char *path)
{
	packet_cmd_t cmd_pkt;
	packet_cmd_init(&cmd_pkt, OPCODE_BOOT, SPACE_FILE, FLAGS_NORESP);
	packet_cmd_set_payload(&cmd_pkt, path);
	
	if(serial_write_packet(port, &cmd_pkt) == ERR) {
		fprintf(stderr, "Error: Failed to send boot command.\n");
		return ERR;
	}
	
	/**
	 * The device takes some time to boot the program. This timer
	 * makes sure that the user doesn't send any commands too
	 * quickly while the device is ready, which can cause lockups.
	 */
	usleep(1000000);
	
	return OK;
}

/**
 * @brief Uploads ROM file from work buffer to the device's storage.
 * 
 * A transfer is initiated with the device, using the PUT opcode. The device 
 * receives preliminary info about the data: the on-device path and the size.
 * Once the device is ready to receive, the raw data is then sent over the
 * serial connection in 512-bytes chunks. When the device has received all the
 * data, it is then ready to take on further commands again.
 * 
 * @warning Once a PUT transfer is initiated with the device, there is
 * absolutely no way to tell the device to stop receiving data. The device will
 * keep taking in data, and won't stop until it has received the exact amount of
 * bytes that we told it to expect. If the program or user aborts the process in
 * the middle of anupload, the device will get stuck and won't respond to any 
 * further commands, and the file that was being uploaded will contain garbage/
 * corrupted data on the device. See the  * functions sd2_send_safety_padding() 
 * and serial_packet_drop_recovery() for more information.
 * 
 * @param[in] port	The active serial port.
 * @param[in] path	The on-device path, where the file will be stored.
 * @param[in] data	The data buffer containing the raw file data.
 * @param[in] size	The size of the data buffer.
 * 
 * @return OK when upload finished without reported issues, ERR on failure.
 */
int sd2_upload_rom(struct sp_port *port, char *path, uint8_t *data, uint32_t size)
{
	print_vrb("Starting ROM upload.\n");

	int res = OK;

	packet_cmd_t cmd_pkt;
	packet_cmd_t resp_pkt;
	packet_raw_t data_pkt;

	/* progress bar variables */
	uint32_t	progress = 0;
	uint32_t	pr_max = size / PACKET_SIZE;
	uint8_t		pr_dig = (uint8_t)snprintf(NULL, 0, "%d", pr_max);
	int			pr_tty = isatty(STDERR_FILENO);
	char		pr_buf[32];
	uint32_t	pr_len, pr_step, pr_next;
	#define 	PROG_REDRAWS 256 /* can be tuned */
	#define 	PROG_SET(...) do { \
					pr_len = snprintf(pr_buf, sizeof pr_buf, __VA_ARGS__); \
					(void)!write(STDERR_FILENO, pr_buf, pr_len); \
				} while (0)
	
	print_vrb("Initiating file transfer to device... \n");

	packet_cmd_init(&cmd_pkt, OPCODE_PUT, SPACE_FILE, FLAGS_NONE);
	packet_cmd_set_size(&cmd_pkt, size);
	packet_cmd_set_payload(&cmd_pkt, path);

	serial_write_packet(port, &cmd_pkt);
	serial_read_packet(port, &resp_pkt);

	uint8_t resp_code;
	if (sd2_check_cmd_response(&resp_pkt, &resp_code) == ERR || resp_code != 0) {
		fprintf(stderr, "Error: Device failed to initiate file transfer.\n");
		res = ERR;
		return res;
	}

	printf("\nUploading ROM.\n");

	pr_step = pr_max / PROG_REDRAWS;
	if (pr_step == 0) pr_step = 1; /* max too small, draw every iteration */
	pr_next = pr_step;

	if (pr_tty) {
		PROG_SET("\r[%*s/%u]", pr_dig, "", pr_max);
	}

	g_is_uploading = true;

	/* send data to device */
	for (uint32_t offset = 0; offset < size; offset += PACKET_SIZE) {
		memset(data_pkt.data, 0, PACKET_SIZE);
		uint32_t chunk = size - offset;

		if (chunk > PACKET_SIZE) {
			chunk = PACKET_SIZE;
		}

		memcpy(data_pkt.data, data + offset, chunk);

		if (serial_write_packet(port, &data_pkt) != PACKET_SIZE) {
			fprintf(stderr, "\nError: Write failed at offset %u\n", offset);
			res = ERR;
			break;
		}

		progress++;
		if (pr_tty && progress >= pr_next) {
			pr_next += pr_step;
			PROG_SET("\r[%*u/%u]", pr_dig, progress, pr_max);
		}
	}

	if (pr_tty) {
		PROG_SET("\r[%*u/%u]\n", pr_dig, pr_max, pr_max);
	}

	#undef PROG_SET
	#undef PROG_REDRAWS

	sd2_send_safety_padding(port);

	g_is_uploading = false;
	return res;
}

/*int sd2_verify_rom(struct sp_port *port, const char *path, const uint8_t *data, 
				   uint32_t size)
{
	fprintf(stderr, "Not implemented yet.\n");
	return ERR;
}*/

/**
 * @brief Hack/fix to handle device hanging due to dropped USB packets.
 * 
 * This will pad out any remaining data that the device expects, in case of 
 * intermittent or random USB packet drops. The padding contains  * dummy data 
 * that the device won't interpret as legitimate commands and is set to a size
 * that should be plenty to provide a bit of cushioning, just so we can prevent
 * the device from hanging if any packets were dropped.
 * 
 * In a best-case scenario, the device will simply ignore the packets.
 * 
 * In a worst-case scenario, the device will fill the (now corrupted) ROM file
 * with the dummy padding, until the device's internal counter runs out, thus
 * freeing the device from hanging and allowing us to run additional commands,
 * such as verifying the data or reporting the incident back to the user.
 * 
 * @note This will NOT solve the problem of the device hanging if the upload
 * process was fully interrupted, either by the user or due to a crash.
 * 
 * @param[in] port	The active serial port.
 * 
 * @return OK on success, ERR on failure.
 */
int sd2_send_safety_padding(struct sp_port *port)
{
	const int pkts = 4;
	const uint8_t pat[4] = { 0xDE, 0xAD, 0xBE, 0xEF };

	packet_raw_t pad;
	for (size_t i = 0; i < PACKET_SIZE; i++) {
		pad.data[i] = pat[i & 3];
	}

	for (int i = 0; i < pkts; i++) {
		print_dbg("Sending safety padding (dummy data), iteration %d of %d\n", i + 1, pkts);
		if (serial_write_packet(port, &pad) != PACKET_SIZE) {
			print_dbg("Error: %s", "Failed to write safety padding to device!");
			return ERR;
		}
	}

	sp_drain(port);
	return OK;
}
