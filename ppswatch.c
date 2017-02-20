/*
 * ppswatch.c -- advanced tool to monitor PPS timestamps
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <signal.h>
#include <math.h>

#include "timepps.h"

/* variables for the command-line parameters */
static char *device;
static int mode = PPS_CAPTURECLEAR;
static int margin = 0;

/* statistic variables */
static int total = 0;
static int overflows = 0;
static int max_unsync = 0;
static int max_divergence = 0;
static double mean = 0.0;
static double M2 = 0.0;

int find_source(char *path, pps_handle_t *handle, int *avail_mode)
{
	pps_params_t params;
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

	if (((*avail_mode) & mode) == mode) {
		/* Get current parameters */
		ret = time_pps_getparams(*handle, &params);
		if (ret < 0) {
			fprintf(stderr, "cannot get parameters (%m)\n");
			return -1;
		}
		params.mode |= mode;
		ret = time_pps_setparams(*handle, &params);
		if (ret < 0) {
			fprintf(stderr, "cannot set parameters (%m)\n");
			return -1;
		}
	} else {
		fprintf(stderr, "selected mode not supported (%m)\n");
		return -1;
	}

	fflush(stdout);
	return 0;
}

static int curr_unsync = 0;

#define NSEC_PER_SEC 1000000000L
int fetch_source(pps_handle_t handle, int avail_mode)
{
	struct timespec timeout = { 3, 0 };
	struct timespec ts = { 0, 0 };
	unsigned long seq = 0;
	pps_info_t infobuf;
	int ret;
	long div;
	double delta, delta2;

	if (avail_mode & PPS_CANWAIT) /* waits for the next event */
		ret = time_pps_fetch(handle, PPS_TSFMT_TSPEC, &infobuf,
				   &timeout);
	else {
		sleep(1);
		ret = time_pps_fetch(handle, PPS_TSFMT_TSPEC, &infobuf,
				   &timeout);
	}
	if (ret < 0) {
		if (errno == EINTR) {
			return -1;
		}

		fprintf(stderr, "time_pps_fetch() error %d (%m)\n", ret);
		return -1;
	}

	if (mode & PPS_CAPTUREASSERT) {
		ts = infobuf.assert_timestamp;
		seq = infobuf.assert_sequence;
	} else if (mode & PPS_CAPTURECLEAR) {
		ts = infobuf.clear_timestamp;
		seq = infobuf.clear_sequence;
	}

	if (ts.tv_nsec > NSEC_PER_SEC / 2) {
		ts.tv_sec++;
		ts.tv_nsec -= NSEC_PER_SEC;
	}

	total++;
	div = ts.tv_nsec;
	delta = div - mean;
	mean += delta / total;
	delta2 = div - mean;
	M2 += delta * delta2;
	if (div < 0)
		div = -div;
	if (max_divergence < div)
		max_divergence = div;
	if (div >= margin) {
		printf("timestamp: %ld, sequence: %ld, offset: % 6ld\n", ts.tv_sec, seq, ts.tv_nsec);
		fflush(stdout);
		overflows++;
		curr_unsync++;
	} else {
		if (max_unsync < curr_unsync)
			max_unsync = curr_unsync;
		curr_unsync = 0;
	}

	return 0;
}

static void usage(char *name)
{
	fprintf(stderr, "usage: %s [-a | -c] <ppsdev>\n", name);
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
			{"assert",		no_argument,		0, 'a'},
			{"clear",		no_argument,		0, 'c'},
			{"margin",		required_argument,	0, 'm'},
			{"help",		no_argument,		0, 'h'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "acm:h", long_options, &option_index);

		/* detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
			case 'a': {
					mode |= PPS_CAPTUREASSERT;
					mode &= ~PPS_CAPTURECLEAR;
					break;
				}
			case 'c': {
					mode |= PPS_CAPTURECLEAR;
					mode &= ~PPS_CAPTUREASSERT;
					break;
				}
			case 'm': {
					char *c;
					margin = strtoul(optarg, &c, 0);
					if (*c) {
						fprintf(stderr, "Error parsing margin value.\n");
						exit(1);
					}
					if (margin < 0) {
						fprintf(stderr, "Negative margin not supported.\n");
						exit(1);
					}
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

	if (optind == argc - 1) {
		device = argv[optind];
	} else {
		printf("Device name missing!\n");
		usage(argv[0]);
		exit(1);
	}
}

void print_stats()
{
	printf("\n\nTotal number of PPS signals: %d\n", total);
	if (margin) {
		printf("Number of overflows:         %d (%f%%)\n", overflows, 100.0 * (double)overflows / (double)total);
		printf("Maximum unsynchronized time: %d\n", max_unsync);
	}
	printf("Maximum divergence: %d\n", max_divergence);
	printf("Mean value: %g\n", mean);
	printf("Standard deviation: %g\n", sqrt(M2 / total));
}

static void sighandler_exit(int signum) {
	print_stats();
}

int main(int argc, char *argv[])
{
	pps_handle_t handle;
	int avail_mode;
	struct sigaction sigact;
	int ret;

	/* Check the command line */
	parse_args(argc, argv);

	if (find_source(device, &handle, &avail_mode) < 0)
		exit(EXIT_FAILURE);

	if (margin)
		printf("using margin %d\n", margin);

	sigact.sa_handler = sighandler_exit;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	/* loop, printing the most recent timestamp every second or so */
	while (1) {
		ret = fetch_source(handle, avail_mode);
		if (ret < 0 && errno == EINTR) {
			ret = 0;
			break;
		}
		if (ret < 0 && errno != ETIMEDOUT)
			break;
	}
	
	time_pps_destroy(handle);

	return ret;
}

