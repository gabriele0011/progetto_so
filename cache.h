#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"
#include "list.h"
typedef unsigned char byte;

static size_t cache_capacity;
static size_t max_storage_file;
static size_t used_mem;
static size_t files_in_cache;

//mutex cache

//struct elemento cache
typedef struct _file
{
	char* f_name;			//nome del file
	size_t f_size;			//dim file
	char* f_data;			//array di byte di lung. f_size
	int f_lock;				//flag lock
	byte f_open;			//file aperto
	pthread_mutex_t mtx;		//mutex
	node* id_list;			//lista dei processi che hanno aperto il file
	struct _file* next;		//ptr nodo successivo
}file;


//prototipi 
void create_cache(int mem_size, int max_file_in_cache);
void cache_dealloc(file** cache);

void cache_capacity_update(int dim_file, int new_file_or_not);
file* cache_research(file* cache, const char* f_name);
int cache_removeFile(file** cache, const char* f_name, int id);
void print_queue(file* cache);

int cache_insert(file** cache, const char* f_name, char* f_data, const size_t dim_f, int id, file** file_expelled);
int cache_enqueue(file** cache, const char* f_name, char* f_data, const size_t dim_f, int id);
file* replace_to_write(file** cache, const char* f_name, char* f_data, const size_t dim_f, int id);
file* replace_to_append(file** cache, const char* f_name, char* f_data, size_t dim_f, int id);
int cache_appendToFile(file** cache, const char* f_name, char* f_data, size_t dim_f, int id, file** file_expelled);

int cache_lockFile(file* cache, const char* f_name, int id);
int cache_unlockFile(file* cache, const char* f_name, int id);
