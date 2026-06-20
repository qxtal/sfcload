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
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>	/* basename() */
#include <unistd.h> /* usleep() */
#include <stdatomic.h>

#include <libserialport.h>

#define DMON_IMPL
#include "deps/dmon/dmon.h"

#include "common.h"
#include "crc32.h"
#include "debug.h"
#include "file.h"
#include "packet.h"
#include "sd2.h"
#include "serial.h"

/* globals */
bool g_verbose 		= false;
bool g_debug 		= false;
bool g_serial_init	= false;
bool g_is_uploading	= false;
//bool g_is_watching	= false;
atomic_bool g_restart = false;

static uint8_t *rom_buffer 	= NULL;
static char *file_path		= NULL;
static char *file_dir		= NULL;
static char *file_name		= NULL;
static char *scan_port		= NULL;
static struct sp_port *port = NULL;

/**
 * @brief Cleans up heap allocations and serial port handler on exit().
 */
static void cleanup()
{
	if (rom_buffer) {
		free(rom_buffer);
		rom_buffer = NULL;
	}

	if (scan_port) {
		free(scan_port);
		scan_port = NULL;
	}

	if (file_path) {
		free(file_path);
		file_path = NULL;
	}

	if (file_dir) {
		free(file_dir);
		file_dir = NULL;
	}

	if (file_name) {
		free(file_name);
		file_name = NULL;
	}

	if (port) {
		sp_drain(port);
		sp_flush(port, SP_BUF_BOTH);
		sp_close(port);
		sp_free_port(port);
		port = NULL;
	}
}

/**
 * @brief Prints program name, version and copyright notice.
 */
static void print_program_name()
{
	printf("%s %s - (c) %s qxtal - www.xtal.net\n", PROGRAM, VERSION, YEAR);
	printf("Licensed under the GNU GPL version 3, see COPYING for details.\n\n");
}

/**
 * @brief Prints program usage.
 * 
 * @param[in] program Program name string.
 * 
 * @note Please make sure each line string is no longer than 80 characters,
 *       preferably 79 characters at a maximum.
 */
static void print_usage(char *program)
{
	/* only get executable's name (no full path) */
	char *name = basename(program);

	print_program_name();

	/* please keep each line string below 80 characters */
	fprintf(stderr,
		"Usage: %s [-vhmbVDFN] [-p port] [-d dir] [-n name] romfile\n"
		"\n"
		"Startup options:\n"
		"  -v        Show version and exit.\n"
		"  -h        Show this help message and exit.\n"
		"\n"
		"File loading options:\n"
		"  -p port   Specify the serial port, using the OS specific port name.\n"
		"              (for example: '/dev/ttyACM0' in Linux, or 'COM1' in Windows.)\n"
		"              If not specified, the program will try to automatically scan for\n"
		"              an available flash cart and use the first available port.\n"
		"  -d dir    Set an optional custom upload directory in the flash cart.\n"
		"              By default, all ROMs are uploaded to an 'SFCLoad ROMs' directory\n"
		"              located in the flash cart's root directory.\n"
		"  -n name   Set an optional custom upload file name in the flash cart.\n"
		"              By default, all ROMs are uploaded with the naming convention of\n"
		"              a timestamp, followed by the ROM title, followed by the checksum.\n"
		"  -m        Monitor file changes.\n"
		"              When enabled, the program will reupload the ROM file whenever any\n"
		"              changes to the file are detected. Use Ctrl+C to quit the program.\n"
		"  -b        Automatically boot the ROM after upload.\n"
		"\n"
		"Advanced options:\n"
		"  -V        Enable verbose messages. Please pass this before any other options.\n"
		"  -D        Enable debug messages. Please pass this before any other options.\n"
		"              Note: Enabling debug messages will also enable verbose messages.\n"
		"  -F        Force file upload, even if file validity can't be confirmed.\n"
		"  -N        Preserve original file name when uploading to flash cart.\n"
		"              WARNING: Will overwrite file on device if it has the same name.\n"
		"\n"
		"Mandatory:\n"
		"  romfile   The ROM file to be uploaded.\n",
		name);
		exit(OK);
}

/**
 * @brief Callback function for file system monitoring.
 * 
 * This is used to check if the ROM file has been changed. If the user has
 * enabled file monitoring, the program will automatically reload the file
 * to the device (that part is handled further down in the main function).
 * 
 * This callback function is for the 'dmon' library by @septag, which requires
 * specific parameters. We don't directly use most of those parameters (dmon
 * does internally), so some of them are cast to void to make sure the compiler
 * doesn't complaint to us.
 * 
 * @note No documentation for parameters, since they're only used by dmon
 * internally. We never call this directly, but instead it is passed onto dmon's
 * dmon_watch() function.
 * 
 * For details, please see the documentation for that library.
 */
void file_mon_callback(dmon_watch_id w_id, dmon_action action, const char *root,
						const char *f_path, const char *o_path, void *user)
{
	/* ignore following parameters (required by dmon callbacks, but not used) */
	(void)w_id; (void)root; (void)o_path; (void)user;

	if (action == DMON_ACTION_MODIFY && strcmp(f_path, file_name) == 0) {
		printf("File change for '%s ' detected.\n", file_name);
		atomic_store(&g_restart, true);
	}
}

/**
 * @brief Main routine and entry point of the program.
 * 
 * The program starts with handling options and arguments, and setting the
 * necessary data needed for the program. The provided ROM file is loaded into
 * heap-allocated memory, then goes through some verification and other
 * preparations.
 * 
 * Afterwards, the serial communication is initialized. If no port is provided
 * by the user, the program will automatically scan for a device with the 
 * matching product name, and assign the first match to the port.
 * 
 * Once a link is established, the program will check if the device is ready to
 * receive commands. If it is, it will check if the necessary file directory
 * exists on the device, and if not, will create it. Once that's done, the
 * program will initiate the upload process, which are sent in bulk to the
 * device in 512-byte raw chunks. This process goes on until the internal device
 * counter has reached zero, in which the device should be ready to boot, in 
 * case everything went according to plan.
 * 
 * @param[in] argc	Argument count.
 * @param[in] argv	Argument vector.
 * 
 * @return OK (0) on success, ERR (-1) or any other non-zero value on failure.
 */
int main(int argc, char **argv)
{
	/* options */
	int 	 opt = 0;
	char 	*opt_port = NULL;
	char 	*opt_file = NULL;
	bool 	 opt_boot = false;
	bool	 opt_keepname = false;
	bool	 opt_custom_name = false;
	bool	 opt_file_monitor = false;
	bool	 opt_force = false;

	/* ROM metadata */
	uint32_t rom_length = 0;
	int		 rom_type = -1;
	uint32_t rom_crc32 = 0;
	char 	 rom_title[22] = "SFCLoadROM";
	
	/* ROM upload path variables */
	/*time_t 	 rom_upload_time = time(NULL);*/ /* unused for now */
	char 	 rom_upload_name[128];
	char	 rom_upload_dir[128] = "SFCLoad ROMs"; /* default dir */
	char	 rom_upload_full_path[256];

	atexit(cleanup);
	
	/* options handling */
	/**
	 * TODO: Some of these checks are a bit complex, and really should be
	 * decoupled from the switch/case, just to clean things up here and reduce.
	 * nesting. For now, it just works, so that'll be for a future version.
	 */
	while ((opt = getopt(argc, argv, "DVFhvbmd:n:p:N")) != -1) {
		switch (opt) {
		case 'D':
			g_debug = true;
			g_verbose = true;
			print_dbg("%s\n\n", "Debug mode on.");
			break;
		case 'V':
			g_verbose = true;
			break;
		case 'F':
			opt_force = true;
			break;
		case 'h':
			print_usage(argv[0]);
			break;
		case 'v':
			printf("%s %s\n", PROGRAM, VERSION);
			exit(OK);
		case 'b':
			opt_boot = true;
			break;
		case 'm':
			opt_file_monitor = true;
			break;
		case 'p':
			opt_port = optarg;
			break;
		case 'N':
			opt_keepname = true;
			break;
		case 'd':
			print_dbg("%s\n", "Options: Processing custom directory...");
			if (optarg == NULL || strlen(optarg) == 0) {
				fprintf(stderr, "Error: Please provide directory when using the -d option.\n");
				exit(ERR);
			} else if (strlen(optarg) >= sizeof(rom_upload_dir)) {
				fprintf(stderr, "Error: Directory cannot exceed 127 characters.\n");
				exit(ERR);
			}
			strcpy(rom_upload_dir, optarg);

			/* prevent Windows path shenanigans (might be susperstitious) */
			for (int i = 0; rom_upload_dir[i]; i++) {
				if (rom_upload_dir[i] == '\\') {
					rom_upload_dir[i] = '/';
				}
			}

			/* strip leading/trailing slashes from dir */
			if (rom_upload_dir[0] == '/') {
				memmove(rom_upload_dir, rom_upload_dir + 1, strlen(rom_upload_dir));
			}
			size_t len = strlen(rom_upload_dir);
			if (len > 0 && rom_upload_dir[len - 1] == '/') {
				rom_upload_dir[len - 1] = '\0';
			}

			if (strchr(rom_upload_dir, '/') != NULL) {
				fprintf(stderr, "Error: Nested directories are not supported.\n");
				exit(ERR);
			}
			break;
		case 'n':
			if (optarg == NULL || strlen(optarg) == 0) {
				fprintf(stderr, "Error: Please provide file name when using the -n option.\n");
				exit(ERR);
			} else if (strlen(optarg) >= sizeof(rom_upload_name)) {
				fprintf(stderr, "Error: File name cannot exceed 127 characters.\n");
				exit(ERR);
			}
			strcpy(rom_upload_name, optarg);

			/**
			 * BUG: sd2snes crashes when trying to start a file with no file
			 *      extension (i.e. if the file name doesn't have a dot in it.)
			 *      To fix this, we add a file extension if one's missing.
			 */
			if (strstr(rom_upload_name, ".") == NULL) {
				print_vrb("Warning: file extension missing from provided name. Adding extension...\n");
				if (strlen(rom_upload_name) > 123) {
					print_vrb("File name at maximum, truncating tail to fit extension in.\n");
					memcpy(rom_upload_name + 123, ".sfc", 4);
					rom_upload_name[127] = '\0';
				} else {
					strcat(rom_upload_name, ".sfc");
				}
			}

			opt_custom_name = true;
			break;
		default:
			break;
		}
	}
	if (optind >= argc) {
		fprintf(stderr, "Error: No ROM file specified.\n");
		fprintf(stderr, "See `%s -h` for usage.\n", basename(argv[0]));
		exit(ERR);
	}
	opt_file  = argv[optind];
	file_path = strdup(opt_file);

	char *fd_tmp = strdup(opt_file);
	file_dir = strdup(dirname(fd_tmp));
	free(fd_tmp);

	char *fn_tmp = strdup(opt_file);
	file_name = strdup(basename(fn_tmp));
	free(fn_tmp);

	print_program_name();

_start:
	/* load the ROM into memory */
	if (load_file_to_work_buffer(opt_file, &rom_length, &rom_buffer) == OK) {
		printf("Loaded file: %s (%d bytes read)\n", opt_file, rom_length);
	} else {
		fprintf(stderr, "Error: Could not load file.\n");
		exit(ERR);
	}

	/* calculate CRC32 */
	rom_crc32 = crc32(rom_buffer, rom_length);
	printf("Calculated ROM checksum (CRC32): %08x\n", rom_crc32);

	uint8_t *header_ptr = NULL;
	rom_type = check_rom_type(rom_buffer, rom_length, &header_ptr);
	switch (rom_type)
	{
	case ROM_TYPE_LOROM:
		print_vrb("ROM type detected as LoROM.\n");
		break;
	case ROM_TYPE_HIROM:
		print_vrb("ROM type detected as HiROM.\n");
		break;
	case ROM_TYPE_EXHIROM:
		print_vrb("ROM type detected as ExHiROM.\n");
		break;
	default:
		print_vrb("Could not determine ROM type (bad header?).\n");
		break;
	}

	if (rom_type == -1 && !opt_force) {
		fprintf(stderr, "Error: Input file doesn't seem to be a valid SNES/SFC ROM.\n");
		fprintf(stderr, "This could also be due to a malformed ROM header. Use -F to force the upload.\n");
		exit(ERR);
	}

	if (!opt_keepname && header_ptr != NULL) {
		memcpy(rom_title, header_ptr, 21);
		rom_title[21] = '\0';

		/* truncate trailing spaces */
		int trail = 20;
		while (trail >= 0 && rom_title[trail] == ' ')
			rom_title[trail--] = '\0';

		/* replace middle spaces and illegal chars with hyphens */
		const char replace[] = " \"*/:<>?\\|";
		for (char *ch = rom_title; *ch; ch++) {
			if (strchr(replace, *ch)) {
				*ch = '_';
			}
		}

		print_vrb("ROM title: %s\n", rom_title);
	}

	/* set file name for the upload */
	if (opt_keepname) {
		if (strlen(file_name) >= sizeof(rom_upload_name)) {
			fprintf(stderr, "Error: File name cannot exceed 127 characters.\n");
			exit(ERR);
		}
		strcpy(rom_upload_name, file_name);
	} else if (!opt_custom_name) {
		snprintf(rom_upload_name, sizeof(rom_upload_name), "rom_%s_%08x.%s",
			rom_title, rom_crc32, "sfc");
	}

	/* set upload full path */
	snprintf(rom_upload_full_path, sizeof(rom_upload_full_path), "%s%s/%s",
		rom_upload_dir[0] == '\0' ? "" : "/", rom_upload_dir, rom_upload_name);

	/* scan for device if serial port not set by user */
	if (!opt_port) {
		if (serial_scan_device(&scan_port) != OK) {
			fprintf(stderr, "Error: Could not scan for device.\n");
			exit(ERR);
		}
		printf("Device found at port %s\n", scan_port);
		opt_port = scan_port;
	}

	if (!g_serial_init) {
		if (sp_get_port_by_name(opt_port, &port) != SP_OK) {
			fprintf(stderr, "Error: Could not set serial port.\n");
			exit(ERR);
		}

		/* open serial port, >>keep it open<< until program finishes */
		if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK ||
			sp_set_baudrate(port, SERIAL_BAUD_RATE) != SP_OK ||
			sp_set_bits(port, SERIAL_BITS) != SP_OK ||
			sp_set_parity(port, SERIAL_PARITY) != SP_OK ||
			sp_set_stopbits(port, SERIAL_STOP_BITS) != SP_OK ||
			sp_set_flowcontrol(port, SERIAL_FLOW_CONTROL) != SP_OK ||
			sp_set_dtr(port, SP_DTR_ON) != SP_OK ||
			sp_set_rts(port, SP_RTS_ON) != SP_OK) {
			fprintf(stderr, "Error: Could not initialize serial port.\n");
			exit(ERR);
		}

		/* give the device some time to settle, then flush the buffers */
		usleep(250000);
		sp_flush(port, SP_BUF_BOTH);

		/**
		 * Makes sure the port initialization only happens once, in
		 * case the routine restarts if file monitoring is enabled.
		 */
		g_serial_init = true;
	}

	/**
	 * TODO: Currently, the check below doesn't account for a mid-transfer
	 * interruption. It will fail at the end of the attempted transfer.
	 * I haven't figured out yet how to check for that gracefully. I might 
	 * implement a locking file on disk to check if a transfer was interrupted
	 * or not, and then handle accordingly. For now, this is a TODO I'm saving
	 * for the next version.
	 */
	/**
	 * After establishing connection with the device, we check to see if the
	 * device is in a good state and ready to work.
	 * 
	 * In some cases (e.g. after an interrupted upload), the device might be
	 * stuck due to packet misalignment caused by USB packets dropping or by
	 * a program interruption. In that case, the device will connect, but it
	 * won't be able to actually do anything useful.
	 * 
	 * If a ping (INFO cmd) fails, we will then attempt to run the recovery
	 * procedure, to set the device back to a good state. Otherwise, we exit.
	 * 
	 * See documentation of serial_packet_drop_recovery() for more information.
	 */
	if (serial_ping(port) == ERR &&
		serial_packet_drop_recovery(port, false) == ERR) {
		fprintf(stderr, "Error: Device is not responsive.\n");
		fprintf(stderr, "Please power-cycle the device and try again.\n");
		exit(ERR);
	}

	/* See if directory exists on device. If it doesn't, then create directory */
	if (sd2_search_fs(port, rom_upload_dir, "/", false) <= 0 &&
		sd2_mkdir(port, rom_upload_dir) == ERR) {
		fprintf(stderr,"Error: Failed to create directory on device for file upload.\n");
		exit(ERR);
	}

	print_vrb("Ready to upload to device: \"%s\"\n", rom_upload_full_path);

	bool retry_upload = false;

_retry:
	if (sd2_upload_rom(port, rom_upload_full_path, rom_buffer, rom_length) != OK) {
		fprintf(stderr, "Error: Failed to upload ROM to device.\n");
		fprintf(stderr, "Please power-cycle the console and try again.\n");
		exit(ERR);
	}

	/**
	 * After uploading, check if the device is OK. On very rare occasions,
	 * the device might hang if a USB packet is dropped during transfer.
	 * If the device hangs, try to recover it, and then report the incident to
	 * the user.
	 */
	if (serial_ping(port) == ERR) {
		fprintf(stderr, "\nError: Device not responding after upload. Upload possibly incomplete.\n");
		if (!retry_upload) {
			retry_upload = true;
			fprintf(stderr, "Reattempting upload on a clean state, please wait...\n");
			serial_packet_drop_recovery(port, true);
			goto _retry;
		} else {
			fprintf(stderr, "Failed after second attempt. Please power-cycle the console and try again.\n");
			exit(ERR);
		}
	}

	printf("Finished uploading ROM.\n");

	if (!opt_boot) {
		char buf[32];

		fputs("Boot up program? (the console will reset) [y/N] ", stdout);
		fflush(stdout);

		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			opt_boot = false;
		} else {
			buf[strcspn(buf, "\n")] = '\0';
			opt_boot = (buf[0] == 'y' || buf[0] == 'Y');
		}
	}

	if (opt_boot) {
		printf("Booting ROM.\n");
		sd2_boot_rom(port, rom_upload_full_path);
	} else {
		printf("ROM boot skipped.\n");
	}

	if (opt_file_monitor) {
		dmon_init();
		dmon_watch_id w_id = dmon_watch(file_dir, file_mon_callback, 0, NULL);
		
		printf("\nMonitoring for file changes...\n");
		printf("To quit the program, press Ctrl + C.\n");

		while(!atomic_load(&g_restart)) {
			usleep(1000000);
		}

		if (atomic_load(&g_restart)) {
			dmon_unwatch(w_id);
			dmon_deinit();
			
			free(rom_buffer);
			rom_buffer = NULL;
			
			atomic_store(&g_restart, false);
			goto _start;
		}
	}

	return OK;
}
