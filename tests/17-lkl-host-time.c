#define _POSIX_SOURCE
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void format_ns_time(unsigned long long timestamp_ns, char* buf, size_t len) {
	time_t secs_part = (time_t)(timestamp_ns / (1000*1000*1000));
	unsigned long ns_part = timestamp_ns % (1000*1000*1000);
	struct tm *time = gmtime((time_t*)&secs_part);
	strftime(buf, len, "%d/%m/%Y %H:%M:%S", time);
	snprintf(buf+strlen(buf), len-strlen(buf), ".%09ld", ns_part);
}

int main() {
	// Check time() compared to host datetime
	unsigned long long real_t = timestamp_ns_now();
	unsigned long long t = lkl_host_ops.time();
	char real_t_buf[30] = {0};
	char t_buf[30] = {0};
	format_ns_time(real_t, real_t_buf, sizeof(real_t_buf));
	format_ns_time(t, t_buf, sizeof(t_buf));

	if (t < (real_t - ALLOWED_CLOCK_DELTA_NS))
		fprintf(stderr, "WARN: in past (real=%s? / guest=%s)) ",
			real_t_buf, t_buf);
	else if(t > (real_t + ALLOWED_CLOCK_DELTA_NS))
		fprintf(stderr, "WARN: in future (real=%s? / guest=%s) ",
			real_t_buf, t_buf);

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

