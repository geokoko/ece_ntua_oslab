#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    pid_t desired_pid = 32767;  // Replace this with the desired PID you want to achieve
    pid_t pid;

    while (1) {
        pid = fork(); // Fork the process

        if (pid < 0) {
            // Fork failed
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            // This block is executed in the child process
            if (getpid() == desired_pid) {
                printf("Child process with desired PID (%d) found!\n", getpid());
                execve("./riddle",NULL,NULL);
                exit(0); // Exit the child process
            } else {
                // Continue forking in the child
                exit(0);
            }
        }
	}

    return 0;
}
