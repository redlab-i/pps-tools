#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <timepps.h>

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
	if ((*avail_mode & PPS_CAPTUREASSERT) == 0) {
		fprintf(stderr, "cannot CAPTUREASSERT\n");
		return -1;
	}
	if ((*avail_mode & PPS_OFFSETASSERT) == 0) {
		fprintf(stderr, "cannot OFFSETASSERT\n");
		return -1;
	}

	/* Capture assert timestamps, and compensate for a 675 nsec
	 * propagation delay */
	ret = time_pps_getparams(*handle, &params);
	if (ret < 0) {
		fprintf(stderr, "cannot get parameters (%m)\n");
		return -1;
	}
	params.assert_offset.tv_sec = 0;
	params.assert_offset.tv_nsec = 675;
	params.mode |= PPS_CAPTUREASSERT | PPS_OFFSETASSERT;
	ret = time_pps_setparams(*handle, &params);
	if (ret < 0) {
		fprintf(stderr, "cannot set parameters (%m)\n");
		return -1;
	}

	return 0;
}

int fetch_source(int i, pps_handle_t *handle, int *avail_mode)
{
	struct timespec timeout;
	pps_info_t infobuf;
	int ret;

	/* create a zero-valued timeout */
	timeout.tv_sec = 3;
	timeout.tv_nsec = 0;

retry:
	if (*avail_mode & PPS_CANWAIT) /* waits for the next event */
		ret = time_pps_fetch(*handle, PPS_TSFMT_TSPEC, &infobuf,
				   &timeout);
	else {
		sleep(1);
		ret = time_pps_fetch(*handle, PPS_TSFMT_TSPEC, &infobuf,
				   &timeout);
	}
	if (ret < 0) {
		if (ret == -EINTR) {
			fprintf(stderr, "time_pps_fetch() got a signal!\n");
			goto retry;
		}

		fprintf(stderr, "time_pps_fetch() error %d (%m)\n", ret);
		return -1;
	}

	printf("source %d - "
	       "assert %ld.%09ld, sequence: %ld - "
	       "clear  %ld.%09ld, sequence: %ld\n",
	       i,
	       infobuf.assert_timestamp.tv_sec,
	       infobuf.assert_timestamp.tv_nsec,
	       infobuf.assert_sequence,
	       infobuf.clear_timestamp.tv_sec,
	       infobuf.clear_timestamp.tv_nsec, infobuf.clear_sequence);

	return 0;
}

void usage(char *name)
{
	fprintf(stderr, "usage: %s <ppsdev> [<ppsdev> ...]\n", name);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int num;
	pps_handle_t handle[4];
	int avail_mode[4];
	int i = 0;
	int ret;

	/* Check the command line */
	if (argc < 2)
		usage(argv[0]);

	for (i = 1; i < argc && i <= 4; i++) {
		ret = find_source(argv[i], &handle[i - 1], &avail_mode[i - 1]);
		if (ret < 0)
			exit(EXIT_FAILURE);
	}

	num = i - 1;
	printf("ok, found %d source(s), now start fetching data...\n", num);

	/* loop, printing the most recent timestamp every second or so */
	while (1) {
		for (i = 0; i < num; i++) {
			ret = fetch_source(i, &handle[i], &avail_mode[i]);
			if (ret < 0 && errno != ETIMEDOUT)
				exit(EXIT_FAILURE);
		}
	}

	for (; i >= 0; i--)
		time_pps_destroy(handle[i]);

	return 0;
}
