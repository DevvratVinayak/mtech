#include <stdio.h>
#include <unistd.h>
#include<string.h>
#include <fcntl.h>
#include <sched.h>


int main() {

        int fd;
        char pid_str[20];
        snprintf(pid_str,sizeof(pid_str),"%d",getpid());
        printf("My PID=%s\n",pid_str);
        fd=open("/sys/kernel/time/target_pid",O_WRONLY);
        if(fd<0){
                perror("open sysfs failed");
                return 1;
        }

        if (write(fd, pid_str, strlen(pid_str)) < 0) {
        perror("write sysfs failed");
        close(fd);
        return 1;
       }

        close(fd);
        printf("PID set in sysfs — kprobe active!\n");
        printf("Before exec — PID = %d\n", getpid());
        printf("Loading new ELF...\n");
//        sleep(100);

       char *args[] = {"/bin/ls", NULL};
       execv("/bin/ls", args);
        printf("This will never print\n");

        return 0;
}
