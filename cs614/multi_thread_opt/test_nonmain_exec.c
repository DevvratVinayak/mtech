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

void* worker_thread(void* arg)
{
    long id = (long)arg;
    printf("worker thread %ld started (TID=%ld)\n", id, syscall(SYS_gettid));
    
    volatile long counter = 0;
    while (1) {
        counter++;
    }
    return NULL;
}

void* exec_thread(void* arg)
{
    int num_workers = *((int*)arg);
    
    printf("exec thread started (TID=%ld)\n", syscall(SYS_gettid));
    
    sleep(0.5);
    
    char *args[] = {"/bin/ls", "-l", NULL};
    
    printf("\n>>> NON-MAIN thread (TID=%ld) calling exec with %d worker threads <<<\n\n", 
           syscall(SYS_gettid), num_workers);
    
    if (execv("/bin/ls", args) == -1) {
        perror("execv failed");
    }
    
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("usage: %s <num_worker_threads> <optimization_on>\n", argv[0]);
        return 1;
    }

    int num_workers = atoi(argv[1]);
    int opt_flag = atoi(argv[2]);

    if (num_workers < 1 || num_workers > 1000) {
        printf("error: num_worker_threads must be between 1 and 1000\n");
        return 1;
    }

    assert(set_tracked_pid(getpid()) == 0);
    assert(set_optimization_on(opt_flag) == 0);

    printf("\n=== EXEC FROM NON-MAIN THREAD TEST ===\n");
    printf("  PID               = %d\n", getpid());
    printf("  Main TID          = %ld\n", syscall(SYS_gettid));
    printf("  optimization      = %d\n", opt_flag);
    printf("  worker threads    = %d\n", num_workers);
    printf("=======================================\n\n");

    pthread_t workers[num_workers];
    pthread_t exec_tid;

    printf("creating %d worker threads\n", num_workers);
    for (long i = 0; i < num_workers; i++) {
        if (pthread_create(&workers[i], NULL, worker_thread, (void*)i) != 0) {
            perror("pthread_create worker");
            exit(EXIT_FAILURE);
        }
    }

    sleep(1);

    printf("creating exec thread\n");
    if (pthread_create(&exec_tid, NULL, exec_thread, &num_workers) != 0) {
        perror("pthread_create exec");
        exit(EXIT_FAILURE);
    }

    printf("main thread (TID=%ld) now joining worker pool\n", syscall(SYS_gettid));
    sleep(100);
    return 0;
}
