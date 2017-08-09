/*
 * ppsctl.c -- control tool for PPS
 *
 * Copyright (C) 2009-2011   Alexander Gordeev <alex@gordick.net>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/timex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>

#include "timepps.h"

/* variables for the command-line operations
 * 0 means no operation
 * 1 mean enable
 * 2 means disable
 */
static int do_bind = 0;		/* are we binding or unbinding? */
static int do_setflags = 0;	/* should we manipulate kernel NTP PPS flags? */
static int opt_edge = PPS_CAPTURECLEAR;	/* which edge to use? */
static char *device;

static int find_source(char *path, pps_handle_t *handle, int *avail_mode)
{
	int ret;

	printf("trying PPS source \"%s\"\n", path);

	/* Try to find the source by using the supplied "path" name */
	ret = open(path, O_RDWR);
	if (ret < 0) {
		fprintf(stderr, "unable to open device \"%s\" (%m)\n", path);
		return ret;
	}

	/* Open the PPS source (and check the file descriptor) */
	ret = time_pps_create(ret, handle);
	if (ret < 0) {
		fprintf(stderr, "cannot create a PPS source from device "
				"\"%s\" (%m)\n", path);
		return -1;
	}
	printf("found PPS source \"%s\"\n", path);

	/* Find out what features are supported */
	ret = time_pps_getcap(*handle, avail_mode);
	if (ret < 0) {
		fprintf(stderr, "cannot get capabilities (%m)\n");
		return -1;
	}
	if ((*avail_mode & PPS_CAPTUREASSERT) == 0) {
		fprintf(stderr, "cannot CAPTUREASSERT\n");
		return -1;
	}

	return 0;
}

static inline int bind(pps_handle_t handle, int edge)
{
	return time_pps_kcbind(handle, PPS_KC_HARDPPS, edge, PPS_TSFMT_TSPEC);
}

static inline int unbind(pps_handle_t handle)
{
	return time_pps_kcbind(handle, PPS_KC_HARDPPS, 0, PPS_TSFMT_TSPEC);
}

static inline int set_flags()
{
	struct timex tmx = { .modes = 0 };

	if (adjtimex(&tmx) == -1) {
		fprintf(stderr, "adjtimex get failed: %s\n", strerror(errno));
		return -1;
	}

	tmx.modes = ADJ_STATUS;
	tmx.status |= (STA_PPSFREQ | STA_PPSTIME);

	if (adjtimex(&tmx) == -1) {
		fprintf(stderr, "adjtimex set failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static inline int unset_flags()
{
	struct timex tmx = { .modes = 0 };

	if (adjtimex(&tmx) == -1) {
		fprintf(stderr, "adjtimex get failed: %s\n", strerror(errno));
		return -1;
	}

	tmx.modes = ADJ_STATUS;
	tmx.status &= ~(STA_PPSFREQ | STA_PPSTIME);

	if (adjtimex(&tmx) == -1) {
		fprintf(stderr, "adjtimex set failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static inline void usage(char *name)
{
	fprintf(stderr, "Usage: %s [-bBfFac] <ppsdev>\n"
			"Commands:\n"
			"  -b   bind kernel PPS consumer\n"
			"  -B   unbind kernel PPS consumer\n"
			"  -f   set kernel NTP PPS flags\n"
			"  -F   unset kernel NTP PPS flags\n"
			"Options:\n"
			"  -a   use assert edge\n"
			"  -c   use clear edge (default)\n",
			name);
}

static void parse_args(int argc, char **argv)
{
	while (1)
	{
		int c;
		/* getopt_long stores the option index here. */
		int option_index = 0;
		static struct option long_options[] =
		{
			{"bind",		no_argument,		0, 'b'},
			{"unbind",		no_argument,		0, 'B'},
			{"set-flags",		no_argument,		0, 'f'},
			{"unset-flags",		no_argument,		0, 'F'},
			{"assert",		no_argument,		0, 'a'},
			{"clear",		no_argument,		0, 'c'},
			{"help",		no_argument,		0, 'h'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "bBfFach", long_options, &option_index);

		/* detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
			case 'b': {
					do_bind = 1;
					break;
				}
			case 'B': {
					do_bind = 2;
					break;
				}
			case 'f': {
					do_setflags = 1;
					break;
				}
			case 'F': {
					do_setflags = 2;
					break;
				}
			case 'a': {
					opt_edge = PPS_CAPTUREASSERT;
					break;
				}
			case 'c': {
					opt_edge = PPS_CAPTURECLEAR;
					break;
				}
			case 'h': {
					usage(argv[0]);
					exit(0);
				}
			default: {
					usage(argv[0]);
					exit(1);
				}
		}
	}

	if ((do_bind == 0) && (do_setflags == 0)) {
		printf("No command specified!\n");
		usage(argv[0]);
		exit(1);
	}

	if (optind == argc - 1) {
		device = argv[optind];
	} else {
		printf("Device name missing!\n");
		usage(argv[0]);
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	pps_handle_t handle;
	int avail_mode;

	/* Check the command line */
	parse_args(argc, argv);

	if (find_source(device, &handle, &avail_mode) < 0)
		exit(EXIT_FAILURE);

	if (do_setflags == 2)
		if (unset_flags() < 0) {
			fprintf(stderr, "Failed to unset flags!\n");
			exit(EXIT_FAILURE);
		}

	if (do_bind == 2)
		if (bind(handle, 0) < 0) {
			fprintf(stderr, "Unbind failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

	if (do_bind == 1)
		if (bind(handle, opt_edge) < 0) {
			fprintf(stderr, "Bind failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

	if (do_setflags == 1)
		if (set_flags() < 0) {
			fprintf(stderr, "Failed to set flags!\n");
			exit(EXIT_FAILURE);
		}

	time_pps_destroy(handle);

	return 0;
}

