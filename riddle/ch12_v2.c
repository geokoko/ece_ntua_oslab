#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

int main (int argc, char** argv) {
	if (argc != 4) {
		printf("Usage: ./ch12_ <memory_address> <char> <filename>");
		return -1;
	}
	char* file = (char*) argv[3];
	printf("%s\n", file);
	int fd = open(file, O_RDWR, 0600);
	
	if (fd == -1) {
        perror("Error opening file");
        return -1;
    }

	off_t offset = strtoul(argv[1], NULL, 16);

	if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("Error seeking file");
        close(fd);
        return -1;
    }

	if (write(fd, argv[2], 1) != 1) {
        perror("Error writing to file");
        close(fd);
        return -1;
    }

	close(fd);

	return 0;
}
