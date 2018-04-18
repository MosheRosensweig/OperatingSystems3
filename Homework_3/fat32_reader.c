/***********************************************************
 * TODO: Fill in this area and delete this line
 * Name of program:FAT reader
 * Authors: Moshe Rosensweig, Yehuda Gale
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
#include <ctype.h>



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
typedef struct loaded_directory{
	temp_file * directory;
	//if I look at file_offsets[x] it will tell me where the file named file_names[x] starts in the directory above
	int * file_offsets;//cluster number of the start of the file names in the corrisponding spot in the next array 
	char * file_names;
}loaded_directory;
static loaded_directory curr_dir;
void insertion_sort(loaded_directory dir);
void swap(loaded_directory dir, int i, int j);
static temp_file * initilize_temp_file();
//this is the length of file names (including null terminators)
//11 charecters, one null terminator and one .
static int NAME_SIZE = 13; 
void initilize(char * file_path);
int read_int(int num_bytes, int offset, char * source);
void print_info();
void free_loaded(loaded_directory to_free);
void add_cluster(temp_file * file_so_far, int cluster_num);
void load_curr_directory(int dir_start, int first, int size);
void get_current_dir_list(char * directory);
int get_file_from_name(char * dir_to_list);
temp_file * get_file(char * source, int fat_start);
loaded_directory load_directory(int dir_start);
int read_int(int num_bytes, int offset, char * source);
int get_file_start(int dir_location, loaded_directory dir);
void clean_name(char * name_to_clean);
int get_file_size(int file_cur_dir_offset);
char * delete_leading_spaces(char * item);
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
	file_map = mmap(0, sz, PROT_READ, MAP_SHARED, fd, 0);
	info.BPB_BytesPerSec = read_int(2, 11, file_map);
	info.BPB_SecPerClus = read_int(1, 13, file_map);
	info.BPB_RsvdSecCnt = read_int(2, 14, file_map);
	info.BPB_NumFATS = read_int(1, 16, file_map);
	info.BPB_FATSz32 = read_int(4, 36, file_map);
	info.cluster_size = info.BPB_SecPerClus * info.BPB_BytesPerSec;
	info.first_cluster = info.BPB_BytesPerSec * (info.BPB_RsvdSecCnt + info.BPB_NumFATS * info.BPB_FATSz32) + 0;
	//reading the first cluster of root directory and loading as current directory
	curr_dir = load_directory(read_int(4, 44, file_map));  
}
void changeDirectory(char * new_dir_name)
{
	int dir_to_cd;
	dir_to_cd = get_file_from_name(new_dir_name);
	if(dir_to_cd == -1)
	{
		fprintf(stderr,"Error: No Directory with that name\n");
		return;
	}
	else if (strchr(new_dir_name, '.') !=0 && strchr(new_dir_name, '.') != new_dir_name){
		printf("Error: That is a file not a directory\n");
		return;
	}
	int begin_cluster = get_file_start(dir_to_cd, curr_dir);
	free_loaded(curr_dir);
	//it thinks the root cluster starts at zero, correct it to true start of root cluster
	if (begin_cluster == 0)
	{
		begin_cluster = read_int(4, 44, file_map);  
	}
	curr_dir = load_directory(begin_cluster);
}
//loads current directory into curr_dir
loaded_directory load_directory(int dir_start)
{
	loaded_directory to_load;
	
	//extra one for null terminator
	to_load.directory = get_file(file_map, dir_start);
	// this is how many files can fit in the number of clusters we have
	//leave one extra space for null termination
	int number_of_files = (info.cluster_size/32) * to_load.directory->num_clusters + 1;
	//not 100% sure why this works
	to_load.file_names = calloc(number_of_files, sizeof(char) * NAME_SIZE -sizeof(char)+ sizeof(char*));
	to_load.file_offsets = calloc(number_of_files, sizeof(int));
	// int have_more = True;
	int name_we_are_up_to = 0;
	for (int file_location = 0; file_location <= to_load.directory->pointer; file_location += 32)
	{
		//make sure the file name list is null terminated i.e. when we read a string name that starts with a null, it's the end
		to_load.file_names[file_location/32* NAME_SIZE] = '\0';
		//checking if it is deleted
		int first_char = 0;
		first_char = to_load.directory->meat[file_location] & 0xFF;
		//ignore deleted files
		if (first_char == 0xE5){
				continue;
		}
		int attribute = to_load.directory->meat[file_location + 11] & 0xFF;
		//skip long names
		if (attribute == 0x0F || attribute == 0x08){
			continue;
		}
		memcpy(&to_load.file_names[(name_we_are_up_to) * NAME_SIZE], &to_load.directory->meat[file_location] , NAME_SIZE -1);
		to_load.file_names[name_we_are_up_to* NAME_SIZE + NAME_SIZE - 2] = '\0';
		clean_name(&to_load.file_names[(name_we_are_up_to) * NAME_SIZE]);
		to_load.file_offsets[name_we_are_up_to] = file_location;
		name_we_are_up_to++;
	}
	insertion_sort(to_load);
	return to_load;
}
void clean_name(char * name_to_clean)
{
	if(name_to_clean[0] == 0) return;
	char extention[5];
	char final_name[NAME_SIZE]; 
	for(int j =0; j < NAME_SIZE; j++) final_name[j] = 0;
	extention[0] = 0;
	if (name_to_clean[8] != ' '){
		extention[0] = '.';
		extention[1] = name_to_clean[8];
		extention[2] = name_to_clean[9];
		extention[3] = name_to_clean[10];
		extention[4] = 0;
	}
	int i;
	for(i = NAME_SIZE - 5; i >= 0; i--){
		if(name_to_clean[i - 1] != ' '){
			break;
		}
	}
	strncpy(final_name, name_to_clean, i);
	strcat(final_name, extention);
	strcpy(name_to_clean, final_name);
}
void ls(char * dir_to_list)
{
	int dir_to_ls;
	dir_to_ls = get_file_from_name(dir_to_list);
	if(dir_to_ls == -1)
	{
		printf("Error: No Directory with that name\n");
		return;
	}
	else if (strchr(dir_to_list, '.') !=0 && strchr(dir_to_list, '.') != dir_to_list){
		printf("Error: That is a file not a directory\n");
		return;
	}
	//by default assume ls has no argument
	loaded_directory dir = curr_dir; 
	//if it has an argument load the sub-directory
	if(dir_to_ls != -2){
		int begin_cluster = get_file_start(dir_to_ls, curr_dir);
		dir = load_directory(begin_cluster);
	}
	for(int name_start = 0; dir.file_names[name_start] != '\0'; name_start += NAME_SIZE){
		printf("%s \t",&dir.file_names[name_start]);
	}
	printf("\n");
	if(dir_to_ls != -2) free_loaded(dir);

}
void free_loaded(loaded_directory to_free)
{
	free(to_free.file_names);
	free(to_free.file_offsets);
	free(to_free.directory->meat);
	free(to_free.directory);
}
//dir_location 	= byte offset where the record is stored
//dir 			= directory it is stored in
int get_file_start(int dir_location, loaded_directory dir)
{
	int clus_start = 0;
	//first read high and mask off first 2 bytes because the addresses are only 28 bit
	clus_start = dir.directory->meat[dir_location + 20] & 0xFF;
	clus_start <<= 16;
	//then read low
	clus_start |= dir.directory->meat[dir_location + 26] & 0xFFFF;
	return clus_start;
}
int get_file_from_name(char * dir_to_list)
{

	//remove starting space
	dir_to_list = delete_leading_spaces(dir_to_list);
	if (dir_to_list[strlen(dir_to_list) - 1] == '\n')
	{
		dir_to_list[strlen(dir_to_list) - 1] = 0;
	}
	//create padded name to allow comparisons
	//test if it's empty
	if(dir_to_list[0] =='\0') return -2;
	char dir_name[NAME_SIZE];
	for(int i = 0; i < NAME_SIZE; i++) dir_name[i] = '\0';
	strcpy(dir_name, dir_to_list);
	for(int i = 0; i < NAME_SIZE; i++) dir_name[i] = toupper(dir_name[i]);
	for(int name_start = 0; curr_dir.file_names[name_start] != '\0'; name_start += NAME_SIZE){
		if (strncmp(&curr_dir.file_names[name_start], dir_name, NAME_SIZE) == 0){
			return curr_dir.file_offsets[name_start / NAME_SIZE];
		}
	}
	return -1;
	
}
int read_int(int num_bytes, int offset, char * source)
{
	int test = 0;
	memcpy(&test, &file_map[offset], num_bytes);
	return test;
}

void print_info()
{
	fprintf(stdout, "BPB_BytesPerSec is 0x%x, %d\n", info.BPB_BytesPerSec, info.BPB_BytesPerSec);
	fprintf(stdout, "BPB_SecPerClus is 0x%x, %d\n", info.BPB_SecPerClus, info.BPB_SecPerClus);
	fprintf(stdout, "BPB_RsvdSecCnt is 0x%x, %d\n", info.BPB_RsvdSecCnt, info.BPB_RsvdSecCnt);
	fprintf(stdout, "BPB_NumFATS is 0x%x, %d\n", info.BPB_NumFATS, info.BPB_NumFATS);
	fprintf(stdout, "BPB_FATSz32 is 0x%x, %d\n", info.BPB_FATSz32, info.BPB_FATSz32);

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
		//might be an off by one error
		//maybe print whole table
		next_cluster = (read_int(4, next_cluster * 4 + fat_beginning, source) & 0x0FFFFFFF);
	}
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
	temp->size = INITIAL_FILE_SIZE * info.cluster_size;

	return temp;
}
int check_and_get_file_size(char * file_name, int file_cur_dir_offset)
{
	// int file_cur_dir_offset = get_file_from_name(file_name);
	if(file_cur_dir_offset < 0)
	{
		printf("Error: No File with that name\n");
		return -1;
	}
	else if (strchr(file_name, '.') ==0 || strchr(file_name, '.') == file_name){
		printf("Error: That is a directory not a file\n");
		return -1;
	}
	return get_file_size(file_cur_dir_offset);
}
void add_cluster(temp_file * file_so_far, int cluster_num)
{ 
	//first cluster is called cluster 2
	memcpy(&file_so_far->meat[file_so_far->pointer], &file_map[info.first_cluster + ((cluster_num - 2) * info.cluster_size)], info.cluster_size);
	//growing the arraylist
	
	file_so_far->pointer += info.cluster_size;
	file_so_far->num_clusters++;
	if (file_so_far->pointer >= file_so_far->size)
	{
		file_so_far->size *= 2;
		file_so_far->meat = realloc(file_so_far->meat, file_so_far->size);
		memset(file_so_far->meat + file_so_far->pointer, 0, (file_so_far->size - file_so_far->pointer));
	}
}
void read_file(char * file_name, int start, int num_bytes)
{

	// char file_name_to_read[NAME_SIZE];
	// for(int i = 0; i < NAME_SIZE; i++) file_name_to_read[i] = '\0';
	// // file_name = delete_leading_spaces(file_name);
	// printf("--%s-- --%s-- --%d--\n", file_name_to_read, file_name, (int) (strchr(file_name, ' ') - file_name));
	// strncpy(file_name_to_read, file_name, (int) (strchr(file_name, ' ') - file_name));
	// printf("--%s-- --%s-- --%d--\n", file_name_to_read, file_name, (int) (strchr(file_name, ' ') - file_name));
	int file_cur_dir_offset = get_file_from_name(file_name);
	// printf("file_name:%s: file_offset:%x:\n",file_name,file_cur_dir_offset );
	// if(file_cur_dir_offset < 0)
	// {
	// 	printf("Error: No File with that name\n");
	// 	return;
	// }
	// else if (strchr(file_name, '.') ==0 || strchr(file_name, '.') == file_name){
	// 	printf("Error: That is a directory not a file\n");
	// 	return;
	// }
	// int file_size = get_file_size(file_cur_dir_offset);
	// printf("%d\n", file_size);
	int file_size = check_and_get_file_size(file_name, file_cur_dir_offset);
	if (file_size == -1)
	{
		return;
	}
	if (file_size < start + num_bytes)
	{
		printf("Error: attempt to read beyond end of file\n");
		return;
	}
	temp_file * file_to_read = get_file(file_map, get_file_start(file_cur_dir_offset, curr_dir));
	file_to_read->meat[start + num_bytes] = 0;
	printf("%s\n", &file_to_read->meat[start]);
	//file_to_read->meat[start + num_bytes] = 8;
	free(file_to_read->meat);
	free(file_to_read);
}
int get_file_size(int file_cur_dir_offset)
{
	int * file_size = (int*)&curr_dir.directory->meat[file_cur_dir_offset + 28];
	return *file_size;
}
/*
	Description: Prints the volume name of the file system image. 
	If there is a volume name it will be found in the root directory. 
	If there is no volume name, print “Error: volume name not found.”
*/
void volume()
{
	//According to the specs, at offset 71 or 0x47, is the volLab (I assume means volume lable) - it's 11 bytes
	char volLab[12];
	volLab[11] = '\0';
	strncpy(volLab, file_map+71, 11);
	if (strcmp(volLab, "NO NAME    ") == 0)
	{
		fprintf(stderr, "Error: volume name not found\n");
	}
	printf("Volume label is: %s\n", volLab);

}
char * delete_leading_spaces(char * item)
{
	while(item[0] == ' '){
		item = &item[1];
	}
	return item;
}
void print_stat(char * dir_name)
{
	int dir_to_stat;
	dir_to_stat = get_file_from_name(dir_name);
	if(dir_to_stat < 0)
	{
		printf("Error: file/directory does not exist\n");
		return;
	}
	int size = get_file_size(dir_to_stat);
	int next_clus_num = get_file_start(dir_to_stat, curr_dir);
	int attr = curr_dir.directory->meat[dir_to_stat + 11] & 0xFF;
	printf("Size is %d\n", size);
	printf("Attributes");
	if(attr & 0x01){
		printf(" ATTR_READ_ONLY");
	}
	if(attr & 0x02){
		printf(" ATTR_HIDDEN");
	}
	if(attr & 0x04){
		printf(" ATTR_SYSTEM");
	}
	if(attr & 0x08){
		printf(" ATTR_VOLUME_ID");
	}
	if(attr & 0x10){
		printf(" ATTR_DIRECTORY");
	}
	if(attr & 0x20){
		printf(" ATTR_ARCHIVE");
	}
	printf("\n");	
	printf("Next cluster number is 0x%x\n", next_clus_num);
	
}
int main(int argc, char *argv[])
{
	char cmd_line[MAX_CMD];
	char * file_path;
	if(argc > 1){
		file_path = argv[1];
	}
	else{
		file_path = "./fat32.img";
	}
	initilize(file_path);
	read_int(2, 11, file_map);
	/* Parse args and open our image file */

	/* Parse boot sector and get information */

	/* Get root directory address */


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
		else if(strncmp(cmd_line,"size",4)==0) {
			int size = check_and_get_file_size(&cmd_line[5], get_file_from_name(&cmd_line[5]));
			printf("Size is %d\n", size);
		}
		else if(strncmp(cmd_line,"volume",6)==0) {
			volume();
		}
		// else if(strncmp(cmd_line,"open",4)==0) {
		// 	int temp_int;
		// 	scanf("%x", &temp_int);
		// 	temp_file * temp = get_file(file_map , temp_int);	
		// 	printf("%s 0x%x %d\n", temp->meat, read_int(4, 0, temp->meat), read_int(4, 0, temp->meat));
		// }

		else if(strncmp(cmd_line,"cd",2)==0) {
			changeDirectory(&cmd_line[3]);
		}

		else if(strncmp(cmd_line,"ls",2)==0) {
			ls(&cmd_line[2]);
		}
		else if(strncmp(cmd_line,"stat",4)==0) {
			print_stat(&cmd_line[4]);
		}
		else if(strncmp(cmd_line,"read",4)==0) {
			int position;
			int num_bytes;
			char file_name[NAME_SIZE + 1];
			file_name[0] = ' ';
			char dummy[4];
			int test = sscanf(cmd_line, "%s %s %d %d", dummy, &file_name[1], &position, &num_bytes);
			if (test != 4)
			{
				fprintf(stderr, "%s\n", "Error: please enter correct parameters for read");
				continue;
			}

			printf("%d\n", test);
			read_file(file_name, position, num_bytes);
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
void insertion_sort(loaded_directory dir)
{
	// int index_of_next_spot = 0;
	// int index_of_min_value = index_of_next_spot;
	for(int outer = 0; dir.file_names[outer*NAME_SIZE] != '\0'; outer += 1){
		for(int inner = outer; dir.file_names[inner*NAME_SIZE] != '\0'; inner += 1){
			if(strncmp(&dir.file_names[inner*NAME_SIZE], &dir.file_names[outer*NAME_SIZE], NAME_SIZE) < 0){
				swap(dir, inner, outer);
			}
		}

	}

}
void swap(loaded_directory dir, int i, int j)
{
	char tmp_name[NAME_SIZE];
	int tmp_loc;
	strncpy(tmp_name, &dir.file_names[i * NAME_SIZE], NAME_SIZE);
	strncpy(&dir.file_names[i * NAME_SIZE], &dir.file_names[j * NAME_SIZE], NAME_SIZE); 
	strncpy(&dir.file_names[j * NAME_SIZE], tmp_name, NAME_SIZE);
	tmp_loc = dir.file_offsets[i];
	dir.file_offsets[i] = dir.file_offsets[j];
	dir.file_offsets[j] = tmp_loc;
	
}

