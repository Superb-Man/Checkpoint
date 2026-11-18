#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

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

        unsigned char page[size];
        size_t bytes_to_print = size < sizeof(page) ? size : sizeof(page);

        // 
        if (fread(page, 1, bytes_to_print, image) != bytes_to_print) {
            printf("  Incomplete memory data\n");
            fclose(image);
            exit(1);
        }

        printf("  First bytes: ");
        for (size_t i = 0; i < size; i++)
            printf("%02x ", page[i]);
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
