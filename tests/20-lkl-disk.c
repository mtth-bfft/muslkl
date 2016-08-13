#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#define MAXPATH 100

char* pretty_print_size(unsigned long bytes)
{
	static const char* suffixes[] = { "B", "KiB", "MiB", "GiB" };
	static char buffer[32];
	int unit = 0;
	double count = bytes;
	while (count > 1024 && unit < 3) {
		unit++;
		count /= 1024.0;
	}
	snprintf(buffer, sizeof(buffer), "%.1f%s", count, suffixes[unit]);
	return buffer;
}

char* pretty_print_mode(unsigned int m)
{
	static char buffer[32] = {0};
	strncpy(buffer, "---------", sizeof(buffer));
	if (m & S_IRUSR) buffer[0] = 'r';
	if (m & S_IWUSR) buffer[1] = 'w';
	if (m & S_IXUSR) buffer[2] = 'x';
	if (m & S_IRGRP) buffer[3] = 'r';
	if (m & S_IWGRP) buffer[4] = 'w';
	if (m & S_IXGRP) buffer[5] = 'x';
	if (m & S_IROTH) buffer[6] = 'r';
	if (m & S_IWOTH) buffer[7] = 'w';
	if (m & S_IXOTH) buffer[8] = 'x';
	return buffer;
}

void list_dir(const char* path, int depth)
{
	char dirpath[MAXPATH] = {0};
	strncpy(dirpath, path, sizeof(dirpath));
	if (dirpath[strlen(dirpath)-1] != '/')
		strncat(dirpath, "/", sizeof(dirpath));
	DIR *d = opendir(dirpath);
	if (d == NULL) {
		fprintf(stderr, "Error: unable to open dir %s\n", dirpath);
		perror("opendir()");
		exit(1);
	}
	struct dirent *item;
	char filepath[MAXPATH] = {0};
	strncpy(filepath, dirpath, sizeof(filepath));
	char *name = filepath + strlen(filepath);
	int buflen = sizeof(filepath) - strlen(filepath);
	while ((item = readdir(d)) != NULL) {
		char *type = "?";
		if (strncmp(item->d_name, "..", 2) == 0 ||
			strncmp(item->d_name, ".", 1) == 0)
			continue;
		strncpy(name, item->d_name, buflen);

		if (item->d_type == DT_REG)
			type = "-";
		else if (item->d_type == DT_FIFO)
			type = "F";
		else if (item->d_type == DT_SOCK)
			type = "S";
		else if (item->d_type == DT_CHR)
			type = "c";
		else if (item->d_type == DT_BLK)
			type = "b";
		else if (item->d_type == DT_DIR)
			type = "d";
		else if (item->d_type == DT_LNK)
			type = "l";

		struct stat stat;
		int res = lstat(filepath, &stat);
		if (res == 0) {
			printf("%s%s %ld:%ld %s\t%s\n", type,
				pretty_print_mode(stat.st_mode),
				(long)stat.st_uid, (long)stat.st_gid,
				pretty_print_size(stat.st_size), filepath);
			if (item->d_type == DT_DIR &&
				strncmp("/sys", filepath, 4) != 0)
				list_dir(filepath, depth+1);
		} else {
			printf("%s????????? ?:? %s\t%s\n", type,
				pretty_print_size(stat.st_size), filepath);
		}
	}
	closedir(d);
}

int main() {
	printf(" [*] Enclave file list:\n");
	list_dir("/", 0);

	printf(" [*] Reading test.txt using stdio\n");
	FILE *f = fopen("/test.txt", "r");
	if (f == NULL) {
		fprintf(stderr, "Error: file /test.txt not found\n");
		perror("fopen()");
		return 2;
	}
	char buffer[20] = {0};
	char *res2 = fgets(buffer, sizeof(buffer), f);
	if (res2 == NULL) {
		fprintf(stderr, "Error: fgets(/test.txt) returned NULL\n");
		perror("fgets()");
		return -1;
	}
	if (strcmp("Hello, World!\n", buffer) != 0)
		fprintf(stderr, "WARN: read wrong string from /test.txt\n");
	printf(" [*] Read: '%s'\n", buffer);
	int ret = fclose(f);
	if (ret != 0) {
		fprintf(stderr, "Error: close(/test.txt) returned %d\n", ret);
		perror("fclose()");
		return ret;
	}
	return 0;
}
