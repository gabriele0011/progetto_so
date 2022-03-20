//note:
//1. considera nella procedure se un file è lockato dal client
//2. deallocazione coda 

#include "err_control.h"
typedef unsigned char byte;

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

#define mutex_lock(addr_mtx, error_msg) \
	if(pthread_mutex_lock(addr_mtx) != 0){ \
			LOG_ERR(errno, error_msg); \
			exit(EXIT_FAILURE);	\
	}
#define mutex_unlock(addr_mtx, error_msg) \
	if(pthread_mutex_unlock(addr_mtx) != 0){ \
			LOG_ERR(errno, error_msg); \
			exit(EXIT_FAILURE);	\
	}


static size_t cache_capacity;
static size_t max_storage_file;
static size_t used_mem;

//struct per definire tipo di un nodo
typedef struct _file
{
	char* f_name;		//nome del file
	size_t f_size;		//lung. in byte
	byte* f_data;		//array di byte di lung. f_size
	byte f_write;		//il file è in scrittura
	int f_lock;		//il file è in locl
	pthread_mutex_t mtx;	//mutex nodo
	struct _file* next;	//nodo successivo
}file;



//prototipi
void create_cache(int mem_size, int max_file_in_cache);
int cache_writeFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname);
file* replacement_algorithm(file* cache, char* f_name, size_t dim_f, char* dirname);
static int cache_capacity_control(int dim_file);
static int cache_duplicate_control(file* cache, char* f_name);
void cache_capacity_update(int dim_file);
file* cache_enqueue(file** cache);

/*
int cache_lockFile(file* cache, char* f_name);
int cache_unlockFile(file* cache, char* f_name);
*/

//inizializza cache
void create_cache(int mem_size, int max_file_in_cache)
{
	cache_capacity = mem_size;
	max_storage_file = max_file_in_cache;
	mutex_lock(&mtx, "cache: lock fallita in create_cache");
	used_mem = 0;
	mutex_unlock(&mtx, "cache: unlock fallita in create_cache");
}

//controllo capacità residua se si scrive un file
int cache_capacity_control(int dim_file)
{
	mutex_lock(&mtx, "cache: lock fallita in cache_capacity_control");
	if(used_mem + dim_file > cache_capacity){
		mutex_unlock(&mtx, "cache: unlock fallita in cache_capacity_control");
		return -1;
	}else{
		mutex_unlock(&mtx, "cache: unlock fallita in cache_capacity_control");
		return 0;
	}
}

//aggiornamento capacità cache
void cache_capacity_update(int dim_file)
{
	mutex_lock(&mtx, "cache: lock fallita in cache_capacity_update");
	used_mem = used_mem + dim_file;
	mutex_unlock(&mtx, "cache: unlock fallita in cache_capacity_update");
	printf("cache used memory = %lu\n", used_mem);
}

//controlla se il file è un duplicato
int cache_duplicate_control(file* cache, char* f_name)
{
	if(cache == NULL) return 0;
	
	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	if(cache->next == NULL){
		if(strcmp(cache->f_name, f_name) == 0 ){
			mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
			return 1;
		}else{
			mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
			return 0;
		}
	}

	//due o piu elementi in coda
	file* prev = cache;
	mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
	file* curr = cache->next;

	mutex_lock(&(prev->mtx), "cache: lock fallita in cache_duplicate_control");
	mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");

	file* aux;
	while (curr->next != NULL){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_duplicate_control");

	}
	int x;
	if (strcmp(prev->f_name, f_name) == 0){
		x = 1;
	}else{
		if (curr->next == NULL && (strcmp(prev->f_name, f_name) == 0)){
				x = 1;
		}
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	return x;
}	


//scrittura di un file in cache (usa enqueue)
int cache_writeFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname)
{	
	printf("scrittura %s... \n", f_name);	
	//controllo preliminare 1: memoria sufficiente?
	if(cache_capacity < dim_f){
		LOG_ERR(-1, "cache: dimensione file maggiore della capacità della cache");
		return -1;
	}

	//conttrollo preliminare 2: è un duplicato?
	if(cache_duplicate_control(*cache, f_name)){
		LOG_ERR(-1, "cache: il file è un duplicato");
		return -1;
	}

	//caso coda non vuota -> controllo rimpiazzo
	file* node_rep = NULL;

	if(*cache != NULL && cache_capacity_control(dim_f) == -1){
		//capienza non sufficiente ->rimpiazzo
		printf("memoria insufficiente, rimpiazzo necessario\n");
		if((node_rep = replacement_algorithm(*cache, f_name, dim_f, dirname)) == NULL){
			printf("cache: rimpiazzo impossibile, scrittura fallita\n");
			return -1;
		}
	}
	
	file* new = NULL;	
	//se rimpiazzo
	if(node_rep != NULL){
		new = node_rep;
		mutex_lock(&mtx, "cache: lock fallita in cache_capacity_update");
		used_mem = used_mem + (node_rep->f_size - dim_f);
		printf("cache used memory = %zu\n", used_mem);
		mutex_unlock(&mtx, "cache: lock fallita in cache_capacity_update");

	}else{
		//no rimpiazzo	
		new = cache_enqueue(cache); //ritorna il ptr al nodo inserito per scrivere il file nel nodo
		cache_capacity_update(dim_f);
	}

	//lock
	mutex_lock(&(new->mtx), "cache: lock in cache_writeFile fallita");
	//setting node new;
	size_t len_f_name = strlen(f_name);
	ec_null((new->f_name = calloc(sizeof(char), len_f_name)), "cache: errore calloc");
	strncpy(new->f_name, f_name, len_f_name);
	new->f_size = dim_f;
	ec_null((new->f_data = calloc(sizeof(byte), dim_f)), "cache: calloc cache_writeFile fallita");
	for (int i = 0; i < dim_f; i++){
		new->f_data[i] = f_data[i];
	}
	//unlock
	mutex_unlock(&(new->mtx), "cache: unlock in cache_writeFile fallita");
	
	return 0;
}

//algoritmo di rimpiazzo
file* replacement_algorithm(file* cache, char* f_name, size_t dim_f, char* dirname)
{
	//cerca nodo che rispetti condizione di rimpiazzo (size_old >= size_new)
	//caso peggiore O(n), caso migliore O(1) (testa e coda sono invertite)
	//return ptr al nodo di rimpiazzo
	
	file* rep;
	
	//caso un elemento in coda -> controllo immediato di rimpiazzo
	mutex_lock(&(cache->mtx), "cache: lock fallita in replacement_algorithm");
	if(cache->next == NULL){
		if(cache->f_size >= dim_f){
			rep = cache;
			mutex_unlock(&(cache->mtx), "cache: unlock fallita in replacement_algorithm");
		}else{
			mutex_unlock(&(cache->mtx), "cache: unlock fallita in replacement_algorithm");
			return NULL;
		}
	//caso ricerca concorrente in coda con piu di due elementi
	}else{
		file* prev = cache;
		mutex_unlock(&(cache->mtx), "cache: unlock fallita in replacement_algorithm");
		file* curr = cache->next;
	
		mutex_lock(&(prev->mtx), "cache: lock fallita in replacement_algorithm");
		mutex_lock(&(curr->mtx), "cache: lock fallita in replacement_algorithm");
		file* aux;
		while (curr->next != NULL && prev->f_size >= dim_f){
			aux = prev;
			prev = curr;
			curr = curr->next;
			mutex_lock(&(curr->mtx), "cache: lock fallita in replacement_algorithm");
			mutex_unlock(&(aux->mtx), "cache: unlock fallita in replacement_algorithm");
		}
		if(prev->f_size >= dim_f){
			rep = prev;
		}else{
			if(curr->next == NULL && curr->f_size >= dim_f)
				rep = curr;
		}

		mutex_unlock(&(prev->mtx), "cache: unlock fallita in replacement_algorithm");
		mutex_unlock(&(curr->mtx), "cache: unlock fallita in replacement_algorithm");
	}
	printf("DEBUG: file da rimpiazzare: %s\n", rep->f_name);
	
	//scrittura del file espulso in dirname
	mutex_lock(&(rep->mtx), "cache: lock fallita in replacement_algorithm");
	if (dirname != NULL){
		//se non esiste, creare directory per scrittura file
		if (mkdir(dirname,  S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
			LOG_ERR(errno, "cache replacement_algorithm:");
		}
		//mi sposto nella cartella e scrivo il file
		chdir(dirname);	
		FILE* fd;
		ec_null((fd = fopen(rep->f_name, "wr")), "cache: fopen fallita in replacement_algorithm");
		ec_meno1(fwrite(rep->f_data, sizeof(char), rep->f_size, fd), "cache: fwrite fallita in replacement_algorithm");
		fclose(fd);
		//torno alla directory di partenza
		chdir("../");				
	}
	mutex_unlock(&(rep->mtx), "cache: lock fallita in replacement_algorithm");
	//printf("DEBUG DEBUG DEBUG: fino a qui tutto bene\n");
	return rep;
}	


//inserisce un codo in testa
file* cache_enqueue(file** cache)
{	
	//creazione nuovo nodo
	file* new;
	ec_null((new = malloc(sizeof(file))), "cache: malloc cache_writeFile fallita");
	new->next = NULL;
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in cache_create_file");
		exit(EXIT_FAILURE);	
	}
	//collocazione nodo

	//caso 1: cache vuota
	if (*cache == NULL){
		mutex_lock(&(new->mtx), "cache: lock fallita in cache_enqueue");
		(*cache) = new;
		mutex_unlock(&(new->mtx), "cache: unlock fallita in cache_enqueue");
		return new;
	}
	//caso cache con un elemento
	if((*cache)->next == NULL){
		mutex_lock(&((*cache)->mtx), "cache: lock fallita in cache_enqueue");
		(*cache)->next = new;
		mutex_unlock(&((*cache)->mtx), "cache: unlock fallita in cache_enqueue");
		return new;
	}

	//caso cache non vuota con almeno due elementi: 
	//collacazione nodo in testa (alla fine della lista)
	mutex_lock(&((*cache)->mtx), "cache: lock fallita in cache_enqueue");
	file* prev = *cache;
	mutex_unlock(&((*cache)->mtx), "cache: unlock fallita in cache_enqueue");
	file* curr = (*cache)->next;

	mutex_lock(&(prev->mtx), "cache: lock fallita in cache_enqueue");
	mutex_lock(&(curr->mtx), "cache: lock fallita in cache_enqueue");

	file* aux;
	while (curr->next != NULL){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_enqueue");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_enqueue");

	}
	curr->next = new;
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_enqueue");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_enqueue");
	return new;
}


int cache_lockFile(file* cache, char* f_name, int id)
{

	mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
	file* prev = cache;
	file* curr = cache->next;

	mutex_lock(&(prev->mtx), "cache: lock fallita in cache_duplicate_control");
	mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");

	file* aux;
	while (curr->next != NULL){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_duplicate_control");

	}
	int x = -1;
	if (strcmp(prev->f_name, f_name) == 0){
		prev->f_lock = id;
		x = 0;
		printf("f_name: %s - f_lock = %d\n", prev->f_name, prev->f_lock);
	}else{
		if (curr->next == NULL && (strcmp(curr->f_name, f_name) == 0)){
			curr->f_lock = id;
			x = 0;
			printf("f_name: %s - f_lock = %d\n", curr->f_name, curr->f_lock);
		}
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	return x;
}


int cache_unlockFile(file* cache, char* f_name, int id)
{
	mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
	file* prev = cache;
	file* curr = cache->next;

	mutex_lock(&(prev->mtx), "cache: lock fallita in cache_duplicate_control");
	mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");

	file* aux;
	while (curr->next != NULL){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_duplicate_control");

	}
	int x = -1;
	if (strcmp(prev->f_name, f_name) == 0){
		if(prev->f_lock == id){
			prev->f_lock = 0;
			x = 0;
		}
	}else{
		if (curr->next == NULL && (strcmp(curr->f_name, f_name) == 0)){
			if(curr->f_lock == id){
				curr->f_lock = 0;
				x = 0;
			}
		}
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	return x;
}
/*
int cache_appendToFile(file* cache, char* f_name, byte* f_data, size_t dim_f, char* dirname)
{
	//controllo preliminare: il file deve esistere
	if(!cache_duplicate_control(cache, f_name)) return -1;
	
	mutex_lock(&mtx, "cache: lock in cache_appendToFile fallita");
	while(cache->next != NULL && strcmp(cache->f_name, f_name) ==0 ){
		cache = cache->next;
	}
	//ce la fai a scrivere in append? se si ok altrimenti è necessario un rimpiazzo
	cache->f_size = cache->f_size+dim_f;
	ec_null((cache->f_data = realloc(cache->f_data, (cache->f_size) * sizeof(byte))), "cache: realloc fallita in cache_appendToFile");


	for (int i = cache->f_size; i < cache->f_size; i++){
		cache->f_data[i] = f_data[i];
	}
	//unlock
	mutex_unlock(&mtx, "cache: unlock in cache_appendToFile fallita");
	return 0;
}

*/
/*
int cache_readFile(file* cache, char* f_name, byte** buf, size_t* dim_buf)

int cache_readNFile(file* cache, size_t N, char* dirname)


//cache_removeFile
int cache_removeFile(file* cache, char* f_name)

*/

void print_queue(file* cache)
{
	while(cache != NULL){
		printf("%s ", cache->f_name);
		cache = cache->next;
	}
	printf("\n");
}

int main(){

	file* cache = NULL;
	create_cache(100, 50);
	char dirname[4] = {'a', 'g', 'o', '\0'};

	char f_name[6] = {'f', 'i', 'l', 'e', '1', '\0'};
	byte data[4] = {'c', 'i', 'a', '\0'};
	cache_writeFile(&cache, f_name, data, 4, dirname);
	//print_queue(cache);


	char f_name1[6] = {'f', 'i', 'l', 'e', '2', '\0'};
	byte data1[8] = {'c', 'o', 'm', 'e', 's', 't', 'a', '\0'};
	cache_writeFile(&cache, f_name1, data1, 8, dirname);
	print_queue(cache);

	//cache_appendToFile(cache, f_name1, data1, 5, dirname);

	char f_name3[6] = {'f', 'i', 'l', 'e', '3', '\0'};
	byte data3[5] = {'s', 't', 'a', 'i', '\0'};
	//cache_writeFile(&cache, f_name3, data3, 5, dirname);
	//print_queue(cache);

	char f_name4[6] = {'f', 'i', 'l', 'e', '4', '\0'};
	byte data4[5] = {'s', 't', 'a', 'i', '\0'};
	//cache_writeFile(&cache, f_name4, data4, 5, NULL);
	//print_queue(cache);

	return 0;

}