/*
 * Author: Roman Yepishev <roman.yepishev@gmail.com>
 * 
 * Permission to copy and modify the code for any purpose provided that this
 * notice appears in the derived work.
 */

/*
 *
 * Usage: ./unpack_acer_mergedosfile [-s] [-f] [-d dir] acer_MergedOSFile.bin
 *	-s	Skip 20-byte header for some files (enabled for A4)
 *	-d dir	Use this directory to extract (default ".")
 *	-f	Continue even if MD5 check fails
 *	-h	This help
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#ifdef WIN32
# include <windows.h>
#else
# include <sys/mman.h>
#endif

#include "md5.h"

#define CLASS_INFO_OFFSET     0x80
#define CLASS_INFO_SIZE	      0x28

#define MD5_INFO_OFFSET	      0x100
#define MD5_INFO_SIZE	      0x10

#define AMSS_INFO_OFFSET      0x120
#define BUILD_INFO_OFFSET     0xc0

#define PAYLOAD_OFFSET	      0x200

#define PARTITION_INFO_OFFSET 0x30
#define PARTITION_HEADER_SIZE 0x14

#define HEADER_MD5_PLACEHOLDER "(ScuKey].XD3#ByT"

struct mergedfile_partition {
    char *name;
    char *filename;
    int has_header;
};

struct mergedfile_partition mergedfile_partitions[] = {
    { "MBN",	      "partition.mbn",	0 },
    { "DBL",	      "dbl.mbn",	0 },
    { "FSBL",	      "fsbl.mbn",	0 },
    { "OSBL",	      "osbl.mbn",	0 },
    { "AMSS",	      "amss.mbn",	0 },
    { "(unknown 1)",  "unknown1.bin",	0 },
    { "APPSBOOT",     "appsboot.mbn",	0 },
    { "(unknown 2)",  "unknown2.bin",	0 },
    { "DSP1",	      "dsp1.mbn",	0 },
    { "(unknown 3)",  "unknown3.bin",	0 },
    { "QPSTHOSTL",    "NPRG8650.hex",	0 },
    { "BOOT",	      "boot.img",	1 },
    { "SYSTEM",	      "system.img",	1 },
    { "USERDATA",     "userdata.img",	1 },
    { "RECOVERY",     "recovery.img",	1 },
    { "SPLASH1",      "initlogo.rle",	1 },
    { "MICROP_FW",    "touch.bin",	1 },
    { "ADSP",	      "adsp.mbn",	0 },
    { "FLEX",	      "flex.img",	1 },
    { "LOGODINFO",    "logodinfo.img",	1 },
};

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


/*
 * Hello, Acer
 *
 * Thanks to the awesome .Net Reflector, all the information about the file
 * structure can be recovered from Acer EUU.
 *
 * 0x08 - 4 bytes, int, partition flags:
 *
 *   0x00001 - mbn - Partition Table
 *   0x00002 - dbl
 *   0x00004 - fsbl
 *   0x00008 - osbl
 *   0x00010 - amss.mbn
 *   0x00040 - appsboot.mbn
 *   0x00100 - dsp1.mbn
 *   0x00400 - QPSThostDL - NPRG8650.hex
 *   0x00800 - boot.img
 *   0x01000 - system.img
 *   0x02000 - userdata.img
 *   0x04000 - recovery.img
 *   0x08000 - initlogo.rle
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
 * Checksum:
 *
 * Checksum is stored as MD5 hexdigest in 0x100 - 0x110, during checksum check
 * the bytes in 0x100..0x110 are set to "(ScuKey].XD3#ByT"
 *
 * The payload starts at 0x200. All the partitions are glued together after
 * this.
 *
 * A4 gets the files mentioned above extracted without the 20 byte header by
 * default.
 *
 */


void print_mergedfile_amssversion(char *mf_data) {
    char buf[0x20];
    memcpy(&buf, mf_data + AMSS_INFO_OFFSET, sizeof(buf));

    buf[0x1f] = '\0';

    printf("AMSS Version: %s\n", buf);
}

void print_mergedfile_builddate(char *mf_data) {
    char buf[0x20];
    memcpy(&buf, mf_data + BUILD_INFO_OFFSET, sizeof(buf));

    buf[0x1f] = '\0';
    printf("Build date: %s\n", buf);
}

int get_mergedfile_class(char *mf_data) {
    char buf[40];
    int i, class_id = FILE_CLASS_UNKNOWN;
    struct mergedfile_class_info *mf_class;
    int num_classes = sizeof(mergedfile_classes) / sizeof(struct mergedfile_class_info);

    memcpy(&buf, mf_data + CLASS_INFO_OFFSET, 40);

    for (i=0; i < num_classes; i++) {
	mf_class = &mergedfile_classes[i];

	if (strstr(buf, mf_class->name)) {
	    class_id = mf_class->class_id;
	    break;
	}
    }

    return class_id;
}

int extract_partitions(char *mf_data, const char* target_dir, int skip_header) {
    int num_partitions = sizeof(mergedfile_partitions) / sizeof(*mergedfile_partitions);
    struct mergedfile_partition *mf_partition;
    int i, size, is_error = 0;
    FILE *output_fh;
    int partitions_present = 0;
    char *output_path;

    off_t cur_offset = PAYLOAD_OFFSET;

    memcpy(&partitions_present, mf_data + 0x8, 4);

    for (i = 0; i < num_partitions; i++) {
	mf_partition = &mergedfile_partitions[i];

	if ((1 << i) & partitions_present) {
	    memcpy(&size, mf_data + PARTITION_INFO_OFFSET + i * 4, 4);

	    if (skip_header) {
		if (mf_partition->has_header) {
		    cur_offset += PARTITION_HEADER_SIZE;
		    size -= PARTITION_HEADER_SIZE;
		}
	    }

	    // abc + / + out.bin + \0
	    output_path = malloc(strlen(target_dir) 
		                 + strlen(mf_partition->filename) 
				 + 2);
	    sprintf(output_path, "%s/%s", target_dir, mf_partition->filename);
	    printf("Extracting %s to %s (%d bytes)\n", mf_partition->name,
							    output_path, size);

	    output_fh = fopen(output_path, "wb");
	    if (output_fh) {
		fwrite(mf_data + cur_offset, size, 1, output_fh);
		fclose(output_fh);
		output_fh = NULL;
	    } else {
		perror(output_path);
		is_error = 1;
	    }
	    
	    free(output_path);
	    output_path = NULL;

	    cur_offset += size;

	    if (is_error)
		break;
	} else {
	    printf("MergedFile is missing %s, skipping.\n", mf_partition->name);
	    continue;
	}
    }

    return is_error;
}

int verify_header(char *mf_data) {
    if (strncmp("Acer", mf_data, 4))
	return 1;
    return 0;
}

#define MD5_CHUNK_SIZE 1048576
int verify_md5(char *mf_data, size_t file_size) {
    unsigned char expected_md5[MD5_INFO_SIZE];
    char *mf_header_copy;
    char hexdigest[33];
    int i, is_error = 0;
    md5_state_t state;
    md5_byte_t digest[16];
    int md5_chunk_size = MD5_CHUNK_SIZE;

    printf("Checking MD5... ");

    memcpy(&expected_md5, mf_data + MD5_INFO_OFFSET, sizeof(expected_md5));
    for (i = 0; i < MD5_INFO_SIZE; i++)
	sprintf(hexdigest + i * 2, "%02x", expected_md5[i]);
    hexdigest[32] = 0;

    mf_header_copy = malloc(PAYLOAD_OFFSET);
    if (!mf_header_copy) {
	perror("verify_md5");
	return 0;
    }

    memcpy(mf_header_copy, mf_data, PAYLOAD_OFFSET);
    memset(mf_header_copy + MD5_INFO_OFFSET, 0, MD5_INFO_SIZE);

    memcpy(mf_header_copy + MD5_INFO_OFFSET, HEADER_MD5_PLACEHOLDER,
					strlen(HEADER_MD5_PLACEHOLDER));

    md5_init(&state);
    md5_append(&state, (const md5_byte_t *)mf_header_copy, PAYLOAD_OFFSET);

    for (i = PAYLOAD_OFFSET; i <= file_size; i += MD5_CHUNK_SIZE) {
	/* Last chunk */
	if (i + MD5_CHUNK_SIZE > file_size)
	    md5_chunk_size = file_size - i;

	md5_append(&state, (const md5_byte_t *)(mf_data + i), md5_chunk_size);
    }

    md5_finish(&state, digest);

    for (i = 0; i < MD5_INFO_SIZE; i++) {
	if (digest[i] != expected_md5[i]) {
	    is_error = 1;
	    break;
	}
    }

    if (is_error) {
	printf("FAIL:\n  Expected:\t%s\n", hexdigest);
	for (i = 0; i < MD5_INFO_SIZE; i++)
	    sprintf(hexdigest + i * 2, "%02x", digest[i]);
	hexdigest[32] = 0;
	printf("  Got:\t\t%s\n", hexdigest);
    } else {
	printf("OK: %s\n", hexdigest);
    }

    free(mf_header_copy);

    return is_error;
}

void usage(char *program_name) {
    fprintf(stderr, "Usage: %s [-s] [-f] [-d dir] acer_MergedOSFile.bin\n"
		    "\t-s\tSkip 20-byte header for some files (enabled for A4)\n"
		    "\t-d dir\tUse this directory to extract (default \".\")\n"
		    "\t-f\tContinue even if MD5 check fails\n"
		    "\t-h\tThis help\n"
		    "\n"
		    "Written by Roman Yepishev <https://github.com/roman-yepishev>\n",
		    program_name);
}

int main(int argc, char** argv) {
    char *path;
    char *mf_data;
    struct stat file_info;
    int mf_fd;
    int mf_class;
    int skip_header = 0, force = 0;
    int opt;
    char *target_directory = ".";
#ifdef WIN32
    HANDLE fm, h;
#endif

    setvbuf(stdout, NULL, _IONBF, 0);

    while((opt = getopt(argc, argv, "sd:fh")) != -1) {
	switch(opt) {
	    case 's':
		skip_header = 1;
		break;
	    case 'd':
		target_directory = optarg;
		break;
	    case 'f':
		force = 1;
	    case 'h':
		usage(argv[0]);
		return EXIT_SUCCESS;
	    default:
		usage(argv[0]);
		return EXIT_FAILURE;
	}
    }

    if (optind >= argc) {
	usage(argv[0]);
	return EXIT_FAILURE;
    }

    path = argv[optind];

    if (stat(path, &file_info)) {
	fprintf(stderr, "Can't stat %s: %s\n", path, strerror(errno));
	return EXIT_FAILURE;
    }

    mf_fd = open(path, O_RDONLY);
    if (mf_fd == -1) {
	fprintf(stderr, "Can't open %s: %s\n", path, strerror(errno));
	return EXIT_FAILURE;
    }

#ifdef WIN32
    /* https://groups.google.com/forum/?fromgroups#!topic/avian/Q8bwM4ksubw */
    h = (HANDLE)_get_osfhandle(mf_fd);

    fm = CreateFileMapping(h, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!fm) {
	fprintf(stderr, "Can't CreateFileMapping: %s\n", strerror(errno));
	return EXIT_FAILURE;
    }

    mf_data = (char *)MapViewOfFile(fm, FILE_MAP_READ, 0, 0, file_info.st_size);
    if (!mf_data) {
	fprintf(stderr, "Can't MapViewOfFile: %s\n", strerror(errno));
	return EXIT_FAILURE;
    }
#else
    mf_data = (char *)mmap(NULL, file_info.st_size, PROT_READ, MAP_SHARED, mf_fd, 0);
    if (mf_data == MAP_FAILED) {
	fprintf(stderr, "Can't allocate memory: %s\n", strerror(errno));
	return EXIT_FAILURE;
    }
#endif

    /* 1. Verify header */
    if (verify_header(mf_data)) {
	fprintf(stderr, "FAILURE: Invalid header\n");
	return EXIT_FAILURE;	
    }

    /* 2. Get file class */
    mf_class = get_mergedfile_class(mf_data);
    if (mf_class == FILE_CLASS_A4)
	skip_header = 1;

    /* 3. Verify MD5 SUM */
    if (verify_md5(mf_data, file_info.st_size)) {
	fprintf(stderr, "FAILURE: Invalid MD5\n");
	if (!force)
	    return EXIT_FAILURE;
    }

    print_mergedfile_amssversion(mf_data);
    print_mergedfile_builddate(mf_data);
    
    /* 4. Extract the contents */
    if (extract_partitions(mf_data, target_directory, skip_header)) {
	fprintf(stderr, "FAILURE: Could not extract partitions\n");
    }

    /* 5. Cleanup */
#ifdef WIN32
    UnmapViewOfFile(mf_data);
    CloseHandle(fm);
#else
    if (munmap(mf_data, file_info.st_size)) {
	fprintf(stderr, "Can't deallocate memory: %s\n", strerror(errno));
	return EXIT_FAILURE;
    }
#endif

    if (close(mf_fd)) {
	fprintf(stderr, "Can't close file: %s\n", strerror(errno));
	return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
