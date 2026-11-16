#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>


// basic process memory map
void print_memory_map(unsigned long *addr_) {
  FILE *maps = fopen("/proc/self/maps", "r");
  if (maps == NULL) {
    perror("fopen");
    return;
  }

  char line[256];
  // while (fgets(line, sizeof(line), maps)) {
  //   printf("%s", line);
  // }

  ucontext_t context;
  if (getcontext(&context) == -1) {
    perror("getcontext");
    fclose(maps);
    return;

  }

  // print the context information
  printf("Context information:\n");
  printf("  Signal mask: %lu\n", context.uc_sigmask);
  printf("  Stack pointer: %p\n", context.uc_stack.ss_sp);
  printf("  Stack size: %zu\n", context.uc_stack.ss_size);


  fseek(maps, 0, SEEK_SET); // rewind to the beginning of the file
  while (fgets(line, sizeof(line), maps)) {
    unsigned long start, end;
    if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
      printf("Memory region containing address %p: %lx-%lx\n", (void *)addr_, start, end);
      printf("Contents of memory region:\n");
      printf("  Address: %lx\n", (unsigned long)addr_);
      end  = (unsigned long)addr_ + 256 ;
      for (unsigned long addr = (unsigned long)addr_; addr < end; addr += 16) {
        printf("%p: ", (void *)addr);
        unsigned char *ptr = (unsigned char *)addr;
        for (int i = 0; i < 16 && addr + i < end; i++) {
          printf("%02x/%u ", ptr[i], ptr[i]);
        }
        printf("\n");
      }
    }
    break; // only print the first memory region for now, to avoid too much output
  }

  fclose(maps);
}


// void
// signal_handler(int signum) {
//   if (signum == SIGUSR1) {
//     printf("Received SIGUSR1, printing memory map:\n");
//     print_memory_map(addr);
//   }
// }https://github.com/Superb-Man/Checkpoint.git

int 
main(int argc, char *argv[]) {


  unsigned long ara[5] = {1, 2, 3, 4, 5};
  printf("Address of ara: %p\n", (void *)&ara[3]);

  print_memory_map(&ara[3]);

  ara[3] = 42; // modify the value at the address
  printf("Modified value at address %p: %lu\n", (void *)&ara[3], ara[3]);
  print_memory_map(&ara[3]);

  // struct sigaction sa;
  // sa.sa_handler = signal_handler;
  // sigemptyset(&sa.sa_mask);
  // sa.sa_flags = 0;
  // if (sigaction(SIGUSR1, &sa, NULL) == -1) {
  //   perror("sigaction");
  //   return 1;
  // }


  // while (1) {
  //   // continue running until a signal is received
  //   pause();
  // }

  return 0;
}