#include <stdio.h>
#include <unistd.h>


int
main() {
    int cnt  = 0;

    printf("PID is : %d\n", getpid());

    while(1) {
        printf("Count is : %d\n", cnt++);
        sleep(1);
        fflush(stdout);
    }

    return 0;
}