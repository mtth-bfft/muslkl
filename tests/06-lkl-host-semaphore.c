#include <lkl.h>
#include <lkl_host.h>

#define UNUSED(x) (void)(x)

static struct lkl_sem* sem1 = NULL;
static struct lkl_sem* sem2 = NULL;

void secondary(void* arg) {
	UNUSED(arg);
	lkl_host_ops.sem_up(sem1);
	lkl_host_ops.sem_down(sem2);
	lkl_host_ops.thread_exit();
}

int main() {
	// Check sem_alloc
	sem1 = lkl_host_ops.sem_alloc(0);
	sem2 = lkl_host_ops.sem_alloc(0);
	if (sem1 == NULL || sem2 == NULL)
		return 1;

	// Check sem_up and non-blocking sem_down
	lkl_host_ops.sem_up(sem1);
	lkl_host_ops.sem_up(sem1);
	lkl_host_ops.sem_down(sem1);
	lkl_host_ops.sem_down(sem1);

	// Check blocking sem_down and ordering
	lkl_thread_t thread = lkl_host_ops.thread_create(&secondary, NULL);
	if (thread == 0)
		return -1;

	lkl_host_ops.sem_up(sem2);
	lkl_host_ops.sem_down(sem1);

	lkl_host_ops.sem_free(sem1);
	lkl_host_ops.sem_free(sem2);

	return 0;
}

