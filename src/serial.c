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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libserialport.h>

#include "common.h"
#include "debug.h"
#include "packet.h"
#include "sd2.h"	/* sd2_check_cmd_response() */
#include "serial.h"

/**
 * @brief Automatically scans for serial devices that matches an sd2snes device.
 * 
 * If no port is specified by the user, this will scan for all serial ports,
 * and find one that has the "sd2snes" USB product string in it.
 * 
 * @note If multiple sd2snes devices are connected, the program will only use
 *       the very first one it finds on the device list.
 * 
 * @param[out] port_name Pointer to a string to store the OS-specific port
 *                       name, if any are found.
 * 
 * @return OK on success, ERR on failure.
 */
int serial_scan_device(char **port_name)
{
	printf("No port specified. Scanning for flash cart, please wait...\n");

	struct sp_port **p_list;
	enum sp_return result = sp_list_ports(&p_list);
	
	if (result != SP_OK) {
		fprintf(stderr, "Error: Failed to list any serial ports.\n");
		return ERR;
	}

	print_dbg("%s\n", "Iterating available serial devices...");
	for (int i = 0; p_list[i] != NULL; i++) {
		struct sp_port *p = p_list[i];

		/**
		 * Match the USB product string with "sd2snes".
		 * If it doesn't match, skip the port.
		 */
		char *pdstr = sp_get_port_usb_product(p);
		print_dbg("Device [%d]: %s\n", i, pdstr);

		if (pdstr && strstr(pdstr, "sd2snes")) {
			*port_name = strdup(sp_get_port_name(p));
			print_dbg("Match found [%d] %s - Port: %s\n", i, pdstr, *port_name);
			sp_free_port_list(p_list);
			return OK;
		}
	}
	fprintf(stderr, "Error: No flash carts found.\n");
	printf("Please make sure the console is powered on, the USB cable is " 
		   "connected, and that the flash cart's firmware supports USB.\n");
	printf("Alternatively, try specifying the port using -p <port>\n");
	sp_free_port_list(p_list);
	return ERR;
}

/**
 * @brief Writes (TX) packet to the device.
 * 
 * @param[in]   port            Pointer to the active serial port.
 * @param[in]   input_packet    Pointer to the packet to send.
 * 
 * @note input_packet must point to either a packet_cmd_t or a
 *        packet_raw_t type.
 * 
 * @return Number of bytes received, or ERR on failure.
 */
int serial_write_packet(struct sp_port *port, void *input_packet)
{
	if (!g_is_uploading)
		print_dbg("Write (TX) packet to port: %s\n", sp_get_port_name(port));
	
	/* DEBUG: Only print hex if it's a CMD packet (NOT a raw packet!). */
	if (g_debug && input_packet != NULL && memcmp(input_packet, "USBA", 4) == 0) {
		print_dbg("%s\n",   "==== START OF TX PACKET CONTENTS ====");
		dbg_print_packet_bytes(input_packet);
		print_dbg("%s\n\n", "===== END OF TX PACKET CONTENTS =====");
	}

	int tx = sp_blocking_write(port, input_packet, PACKET_SIZE, SERIAL_TIMEOUT);
	
	if (!g_is_uploading)
		print_dbg("Received %d bytes in response from device.\n", tx);

	if (tx < 0) {
		fprintf(stderr, "Error: Failed to send packet to serial device.\n");
		return ERR;
	}

	if (tx == 0) {
		fprintf(stderr, "Error: Received zero-length response (possibly timed out)\n");
		fprintf(stderr, "Expected %d, got %d\n", PACKET_SIZE, tx);
		return ERR;
	}

	if (tx != PACKET_SIZE) {
		fprintf(stderr, "Error: Invalid response length (expected %d, got %d)\n",
				PACKET_SIZE, tx);
		return ERR;
	}
	return tx;
}

/**
 * @brief Reads (RX) packet from the device.
 * 
 * @param[in]   port            Pointer to the active serial port.
 * @param[out]  output_packet   Buffer to store the received packet.
 *
 * @note input_packet must point to either a packet_cmd_t or a
 * 	     packet_raw_t type.
 * 
 * @return Number of bytes received, or ERR on failure.
 */
int serial_read_packet(struct sp_port *port, void *output_packet)
{
	print_dbg("Read (RX) packet from port: %s\n", sp_get_port_name(port));
	int rx = sp_blocking_read(port, output_packet, PACKET_SIZE, SERIAL_TIMEOUT);
	print_dbg("Received %d bytes from device.\n", rx);

	if (g_debug && output_packet != NULL) {
		print_dbg("%s\n",   "==== START OF RX PACKET CONTENTS ====");
		dbg_print_packet_bytes(output_packet);
		print_dbg("%s\n\n", "===== END OF RX PACKET CONTENTS =====");
	}

	if (rx < 0) {
		fprintf(stderr, "Error: Failed to send packet to serial device.\n");
		return ERR;
	}

	if (rx == 0) {
		fprintf(stderr, "Error: Received zero-length response (possibly timed out)\n");
		fprintf(stderr, "Expected %d, got %d\n", PACKET_SIZE, rx);
		return ERR;
	}

	if (rx != PACKET_SIZE) {
		fprintf(stderr, "Error: Invalid response length (expected %d, got %d)\n",
				PACKET_SIZE, rx);
		return ERR;
	}
	return rx;
}

/**
 * @brief Check if the device is alive.
 * 
 * This sends an INFO packet to the device, and a response is expected.
 * If a response is received, then the device is alive and accepting commands.
 * If no response is received, the device has hung and is not reachable.
 * 
 * @note We are using sp_ functions here directly, instead of our own serial
 * helper functions. This is intentional, as we want to directly interface with
 * the serial device, in case the device is stuck, without the helper functions
 * muddying things for us. This just keeps our sanity in check.
 * 
 * @param[in] port	The active serial port.
 * 
 * @return OK on success, ERR on failure.
 */
int serial_ping(struct sp_port *port)
{
	packet_cmd_t cmd, resp;
	packet_cmd_init(&cmd, OPCODE_INFO, SPACE_FILE, FLAGS_NONE);

	int c = sp_blocking_write(port, &cmd, sizeof(cmd), SERIAL_TIMEOUT);
	int r = sp_blocking_read(port, &resp, sizeof(resp), SERIAL_TIMEOUT);
	
	if (c < PACKET_SIZE || r < PACKET_SIZE) {
		print_dbg("Error: %s", "Device ping failed!\n");
		return ERR;
	}
	uint8_t result;
	
	if (sd2_check_cmd_response(&resp, &result) == OK && result == 0)
		return OK;
	else
		return ERR;
}

/**
 * @brief Runs recovery procedures to fix device hanging due to a bad state.
 * 
 * The device expects all communications to be done in 512-bytes packets.
 * However, in some cases, data might get dropped due to a variety of factors,
 * such as program interruption, USB packet loss, or simply an unplugged cable.
 * If a smaller USB packet (64 bytes long) is dropped for any reason, it will 
 * cause misalignment from the outer 512-bytes boudary the device expects, thus 
 * causing the device to be unresponsive to serial commands.
 * 
 * This can especially be problematic if a packet is dropped mid-upload. Since
 * the sd2snes firmware has no idea how to detect or report packet loss at all,
 * it will just blindly fill up any missing bytes with garbage data, usually 
 * eating up serial commands or other arbitrary data. Some safe guards are 
 * implemented for this specific case in this program, which can be seen in
 * sd2_send_safety_padding() (sd2.c).
 * 
 * This function simply realigns the serial packets with the device. It does so
 * by sending a smaller 64-bytes packet filled with zeros, then checks if the
 * device responds to an INFO command. If it doesn't, it'll send another 64
 * bytes to the device (up to 8 times), until it eventually (and hopefully)
 * receives a valid packet that is perfectly aligned, which at this point the
 * device is should be able to receive packets again.
 * 
 * @param[in] port		The active serial port.
 * @param[in] silent	Disables or enables informative text output to the user.
 * 						If set to 'true', no text will be outputted.
 * 
 * @return OK on success, ERR on failure.
 */
int serial_packet_drop_recovery(struct sp_port *port, bool silent)
{
	uint8_t pad[64] = {0};
	const int max_pkts = 8;

	if (!silent) {
		fprintf(stderr, "\nDevice is active, but is not responding as expected.\n");
		fprintf(stderr,
			"This usually happens due to dropped USB packets, if the program stopped\n"
			"unexpectedly, or if your computer is having performance issues.\n\n");
		fprintf(stderr, "Attempting to recover device state. Please wait");
	}

	for (int i = 0; i < max_pkts; i++) {
		if (!silent) fprintf(stderr, ".");
		
		int pad_resp = sp_blocking_write(port, pad, sizeof(pad), SERIAL_TIMEOUT);

		if (pad_resp == sizeof(pad) && serial_ping(port) == OK) {
			if (!silent) fprintf(stderr, " OK!\n");
			if (!silent) fprintf(stderr, "Device is ready again! Continuing.\n\n");
			return OK;
		}
	}
	/* he's dead jim */
	if (!silent) fprintf(stderr, " failed.\n");
	return ERR;
}