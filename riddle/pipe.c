#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
	int original_fd = open("magic_mirror", O_RDWR);


	if (original_fd == -1) {
		perror("open");	
		return 1;
	}

	if (dup2(original_fd, 33) == -1) {
		perror("dup2");
		return 1;
	}
	if (dup2(original_fd, 34) == -1) {
		perror("dup2");
		return 1;
	}

	if (dup2(original_fd, 54) == -1) {
		perror("dup2");
		return 1;
	}

	if (dup2(original_fd, 53) == -1) {
		perror("dup2");
		return 1;
	}

	execve("./riddle", NULL, NULL);

}


