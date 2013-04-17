/*
 * Author: Roman Yepishev <roman.yepishev@gmail.com>
 * 
 * Permission to copy and modify the code for any purpose provided that this
 * notice appears in the derived work.
 */

/*
 * Usage: ./a.out [-s] acer_MergedOSFile.bin
 *  -s	Skip 20-byte header for some files (enabled for A4)
 *    -d dir  Use this directory to extract (default ".")
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

enum {
    PART_MBN	  = 1,
    PART_DBL	  = 1 << 1,
    PART_FSBL	  = 1 << 2,
    PART_OSBL	  = 1 << 3,
    PART_AMSS	  = 1 << 4,
    // undefined 0x20 1 << 5
    PART_APPSBOOT = 1 << 6,
    // undefined 0x80 1 << 7
    PART_DSP1     = 1 << 8,
    // undefined 0x200 = 1 << 9 
    PART_QPSTHOSTL= 1 << 10,
    PART_BOOT     = 1 << 11,
    PART_SYSTEM   = 1 << 12,
    PART_USERDATA = 1 << 13,
    PART_RECOVERY = 1 << 14,
    PART_SPLASH1  = 1 << 15,
    PART_MICROP_FW= 1 << 16,
    PART_ADSP     = 1 << 17,
    PART_FLEX     = 1 << 18,
    PART_LOGODINFO= 1 << 19,
};

#define EXTRA_HEADER_PARTS (PART_BOOT | PART_SYSTEM | PART_USERDATA \
			   | PART_RECOVERY | PART_SPLASH1 | PART_MICROP_FW \
			   | PART_FLEX | PART_LOGODINFO)

enum {
    FILE_CLASS_A1,
    FILE_CLASS_A2,
    FILE_CLASS_A3,
    FILE_CLASS_A4,
    FILE_CLASS_UNKNOWN,
};

struct mergedfile_class_info {
    char *name;
    int class_id;
};

struct mergedfile_class_info mergedfile_classes[] = {
    { "LiquidMetal", FILE_CLASS_A4 },
    { "Liquid",      FILE_CLASS_A1 },
    { "Stream",      FILE_CLASS_A3 },
    { "Krumping",    FILE_CLASS_A3 },
    { "A3",          FILE_CLASS_A3 },
    { "A4",          FILE_CLASS_A4 },
};

struct mergedfile_part_info {
    int id;
    off_t size_offset;
    char *filename;
};

struct mergedfile_part_info mergedfile_partitions[] = {
    { PART_MBN,	      0x30, "partition.mbn" },
    { PART_DBL,	      0x34, "dbl.mbn" },
    { PART_FSBL,      0x38, "fsbl.mbn" },
    { PART_OSBL,      0x3c, "osbl.mbn" },
    { PART_AMSS,      0x40, "amss.mbn" },
    { PART_APPSBOOT,  0x48, "appsboot.mbn" },
    { PART_DSP1,      0x50, "dsp1.mbn" },
    { PART_QPSTHOSTL, 0x58, "NPRG8650.hex" },
    { PART_BOOT,      0x5c, "boot.img" },
    { PART_SYSTEM,    0x60, "system.img" },
    { PART_USERDATA,  0x64, "userdata.img" },
    { PART_RECOVERY,  0x68, "recovery.img" },
    { PART_SPLASH1,   0x6c, "initlogo.rle" },
    { PART_MICROP_FW, 0x70, "touch.bin" },
    { PART_ADSP,      0x74, "adsp.mbn" },
    { PART_FLEX,      0x78, "flex.img" },
    { PART_LOGODINFO, 0x7c, "logodinfo.img" },
};

#define MERGEDFILE_PAYLOAD_START 0x200


/*
 * Hello, Acer
 *
 * Thanks to the awesome .Net Reflector, all the information about the file
 * structure can be recovered from Acer EUU.
 *
 * 0x08 - 4 bytes, int, partition flags:
 *
 *   0x1 - mbn - Partition Table
 *   0x2 - dbl
 *   0x4 - fsbl
 *   0x8 - osbl
 *   0x10 - amss.mbn
 *   0x40 - appsboot.mbn
 *   0x100 - dsp1.mbn
 *   0x400 - QPSThostDL - NPRG8650.hex
 *   0x800 - boot.img
 *   0x1000 - system.img
 *   0x2000 - userdata.img
 *   0x4000 - recovery.img
 *   0x8000 - initlogo.rle
 *   0x10000 - touch.bin
 *   0x20000 - adsp.mbn
 *   0x40000 - flex.img
 *   0x80000 - logodinfo.img
 *
 * A4 (LiquidMetal) has boot,system,userdata,recovery,initlogo,flex and
 * logodinfo without header (-20 bytes from the beginning).
 *
 * 0x80 - 40 bytes - Merged file information, null terminated
 *
 * Partition table size offsets:
 *
 *   0x30 - partition table for the device
 *   0x34 - DBL
 *   0x38 - FSBL
 *   0x3C - OSBL
 *   0x40 - AMSS
 *   0x48 - AppsBoot
 *   0x50 - DSP1
 *   0x58 - QPSThostDL - NPRG8650.hex
 *   0x5c - Boot
 *   0x60 - System
 *   0x64 - User Data
 *   0x68 - Recovery
 *   0x6c - Splash1
 *   0x70 - microP_FW (touchpad firmware?)
 *   0x74 - ADSP
 *   0x78 - Flex
 *   0x7c - LogoDinfo 
 *
 * Checksum:
 *
 * Checksum is stored as MD5 hexdigest in 0x100 - 0x110, during checksum check
 * the bytes in 0x100..0x110 are set to 0xff.
 *
 * In case the file is for A4, the bytes are set to "[mgfl_k]:t@ac.tw"
 *
 * In case of EUU update, the bytes are set to "(ScuKey].XD3#ByT"
 *
 * The payload starts at 0x200. All the partitions are glued together after
 * this.
 *
 * A4 gets the files mentioned above extracted without the 20 byte header by
 * default.
 *
 */

static char* mf_data;

int main(int argc, char** argv) {
    char* path;
    struct stat file_info;
    int mf_fd;
    int mf_class;
    int skip_header = 0;
    int opt;
    char *target_directory = ".";

    while((opt = getopt(argc, argv, "sd:")) != -1) {
	switch(opt) {
	    case 's':
		skip_header = 1;
		break;
	    case 'd':
		target_directory = optarg;
		break;
	    default:
		fprintf(stderr, "Usage: %s [-s] acer_MergedOSFile.bin\n", argv[0]);
		fprintf(stderr, "\t-s\tSkip 20-byte header for some files (enabled for A4)\n");
		fprintf(stderr, "\t-d dir\tUse this directory to extract (default \".\")\n");
		return EXIT_FAILURE;
	}
    }

    if (optind >= argc) {
	fprintf(stderr, "Path to acer_MergedOSFile.bin expected.\n");
	return EXIT_FAILURE;
    }

    path = argv[optind];

    if (stat(path, &file_info)) {
	perror("Can't stat file");
	return EXIT_FAILURE;
    }

    mf_fd = open(path, O_RDONLY);
    if (mf_fd == -1) {
	perror("Can't open file");
	return EXIT_FAILURE;
    }

    mf_data = (char *)mmap(NULL, file_info.st_size, PROT_READ, MAP_SHARED, mf_fd, 0);
    if (mf_data == MAP_FAILED) {
	perror("Can't allocate memory");
	return EXIT_FAILURE;
    }

    // 1. Verify header
    if (verify_header()) {
	printf("Invalid header");
	return EXIT_FAILURE;	
    }

    // 2. TODO: Verify MD5 SUM

    // 3. Get file class
    mf_class = get_mergedfile_class();
    if (mf_class == FILE_CLASS_A4)
	skip_header = 1;
    
    // 4. Extract the contents
    if (extract_partitions(target_directory, skip_header)) {
	printf("Could not extract partitions");
    }

    // 5. Cleanup
    if (munmap(mf_data, file_info.st_size)) {
	perror("Can't deallocate memory");
	return EXIT_FAILURE;
    }

    if (close(mf_fd)) {
	perror("Can't close file");
	return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int verify_header() {
    if (strncmp("Acer", mf_data, 4))
	return 1;
    return 0;
}

int get_mergedfile_class() {
    char buf[40];
    int i, class_id = FILE_CLASS_UNKNOWN;
    struct mergedfile_class_info *mf_class;
    int num_classes = sizeof(mergedfile_classes) / sizeof(struct mergedfile_class_info);

    memcpy(&buf, mf_data + 0x80, 40);

    for (i=0; i < num_classes; i++) {
	mf_class = &mergedfile_classes[i];

	if (strstr(buf, mf_class->name)) {
	    class_id = mf_class->class_id;
	    break;
	}
    }

    return class_id;
}

int extract_partitions(const char* target_dir, int skip_header) {
    int num_partitions = sizeof(mergedfile_partitions) / sizeof(struct mergedfile_part_info);
    struct mergedfile_part_info *mf_partition;
    int i, size, chunk, written_bytes, is_error = 0;
    FILE *output_fh;
    int partitions_present = 0;
    char *mf_filename;
    char *output_path;

    off_t cur_offset = MERGEDFILE_PAYLOAD_START;

    memcpy(&partitions_present, mf_data + 0x8, 4);

    for (i=0; i<num_partitions; i++) {
	mf_partition = &mergedfile_partitions[i];
	mf_filename = mf_partition->filename;

	if (mf_partition->id & partitions_present) {
	    memcpy(&size, mf_data + mf_partition->size_offset, 4);

	    if (skip_header) {
		if (mf_partition->id & EXTRA_HEADER_PARTS) {
		    cur_offset += 20;
		    size -= 20;
		}
	    }


	    // abc + / + out.bin + \0
	    output_path = malloc(strlen(target_dir) + strlen(mf_filename) + 2);
	    sprintf(output_path, "%s/%s", target_dir, mf_filename);
	    printf("Extracting %s to %s (%d bytes)\n", mf_filename, output_path, size);

	    output_fh = fopen(output_path, "wb");
	    if (output_fh) {
		fwrite(mf_data + cur_offset, size, 1, output_fh);
		fclose(output_fh);
		output_fh = NULL;
	    }
	    else {
		perror(output_path);
		is_error = 1;
	    }
	    
	    free(output_path);
	    output_path = NULL;

	    cur_offset += size;

	    if (is_error)
		break;
	}
	else {
	    printf("MergedFile is missing %s, skipping.\n", mf_partition->filename);
	    continue;
	}
    }

    return is_error;
}
