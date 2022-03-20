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
	//byte f_write;		//flag scrittura
	int f_lock;		//flag lock
	pthread_mutex_t mtx;	//mutex nodo
	struct _file* next;	//nodo successivo
}file;



//prototipi 
void create_cache(int mem_size, int max_file_in_cache);

static void cache_writeInDir(file* node, char* dirname);
static file* cache_research(file* cache, char* f_name);
static int cache_capacity_control(int dim_file);
static void cache_capacity_update(int dim_file);
static int cache_duplicate_control(file* cache, char* f_name);
static file* replacement_algorithm(file* cache, size_t dim_f, char* dirname);
static file* cache_enqueue(file** cache);

int cache_lockFile(file* cache, char* f_name, int id);
int cache_unlockFile(file* cache, char* f_name, int id);
int cache_removeFile(file** cache, char* f_name, int id);
int cache_writeFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id);
int cache_appendToFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id);
int cache_readFile(file* cache, char* f_name, byte** buf, size_t* dim_buf, int id);
int cache_readNFile(file* cache, size_t N, char* dirname, int id);
void print_queue(file* cache);

void create_cache(int mem_size, int max_file_in_cache)
{
	cache_capacity = mem_size;
	max_storage_file = max_file_in_cache;
	mutex_lock(&mtx, "cache: lock fallita in create_cache");
	used_mem = 0;
	mutex_unlock(&mtx, "cache: unlock fallita in create_cache");
}

static void cache_writeInDir(file* node, char* dirname)
{
	if (dirname != NULL){
		//se non esiste, creare directory per scrittura file
		if (mkdir(dirname,  S_IRWXU | S_IRWXG | S_IRWXO) == -1)/*{
			//LOG_ERR(errno, "cache_writeInDir");
		}*/
		//mi sposto nella cartella e scrivo il file
		ec_meno1(chdir(dirname), "cache_writeInDir: chdir fallita");	
		FILE* fd;
		ec_null((fd = fopen(node->f_name, "wr")), "cache_writeInDir: fopen fallita");
		ec_meno1(fwrite(node->f_data, sizeof(char), node->f_size, fd), "cache_writeInDir: fwrite fallita");
		fclose(fd);
		//torno alla directory di partenza
		ec_meno1(chdir("../"), "cache_writeInDir: chdir fallita");
		printf("DEBUG: ho scritto %s in %s\n", node->f_name, dirname);
	}
}

static file* cache_research(file* cache, char* f_name)
{
	//due o piu elementi in coda
	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	file* prev = cache;
	mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
	file* curr = cache->next;

	mutex_lock(&(prev->mtx), "cache: lock fallita in cache_duplicate_control");
	mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");

	file* aux;
	while (curr->next != NULL && strcmp(prev->f_name, f_name) != 0){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_duplicate_control");

	}
	file* node = NULL;
	if (strcmp(prev->f_name, f_name) == 0){
		node = prev;
	}else{
		if (curr->next == NULL && (strcmp(curr->f_name, f_name) == 0))
			node = curr;
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	return node;
}

static int cache_capacity_control(int dim_file)
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


static void cache_capacity_update(int dim_file)
{
	mutex_lock(&mtx, "cache: lock fallita in cache_capacity_update");
	used_mem = used_mem + dim_file;
	printf("cache used memory = %lu di %zu\n", used_mem, cache_capacity);
	mutex_unlock(&mtx, "cache: unlock fallita in cache_capacity_update");
}

static int cache_duplicate_control(file* cache, char* f_name)
{
	if(cache == NULL) return -1;
	
	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	if(cache->next == NULL && strcmp(cache->f_name, f_name) == 0 ){
		mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
		return 0;
	}else{
		mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
		return -1;
	}

	//due o piu elementi in coda
	file* prev = cache;
	mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
	file* curr = cache->next;
	mutex_lock(&(prev->mtx), "cache: lock fallita in cache_duplicate_control");
	mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
	file* aux;
	while (curr->next != NULL && strcmp(prev->f_name, f_name) == 0){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_duplicate_control");

	}
	int x = -1;
	if ( (strcmp(prev->f_name, f_name) == 0 || (curr->next == NULL && strcmp(curr->f_name, f_name) == 0)) ) x = 0;
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	return x;
}	



static file* replacement_algorithm(file* cache, size_t dim_f, char* dirname)
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
	printf("DEBUG: file da rimpiazzare: %s (%zu byte)\n", rep->f_name, rep->f_size);
	
	//scrittura del file espulso in dirname
	cache_writeInDir(rep, dirname);
	//printf("DEBUG DEBUG DEBUG: fino a qui tutto bene\n");
	return rep;
}	


static file* cache_enqueue(file** cache)
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
	//controllo che il file non sia gia lockato (!)
	file* node;
	if ((node = cache_research(cache, f_name)) == NULL){
		printf("file non trovato\n");
		return -1;
	}
	node->f_lock = id;
	return 0;
}

int cache_unlockFile(file* cache, char* f_name, int id)
{	
	file* node;
	if ((node = cache_research(cache, f_name)) == NULL){
		printf("file non trovato\n");
		return -1;
	}
	if(node->f_lock == id)
		node->f_lock = 0;
	return 0;
}

int cache_removeFile(file** cache, char* f_name, int id)
{
	//caso lista vuota
	if(*cache == NULL) return -1;
	
	
	//caso un elemento in lista
	if((*cache)->next == NULL && strcmp((*cache)->f_name, f_name) == 0){
		if((*cache)->f_lock != 0 || (*cache)->f_lock == id ){
			*cache = NULL;
			free(*cache);
			return 0;
		}
		return -1;
	}
	
	//caso lista con due o piu elementi
	mutex_lock(&((*cache)->mtx), "cache: lock fallita in cache_enqueue");
	file* prev = (*cache);
	mutex_unlock(&((*cache)->mtx), "cache: unlock fallita in cache_enqueue");
	file* curr = (*cache)->next;

	mutex_lock(&(prev->mtx), "cache: lock fallita in cache_enqueue");
	mutex_lock(&(curr->mtx), "cache: lock fallita in cache_enqueue");

	file* aux;

	while (curr->next != NULL && strcmp(prev->f_name, f_name) != 0){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_enqueue");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_enqueue");

	}
	int x = -1;
	//se il nodo da rimuovere è prev
	if (strcmp(prev->f_name, f_name) == 0 && (prev->f_lock != 0 || prev->f_lock == id) ){
		//se si tratta del primo nodo della coda
		if (prev == *cache) *cache = curr;
		//altrimenti collego aux a curr (prev è in mezzo)
		else aux->next = curr;
		//aggiornamento capacità cache
		cache_capacity_update(-(prev->f_size));
		free(prev);
		x = 0;
	}else{
		//se si tratta dell'ultimo elemento della lista
		if (curr->next == NULL && (strcmp(curr->f_name, f_name) == 0) && (curr->f_lock != 0 || curr->f_lock == id)){
				prev->next = NULL;
				free(curr);
				//aggiornamento capacità cache
				cache_capacity_update(-(curr->f_size));
				x = 0;
		}
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_enqueue");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_enqueue");
	return x;
}

int cache_writeFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id)
{	
	printf("DEBUG: scrittura %s (%zu byte)... \n", f_name, dim_f);	
	//controllo preliminare 1: memoria sufficiente?
	if(cache_capacity < dim_f){
		LOG_ERR(-1, "cache: dimensione file maggiore della capacità della cache");
		return -1;
	}

	//conttrollo preliminare 2: è un duplicato?
	if(cache_duplicate_control(*cache, f_name) == 0){
		LOG_ERR(-1, "cache: il file è un duplicato");
		return -1;
	}

	//caso coda non vuota -> controllo rimpiazzo
	file* node_rep = NULL;

	if(*cache != NULL && cache_capacity_control(dim_f) == -1){
		//capienza non sufficiente ->rimpiazzo
		printf("(!) memoria insufficiente, rimpiazzo necessario\n");
		if((node_rep = replacement_algorithm(*cache, dim_f, dirname)) == NULL){
			printf("cache: rimpiazzo impossibile, scrittura fallita\n");
			return -1;
		}
	}
	
	file* new = NULL;	
	//se rimpiazzo
	if(node_rep != NULL){
		new = node_rep;
		mutex_lock(&mtx, "cache: lock fallita in cache_capacity_update");
		used_mem = used_mem - (node_rep->f_size - dim_f);
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

	//(!) ATTENZIONE: IN BASE AL VALORE DI id SETTA lock. es. se id = -77 vuol dire che deve essere creato in f_lock = 1;
	//unlock
	mutex_unlock(&(new->mtx), "cache: unlock in cache_writeFile fallita");
	
	return 0;
}


int cache_appendToFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id)
{
	printf("DEBUG: scrittura in append %s...(%zu byte)\n", f_name, dim_f);
	//controllo preliminare: il file deve esistere
	file* node;
	if((node = cache_research(*cache, f_name)) == NULL){
		printf("cache: cache_research non ha trovato %s in cache_appendToFile\n", f_name);
		return -1;
	}

	//controllo capacità memoria
	if(cache_capacity_control(dim_f) == -1){
		file* temp;
		if((temp = replacement_algorithm(*cache, dim_f, dirname)) != NULL){
			//nodo di rimpiazzo trovato -> elmin. nodo per liberare memoria
			if(cache_removeFile(cache, node->f_name, id) == -1){
				printf("cache: cache_removeFile fallita\n");
				return -1;
			}
		}else{
			printf("cache: replacement_algorithm fallita");
			return -1;
		}
	}
	if(node->f_lock == 0 || node->f_lock == id){
		printf("cache_readFile: nodo locked, operazione fallita");
		return -1;
	}
	mutex_lock(&mtx, "cache: lock fallita in cache_appendToFile");
	size_t new_size = dim_f + node->f_size;
	ec_null((node->f_data = realloc(node->f_data, sizeof(byte)*new_size)), "cache: calloc cache_writeFile fallita");
	
	int j = 0, i = node->f_size;
	while(j < dim_f && i < new_size){
		node->f_data[i] = f_data[j];
		j++; i++;
	}

	used_mem = used_mem + dim_f;
	printf("cache used memory = %lu di %zu\n", used_mem, cache_capacity);

	node->f_size = new_size;

	mutex_unlock(&mtx, "cache: lock fallita in cache_appendToFile");
	printf("DEBUG: cache_appendToFile riuscita su %s\n", node->f_name);
	return 0;
}


int cache_readFile(file* cache, char* f_name, byte** buf, size_t* dim_buf, int id)
{
	//controllo preliminare: il file deve esistere
	file* node;
	if((node = cache_research(cache, f_name)) == NULL){
		printf("cache: cache_research non ha trovato %s (cache_readFile)\n", f_name);
		return -1;
	}
	//controllo f_lock
	if(node->f_lock == 0 || node->f_lock == id){
		printf("cache_readFile: nodo locked, operazione fallita");
		return -1;
	}
	mutex_lock(&(node->mtx), "cache: lock in cache_readFile fallita");
	*dim_buf = node->f_size;
	ec_null((*buf = calloc(sizeof(byte), *dim_buf)), "cache: calloc in cache_readFile fallita");
	for (int i = 0; i < *dim_buf; i++){
		(*buf)[i] = node->f_data[i];
	}
	printf("DEBUG: fino a qui tutto bene\n");
	mutex_unlock(&(node->mtx), "cache: unlock in cache_readFile fallita");
	return 0;
}



int cache_readNFile(file* cache, size_t N, char* dirname, int id)
{
	if(N <= 0) return 0;
	if(cache->next == NULL){}

	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	file* prev = cache;
	mutex_unlock(&(cache->mtx), "cache: unlock fallita in cache_duplicate_control");
	file* curr = cache->next;

	mutex_lock(&(prev->mtx), "cache: lock fallita in cache_duplicate_control");
	mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");

	file* aux;
	size_t scan = 0;
	while (curr->next != NULL && N > 0 ){
		if(prev->f_lock == 0 || prev->f_lock == id)
			cache_writeInDir(prev, dirname);
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_duplicate_control");
		N--;
		scan++;

	}
	if(curr->next == NULL){
		if (--N > 0 && scan++){
			if(prev->f_lock == 0 || prev->f_lock == id)
				cache_writeInDir(prev, dirname);
		}
		if(--N > 0 && scan++){
			if(prev->f_lock == 0 || prev->f_lock == id)
				cache_writeInDir(curr, dirname);
		}

	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	printf("DEBUG: file letti = %zu\n", scan);
	return scan;
}


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

	char f_name1[6] = {'f', 'i', 'l', 'e', '1', '\0'};
	byte data1[4] = {'c', 'i', 'a', '\0'};
	cache_writeFile(&cache, f_name1, data1, 4, dirname, 1996);
	//print_queue(cache);

	char f_name2[6] = {'f', 'i', 'l', 'e', '2', '\0'};
	byte data2[8] = {'c', 'o', 'm', 'e', 's', 't', 'a', '\0'};
	cache_writeFile(&cache, f_name2, data2, 8, dirname, 1996);
	//print_queue(cache);

	char f_name3[6] = {'f', 'i', 'l', 'e', '3', '\0'};
	byte data3[5] = {'s', 't', 'a', 'i', '\0'};
	cache_writeFile(&cache, f_name3, data3, 5, dirname, 1996);
	//print_queue(cache);

	char f_name4[6] = {'f', 'i', 'l', 'e', '4', '\0'};
	byte data4[5] = {'s', 't', 'a', 'i', '\0'};
	cache_writeFile(&cache, f_name4, data4, 5, NULL, 1996);
	print_queue(cache);

/*
	byte datax[4] = {'c', 'i', 'a', '\0'};
	cache_appendToFile(&cache, f_name3, datax, 4, NULL, 0);
		
	byte* buf;
	size_t dim_buf;
	cache_readFile(cache, f_name1, &buf, &dim_buf);
	if(buf){
		printf("leggo: %s\n", f_name1);
		for(int i = 0; i < dim_buf; i++)
			printf("%c", buf[i]);
		printf("\n");
	}


	cache_readNFile(cache, 10, "file_letti", 1996);
	cache_lockFile(cache, "file1", 1996);
	cache_research(cache, "file2");
	cache_removeFile(&cache, f_name3, 1996);
	cache_removeFile(&cache, f_name4, 1997);
*/
	
	return 0;

}