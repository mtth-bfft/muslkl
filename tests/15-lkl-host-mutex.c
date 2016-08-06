#include <stdio.h>
#include <lkl.h>
#include <lkl_host.h>

#define UNUSED(x) (void)(x)

static struct lkl_mutex* mut = NULL;

void secondary(void* arg) {
	UNUSED(arg);
	// Cannot use wait()/sleep() operations
	volatile int i = 0;
	while (i < 10000000L)
		i++;
	lkl_host_ops.mutex_unlock(mut);
}

int main() {
	// Check mutex_alloc
	mut = lkl_host_ops.mutex_alloc();
	if (mut == NULL) {
		fprintf(stderr, "Failed to mutex_alloc\n");
		return 1;
	}

	// Check non-blocking mutex_lock
	lkl_host_ops.mutex_lock(mut);

	// Check blocking mutex_lock and ordering
	lkl_thread_t thread = lkl_host_ops.thread_create(&secondary, NULL);
	if (thread == 0) {
		fprintf(stderr, "Failed to thread_create\n");
		return 2;
	}

	lkl_host_ops.mutex_lock(mut);

	// Check mutex_free
	lkl_host_ops.mutex_free(mut);

	return 0;
}

