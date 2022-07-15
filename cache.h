#include "err_control.h"

typedef unsigned char byte;
static size_t cache_capacity;
static size_t max_storage_file;
static size_t used_mem;
static size_t files_in_cache;

//mutex cache
pthread_mutex_t mtx;

typedef struct _file
{
	char* f_name;			//nome del file
	size_t f_size;			//lung. in byte
	char* f_data;			//array di byte di lung. f_size
	byte f_read;			//flag lettura
	byte f_write;			//flag scrittura
	int f_lock;				//flag lock
	pthread_mutex_t mtx;	//mutex nodo
	struct _file* next;		//nodo successivo
}file;

//prototipi 
void create_cache(int mem_size, int max_file_in_cache);
void cache_capacity_update(int dim_file, int new_file_or_not);
file* cache_research(file* cache, char* f_name);
int cache_removeFile(file** cache, char* f_name, int id);
int cache_duplicate_control(file* cache, char* f_name, int id);
void print_queue(file* cache);
file* cache_dealloc(file* cache);
void print_queue(file* cache);


file* replace_to_append(file** cache, char* f_name, char* f_data, size_t dim_f, int id);
int cache_appendToFile(file** cache, char* f_name, char* f_data, size_t dim_f, int id, file** file_expelled);

int cache_insert(file** cache, char* f_name, char* f_data, size_t dim_f, int id, file** file_expelled);
file* replace_to_write(file** cache, char* f_name, char* f_data, size_t dim_f, int id);
int cache_enqueue(file** cache, char* f_name, char* f_data, size_t dim_f, int id);

int cache_readFile(file* cache, char* f_name, char** buf, size_t* dim_buf, int id);
int cache_readNFile(file* cache, int N, int id, file*** array);

int cache_lockFile(file* cache, char* f_name, int id);
int cache_unlockFile(file* cache, char* f_name, int id);
