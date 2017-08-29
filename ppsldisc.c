/*
 * ppsldics.c -- setup PPS line discipline for RS232
 *
 * Copyright (C) 2005-2007   Rodolfo Giometti <giometti@linux.it>
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
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <linux/tty.h>

void usage(char *name)
{
	fprintf(stderr, "usage: %s <ttyS>\n", name);

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int fd;
	int ldisc = N_PPS;
	int ret;

	if (argc < 2)
		usage(argv[0]);

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	ret = ioctl(fd, TIOCSETD, &ldisc);
	if (ret < 0) {
		perror("ioctl(TIOCSETD)");
		exit(EXIT_FAILURE);
	}

	pause();

	return 0;
}
