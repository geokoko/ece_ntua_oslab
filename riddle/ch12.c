#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
int main (int argc, char** argv) {
	if (argc != 3) {
		printf("Usage: ./ch12 <memory_address> <target_char>");
		return -1;
	}

	size_t map_size = 4096;
	char target_char = argv[2][0];
	
	unsigned long addr = strtoul(argv[1], NULL, 0);
    char* target_mapped_addr = (char*)addr;

	*target_mapped_addr = target_char;

	if (msync((void*)argv[1], 4096, MS_SYNC) == -1)
		perror("msync");

	return 0;
}
