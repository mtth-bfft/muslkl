#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>

#define ARGVAL ((void*)0xDEADC0DE)
#define RETVAL ((void*)0xBAADC0DE)
#define INVALID_RETVAL ((void*)0xDEADDEAD)
#define UNUSED(x) (void)(x)

static pthread_t main_thread_tid = 0;
static volatile int busy_wait_join_detached = 0;
static volatile int canceled_but_still_running = 0;

void check_state(int newstate) {
	static int state = 0;
	if (newstate != state && newstate != (state+1)) {
		fprintf(stderr, "State %d reached from state %d\n",
			newstate, state);
		exit(4);
	}
	state = newstate;
}

void* secondary_exit(void *arg) {
	check_state(2);
	if (arg != ARGVAL) {
		fprintf(stderr, "Wrong argument %p received in thread\n", arg);
		exit(2);
	}

	pthread_t tid = pthread_self();
	if (tid == 0 || tid == main_thread_tid)
		fprintf(stderr, "WARN: child thread has tid %lld, main has %lld ",
			(unsigned long long)tid,
			(unsigned long long)main_thread_tid);

	// Cannot use wait()/sleep() operations
	volatile int i = 0;
	while (i < 10000000L)
		i++;
	printf("Secondary exit thread alive\n");
	check_state(3);

	pthread_exit(RETVAL);
	// Should never be executed
	return INVALID_RETVAL;
}

void* secondary_return(void *arg) {
	UNUSED(arg);
	check_state(5);

	// Cannot use wait()/sleep() operations
	volatile int i = 0;
	while (i < 10000000L)
		i++;
	printf("Secondary return thread alive\n");

	check_state(6);
	return RETVAL;
}

void* secondary_detach(void *arg) {
	UNUSED(arg);
	pthread_t tid = pthread_self();
	if (tid == 0) {
		fprintf(stderr, "Error: pthread_self() returned 0\n");
		exit(1);
	}
	int res = pthread_detach(tid);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_detach(%lld) returned %d\n",
			(unsigned long long)tid, res);
		exit(res);
	}

	// Signal the main thread that this test has succeeded and it can move on
	busy_wait_join_detached = 1;
	printf("Secondary detached thread alive\n");
	pthread_exit(RETVAL);
	// Should never be executed
	return INVALID_RETVAL;
}

void* secondary_cancel(void *arg) {
	UNUSED(arg);
	usleep(1000*1000);
	canceled_but_still_running = 1;
	return RETVAL;
}

int main() {
	// Check pthread_self()
	main_thread_tid = pthread_self();

	// Check pthread_create()
	check_state(1);
	pthread_t thread1;
	printf("Creating secondary_exit thread\n");
	int res = pthread_create(&thread1, NULL, &secondary_exit, ARGVAL);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_create() returned %d\n", res);
		return res;
	}
	printf("Main survived\n");

	// Check pthread_exit() and pthread_join()
	check_state(2);
	void *returned = NULL;
	res = pthread_join(thread1, &returned);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_join() returned %d\n", res);
		return res;
	}
	if (returned != RETVAL)
		fprintf(stderr, "WARN: wrong pthread_join() return %p ", returned);
	check_state(4);

	// Check return from thread secondary function
	pthread_t thread2;
	res = pthread_create(&thread2, NULL, &secondary_return, ARGVAL);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_create() returned %d\n", res);
		return res;
	}

	check_state(5);
	returned = NULL;
	res = pthread_join(thread2, &returned);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_join() returned %d\n", res);
		return res;
	}
	check_state(7);

	// Check pthread_detach()
	pthread_t thread3;
	res = pthread_create(&thread3, NULL, &secondary_detach, ARGVAL);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_create() returned %d\n", res);
		return res;
	}
	// Emulate a pthread_join on detached thread
	while (!busy_wait_join_detached) {
		printf(".");
	}
	printf("\n");

	// Check pthread_cancel() + pthread_join()
	pthread_t thread4;
	res = pthread_create(&thread4, NULL, &secondary_cancel, ARGVAL);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_create() returned %d\n", res);
		return res;
	}
	usleep(1000);
	res = pthread_cancel(thread4);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_cancel() returned %d\n", res);
		return res;
	}
	returned = NULL;
	res = pthread_join(thread4, &returned);
	if (res != 0) {
		fprintf(stderr, "Error: pthread_join(canceled) returned %d\n",
			res);
		return res;
	}
	if (returned != PTHREAD_CANCELED)
		fprintf(stderr, "WARN: returned %p, not PTHREAD_CANCELED ",
			returned);

	return 0;
}

