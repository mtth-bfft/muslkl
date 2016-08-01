#include <stdio.h>
#include <stdlib.h>
#include <lkl.h>
#include <lkl_host.h>

#define UNUSED(x) (void)(x)

#define VAL1_1 ((void*)0xCAFE0011)
#define VAL2_1 ((void*)0xCAFE0021)
#define VAL1_2 ((void*)0xCAFE0012)
#define VAL2_2 ((void*)0xCAFE0022)

static unsigned int key1 = 0;
static unsigned int key2 = 0;

void secondary(void* arg) {
	UNUSED(arg);
	lkl_host_ops.tls_set(key1, VAL1_2);
	do {
		arg = lkl_host_ops.tls_get(key2);
		if (arg == NULL) {
			fprintf(stderr, "Second tls_get(%d) returned NULL\n", key2);
			exit(6);
		}
	} while (arg != VAL2_2);
	lkl_host_ops.tls_set(key1, VAL1_1);
}

int main() {
	// Check TLS alloc
	int res = lkl_host_ops.tls_alloc(&key1);
	if (res != 0) {
		fprintf(stderr, "Failed first tls_alloc, returned %d\n", res);
		return res;
	}
	if (key1 == 0)
		fprintf(stderr, "Warning: null TLS first key ");
	res = lkl_host_ops.tls_alloc(&key2);
	if (res != 0) {
		fprintf(stderr, "Failed second tls_alloc, returned %d\n", res);
		return res;
	}
	if (key2 == 0)
		fprintf(stderr, "Warning: TLS keys %d and %d ", key1, key2);

	// Check tls_set and tls_get in the same thread
	res = lkl_host_ops.tls_set(key1, VAL1_1);
	if (res != 0) {
		fprintf(stderr, "First tls_set failed, returned %d\n", res);
		return res;
	}
	void *val = lkl_host_ops.tls_get(key1);
	if (val != VAL1_1) {
		fprintf(stderr, "Incorrect first tls_get result: %p\n", val);
		return 1;
	}
	res = lkl_host_ops.tls_set(key2, VAL2_1);
	if (res != 0) {
		fprintf(stderr, "Second tls_set failed, returned %d\n", res);
		return res;
	}
	val = lkl_host_ops.tls_get(key2);
	if (val != VAL2_1) {
		fprintf(stderr, "Incorrect second tls_get result: %p\n", val);
		return 2;
	}

	// Check tls_get/set in multiple threads
	lkl_thread_t thread = lkl_host_ops.thread_create(&secondary, NULL);
	if (thread == 0) {
		fprintf(stderr, "Failed to thread_create\n");
		return 3;
	}

	do {
		val = lkl_host_ops.tls_get(key1);
		if (val == NULL) {
			fprintf(stderr, "First tls_get(%d) returned NULL\n", key1);
			return 4;
		}
	} while (val != VAL1_2);

	res = lkl_host_ops.tls_set(key2, VAL2_2);
	if (res != 0) {
		fprintf(stderr, "Second tls_set failed, returned %d\n", res);
		return res;
	}

	do {
		val = lkl_host_ops.tls_get(key1);
		if (val == NULL) {
			fprintf(stderr, "First tls_get(%d) returned NULL\n", key1);
			return 5;
		}
	} while (val != VAL1_1);

	// Check tls_free
	res = lkl_host_ops.tls_free(key1);
	if (res != 0) {
		fprintf(stderr, "First tls_free failed, returned %d\n", res);
		return res;
	}
	res = lkl_host_ops.tls_free(key2);
	if (res != 0) {
		fprintf(stderr, "Second tls_free failed, returned %d\n", res);
		return res;
	}

	return 0;
}

