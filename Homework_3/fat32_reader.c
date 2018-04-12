/***********************************************************
 * TODO: Fill in this area and delete this line
 * Name of program:
 * Authors:
 * Description:
 **********************************************************/

/* These are the included libraries.  You may need to add more. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>


/* Put any symbolic constants (defines) here */
#define True 1  /* C has no booleans! */
#define False 0
#define INITIAL_FILE_SIZE 2
#define MAX_CMD 80
#define DEFAULT_NUM_FILES 2

/* This is the main function of your project, and it will be run
 * first before all other functions.
 */
static char *file_map;
typedef struct fat_info{
	int BPB_BytesPerSec;
	int BPB_SecPerClus;
	int BPB_RsvdSecCnt;
	int BPB_NumFATS;
	int BPB_FATSz32;
	int cluster_size;
	//first cluster is the start of the data region
	int first_cluster;
}fat_info;
typedef struct temp_file{
	int num_clusters;
	long int pointer;
	long int size;
	char * meat;
} temp_file;
fat_info info;
typedef struct current_directery{
	temp_file * directory;
	//if I look at file_offsets[x] it will tell me where the file named file_names[x] starts in the directory above
	int * file_offsets;//cluster number of the start of the file names in the corrisponding spot in the next array 
	char ** file_names;
}current_directery;
static current_directery curr_dir;
static temp_file * initilize_temp_file();

void initilize(char * file_path);
int read_int(int num_bytes, int offset, char * source);
void print_info();
void add_cluster(temp_file * file_so_far, int cluster_num);
void load_curr_directory(int dir_start, int first, int size);
void get_current_dir_list(char * directory);
temp_file * get_file(char * source, int fat_start);
int read_int(int num_bytes, int offset, char * source);
void initilize(char * file_path)
{


	int fd;
	//used https://www.linuxquestions.org/questions/programming-9/mmap-tutorial-c-c-511265/
	fd = open(file_path, O_RDONLY);
	FILE * fp = fdopen(fd, "r");
	if (fd == -1) {
		perror("Error opening file for reading");
		exit(EXIT_FAILURE);
	}
	//used https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
	fseek(fp, 0, SEEK_END);
	int sz = ftell(fp);
	fprintf(stderr, "file size is %d bytes\n", sz);
	file_map = mmap(0, sz, PROT_READ, MAP_SHARED, fd, 0);
	info.BPB_BytesPerSec = read_int(2, 11, file_map);
	info.BPB_SecPerClus = read_int(1, 13, file_map);
	info.BPB_RsvdSecCnt = read_int(2, 14, file_map);
	info.BPB_NumFATS = read_int(1, 16, file_map);
	info.BPB_FATSz32 = read_int(4, 36, file_map);
	info.cluster_size = info.BPB_SecPerClus * info.BPB_BytesPerSec;
	info.first_cluster = info.BPB_BytesPerSec * (info.BPB_RsvdSecCnt + info.BPB_NumFATS * info.BPB_FATSz32) + 0;
	//reading the first cluster of root directory and loading as current directory
	load_curr_directory(read_int(4, 44, file_map), True, 0);  
}
//loads current directory into curr_dir
void load_curr_directory(int dir_start, int first, int size)
{
	if(!first)
	{
		//free stuff
		// number_of_files = size / 32;
	}
	else{

	}
	//extra one for null terminator
	printf("%d\n", dir_start);
	scanf("%d", &dir_start);
	curr_dir.directory = get_file(file_map, dir_start);
	int number_of_files = (info.cluster_size/32) * curr_dir.directory->num_clusters;
	fprintf(stderr, "%d\n", read_int(4, 0,curr_dir.directory->meat));
	curr_dir.file_names = calloc(number_of_files, sizeof(char *) + sizeof(char)* 12);
	curr_dir.file_offsets = calloc(number_of_files, sizeof(int));
	int have_more = True;
	int file_location = 0;
	while(file_location <= curr_dir.directory->pointer * info.cluster_size)
	{
		
	}
}
int read_int(int num_bytes, int offset, char * source)
{
	int test = 0;
	fprintf(stderr, "offset--0x%x--%d-- num_bytes--%d--\n", offset, offset, num_bytes);
	memcpy(&test, &file_map[offset], num_bytes);
	//fprintf(stdout, "%d\n", test);
	return test;
}
// int read_int(int num_bytes, int offset, char * source)
// {
// 	//weird things are happening
// 	int test[1];
// 	memcpy(test, &file_map[offset], num_bytes);
// 	fprintf(stdout, "%d\n", *test);
// 	int ret_val = *test;
// 	return ret_val;
// }

void print_info()
{
	fprintf(stdout, "BPB_BytesPerSec is 0x%x, %d\n", info.BPB_BytesPerSec, info.BPB_BytesPerSec);
	fprintf(stdout, "BPB_SecPerClus is 0x%x, %d\n", info.BPB_SecPerClus, info.BPB_SecPerClus);
	fprintf(stdout, "BPB_RsvdSecCnt is 0x%x, %d\n", info.BPB_RsvdSecCnt, info.BPB_RsvdSecCnt);
	fprintf(stdout, "BPB_NumFATS is 0x%x, %d\n", info.BPB_NumFATS, info.BPB_NumFATS);
	fprintf(stdout, "BPB_FATSz32 is 0x%x, %d\n", info.BPB_FATSz32, info.BPB_FATSz32);

}
// void print_info()
// {
// 	int temp_var = read_int(2, 11, file_map);
// 	fprintf(stdout, "BPB_BytesPerSec is 0x%x, %d\n", info.BPB_BytesPerSec, info.BPB_BytesPerSec);
// 	temp_var = read_int(1, 13, file_map);
// 	fprintf(stdout, "BPB_SecPerClus is 0x%x, %d\n", temp_var, temp_var);
// 	temp_var = read_int(2, 14, file_map);
// 	fprintf(stdout, "BPB_RsvdSecCnt is 0x%x, %d\n", temp_var, temp_var);
// 	temp_var = read_int(1, 16, file_map);
// 	fprintf(stdout, "BPB_NumFATS is 0x%x, %d\n", temp_var, temp_var);
// 	temp_var = read_int(4, 36, file_map);
// 	fprintf(stdout, "BPB_FATSz32 is 0x%x, %d\n", temp_var, temp_var);

// }
void get_current_dir_list(char * directory)
{

}
temp_file * get_file(char * source, int fat_start)
{
	temp_file * new_file = initilize_temp_file();
	//finds first fat entry byte number (4 bytes per entry) i.e the begging of the FAT linked list 
	// long int start_entry = info.BPB_RsvdSecCnt * info.BPB_BytesPerSec + fat_start * 4;
	int next_cluster = fat_start;
	int fat_beginning = info.BPB_RsvdSecCnt * info.BPB_BytesPerSec;
	//while we have a valid next cluster
	//traverse the fat, getting data as we go
	while(next_cluster && 2 <= next_cluster && next_cluster <= 0x0FFFFFEF ){
		add_cluster(new_file, next_cluster);
		printf("this is pointer %lu this is size %lu this is num_clusters %d\n", new_file->pointer, new_file->size, new_file->num_clusters);
		//might be an off by one error
		//maybe print whole table
		next_cluster = (read_int(4, next_cluster * 4 + fat_beginning, source) & 0x0FFFFFFF);
		printf("next_cluster is 0x%x--%d\n", next_cluster,next_cluster);
	}
	printf("read file\n");
	if (next_cluster != 0 && next_cluster < 0x0FFFFFF8)
	{
		if (next_cluster == 0x0FFFFFF7)
		{
			fprintf(stderr, "ERROR: bad cluster reading file starting at %d exiting program\n", fat_start);
		}
		else{
			fprintf(stderr, "ERROR: reading file starting at %d exiting program\n", fat_start);
		}
		//put cleaner exit when done
		exit(1);
	}
	return new_file;
}
temp_file * initilize_temp_file()
{
	temp_file * temp = calloc(1, sizeof(temp_file));
	temp->meat = calloc(info.cluster_size * INITIAL_FILE_SIZE, sizeof(char));
	temp->pointer = 0;
	temp->num_clusters = 0;
	temp->size = INITIAL_FILE_SIZE;

	return temp;
}
void add_cluster(temp_file * file_so_far, int cluster_num)
{ 
	//first cluster is called cluster 2
	//printf("cluster read from 0x%x--%d--first cluster is 0x%x--\n", info.first_cluster + ((cluster_num - 2) * info.cluster_size), info.first_cluster + ((cluster_num - 2) * info.cluster_size), info.first_cluster);
	memcpy(&file_so_far->meat[file_so_far->pointer * info.cluster_size], &file_map[info.first_cluster + ((cluster_num - 2) * info.cluster_size)], info.cluster_size);
	//growing the arraylist
	
	file_so_far->pointer++;
	file_so_far->num_clusters++;
	//printf("%s\n", &file_map[info.first_cluster + ((cluster_num - 2) * info.cluster_size)]);
	if (file_so_far->pointer >= file_so_far->size)
	{
		file_so_far->size *= 2;
		file_so_far->meat = realloc(file_so_far->meat, file_so_far->size * info.cluster_size);
		memset(file_so_far->meat + file_so_far->pointer * info.cluster_size, 0, (file_so_far->size - file_so_far->pointer) * info.cluster_size);
	}
}
int main(int argc, char *argv[])
{
	char cmd_line[MAX_CMD];
	char * file_path = "./fat32.img";
	initilize(file_path);
	read_int(2, 11, file_map);
	/* Parse args and open our image file */

	/* Parse boot sector and get information */

	/* Get root directory address */
	//printf("Root addr is 0x%x\n", root_addr);


	/* Main loop.  You probably want to create a helper function
       for each command besides quit. */

	while(True) {
		bzero(cmd_line, MAX_CMD);
		printf("/]");
		fgets(cmd_line,MAX_CMD,stdin);

		/* Start comparing input */
		if(strncmp(cmd_line,"info",4)==0) {
			print_info();
		}

		else if(strncmp(cmd_line,"open",4)==0) {
			int temp_int;
			scanf("%x", &temp_int);
			temp_file * temp = get_file(file_map , temp_int);	
			printf("%s 0x%x %d\n", temp->meat, read_int(4, 0, temp->meat), read_int(4, 0, temp->meat));
		}

		else if(strncmp(cmd_line,"close",5)==0) {
			printf("Going to close!\n");
		}
		
		else if(strncmp(cmd_line,"size",4)==0) {
			printf("Going to size!\n");
		}

		else if(strncmp(cmd_line,"cd",2)==0) {
			printf("Going to cd!\n");
		}

		else if(strncmp(cmd_line,"ls",2)==0) {
			printf("Going to ls.\n");
		}

		else if(strncmp(cmd_line,"read",4)==0) {
			printf("Going to read!\n");
		}
		
		else if(strncmp(cmd_line,"quit",4)==0) {
			printf("Quitting.\n");
			break;
		}
		else
			printf("Unrecognized command.\n");


	}

	/* Close the file */

	return 0; /* Success */
}

