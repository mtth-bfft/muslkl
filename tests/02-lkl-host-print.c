#include <stdio.h>
#include <lkl.h>
#include <lkl_host.h>

int main() {
	lkl_host_ops.print("OK\n", 3);
	return 0;
}

