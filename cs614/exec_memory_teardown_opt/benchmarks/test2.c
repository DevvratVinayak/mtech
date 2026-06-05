#include <stdio.h>
#include <unistd.h>
#include<string.h>
#include <fcntl.h>
#include <sched.h>


static char bss_data[100 * 1024 * 1024];   /* 100 MB BSS */



/* Touch pages to force page table entries */
static void touch_pages(char *buf, size_t size)
{
    size_t i;
    for (i = 0; i < size; i += 4096)
        buf[i] = 0x01;
}



int main() {

        int fd;
        char pid_str[20];

        printf("Touching BSS pages to create PTEs for test2...\n");

        /* Touch all pages - forces kernel to create page table entries */
        touch_pages(bss_data, sizeof(bss_data));
//        touch_pages(data_section, sizeof(data_section));

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


//      char *args[] = {"ls", "-l", NULL};
 //      execv("/usr/bin/ls", args);
         printf("This will never print\n");

        return 0;
}
