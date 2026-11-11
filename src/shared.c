#include "shared.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


static void cleanup_shared(const char* name, int fd, void* ptr, int sem_write_created, int sem_read_created) 
{
    if (sem_read_created) {
        if (sem_destroy(&((SharedData*)ptr)->can_read) == -1)
            perror("sem_destroy can_read");
    }

    if (sem_write_created) {
        if (sem_destroy(&((SharedData*)ptr)->can_write) == -1)
            perror("sem_destroy can_write");
    }

    if (ptr != MAP_FAILED) {
        if (munmap(ptr, sizeof(SharedData)) == -1) perror("munmap");
    }

    if (fd != -1 && (close(fd) == -1)) perror("close");

    if (shm_unlink(name) == -1) perror("shm_unlink");
}


int create_shared(const char* name, SharedData** data) {
    *data = NULL;

    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        cleanup_shared(name, fd, MAP_FAILED, 0, 0);
        return -1;
    }

    void* ptr = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ptr == MAP_FAILED) {
        perror("mmap");
        cleanup_shared(name, fd, MAP_FAILED, 0, 0);
        return -1;
    }

    if (close(fd) == -1) {
        perror("close");
        cleanup_shared(name, -1, ptr, 0, 0);
        return -1;
    }

    SharedData* shared = (SharedData*)ptr;

    if (sem_init(&shared->can_write, 1, 1) == -1) {
        perror("sem_init can_write");
        cleanup_shared(name, -1, ptr, 0, 0);
        return -1;
    }

    if (sem_init(&shared->can_read, 1, 0) == -1) {
        perror("sem_init can_read");
        cleanup_shared(name, -1, ptr, 1, 0);
        return -1;
    }

    shared->length = 0;
    shared->end_of_line = 0;
    shared->finished = 0;

    *data = shared;
    return 0;
}



int open_shared(const char* name, SharedData** data) {
    *data = NULL;

    int fd = shm_open(name, O_RDWR, 0);
    if (fd == -1) {
        perror("shm_open");
        return -1;
    }

    void* ptr = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ptr == MAP_FAILED) {
        perror("mmap");
        if (close(fd) == -1) perror("close");
        return -1;
    }

    if (close(fd) == -1) {
        perror("close");
        if (munmap(ptr, sizeof(SharedData)) == -1) perror("munmap");
        return -1;
    }

    *data = (SharedData*)ptr;
    return 0;
}

int close_shared(SharedData* data) {
    if (munmap(data, sizeof(SharedData)) == -1) {
        perror("munmap");
        return -1;
    }

    return 0;
}

int destroy_shared(SharedData* data, const char* name) {
    int result = 0;

    if (sem_destroy(&data->can_write) == -1) {
        perror("sem_destroy can_write");
        result = -1;
    }

    if (sem_destroy(&data->can_read) == -1) {
        perror("sem_destroy can_read");
        result = -1;
    }

    if (munmap(data, sizeof(SharedData)) == -1) {
        perror("munmap");
        result = -1;
    }

    if (shm_unlink(name) == -1) {
        perror("shm_unlink");
        result = -1;
    }

    return result;
}
