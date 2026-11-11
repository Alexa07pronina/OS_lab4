#include "shared.h"
#include "filter.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int wait_for_data(sem_t* sem) {
    while (sem_wait(sem) == -1) {
        if (errno != EINTR) {
            perror("sem_wait");
            return -1;
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: child SHM_NAME OUTPUT_FILE\n");
        return 1;
    }

    const char* shared_name = argv[1];
    const char* file_name = argv[2];
    int child_number = atoi(argv[3]);

    int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("dup2");

        if (close(fd) == -1)
            perror("close");

        return 1;
    }

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    SharedData* data = NULL;

    if (open_shared(shared_name, &data) == -1)
        return 1;
    
    fprintf(stderr, "[Child %d] PID: %ld, started\n",child_number, (long)getpid());

    int result = 0;

    while (1) {
        if (wait_for_data(&data->can_read) == -1) {
            result = 1;
            break;
        }

        if (data->finished) {
            if (sem_post(&data->can_write) == -1) {
                perror("sem_post");
                result = 1;
            }
            break;
        }

        if (data->length > BUFFER_SIZE) {
            fprintf(stderr, "Invalid message length\n");
            result = 1;
            break;
        }

        size_t length = remove_vowels(data->text, data->length);

        if (length > 0) {
            if (fwrite(data->text, 1, length, stdout) != length) {
                perror("fwrite");
                result = 1;
                break;
            }
        }

        if (data->end_of_line) {
            if (fputc('\n', stdout) == EOF) {
                perror("fputc");
                result = 1;
                break;
            }
        }

        if (sem_post(&data->can_write) == -1) {
            perror("sem_post");
            result = 1;
            break;
        }
    }

    if (fflush(stdout) == EOF) {
        perror("fflush");
        result = 1;
    }

    if (close_shared(data) == -1)
        result = 1;

    fprintf(stderr, "[Child %d] PID: %ld, finished\n",child_number, (long)getpid());

    return result;
}
