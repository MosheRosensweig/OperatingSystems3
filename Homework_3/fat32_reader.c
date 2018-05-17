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
#include <time.h>


/* Put any symbolic constants (defines) here */
#define True 1  /* C has no booleans! */
#define False 0
#define INITIAL_FILE_SIZE 2
#define MAX_CMD 80
#define DEFAULT_NUM_FILES 2

/* This is the main function of your project, and it will be run
 * first before all other functions.
 */
static unsigned char *file_map;
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
	unsigned char * meat;
} temp_file;
fat_info info;
typedef struct loaded_directory{
	temp_file * directory;
	//if I look at file_offsets[x] it will tell me where the file named file_names[x] starts in the directory above
	int * file_offsets;//cluster number of the start of the file names in the corrisponding spot in the next array 
	char * file_names;
	int fmap_start;
	int fat_start;

}loaded_directory;
typedef struct free_list{
	int head;
	int foot;
	int total_size;
	int current_size;
	int * list;
}free_list;
free_list * free_clusters;
static loaded_directory curr_dir;
void insertion_sort(loaded_directory dir);
void swap(loaded_directory dir, int i, int j);
static temp_file * initilize_temp_file();
//this is the length of file names (including null terminators)
//11 charecters, one null terminator and one .
static int NAME_SIZE = 13; 
void initilize(char * file_path);
int allocate_space(int numOfClusters);
void print_freelist_info();
int read_int(int num_bytes, int offset, unsigned char * source);
void print_info();
void make_file(char * file_name, int num_bytes);
void copyTwo(int start_address, unsigned int data);
int * peek_free_list(int num_to_peek);
void free_loaded(loaded_directory to_free);
int add_cluster(temp_file * file_so_far, int cluster_num, int dummy_var);
void load_curr_directory(int dir_start, int first, int size);
void get_current_dir_list(char * directory);
int get_file_from_name(char * dir_to_list);
temp_file * get_file(unsigned char * source, int fat_start);
int find_byte(temp_file * dummy_file, int cluster_num, int offset_left);
// temp_file * traverse_fat(unsigned char * source, int fat_start, temp_file (*start_function)(void), void (*step_function)(temp_file *, int));
loaded_directory load_directory(int dir_start);
int write_dir_entry(char * file_name, int num_bytes, long int pointer);
int read_int(int num_bytes, int offset, unsigned char * source);
int get_file_start(int dir_location, loaded_directory dir);
void clean_name(char * name_to_clean);
int get_file_size(int file_cur_dir_offset);
char * delete_leading_spaces(char * item);
void push_free_list(int to_push, free_list * push_list);
free_list * init_free_list(unsigned char * source, int fat_start, int fat_size);
temp_file * dummy_func();
temp_file * traverse_fat(unsigned char * source, int fat_start, temp_file *(*start_function)(), int (*step_function)(temp_file *, int, int), int goal_offset);
int zero_out_cluster_and_add_to_free(temp_file * dummy, int cluster_location, int dummy_var);
void initilize(char * file_path)
{
	int fd;
	//used https://www.linuxquestions.org/questions/programming-9/mmap-tutorial-c-c-511265/
	//and https://jameshfisher.com/2017/01/28/mmap-file-write.html
	fd = open(file_path,  O_RDWR | O_CREAT, (mode_t)0600);
	FILE * fp = fdopen(fd, "r");
	if (fd == -1) {
		perror("Error opening file for reading");
		exit(EXIT_FAILURE);
	}
	//used https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
	fseek(fp, 0, SEEK_END);
	int sz = ftell(fp);
	file_map = mmap(0, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	info.BPB_BytesPerSec = read_int(2, 11, file_map);
	info.BPB_SecPerClus = read_int(1, 13, file_map);
	info.BPB_RsvdSecCnt = read_int(2, 14, file_map);
	info.BPB_NumFATS = read_int(1, 16, file_map);
	info.BPB_FATSz32 = read_int(4, 36, file_map);
	info.cluster_size = info.BPB_SecPerClus * info.BPB_BytesPerSec;
	info.first_cluster = info.BPB_BytesPerSec * (info.BPB_RsvdSecCnt + info.BPB_NumFATS * info.BPB_FATSz32) + 0;
	//reading the first cluster of root directory and loading as current directory
	curr_dir = load_directory(read_int(4, 44, file_map));
	free_clusters = init_free_list(file_map, info.BPB_RsvdSecCnt * info.BPB_BytesPerSec, info.BPB_FATSz32 * info.BPB_BytesPerSec / 4);
}
free_list * init_free_list(unsigned char * source, int fat_start, int fat_size)
{
	free_list * new_list = calloc(1, sizeof(free_list));
	new_list->list = calloc(fat_size, sizeof(int));
	new_list->total_size = fat_size;
	new_list->current_size = 0;
	new_list->head = 0;
	new_list->foot = 0;
	for (int i = 2; i < fat_size * 4; i += 4)
	{
		// printf("i is %d item at i is %d offset is %x\n",i, read_int(4, fat_start + i, file_map), fat_start + i);
		if (read_int(4, fat_start + i, file_map) == 0)
		{
			push_free_list(i, new_list);
		}
	}
	return new_list;
}
void print_freelist_info()
{
	int * first_3 = peek_free_list(3);
	printf("There are %d clusters free\n", free_clusters->current_size);
	printf("the first 3 are [%x, %x, %x]\n", first_3[0], first_3[1], first_3[2]);
	free(first_3);
}
int pop_free_list(free_list * pop_list)
{
/*
int head;
	int foot;
	int size;
	int clusters_left;
	int * list;
*/	if(pop_list->current_size <= 0)
	{
		fprintf(stderr, "Error: free_list is empty\n");
		return 0;
	}
	int return_clus = pop_list->list[pop_list->head];
	pop_list->head = (pop_list->head+1)%pop_list->total_size;
	pop_list->current_size--;
	return return_clus;

}
void push_free_list(int to_push, free_list * push_list)
{
	if (push_list->current_size >= push_list->total_size)
	{
		fprintf(stderr, "Error: free_list is full\n");
		return;
	}
	push_list->list[push_list->foot] = to_push;
	push_list->foot = (push_list->foot+1)%push_list->total_size;
	push_list->current_size++;
}
int * peek_free_list(int num_to_peek)
{
	int * first_3 = calloc(num_to_peek, sizeof(int));
	for (int i = 0; i < num_to_peek; i++)
	{
		first_3[i] = free_clusters->list[(free_clusters->head + i) % free_clusters->total_size];
	}
	return first_3;
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
	printf("offset after loading %x\n",to_load.directory->meat[251] );
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
	to_load.fat_start = dir_start;
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
/*
	Description: mark the designated file as deleted in the current directory and all its contents as free in the FAT. 
	ls should no longer list the file, though its directory entry should still be visible in hexedit, marked as deleted.
*/
void delete_file(char * file_name)
{
	/*
		1] Find the file in curr_dir
		2] add that file's cluster to freelist
		3] Remove that file from curr_dir and make sure to clean up curr_dir


	*/
	if (strchr(file_name, '.') ==0 || strchr(file_name, '.') == file_name){
		printf("Error: That is a directory not a file\n");
		return;
	}
	int file_index_in_curr_dir = get_file_from_name(file_name);
	if(file_index_in_curr_dir < 0)
	{
		printf("Error: No File with that name\n");
		return;
	}
	//this is the start of the file in the FAT
	int file_start = get_file_start(file_index_in_curr_dir, curr_dir);
	//this removes the file from the fat and adds each cluster to the free list
	traverse_fat(file_map, file_start, dummy_func, zero_out_cluster_and_add_to_free, 0);
	printf("\tPARAM %d %d\n",curr_dir.fat_start, file_index_in_curr_dir);
	//this looks through the directory called curr_dir and overwrites the name to start with 0xE5
	traverse_fat(file_map, curr_dir.fat_start, dummy_func, find_byte, file_index_in_curr_dir);
	curr_dir = load_directory(curr_dir.fat_start);
	// printf("%s\n", &file_to_read->meat[start]);
	//file_to_read->meat[start + num_bytes] = 8;
} 
//dir_location 	= byte offset where the record is stored
//dir 			= directory it is stored in
int get_file_start(int dir_location, loaded_directory dir)
{
	printf("stat after getting start %x\n",curr_dir.directory->meat[251] );
	printf("stat after getting start %x\n",dir.directory->meat[251] );
	printf("dir_location %d\n", dir_location);
	printf("gfs later mapped 0x%x\n", file_map[1049851]);
	printf("dir_location + 20 is: %d\n", dir_location + 20);
	int clus_start = 0;
	//first read high and mask off first 2 bytes because the addresses are only 28 bit
	clus_start = dir.directory->meat[dir_location + 20] & 0xFF;
	clus_start = (dir.directory->meat[dir_location + 21] & 0xFF ) << 8;
	clus_start <<= 16;
	//then read low
	clus_start |= dir.directory->meat[dir_location + 26] & 0xFF;
	clus_start |= (dir.directory->meat[dir_location + 27] & 0xFF) << 8;
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
int read_int(int num_bytes, int offset, unsigned char * source)
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
int zero_out_cluster_and_add_to_free(temp_file * dummy, int cluster_location, int dummy_var)
{
	int fat_beginning = info.BPB_RsvdSecCnt * info.BPB_BytesPerSec;
	int fat_beginning2 = fat_beginning + info.BPB_FATSz32 * 4;
	push_free_list(cluster_location, free_clusters);
	printf("overwriteing: Ox%x and : 0x%x\n",fat_beginning + cluster_location * 4,  fat_beginning2 + cluster_location * 4);
	memset(&file_map[fat_beginning + cluster_location * 4], 0, 4);
	if (info.BPB_NumFATS >= 2) memset(&file_map[fat_beginning2 + cluster_location * 4], 0, 4);
	return 0;
}
temp_file * dummy_func(){
	return (temp_file*) NULL;
}
temp_file * store_offset()
{
	temp_file * start_file = calloc(1, sizeof(temp_file));
	start_file->size = 0;
	return start_file;
}
int fill_file(temp_file *modifier, int current_cluster, int goal_bytes)
{
	char * toWrite = "New File.\r\n";
	// int length = strlen()
	int start_val = info.first_cluster + ((current_cluster - 2) * info.cluster_size);
	int i = 0;
	for (i = start_val; i < start_val + (info.BPB_BytesPerSec * info.BPB_SecPerClus) &&  modifier->size < goal_bytes; i++)
	{
		file_map[i] = toWrite[modifier->size % 11];
		modifier->size++;
		//11 is size of toWrite
	}
	if (modifier->size == goal_bytes)
	{
		file_map[i - 1] = '\0';
		printf("%x\n", i);
		return -1;
	}
	return goal_bytes;
}
temp_file * traverse_fat(unsigned char * source, int fat_start, temp_file *(*start_function)(), int (*step_function)(temp_file *, int, int), int goal_offset)
{
	printf("this is the start %d\n", fat_start);
	temp_file * new_file = start_function();
	//finds first fat entry byte number (4 bytes per entry) i.e the begging of the FAT linked list 
	// long int start_entry = info.BPB_RsvdSecCnt * info.BPB_BytesPerSec + fat_start * 4;
	int next_cluster = fat_start;
	int fat_beginning = info.BPB_RsvdSecCnt * info.BPB_BytesPerSec;
	int tmp_cluster;
	//while we have a valid next cluster
	//traverse the fat, getting data as we go
	while(next_cluster && 2 <= next_cluster && next_cluster <= 0x0FFFFFEF ){
		tmp_cluster = next_cluster;
		next_cluster = (read_int(4, tmp_cluster * 4 + fat_beginning, source) & 0x0FFFFFFF);
		printf("\nthis is the step input: tmp_cluster: %d goal_offset: %d\n",tmp_cluster, goal_offset);
		goal_offset = step_function(new_file, tmp_cluster, goal_offset);
		if (goal_offset == -1)
		{
			return new_file;
		}
		//might be an off by one error
		//maybe print whole table
		printf("next_cluster is %d, tmp_cluster is %d\n", next_cluster, tmp_cluster);
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
temp_file * get_file(unsigned char * source, int fat_start)
{
	return traverse_fat(source, fat_start, initilize_temp_file, add_cluster, 0);
}
/*
temp_file * get_file(unsigned char * source, int fat_start, (temp_file *) start_function(void), void *step_function(temp_file *, int))
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
*/
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
int find_byte(temp_file * dummy_file, int cluster_num, int offset_left)
{
	printf("offset_left %x cluster_num %d\n",offset_left, cluster_num );
	if (offset_left > (info.cluster_size))
	{
		return offset_left - (info.cluster_size);
	}
	else{
		printf("yay\n");
		file_map[info.first_cluster + ((cluster_num - 2) * info.cluster_size) + offset_left] = 0xE5;
		return -1;
	}
}
int add_cluster(temp_file * file_so_far, int cluster_num, int dummy_var)
{ 
	printf("later mapped 0x%x\n", file_map[1049851]);
	printf("mapping + %d \n", info.first_cluster + ((cluster_num - 2) * info.cluster_size));
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
	return 0;
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
	strncpy(volLab, (char *) file_map+71, 11);
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
temp_file * hack_temp_file_to_store_results()
{
	temp_file * hacked_file = calloc(1, sizeof(temp_file));
	//this tells us if we need to make zero space
	hacked_file->num_clusters = 0;
	//this tells us where the space is
	hacked_file->pointer = -1;
	//this tells us the last cluster number we only care about this if we fail
	hacked_file->size = -1;
	return hacked_file;
}
/*
  Description: In the current directory, create a new file with the given name (8+3 name will be specified. 
  Also initialize it to the given size by assigning it clusters that are currently free on the FAT.
  Make sure the directory entry reflects the assigned size. 
  The contents of your newly created file should be the string “New File.\r\n” repeated over and over.
  (\r\n stands for the Microsoft newline character sequence).
*/
int find_space_in_directory(temp_file * hacked_file, int current_clus, int dummy)
{
	int start_val = info.first_cluster + ((current_clus - 2) * info.cluster_size);
	for (int i = start_val; i < start_val + info.cluster_size; i += 32)
	{
		if (file_map[i] == 0xE5 || file_map[i] == 0)
		{
			//if we have a zero in the middle of a cluster
			if (file_map[i] == 0 && i + 32 != start_val + info.cluster_size)
			{
				//we need to add a zero after this cluster
				hacked_file->num_clusters = 1;
			}
			//the space is here
			hacked_file->pointer = i;
			//we return -1 to let the calling function know we can stop searching
			return -1;
		}
	}
	//store most recent cluster number in case this is the last cluster
	hacked_file->size = current_clus;
	return 0;
}
void make_file(char * file_name, int num_bytes)
{
	/**
	  1] Find the end of the current direcotry
	  2] Check if there is enough space in this cluster to hold another 32 bits (i.e. the size of a dir_struct)
	  3] If no, get pop the freelist and add that to curr_dur's list --> make sure to update the actual img as well
	  4] Make a new dir_stuct. Zero out the location in memory, and then "or" this to that location. (i.e. copy it over)
	  	[a] set the name
	  	[b] set the attr
	  	[c] set the date
	  	[d] etc... see page 23

	  	[now actually make the file]
	  1] We neet to make a file struct (?) Can we just reuse the "loaded_dir" struct we already have? Should we?
	  2] Figure out how many clusters we will need to store the file 
	  3] Pop the free list that many times and add them to the file's list of clustors, after the last one, put an end of file
	  	 i.e. take number_of_clusters+1, the exta one is needed to hold the tail --> make sure to update the actual img as well
		 --> Make sure to split the first cluster number into high bits and low bits and store it in the file
	  4] Iterate throught the list of clusters and write “New File.\r\n” as many times as needed
	  5] Done
	*/
	// int endOfFileName = strlen(file_name);
	// if (file_name[endOfFileName - 4] != '.')
	// {
	// 	fprintf(stderr, "%s\n", );
	// }
	temp_file * space = traverse_fat(file_map, curr_dir.fat_start, hack_temp_file_to_store_results,find_space_in_directory , 0);
	//this tells us if we never found a free space
	
	if (space->pointer == -1){
		//if no free space allocate more space
		int new_space = allocate_space(1);
		int fat_beginning = info.BPB_RsvdSecCnt * info.BPB_BytesPerSec;
		int fat_beginning2 = fat_beginning + info.BPB_FATSz32 * 4;
		memcpy(&file_map[fat_beginning + space->size * 4], &new_space, 4);
		if (info.BPB_NumFATS >= 2) memcpy(&file_map[fat_beginning2 + space->size * 4], &new_space, 4);
		space->pointer = info.first_cluster + ((new_space - 2) * info.cluster_size);
		space->num_clusters = 1;
		// int * 
		//set the two new locations to be acceptable last values in the fat
		//set the two fats to point to this new location
	}
	//if we need to add a zero add a zero
	if (space->num_clusters)
	{
		file_map[space->pointer + 32] = 0;
	}
	//copy over the first 8 chars padding with spaces
	// char found = 0;
	// for (int i = 0; i < 8; i++)
	// {
	// 	if (file_name[i] == '.')
	// 	{
	// 		found = 1;
	// 	}
	// 	if (found)
	// 	{
	// 		file_map[space->pointer + i] = ' ';	
	// 	}
	// 	else{
	// 		file_map[space->pointer + i] = toupper(file_name[i]);	
	// 	}
	// }
	// //write extention
	// int end = strlen(file_name);
	// for (int i = 0; i < 3; i++)
	// {
	// 	file_map[space->pointer + 8 + i] = toupper(file_name[end - 3 + i]);
	// }
	// for (int i = 0; i < 11; ++i)
	// {
	// 	fprintf(stderr, "--%c--\n", file_map[space->pointer + i]);
	// }

	// file_map[space->pointer + 11] = 0x20;
	// file_map[space->pointer + 12] = 0x00;
	// file_map[space->pointer + 13] = 0x00;
	// //used https://stackoverflow.com/questions/1442116/how-to-get-the-date-and-time-values-in-a-c-program
	// time_t tim = time(NULL);
	// struct tm tm = *localtime(&tim);
	// int create_time = (tm.tm_sec >> 1) + (tm.tm_min << 5) + (tm.tm_hour << 11);
	// int create_date = tm.tm_mday + (tm.tm_mon << 5) + ((tm.tm_year - 80) << 9);
	// memcpy(&file_map[14], &create_time, 2);
	// memcpy(&file_map[16], &create_date, 2);
	// memcpy(&file_map[18], &create_date, 2);
	// memcpy(&file_map[22], &create_time, 2);
	// memcpy(&file_map[24], &create_date, 2);
	// int fileFatStart = allocate_space(num_bytes / (info.BPB_BytesPerSec * info.BPB_SecPerClus) + !!(num_bytes % (info.BPB_BytesPerSec * info.BPB_SecPerClus)));
	// unsigned int fileFatStartHi = (fileFatStart & 0xFFFF0000) >> 16;
	// unsigned int fileFatStartLo = (fileFatStart & 0x0000FFFF);
	// memcpy(&file_map[20], &fileFatStartHi, 2);
	// memcpy(&file_map[26], &fileFatStartLo, 2);
	// memcpy(&file_map[28], &num_bytes, 4);
	int fat_start = write_dir_entry(file_name, num_bytes, space->pointer);
	if (fat_start == -1)
	{
		printf("Error please enter valid file name\n");
		return;
	}
	curr_dir = load_directory(curr_dir.fat_start);
	traverse_fat(file_map, fat_start ,store_offset, fill_file, num_bytes);
}
int write_dir_entry(char * file_name, int num_bytes, long int pointer)
{
	printf("%lx\n", pointer);
	char found = 0;
	for (int i = 0; i < 8; i++)
	{
		if (file_name[i] == '.')
		{
			found = 1;
		}
		if (found)
		{
			file_map[pointer + i] = ' ';	
		}
		else{
			file_map[pointer + i] = toupper(file_name[i]);	
		}
	}
	//write extention
	// int end = strlen(file_name);
	// char * lastDot = strrchr(file_name, '.');
	// lastDot++;
	// int counter = 0;
	// found = 0;
	// for (char * i = lastDot; i < file_name + end; i++)
	// {
	// 	file_map[pointer + 8 + counter] = toupper(*i);
	// 	counter++;
	// 	if (counter >= 3)
	// 	{
	// 		return -1;
	// 	}
	// }

	//write extention
	found = 0;
	int end = strlen(file_name);
	int j = 0; //find the index of the dot
	for(j = end-1; file_name[j] != '.' && j > 0; j--);
	printf("j is%d end is%d\n",j, end );
	if(j < 0 || j <= end-5) return -1; //make sure the input is correct otherwise fail
	printf("j is%d end is%d\n",j, end );
	for (int i = 0; i < 3; i++)
	{
		// printf("--%c--\n", );
		if (i == (end -1 - j))
		{
			found = 1;
		}
		if (found)
		{
			file_map[pointer + i + 8] = ' ';	
		}
		else{
			file_map[pointer + i + 8] = toupper(file_name[i + j + 1]);	
		}
	}


	// for (int i = 0; i < 11; ++i)
	// {
	// 	fprintf(stderr, "--%c--\n", file_map[pointer + i]);
	// }

	file_map[pointer + 11] = 0x20;
	file_map[pointer + 12] = 0x00;
	file_map[pointer + 13] = 0x00;
	//used https://stackoverflow.com/questions/1442116/how-to-get-the-date-and-time-values-in-a-c-program
	time_t tim = time(NULL);
	struct tm tm = *localtime(&tim);
	printf("%d/%d/%d %d:%d:%d\n",(tm.tm_mon + 1),tm.tm_mday, (tm.tm_year - 80), tm.tm_hour,tm.tm_min, tm.tm_sec );
	int create_time = (tm.tm_sec >> 1) + (tm.tm_min << 5) + (tm.tm_hour << 11);
	int create_date = tm.tm_mday + ((tm.tm_mon + 1) << 5) + ((tm.tm_year - 80) << 9);
	memcpy(&file_map[pointer + 14], &create_time, 2);
	memcpy(&file_map[pointer + 16], &create_date, 2);
	memcpy(&file_map[pointer + 18], &create_date, 2);
	memcpy(&file_map[pointer + 22], &create_time, 2);
	memcpy(&file_map[pointer + 24], &create_date, 2);
	int fileFatStart = allocate_space(num_bytes / (info.BPB_BytesPerSec * info.BPB_SecPerClus) + !!(num_bytes % (info.BPB_BytesPerSec * info.BPB_SecPerClus)));
	unsigned int fileFatStartHi = (fileFatStart & 0xFFFF0000) >> 16;
	unsigned int fileFatStartLo = (fileFatStart & 0x0000FFFF);
	// char fileFatStartHi2 = (fileFatStartHi) & 0xff;
	// char fileFatStartHi21 = (fileFatStartHi >> 8) & 0xff;
	// char fileFatStartLo2 = (fileFatStartLo) & 0xff;
	// char fileFatStartLo21 = (fileFatStartLo >> 8) & 0xff;

	// file_map[pointer + 26] = fileFatStartLo2;
	// file_map[pointer + 27] = fileFatStartLo21;
	// file_map[pointer + 20] = fileFatStartHi2;
	// file_map[pointer + 21] = fileFatStartHi21;

	copyTwo(pointer + 20, fileFatStartHi);
	copyTwo(pointer + 26, fileFatStartLo);
	// memcpy(&file_map[pointer + 20], &fileFatStartHi, 2);
	// memcpy(&file_map[pointer + 26], &fileFatStartLo, 2);
	printf("data on drive 3 %x\n", file_map[pointer + 21]);
	memcpy(&file_map[pointer + 28], &num_bytes, 4);
	printf("hi:%x lo:%x\n",fileFatStartHi, fileFatStartLo);
	printf("%x\n", fileFatStart);
	printf("later mapped 0x%x\n", file_map[1049851]);
	return fileFatStart;
}
void copyTwo(int start_address, unsigned int data)
{
	// unsigned int * tempMap = (unsigned int *) file_map;
	// file_map[]
	printf("%x\n", data);
	file_map[start_address] = 0;
	file_map[start_address + 1] = 0;
	printf("here is data %x here is address\n",  (unsigned char) (data & 0xff));
	printf("start_address %x\n", start_address + 1);
	file_map[start_address] = (unsigned char) (data & 0xff);
	printf("here is data on drive%x\n",(unsigned char) file_map[start_address]);
	data = data >> 8;

	file_map[start_address + 1] = (unsigned char) (data & 0xff);
	printf("here is data 2 0x%x\n",(unsigned char) (data & 0xff));
	printf("here is data on drive2 0x%x\n",file_map[start_address + 1]);
	printf("start address is: %d\n", start_address);
}
int allocate_space(int numOfClusters)
{
	int fat_beginning = info.BPB_RsvdSecCnt * info.BPB_BytesPerSec;
	int fat_beginning2 = fat_beginning + info.BPB_FATSz32 * 4;
	int newClusNum = 0;
	int prev = 0;
	int ret = 0;
	//chaining clusters together in fat
	for (int i = 0; i < numOfClusters; i++)
	{
		newClusNum = pop_free_list(free_clusters);
		if (prev != 0)
		{
			memcpy(&file_map[fat_beginning + prev * 4], &newClusNum, 4);
			if (info.BPB_NumFATS >= 2) memcpy(&file_map[fat_beginning2 + prev * 4], &newClusNum, 4);
		}
		else{
			//store the beggining of the list
			ret = newClusNum;
		}
		prev = newClusNum;
		/* code */
	}
	memset(&file_map[fat_beginning + newClusNum * 4], 0xFF, 4);
	if (info.BPB_NumFATS >= 2) memset(&file_map[fat_beginning2 + newClusNum * 4], 0xFF, 4);
	return ret;

}
void print_stat(char * dir_name)
{
	printf("stat later mapped 0x%x\n", file_map[1049851]);
	printf("stat after loading %x\n",curr_dir.directory->meat[251] );
	int dir_to_stat;
	dir_to_stat = get_file_from_name(dir_name);
	printf("dir offset %x", dir_to_stat);
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
		else if(strncmp(cmd_line, "freelist", 8) == 0) {
			print_freelist_info();
		}
		else if(strncmp(cmd_line, "delete", 6) == 0) {
			delete_file(&cmd_line[6]);
		}
		else if(strncmp(cmd_line,"ls",2)==0) {
			ls(&cmd_line[2]);
		}
		else if(strncmp(cmd_line,"stat",4)==0) {
			print_stat(&cmd_line[4]);
		}
		else if(strncmp(cmd_line,"newfile",7)==0) {
			char file_name[NAME_SIZE];
			char dummy[7];
			int num_bytes; //I'm assuimg "size" is in bytes
			int test = sscanf(cmd_line, "%s %s %d", dummy, file_name, &num_bytes);
			if (test != 3)
			{
				fprintf(stderr, "%s\n", "Error: please enter correct parameters for newfile");
				continue;
			}
			printf("%d\n", test);
			make_file(file_name, num_bytes);
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

