#include <stdio.h>
#include <stdlib.h>
#include <lkl.h>
#include <lkl_host.h>

#define UNUSED(x) (void)(x)

static long main_thread_tid = 0;

void check_state(int newstate) {
	static int state = 0;
	if (newstate != state && newstate != (state+1)) {
		fprintf(stderr, "State %d reached from state %d\n",
			newstate, state);
		exit(4);
	}
	state = newstate;
}

void secondary_exit(void* arg) {
	check_state(2);
	if (arg != (void*)0xBAADC0DE) {
		fprintf(stderr, "Wrong argument received in secondary thread\n");
		exit(2);
	}

	long tid = lkl_host_ops.gettid();
	if (tid == 0 || tid == main_thread_tid)
		fprintf(stderr, "Warning: child thread has tid %ld, main has %ld ",
			tid, main_thread_tid);

	// Cannot use wait()/sleep() operations
	volatile int i = 0;
	while (i < 10000000L)
		i++;
	check_state(3);

	lkl_host_ops.thread_exit();
}

void secondary_return(void* arg) {
	UNUSED(arg);
	check_state(5);

	// Cannot use wait()/sleep() operations
	volatile int i = 0;
	while (i < 10000000L)
		i++;

	check_state(6);
	return;
}

void secondary_detach(void* arg) {
	UNUSED(arg);
	lkl_host_ops.thread_detach();
	lkl_host_ops.thread_exit();
}

int main() {
	// Check gettid
	main_thread_tid = lkl_host_ops.gettid();

	// Check thread_create
	check_state(1);
	lkl_thread_t thread = lkl_host_ops.thread_create(&secondary_exit,
		(void*)0xBAADC0DE);
	if (thread == 0) {
		fprintf(stderr, "Failed to create thread, returned 0\n");
		return 1;
	}

	// Check thread_exit and thread_join
	check_state(2);
	int res = lkl_host_ops.thread_join(thread);
	if (res != 0) {
		fprintf(stderr, "Failed to join thread, returned %d\n", res);
		return res;
	}
	check_state(4);

	// Check return from thread secondary function
	thread = lkl_host_ops.thread_create(&secondary_return,
		(void*)0xBAADC0DE);
	if (thread == 0) {
		fprintf(stderr, "Failed to create thread, returned 0\n");
		return 1;
	}

	check_state(5);
	res = lkl_host_ops.thread_join(thread);
	if (res != 0) {
		fprintf(stderr, "Failed to join thread, returned %d\n", res);
		return res;
	}
	check_state(7);

	// Check thread_detach
	thread = lkl_host_ops.thread_create(&secondary_detach,
		(void*)0xBAADC0DE);
	if (thread == 0) {
		fprintf(stderr, "Failed to create thread, returned 0\n");
		return 1;
	}

	return 0;
}

