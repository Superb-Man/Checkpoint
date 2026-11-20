#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <asm/prctl.h>
#include <unistd.h>


char checkpoint_image[1000];


struct memory_region {
    void* start_addr;
    void* end_addr;
    int is_r;
    int is_w;
    int is_x;
};

void*
get_temp_stack() {
    void* stack = mmap(
        (void *)0x1000000,
        1<<20,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
        -1,
        0
    );

    if (stack == MAP_FAILED) {
        perror("temporary stack mmap failed");
        exit(1);
    }

    return stack;
}

void*
find_temp_stack(const char* filename, size_t stack_size) {
    FILE *image = fopen(filename, "rb");
    if (image == NULL) {
        perror("fopen");
        exit(1);
    }

    // Skip fs_base and saved context; scan only mr headers.
    if (fseeko(image, (off_t)(sizeof(unsigned long) + sizeof(ucontext_t)), SEEK_SET) != 0) {
        perror("fseeko");
        fclose(image);
        exit(1);
    }

    struct memory_region mr;
    uintptr_t prev_end = 0;

    while (fread(&mr, sizeof(mr), 1, image) == 1) {
        uintptr_t start = (uintptr_t)mr.start_addr;
        uintptr_t end = (uintptr_t)mr.end_addr;

        if (end <= start || (prev_end && start < prev_end)) {
            fprintf(stderr, "Invalid or unsorted checkpoint mrs\n");
            fclose(image);
            exit(1);
        }

        if (prev_end && start - prev_end >= stack_size) {
            void* candidate = (void *)prev_end;
            void* stack = mmap(candidate, 
                               stack_size,
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                               -1, 0
                            );

            if (stack == candidate) {
                fclose(image);
                return stack;
            }

            if (stack != MAP_FAILED)
                munmap(stack, stack_size);
        }

        prev_end = end;
        size_t size = end - start;
        if (size > INT64_MAX || fseeko(image, (off_t)size, SEEK_CUR) != 0) { /*to avoid reading contents*/
            perror("fseeko mr data");
            fclose(image);
            exit(1);
        }
    }

    if (ferror(image))
        perror("fread mr metadata");
    else
        fprintf(stderr, "No usable temporary-stack gap found\n");

    fclose(image);
    exit(1);
}

void
restore(char* filename) {
    FILE *image = fopen(filename, "rb");
    if (image == NULL) {
        perror("fopen");
        exit(1);

    }

    uintptr_t fs_base;
    fread(&fs_base, sizeof(fs_base), 1, image);

    ucontext_t context;
    if (fread(&context, sizeof(context), 1, image) != 1) {
        printf("Could not read saved context\n");
        fclose(image);
        exit(1);

    }

    // printf("Saved context: %zu bytes\n\n", sizeof(context));

    struct memory_region mr;
    // int mr_number = 0;

    while (fread(&mr, sizeof(mr), 1, image) == 1) {
        size_t size = mr.end_addr - mr.start_addr;

        // printf("mr %d\n", mr_number);
        // printf("  Address:     %p - %p\n", mr.start_addr, mr.end_addr);
        // printf("  Size:        %zu bytes\n", size);
        // printf("  Permissions: %c%c%c\n", mr.is_r ? 'r' : '-', mr.is_w ? 'w' : '-', mr.is_x ? 'x' : '-');

        void* mapped_addr = mmap(
            mr.start_addr, 
            size, 
            PROT_READ | PROT_WRITE | PROT_EXEC, 
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, 
            -1, 0
        );

        if (mapped_addr == MAP_FAILED) {
            perror("mmap failed");
            exit(1);
        }


        // unsigned char page[16];
        // size_t bytes_to_print = size < sizeof(page) ? size : sizeof(page);
        // (void)page;
        // (void)bytes_to_print;

        // for verifying the memory data, we read the first bytes of the mr from the checkpoint image
        // if (fread(page, 1, bytes_to_print, image) != bytes_to_print) {
        //     printf("  fread memory data failed\n");
        //     fclose(image);
        //     exit(1);
        // }

        // for checking
        // fseek(image, (long)(-bytes_to_print), SEEK_CUR);

        if (fread(mapped_addr, 1, size, image) != size) {
            printf("  fread memory data failed\n");
            fclose(image);
            exit(1);
        }

        int protection = 0;

        if (mr.is_r) protection |= PROT_READ;   
        if (mr.is_w) protection |= PROT_WRITE;
        if (mr.is_x) protection |= PROT_EXEC;

        if (mprotect(mapped_addr, size, protection) != 0) {
            perror("mprotect failed");
            exit(1);
        }

        // printf("  First bytes: ");
        // for (size_t i = 0; i < bytes_to_print; i++) {
        //     // assert(mapped_addr + i == page[i]); // Verify that the memory was restored correctly
        //     if ((*(unsigned char*)(mapped_addr + i)) != page[i]) {
        //         printf("Memory mismatch at offset %zu: expected %02x, got %02x\n", i, page[i], (*(unsigned char*)(mapped_addr + i)));
        //         exit(1);
        //     }
        //     printf("%02x ", page[i]);
        // }
        // printf("\n\n");

        // if (fseek(image, (long)(size - bytes_to_print), SEEK_CUR) != 0) {
        //     printf("Could not skip memory-mr data\n");
        //     fclose(image);
        //     exit(1);

        // }

        // mr_number++;
    }

    // printf("Total mrs: %d\n", mr_number);
    fclose(image);

    /**
     * Without this syscall, the restored program will 
     * FS_register must point to the hello's TLS area
     * Ex - FS_BASE address is 0x7f8e5c000000 for the checkpointing program, 
     * but it should be 0x7f8e5c400000 for the restored program.
     * This is because the FS register is not automatically restored 
     * when we restore the context using setcontext().
     */
    if (syscall(SYS_arch_prctl, ARCH_SET_FS, fs_base) == -1) {
        exit(1);
    }
    
    setcontext(&context);
    exit(1);
}

int
main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s CHECKPOINT_IMAGE\n", argv[0]);
        return 1;
    }

    if (strlen(argv[1]) >= sizeof(checkpoint_image)) {
        printf("Checkpoint filename is too long\n");
        return 1;
    }
    strcpy(checkpoint_image, argv[1]);

    size_t stack_size = 1 << 20;

    // void* stack_top = (char *)get_temp_stack() + stack_size;
    void* stack_top = (char*) find_temp_stack(checkpoint_image, stack_size) + stack_size;
    __asm__ volatile("mov %0, %%rsp" :: "r"(stack_top) : "memory");

    restore(checkpoint_image);
    return 0;
}
