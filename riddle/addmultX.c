#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main () {
        for (int i = 0; i < 10; i++) {
                char filename[30];

                sprintf(filename, "bf%02d", i);
                int fd = open(filename, O_WRONLY | O_CREAT, 0644);

                if (fd == -1) {
                        perror("Error opening file");
                        return -1;
                }

                off_t offset = 1073741824;

                if (lseek(fd, offset, SEEK_SET) == -1) {
                        perror("lseek");
                        close(fd);
                        return -1;
                }

                dprintf(fd, "X");
                close(fd);
        }

        execve("./riddle", NULL, NULL);

        return 0;
}


