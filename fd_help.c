#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main () {
	int fd = open("magic_mirror", O_RDWR);

	if (fd == -1) {
		perror("open");
		return -1;
	}

	printf("File descriptor for magic mirror: %d\n", fd); 

	if (dup2(fd, 99) == -1) {
		perror("dup2");
		return -2;
	}

	printf("dp run succesfully! new fd: %d\n", fd);

	close(fd);
	
	if (fcntl(99, F_GETFD) == -1) {
        perror("fcntl");
        close(99);  // Close FD 99 if it is somehow invalid
        return -3;
    }

	execve("./riddle", NULL, NULL);

	return 0;
}

		
