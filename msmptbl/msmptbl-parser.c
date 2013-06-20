#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <inttypes.h>

/* from arch/arm/mach-msm/nand_partitions.c */
struct msm_ptbl_entry {
    char name[16];
    uint32_t offset; 
    uint32_t size;
    uint32_t flags;
};

struct msm_ptbl_header {
    uint32_t magic1;
    uint32_t magic2;
    uint32_t version;
    uint32_t numparts;
};

int main(int argc, char **argv)
{
    int i;
    char *filename = argv[1];
    FILE *fh;
    size_t items_read;
    struct msm_ptbl_header pheader;
    struct msm_ptbl_entry pentry;

    if (argc != 2) {
	printf("Usage: %s partition.mbn\n", argv[0]);
	return EXIT_FAILURE;
    }

    fh = fopen(filename, "rb");
    if (!fh) {
	perror("Failed to open input file");
	return EXIT_FAILURE;
    }

    items_read = fread(&pheader, sizeof(pheader), 1, fh);
    if (!items_read) {
	printf("Failed to read partition header\n");
	return EXIT_FAILURE;
    }

    printf("Partition header:\n"
	   "  MAGIC1: %08" PRIx32 "\n"
	   "  MAGIC2: %08" PRIx32 "\n"
	   "  Version: %" PRId32 "\n"
	   "  Number of partitions: %" PRId32 "\n",
	   pheader.magic1,
	   pheader.magic2,
	   pheader.version,
	   pheader.numparts);

    printf("Partition table:\n");

    for (i = 0; i < pheader.numparts; i++) {
	items_read = fread(&pentry, sizeof(pentry), 1, fh);
	if (! items_read) {
	    printf("Failed to read partition entry\n");
	    return EXIT_FAILURE;
	}

	if (pentry.name[0] == 0)
	    break;

	printf("  %02d: %-16s  offset=%-8" PRId32 " size=%-8" PRId32
	       " flags=%08" PRIx32 "\n", i, pentry.name, pentry.offset,
	       pentry.size, pentry.flags);
    }

    fclose(fh);

    return EXIT_SUCCESS;
}
