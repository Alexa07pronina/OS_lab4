#include "shared.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char name[64];
    SharedData* data;
    pid_t pid;
    int reaped;
    int status;
} ChildInfo;

static int send_message(ChildInfo* child, const char* text, size_t length, int end_of_line, int finished) {
    if (length > BUFFER_SIZE)
        return -1;

    while (1) {
        struct timespec deadline;

        if (clock_gettime(CLOCK_REALTIME, &deadline) == -1) {
            perror("clock_gettime");
            return -1;
        }

        deadline.tv_sec++;

        if (sem_timedwait(&child->data->can_write, &deadline) == 0) break;

        if (errno != EINTR && errno != ETIMEDOUT) {
            perror("sem_timedwait");
            return -1;
        }

        int status;
        pid_t result = waitpid(child->pid, &status, WNOHANG);

        if (result == -1) {
            perror("waitpid");
            return -1;
        }

        if (result == child->pid) {
            child->reaped = 1;
            child->status = status;
            fprintf(stderr, "Child %ld exited unexpectedly\n", (long)child->pid);
            return -1;
        }
    }

    if (length > 0) memcpy(child->data->text, text, length);

    child->data->length = length;
    child->data->end_of_line = end_of_line;
    child->data->finished = finished;

    if (sem_post(&child->data->can_read) == -1) {
        perror("sem_post");
        return -1;
    }

    return 0;
}

static int send_line(ChildInfo* child, const char* line, size_t length) {
    size_t offset = 0;

    do {
        size_t part = length - offset;
        if (part > BUFFER_SIZE) part = BUFFER_SIZE;

        int last = (offset + part == length);
        if (send_message(child, line + offset, part, last, 0) == -1)
            return -1;
        offset += part;

    } while (offset < length);

    return 0;
}

static int start_child(ChildInfo* child, const char* file_name, int child_number) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        char number[12];

        snprintf(number, sizeof(number), "%d", child_number);

        char* args[] = {
            "./child", child->name,
            (char*)file_name, number, NULL};

        execvp(args[0], args);

        perror("execvp");
        _exit(127);
    }

    child->pid = pid;
    printf("Child PID: %ld\n", (long)pid);

    return 0;
}

int main(void) {
    ChildInfo children[2] = {0};
    char* file_names[2] = {NULL, NULL};
    char* line = NULL;
    size_t capacity = 0;
    int success = 1;

    for (int i = 0; i < 2; i++) {
        printf("Output file for child %d: ", i + 1);

        if (fflush(stdout) == EOF) {
            perror("fflush");
            success = 0;
            break;
        }

        ssize_t length = getline(&line, &capacity, stdin);

        if (length == -1) {
            if (ferror(stdin)) perror("getline");
            else  fprintf(stderr, "Unexpected end of input\n");
            success = 0;
            break;
        }

        if (length > 0 && line[length - 1] == '\n')
            line[--length] = '\0';

        if (length == 0) {
            fprintf(stderr, "Empty filename\n");
            success = 0;
            break;
        }

        file_names[i] = strdup(line);

        if (file_names[i] == NULL) {
            perror("strdup");
            success = 0;
            break;
        }
    }

    if (success && strcmp(file_names[0], file_names[1]) == 0) {
        fprintf(stderr, "Output filenames must differ\n");
        success = 0;
    }

    if (success) {
        printf("Parent PID: %ld\n", (long)getpid());

        for (int i = 0; i < 2; i++) {
            int count = snprintf(children[i].name, sizeof(children[i].name),
                                 "/lab4_%ld_%d", (long)getpid(), i + 1);

            if (count < 0 || (size_t)count >= sizeof(children[i].name)) {
                fprintf(stderr, "Shared memory name too long\n");
                success = 0;
                break;
            }

            if (create_shared(children[i].name, &children[i].data) == -1) {
                success = 0;
                break;
            }

            if (start_child(&children[i], file_names[i], i+1) == -1) {
                success = 0;
                break;
            }
        }
    }

    if (success) {
        printf("Enter lines (Ctrl+D to finish):\n");

        size_t line_number = 0;

        while (1) {
            ssize_t length = getline(&line, &capacity, stdin);

            if (length == -1) {
                if (feof(stdin)) break;
                perror("getline");
                success = 0;
                break;
            }

            if (length > 0 && line[length - 1] == '\n') length--;

            ChildInfo* child = &children[line_number % 2];

            if (send_line(child, line, (size_t)length) == -1) {
                success = 0;
                break;
            }

            line_number++;
        }
    }

    if (success) {
        for (int i = 0; i < 2; i++) {
            if (send_message(&children[i], NULL, 0, 0, 1) == -1) {
                success = 0;
                break;
            }
        }
    }

    if (!success) {
        for (int i = 0; i < 2; i++) {
            if (children[i].pid > 0 && !children[i].reaped) {
                if (kill(children[i].pid, SIGTERM) == -1 && errno != ESRCH)
                    perror("kill");
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        if (children[i].pid > 0 && !children[i].reaped) {
            int status;
            pid_t result;

            do {
                result = waitpid(children[i].pid, &status, 0);
            } while (result == -1 && errno == EINTR);

            if (result == -1) {
                perror("waitpid");
                success = 0;
            }
            else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                fprintf(stderr, "Child %ld failed\n", (long)children[i].pid);
                success = 0;
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        if (children[i].data != NULL) {
            if (destroy_shared(children[i].data, children[i].name) == -1)
                success = 0;
        }
        free(file_names[i]);
    }

    free(line);
    return success ? 0 : 1;
}
