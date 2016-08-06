#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#define UNUSED(x) (void)(x)

#define SLEEP_TIME_S 2
#define SLEEP_TIME_MS (1000*SLEEP_TIME_S)
#define SLEEP_TIME_NS ((1000*1000)*SLEEP_TIME_MS)

// lthread has millisecond accuracy at best
#define SLEEP_DRIFT_ALLOWED_NS (1000*1000)

volatile int join_barrier_reached = 0;

// lthread headers are not exported properly for now
void lthread_sleep(uint64_t msecs);

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

void check_state(int newstate) {
	static int state = 0;
	if (newstate != state && newstate != (state+1)) {
		fprintf(stderr, "State %d reached from state %d\n",
			newstate, state);
		exit(1);
	}
	state = newstate;
}

void* secondary_sleep(void* arg) {
	UNUSED(arg);
	unsigned long long target_ns = SLEEP_TIME_S*1000*1000*1000UL;
	unsigned long long start_ns = timestamp_now_ns();

	check_state(2);
	printf("Testing lthread_sleep(%d ms)...\n", SLEEP_TIME_MS);
	lthread_sleep(SLEEP_TIME_MS);
	check_state(3);

	unsigned long long elapsed_ns = timestamp_now_ns() - start_ns;
	long long diff_ns = (long long)elapsed_ns - (long long)target_ns;
	printf("Returned from lthread_sleep()\n");
	double drift = (diff_ns*100.0)/target_ns;
	if (diff_ns < -SLEEP_DRIFT_ALLOWED_NS || diff_ns > SLEEP_DRIFT_ALLOWED_NS) {
		fprintf(stderr, "WARN: lthread_sleep(%lld) drift %lld (%.3f %%) ",
			(long long)SLEEP_TIME_S, diff_ns, drift);
	}

	join_barrier_reached = 1;
	return NULL;
}

int main() {
	check_state(1);
	pthread_t thread1;
	printf("Creating secondary_sleep thread\n");
	int res = pthread_create(&thread1, NULL, &secondary_sleep, NULL);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_create() returned %d\n", res);
		return res;
	}
	check_state(2);
	printf("Main thread alive, waiting for secondary wakeup\n");
	/*res = pthread_join(thread1, NULL);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_join() returned %d\n", res);
		return res;
	}*/
	while (!join_barrier_reached) {
		printf(".");
		for(volatile int i = 0; i < 100000; i++) { }
		lthread_sleep(0); // yield
	}

	check_state(4);
	return 0;
}

