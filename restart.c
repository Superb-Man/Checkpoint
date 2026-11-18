#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/mman.h>

struct memory_region {
    void *start_addr;
    void *end_addr;
    int is_r;
    int is_w;
    int is_x;
};

void
print_img(char *filename) {
    FILE *image = fopen(filename, "rb");
    if (image == NULL) {
        perror("fopen");
        exit(1);

    }

    ucontext_t context;
    if (fread(&context, sizeof(context), 1, image) != 1) {
        printf("Could not read saved context\n");
        fclose(image);
        exit(1);

    }

    printf("Saved context: %zu bytes\n\n", sizeof(context));

    struct memory_region region;
    int region_number = 0;

    while (fread(&region, sizeof(region), 1, image) == 1) {
        size_t size = region.end_addr - region.start_addr;

        printf("Region %d\n", region_number);
        printf("  Address:     %p - %p\n", region.start_addr, region.end_addr);
        printf("  Size:        %zu bytes\n", size);
        printf("  Permissions: %c%c%c\n", region.is_r ? 'r' : '-', region.is_w ? 'w' : '-', region.is_x ? 'x' : '-');

        void* mapped_addr = mmap(
            region.start_addr, 
            size, 
            PROT_READ | PROT_WRITE | PROT_EXEC, 
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, 
            -1, 0
        );

        if (mapped_addr == MAP_FAILED) {
            perror("mmap failed");
            exit(1);
        }


        unsigned char page[size];
        size_t bytes_to_print = size < sizeof(page) ? size : sizeof(page);

        // for verifying the memory data, we read the first bytes of the region from the checkpoint image
        if (fread(page, 1, bytes_to_print, image) != bytes_to_print) {
            printf("  fread memory data failed\n");
            fclose(image);
            exit(1);
        }

        // for checking
        fseek(image, (long)(-bytes_to_print), SEEK_CUR);

        if (fread(mapped_addr, 1, bytes_to_print, image) != bytes_to_print) {
            printf("  fread memory data failed\n");
            fclose(image);
            exit(1);
        }

        int protection = 0;

        if (region.is_r) protection |= PROT_READ;   
        if (region.is_w) protection |= PROT_WRITE;
        if (region.is_x) protection |= PROT_EXEC;

        if (mprotect(mapped_addr, size, protection) != 0) {
            perror("mprotect failed");
            exit(1);
        }

        printf("  First bytes: ");
        for (size_t i = 0; i < size; i++) {
            // assert(mapped_addr + i == page[i]); // Verify that the memory was restored correctly
            if ((*(unsigned char*)(mapped_addr + i)) != page[i]) {
                printf("Memory mismatch at offset %zu: expected %02x, got %02x\n", i, page[i], (*(unsigned char*)(mapped_addr + i)));
                exit(1);
            }
            printf("%02x ", page[i]);
        }
        printf("\n\n");

        if (fseek(image, (long)(size - bytes_to_print), SEEK_CUR) != 0) {
            printf("Could not skip memory-region data\n");
            fclose(image);
            exit(1);

        }

        region_number++;
    }

    printf("Total regions: %d\n", region_number);
    fclose(image);

    return;
}

int
main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s CHECKPOINT_IMAGE\n", argv[0]);
        return 1;
    }

    print_img(argv[1]);
    return 0;
}
