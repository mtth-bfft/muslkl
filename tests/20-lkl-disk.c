#include <stdio.h>
#include <stdlib.h>
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#define MAXPATH 8192

char* pretty_print_size(unsigned long bytes) {
	static const char* suffixes[] = { "B", "KiB", "MiB", "GiB" };
	static char buffer[32];
	int unit = 0;
	int count = bytes;
	while (count > 0) {
		unit++;
		count /= 1024;
	}
	snprintf(buffer, sizeof(buffer), "%d %s", count, suffixes[unit]);
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
	strncpy(filepath, path, sizeof(filepath));
	char *name = filepath + strlen(filepath);
	int buflen = sizeof(filepath) - strlen(filepath);
	while ((item = readdir(d)) != NULL) {
		char *type = "?";
		//int mode = 0;
		struct stat stat;
		if (strncmp(item->d_name, "..", 2) == 0 || strncmp(item->d_name, ".", 1) == 0)
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
		else if (item->d_type == DT_LNK) {
			type = "l";
		}
		int res = lstat(filepath, &stat);
		if (res != 0) {
			
		}
		printf("%s %ld:%ld %s\t%s\n", type, (long)stat.st_uid,
			(long)stat.st_gid, pretty_print_size(stat.st_size), filepath);
		if (item->d_type == DT_DIR)
			list_dir(filepath, depth+1);
	}
	closedir(d);
}

int main() {
	printf(" [*] Enclave file list:\n");
	list_dir("/", 0);

	printf(" [*] Looking for test.txt\n");
	FILE *f = fopen("/test.txt", "r");
	if (f == NULL) {
		fprintf(stderr, "Error: file /test.txt not found\n");
		perror("fopen()");
		return 2;
	}
	char buffer[3];
	if (fgets(buffer, 1, f) == NULL) {
		fprintf(stderr, "Error: unable to read /test.txt\n");
		perror("fgets()");
		return 3;
	}
	fclose(f);
	return 0;
}
