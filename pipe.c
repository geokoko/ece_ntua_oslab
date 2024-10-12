#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main () {
	int fd = open("magic_mirror", O_RDWR);

	if (fd == -1) {
		perror("open");
		return -1;
	}

	if (dup2(fd, 33) == -1 || dup2(fd, 34) == -1) {
		perror("dup2");
		return -1;
	}

	execve("./riddle", NULL, NULL);
}
