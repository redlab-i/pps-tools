#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>

#include <timepps.h>

/* variables for the command-line parameters */
static int bind = 1; /* are we binding or unbinding? */
static char *device;

int find_source(char *path, pps_handle_t *handle, int *avail_mode)
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
	if ((*avail_mode & PPS_OFFSETASSERT) == 0) {
		fprintf(stderr, "cannot OFFSETASSERT\n");
		return -1;
	}

	return 0;
}

void usage(char *name)
{
	fprintf(stderr, "usage: %s [-u] <ppsdev>\n", name);
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
			{"unbind",		no_argument,		0, 'u'},
			{"help",		no_argument,		0, 'h'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "uh", long_options, &option_index);

		/* detect the end of the options. */
		if (c == -1)
			break;

		switch (c)
		{
			case 'u': {
					bind = 0;
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

int main(int argc, char *argv[])
{
	pps_handle_t handle;
	int avail_mode;

	/* Check the command line */
	parse_args(argc, argv);

	if (find_source(device, &handle, &avail_mode) < 0)
		exit(EXIT_FAILURE);

	time_pps_kcbind(handle, PPS_KC_HARDPPS, bind ? PPS_CAPTURECLEAR : 0, PPS_TSFMT_TSPEC);

	time_pps_destroy(handle);

	return 0;
}

