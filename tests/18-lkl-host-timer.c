#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <lkl.h>
#include <lkl_host.h>

#define NUM_TIMERS 1
#define TEST_DURATION_NS (1000*1000*1000*2UL)
#define TIMER_INTERVAL_NS 1000*1000UL

// lthread has millisecond accuracy at best
#define SLEEP_DRIFT_ALLOWED_NS (2*1000*1000)

static unsigned int timer_nums[NUM_TIMERS] = { 0 };
static unsigned long long timer_fire_count[NUM_TIMERS] = { 0 };
static void* timers[NUM_TIMERS] = { NULL };
static unsigned long long timer_next_expected_ns[NUM_TIMERS] = { 0 };
static int warn = 0;
static int test_complete = 0;

unsigned long long timestamp_now_ns() {
	struct timespec ts;
	int res = clock_gettime(CLOCK_REALTIME, &ts);
	if (res != 0) {
		fprintf(stderr, "clock_gettime() returned %d\n", res);
		perror("clock_gettime()");
		return res;
	}
	return (ts.tv_sec * (1000*1000*1000UL) + ts.tv_nsec);
}

void timer_fn(void* arg) {
	unsigned int num = *((unsigned int*)arg);
	unsigned long long now = timestamp_now_ns();
	long long delta_ns = now - timer_next_expected_ns[num];
	timer_fire_count[num]++;

	if (!warn && (delta_ns > SLEEP_DRIFT_ALLOWED_NS || delta_ns < -SLEEP_DRIFT_ALLOWED_NS)) {
		fprintf(stderr, "WARN: timer %u fired with delta %lld ns ",
			num, delta_ns);
		warn = 1;
	}

	if (test_complete)
		return;

	int res = lkl_host_ops.timer_set_oneshot(timers[num], TIMER_INTERVAL_NS);
	if (res != 0) {
		fprintf(stderr, "timer_set_oneshot() returned %d after %llu triggers",
			res, timer_fire_count[num]);
		exit(res);
	}
}

int main()
{
	for (unsigned int i = 0; i < NUM_TIMERS; i++)
		timer_nums[i] = i;

	// Check timer_alloc()
	for (unsigned int i = 0; i < NUM_TIMERS; i++) {
		timers[i] = lkl_host_ops.timer_alloc(&timer_fn, &(timer_nums[i]));
		if (timers[i] == NULL) {
			fprintf(stderr, "%dth timer_alloc() returned NULL\n", i);
			return 1;
		}
	}

	unsigned long long now = timestamp_now_ns();
	for (unsigned int i = 0; i < NUM_TIMERS; i++) {
		unsigned long long delta_ns = TIMER_INTERVAL_NS + \
			(i * TIMER_INTERVAL_NS)/NUM_TIMERS;
		int res = lkl_host_ops.timer_set_oneshot(timers[i], delta_ns);
		timer_next_expected_ns[i] = now + delta_ns;
		if (res != 0) {
			fprintf(stderr, "timer_set_oneshot() returned %d\n", res);
			return 2;
		}
	}

	sleep((2 * TEST_DURATION_NS) / (1000*1000*1000UL));

	printf("End of test, stopping timers\n");
	test_complete = 1;

	// Check timer_free()
	for (unsigned int i = 0; i < NUM_TIMERS; i++)
		lkl_host_ops.timer_free(timers[i]);

	return 0;
}

