#include <lkl.h>
#include <lkl_host.h>

// Checks that the allocated area is R/W
void check(volatile char* area, unsigned long size) {
	unsigned long i;
	for(i = 0; i < size; i++)
		*(area + i) = (char)i;
}

int main() {
	unsigned long size;
	for(size = 10; size <= 1024*1024*10; size *= 10) {
		void* test = lkl_host_ops.mem_alloc(size);
		if (test == NULL)
			return (int)size;
		check(test, size);
		lkl_host_ops.mem_free(test);
	}
	return 0;
}

