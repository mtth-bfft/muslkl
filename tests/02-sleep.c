#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define SLEEP_TIME_NS (1*1000*1000*1000UL)
#define USLEEP_TIME_NS (500*1000*1000UL)
#define NSLEEP_TIME_NS 1000*1000UL
// We require at least millisecond precision for LKL timers.
#define SLEEP_DRIFT_TOLERANCE_NS (500*1000UL)

static unsigned long long timestamp_ns_now() {
	struct timespec ts;
	int res = clock_gettime(CLOCK_REALTIME, &ts);
	if (res != 0) {
		fprintf(stderr, "clock_gettime() returned %d\n", res);
		perror("clock_gettime()");
		return res;
	}
	return (ts.tv_sec * (1000*1000*1000UL) + ts.tv_nsec);
}

int main() {
	printf("Going to sleep()...\n");
	unsigned long long ns_start = timestamp_ns_now();
	unsigned int left = sleep(SLEEP_TIME_NS/(1000*1000*1000UL));
	unsigned long long ns_stop = timestamp_ns_now();
	if (left != 0) {
		fprintf(stderr, "Error, sleep() interrupted: %ds left\n", left);
		return left;
	}
	if ((ns_stop - ns_start) < (SLEEP_TIME_NS - SLEEP_DRIFT_TOLERANCE_NS) ||
		((ns_stop - ns_start) > (SLEEP_TIME_NS + SLEEP_DRIFT_TOLERANCE_NS))) {
		fprintf(stderr, "Error: sleep() %llu ns instead of %llu\n",
			ns_stop-ns_start, (unsigned long long)SLEEP_TIME_NS);
		return 1;
	}

	printf("Going to usleep()...\n");
	ns_start = timestamp_ns_now();
	int res = usleep(USLEEP_TIME_NS/(1000UL));
	ns_stop = timestamp_ns_now();
	if (res != 0) {
		fprintf(stderr, "Error, usleep() returned %d\n", res);
		return res;
	}
	if ((ns_stop - ns_start) < (USLEEP_TIME_NS - SLEEP_DRIFT_TOLERANCE_NS) ||
		((ns_stop - ns_start) > (USLEEP_TIME_NS + SLEEP_DRIFT_TOLERANCE_NS))) {
		fprintf(stderr, "Error: usleep() %llu ns instead of %llu\n",
			ns_stop-ns_start, (unsigned long long)USLEEP_TIME_NS);
		return 1;
	}

	printf("Going to nanosleep()...\n");
	ns_start = timestamp_ns_now();
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = NSLEEP_TIME_NS;
	res = nanosleep(&ts, NULL);
	ns_stop = timestamp_ns_now();
	if (res != 0) {
		fprintf(stderr, "Error, nanosleep() returned %d\n", res);
		return res;
	}
	if ((ns_stop - ns_start) < (NSLEEP_TIME_NS - SLEEP_DRIFT_TOLERANCE_NS) ||
		((ns_stop - ns_start) > (NSLEEP_TIME_NS + SLEEP_DRIFT_TOLERANCE_NS))) {
		fprintf(stderr, "Error: nanosleep() %llu ns instead of %llu\n",
			ns_stop-ns_start, (unsigned long long)NSLEEP_TIME_NS);
		return 1;
	}
	return 0;
}

