#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

void* worker(void* arg)
{
    long id = (long)arg;
    pid_t pid = getpid();
    pid_t tid = syscall(SYS_gettid);

    printf("[worker %ld] started (PID=%d, TID=%d)\n", id, pid, tid);

    int counter = 0;
    while (1) {
        sleep(0.5);
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    printf("=== NEW BINARY STARTED ===\n");
    printf("PID = %d\n", getpid());

    int num_threads = 4;
    if (argc > 1) {
        num_threads = atoi(argv[1]);
        if (num_threads <= 0) num_threads = 4;
    }

    printf("Creating %d threads...\n", num_threads);

    pthread_t threads[num_threads];

    for (long i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker, (void*)i) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    sleep(0.5);
    int times = 2;
    while (times--) {
        printf("[main] alive...\n");
        sleep(1);
    }

    printf("\n>>> main thread (TID=%d) calling exec [second exec]<<<\n\n", getpid());

    char *args[] = {"/bin/ls", "-l", NULL};
    if (execv("/bin/ls", args) == -1) {
        perror("execv failed");
    }

    return 0;
}