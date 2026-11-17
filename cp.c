#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ucontext.h>
#include <signal.h>

// Global contructor that gets called even before main

void checkpoint();
static volatile sig_atomic_t flag = 0;

void 
signal_handler(int signum) {
    (void)signum;
    checkpoint();
}
__attribute__((constructor))
void myconstructor() {
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR2, &sa, NULL) == -1)
        exit(1);
    }

struct memory_region {
    void *start_addr;
    void *end_addr;
    int  is_r;
    int  is_w;
    int  is_x;
};


void
print_process_memory() {
    FILE *ifp = fopen("/proc/self/maps", "r");
    if (ifp == NULL) {
        printf("Error opening memory mapping file: %s\n", strerror(errno));
        exit(1);
    }

    size_t cap = 0;
    char* line = NULL;
    while (getline(&line, &cap, ifp) != -1) {
        printf("%s", line);
    }

    free(line);
    fclose(ifp);

}

int 
parse_mem(char* line, struct memory_region* mr) {
    char *ep;
    printf("line is %s\n", line);
    unsigned long long start = strtoull(line, &ep, 16);
    printf("start_address is %p\n", (void*)start);
    if (ep == line || *ep != '-')
        return 0;
    
    // ep is on '-'
    // Now start of end address, advance to 1 byte from ep
    const char *s2 = ep + 1;

    unsigned long long end = strtoull(s2, &ep, 16);
    if (ep == s2 || *ep != ' ')
        return 0;

    const char *perms = ep + 1;              /* e.g. "r-xp" */
    if (strlen(perms) < 4)
        return 0;

    mr->start_addr    = (void *)start;
    mr->end_addr      = (void *)end;
    mr->is_r          = (perms[0] == 'r');
    mr->is_w          = (perms[1] == 'w');
    mr->is_x          = (perms[2] == 'x');
    
    return 1;
}

void 
checkpoint() {
    // print_process_memory();
    FILE *ifp = NULL;
    FILE *ofp = NULL;
    ifp = fopen("/proc/self/maps", "r");
    ofp = fopen("myckpt", "wb");

    if (ifp == NULL) {
        printf("Error opening memory mapping file: %s\n", strerror(errno));
        exit(1);
    }
    if (ofp == NULL) {
        printf("Error opening output file: %s\n", strerror(errno));
        exit(1);
    }

    ucontext_t context;

    if(getcontext(&context) == -1) {
        printf("Error in getting context: %s\n", strerror(errno));
        exit(1);
    }

    if (flag == 1) {
        return;
    } else {
        flag++;
    }

    fwrite(&context, sizeof(ucontext_t), 1, ofp);
    
    char* line = NULL;
    size_t cap = 0;

    while (getline(&line, &cap, ifp) != -1) {
        
        if (strstr(line, "[vsyscall]") || strstr(line, "[vvar]")
            || strstr(line, "[vvar_sigpage]") || strstr(line, "[vdso]") || strstr(line, "[vvar_vclock]"))
            continue;
        
        struct memory_region mr;
        
        // parse the memory here

        if (!parse_mem(line, &mr))
            continue;

        if (!mr.is_r) // memory region not readable
            continue;
        
        
        // size of the memeory region
        size_t size = (size_t) (mr.end_addr - mr.start_addr);

        // write the bytes into the image file
        if (fwrite(&mr, sizeof(mr), 1, ofp) != 1 || fwrite(mr.start_addr, 1, size, ofp) != size) {
            printf("Error in memory writing: %s\n", strerror(errno));
            fprintf(stderr, " Write failed error no = %d (%s)\n region %p - %p and size = %zu\n maps line : %s\n",
                                errno, strerror(errno), mr.start_addr, mr.end_addr, size, line);
            exit(1);
        }
        
    }

    // free(line);
    fclose(ifp);
    fclose(ofp);

    return;
    
}

// int 
// main() {
//     print_process_memory();
// }