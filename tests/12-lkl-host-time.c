#define _POSIX_SOURCE
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <lkl.h>
#include <lkl_host.h>

#define ALLOWED_CLOCK_DELTA_NS (1000*1000UL)
#define TEST_DURATION_NS (1000*1000*1000*2UL)

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

static char* format_ns_time(unsigned long long timestamp_ns) {
	static char buffer[30] = {0};
	time_t secs_part = (time_t)(timestamp_ns / (1000*1000*1000));
	unsigned long ns_part = timestamp_ns % (1000*1000*1000);
	struct tm *time = localtime((time_t*)&secs_part);
	strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S.", time);
	snprintf(buffer+20, sizeof(buffer)-20, "%09ld", ns_part);
	return buffer;
}

int main() {
	// Check time() compared to host datetime
	unsigned long long real_t = timestamp_ns_now();
	unsigned long long t = lkl_host_ops.time();
	printf("%s ", format_ns_time(t));

	if (t < (real_t - ALLOWED_CLOCK_DELTA_NS))
		fprintf(stderr, "WARN: in past (real=%s?) ", format_ns_time(real_t));
	else if(t > (real_t + ALLOWED_CLOCK_DELTA_NS))
		fprintf(stderr, "WARN: in future (real=%s?) ", format_ns_time(real_t));

	// Check that time() is monotonic and actually increases
	unsigned long long ns_elapsed = 0;
	unsigned long long previous_timestamp = lkl_host_ops.time();
	int warn = 0;
	while (ns_elapsed < TEST_DURATION_NS) {
		long long delta = lkl_host_ops.time() - previous_timestamp;
		if (delta < 0 && !warn) {
			fprintf(stderr, "WARN: clock went backwards %lldns", delta);
			warn = 1;
		}
		ns_elapsed += delta;
	}

	return 0;
}

