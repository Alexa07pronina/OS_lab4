#ifndef SHARED_H
#define SHARED_H

#include <stddef.h>
#include <semaphore.h>
#define BUFFER_SIZE 4096

typedef struct {
    sem_t can_write;
    sem_t can_read;
    size_t length;
    int end_of_line;
    int finished;
    char text[BUFFER_SIZE];
} SharedData;

int create_shared(const char* name, SharedData** data);
int open_shared(const char* name, SharedData** data);
int close_shared(SharedData* data);
int destroy_shared(SharedData* data, const char* name);

#endif
