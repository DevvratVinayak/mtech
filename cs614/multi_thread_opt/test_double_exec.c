#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <syscall.h>

int set_tracked_pid(int pid)
{
    char buf[16];
    int fd = open("/sys/kernel/cs614hook/tracked_pid", O_RDWR);
    if (fd < 0) {
        perror("open");
        return fd;
    }

    sprintf(buf, "%d", pid);

    if (write(fd, buf, strlen(buf)) < 0) {
        perror("write");
        close(fd);
        return -1;
    }

    printf("process %d is being tracked now\n", pid);
    close(fd);
    return 0;
}

int set_optimization_on(int flag)
{
    char buf[16];
    int fd = open("/sys/kernel/cs614hook/optimization_on", O_RDWR);
    if (fd < 0) {
        perror("open");
        return fd;
    }

    sprintf(buf, "%d", flag);

    if (write(fd, buf, strlen(buf)) < 0) {
        perror("write");
        close(fd);
        return -1;
    }

    printf("optimization is: %d\n", flag);
    close(fd);
    return 0;
}

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* blocked_thread(void* arg)
{
    long id = (long)arg;
    printf("thread %ld: blocked on mutex/condition started (PID=%d, TID=%ld)\n", 
           id, getpid(), syscall(SYS_gettid));

    pthread_mutex_lock(&lock);
    while (1) {
        pthread_cond_wait(&cond, &lock);
    }
    pthread_mutex_unlock(&lock);

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("usage: %s <num_blocked_threads> <optimization_on>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    int opt_flag = atoi(argv[2]);

    if (num_threads < 1 || num_threads > 1000) {
        printf("error: num_threads must be between 1 and 1000\n");
        return 1;
    }

    assert(set_tracked_pid(getpid()) == 0);
    assert(set_optimization_on(opt_flag) == 0);

    printf("\n=== BLOCKED THREADS & DOUBLE EXEC TEST ===\n");
    printf("  PID               = %d\n", getpid());
    printf("  optimization      = %d\n", opt_flag);
    printf("  blocked threads   = %d\n", num_threads);
    printf("==================================\n\n");

    pthread_t threads[num_threads];

    printf("creating %d blocked threads\n", num_threads);

    for (long i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, blocked_thread, (void*)i) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    sleep(1); 

    char *args[] = {NULL};

    printf("\n>>> main thread (TID=%d) calling exec [first exec]<<<\n\n", getpid());
    if (execv("./new_binary", args) == -1) {
        perror("execv failed");
    }

    return 0;
}
