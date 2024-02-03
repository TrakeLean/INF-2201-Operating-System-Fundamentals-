#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define IMAGE_FILE "image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

/* Variable to store pointer to program name */
char *progname;

/* Variable to store pointer to the filename for the file being read. */
char *elfname;

/* Structure to store command line options */
static struct {
	int vm;
	int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);

int main(int argc, char **argv) {
	/* Process command line options */
	progname = argv[0];
	options.vm = 0;
	options.extended = 0;
	while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
		char *option = &argv[1][2];

		if (strcmp(option, "vm") == 0) {
			options.vm = 1;
		}
		else if (strcmp(option, "extended") == 0) {
			options.extended = 1;
		}
		else {
			error("%s: invalid option\nusage: %s %s\n", progname, progname, ARGS);
		}
		argc--;
		argv++;
	}
	if (options.vm == 1) {
		/* This option is not needed in project 1 so we doesn't bother
		 * implementing it*/
		error("%s: option --vm not implemented\n", progname);
	}
	if (argc < 3) {
		/* at least 3 args (createimage bootblock kernel) */
		error("usage: %s %s\n", progname, ARGS);
	}
	create_image(argc - 1, argv + 1);
	return 0;
}

void find_bootblock_size(char *file){
	FILE *bb_fp = fopen(file, "rb");
	char bb_size[10] = "KernelSize";
	fscanf(bb_fp, "%d", &bb_size);
	printf("Bootblock size: %d\n", bb_size);
}


void read_bootblock(FILE* fp, int offset, int size_file, FILE *write_image, int kernel_size){
	unsigned char curr_sector[size_file];
	// Seek forward to start at the begining of the elf code sections
	fseek(fp, offset, SEEK_SET);
	// Read the elf code sections into the curr_sector array with the size fo the code section part.
	int ret = fread(&curr_sector, size_file, 1, fp);
	if (ret == 1){
		// Write to new binary file
		fwrite(&curr_sector, size_file, 1, write_image);
		// Pad from size of file to 510 (not 512 because of 2 bytes for 0x55aa)
		for (;size_file < 510; size_file+=2){
			unsigned short pad = 0;
			fwrite(&pad, sizeof(unsigned short), 1, write_image);
		}
		unsigned short ending = 0xaa55;
		fwrite(&ending, sizeof(unsigned short), 1, write_image);
		printf("Bootblock written to image\n");
		char pos;
		// Store current position in file
		fgetpos(write_image, &pos);
		// Seek to the position of the kernel size
		fseek(write_image, 2, SEEK_SET);
		// Write the kernel size to the image
		fwrite(&kernel_size, sizeof(unsigned short), 1, write_image);
		// Return to the position in the file
		fsetpos(write_image, &pos);
		}
	else{
		printf("Error reading bootblock\n");
	}
}

void read_exe(FILE* fp, int offset, int size_file, FILE *write_image){
	unsigned char curr_sector[size_file+100];
	// Seek forward to start at the begining of the elf code sections
	fseek(fp, offset, SEEK_SET);
	// Read the elf code sections into the curr_sector array with the size fo the code section part.
	int ret = fread(&curr_sector, size_file+100, 1, fp);
	if (ret == 1){
		// Write to new binary file
		fwrite(&curr_sector, size_file+100, 1, write_image);
		printf("Executable written to image\n");
	}
	else{
		printf("Error reading executable\n");
	}
	// Rookie safety pad
	// for (int i = 0; i < 250; i++)
	// {
	// 	unsigned short safety_pad = 0;
	// 	fwrite(&safety_pad, sizeof(unsigned short), 1, write_image);
	// }
}

void read_programheader(FILE *fp, Elf32_Ehdr header, char *file_name, FILE *write_image, int kernel_size){
	Elf32_Phdr programheader;

	int ret = fread(&programheader, sizeof(Elf32_Phdr), 1, fp);
	if (ret == 1){
		printf("File name: %s \n", file_name);
		// Offset needed to know locations of code sections
		int program_offset = programheader.p_offset;
		// Size needed to know how much to read from the code sections
		int size_file = programheader.p_filesz;
		printf("File size: %d\n", size_file);

		if (file_name[0] == 'b' || file_name[0] == 'B'){
			printf("Reading Bootblock\n");
			read_bootblock(fp, program_offset, size_file, write_image, kernel_size);
		}
		else{
			printf("Reading Executable\n");
			read_exe(fp, program_offset, size_file, write_image);
		}
	}
}


static void create_image(int nfiles, char *files[]) {
	Elf32_Ehdr header;

	printf("\n\n");
	/* Open new file to write to has to be opened here
	 * because we want to keep hold of where we are in the file
	 * so we can write to it in the read_programheader function
	 * from the end.
	*/
	FILE *write_image = fopen("image", "wb");
	for (int i = 0; i < nfiles; i++)
	{
		FILE *fp = fopen(files[i], "rb");
		int ret = fread(&header, sizeof(Elf32_Ehdr), 1, fp);
		if (ret == 1)
		{
			// printf("File name: %s \n", files[i]);
			// printf("Header Offset: %i\n", header.e_ehsize);
			// printf("Header memory address: %i\n", header.e_entry);
			// printf("SH string index: %i\n\n", header.e_shstrndx);
			read_programheader(fp, header, files[i], write_image, header.e_shstrndx);
		}	
		else
		{
			error("Error reading ELF header");
		}
	}
	find_bootblock_size(files[0]);
	printf("\n:)\n");
	//exit(-1);
}


/* print an error message and exit */
static void error(char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	if (errno != 0) {
		perror(NULL);
	}
	exit(EXIT_FAILURE);
}
