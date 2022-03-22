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
static size_t files_in_cache;

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

//static void cache_writeInDir(file* node, char* dirname);
static file* cache_research(file* cache, char* f_name);
static void cache_capacity_update(int dim_file);
static int cache_duplicate_control(file* cache, char* f_name, int id);
static int cache_enqueue(file** cache, char* f_name, byte* f_data, size_t dim_f, int id);
static file* replacement_algorithm_append(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id);
static file* replacement_algorithm_write(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id);


int cache_lockFile(file* cache, char* f_name, int id);
int cache_unlockFile(file* cache, char* f_name, int id);
int cache_removeFile(file** cache, char* f_name, int id);
int cache_writeFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id);
int cache_appendToFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id);
int cache_readFile(file* cache, char* f_name, byte** buf, size_t* dim_buf, int id);
//int cache_readNFile(file* cache, size_t N, char* dirname, int id);
void print_queue(file* cache);

void create_cache(int mem_size, int max_file_in_cache)
{
	cache_capacity = mem_size;
	max_storage_file = max_file_in_cache;
	//memoria attualmente occupata
	used_mem = 0;
	//numero di file memorizzati in cache
	files_in_cache = 0;
}

static file* cache_research(file* cache, char* f_name)
{
	
	//controllo coda vuota
	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	if (cache == NULL){
		mutex_unlock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
		return NULL;
	}
	mutex_unlock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	
	//controllo coda con un elemento
	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	if (cache->next == NULL && strcmp(cache->f_name, f_name) == 0){
		mutex_unlock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
		return cache;
	}
	mutex_unlock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	
	//due o piu elementi in coda
	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	file* prev = cache;
	mutex_lock(&(cache->next->mtx), "cache: unlock fallita in cache_duplicate_control");
	file* curr = cache->next;

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
		if (strcmp(curr->f_name, f_name) == 0)
			node = curr;
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	return node;
}

static void cache_capacity_update(int dim_file)
{
	mutex_lock(&mtx, "cache: lock fallita in cache_capacity_update");
	used_mem = used_mem + dim_file;
	if (dim_file > 0 ) files_in_cache++;
	else files_in_cache--;
	printf("cache used memory = %lu di %zu - files in cache = %zu\n", used_mem, cache_capacity, files_in_cache);
	mutex_unlock(&mtx, "cache: unlock fallita in cache_capacity_update");
}


static file* replacement_algorithm_append(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id)
{
	
	file* rep;
	file* node = NULL;
	size_t found = 0;
	
	//caso cache vuota - utile?
	mutex_lock(&mtx, "replacement_algorithm_append: lock fallita");
	if(*cache == NULL){
		mutex_unlock(&mtx, "replacement_algorithm_append: unlock fallita");
		return NULL;
	}
	mutex_unlock(&mtx, "replacement_algorithm_append: unlock fallita");
	
	//caso cache con un elemento - append con rimpiazzo impossibile
	mutex_lock(&((*cache)->mtx), "replacement_algorithm_append: lock fallita");
	if ((*cache)->next == NULL){
		mutex_unlock(&((*cache)->mtx), "replacement_algorithm_append: unlock fallita");
		return NULL;
	}else{
		/*************************************************************************************************/
		//caso coda con piu di due elementi
		
		//CICLO 1: eliminazione del file da rimpiazzare
		file* prev = *cache;
		mutex_lock(&((*cache)->next->mtx), "replacement_algorithm_write: unlock fallita !!");
		file* curr = (*cache)->next;
		
		size_t delete = 0;
		file* aux = prev;
		while ( !(prev->f_size >= dim_f && (prev->f_lock == 0 || prev->f_lock == id) && strcmp(prev->f_name, f_name) != 0) && curr->next != NULL ){
			aux = prev;
			prev = curr;
			curr = curr->next;
			mutex_lock(&(curr->mtx), "replacement_algorithm_append: lock fallita");
			mutex_unlock(&(aux->mtx), "replacement_algorithm_append: unlock fallita");
		}
		//il nodo da rimpiazzare è prev
		if (prev->f_size >= dim_f && (prev->f_lock == 0 || prev->f_lock == id) && strcmp(prev->f_name, f_name) != 0){
			//se prev è il primo elemento della lista
			if (aux == prev){
			printf("DEBUG: fino a qui tutto bene\n");
				//printf("DEBUG: rimpiazzo primo elem: %s\n", prev->f_name);
				*cache = curr;
			//prev sta tra aux e curr
			}else{
				mutex_lock(&(aux->mtx), "replacement_algorithm_append: lock fallita");
				aux->next = curr;
				mutex_unlock(&(aux->mtx), "replacement_algorithm_append: unlock fallita");
			}
				rep = prev;
				delete = 1;
		}else{
			//il nodo da rimpiazzare è curr?
			if (curr->f_size >= dim_f && !delete && (curr->f_lock == 0 || curr->f_lock == id) && strcmp(curr->f_name, f_name) != 0){
				//printf("DEBUG: rimpiazzo ultimo: %s\n", curr->f_name);
				rep = curr;
				prev->next = NULL;
				delete = 1;
			}
		}
		mutex_unlock(&(prev->mtx), "replacement_algorithm_append: unlock fallita");
		mutex_unlock(&(curr->mtx), "replacement_algorithm_append: unlock fallita");
		if (!delete){
			return NULL;
		}else{
			cache_capacity_update(-(rep->f_size));
		}
      		
      		/*************************************************************************************************/
		//CICLO 2: cerco file su cui fare la append
		mutex_lock(&((*cache)->mtx), "cache: lock fallita in replacement_algorithm");
		prev = *cache;
		mutex_lock(&((*cache)->next->mtx), "cache: unlock fallita in replacement_algorithm");
		curr = (*cache)->next;
	
		while (curr->next != NULL && strcmp(prev->f_name, f_name) != 0){
			aux = prev;
			prev = curr;
			curr = curr->next;
			mutex_lock(&(curr->mtx), "cache: lock fallita in replacement_algorithm");
			mutex_unlock(&(aux->mtx), "cache: unlock fallita in replacement_algorithm");
		}
		
		if (strcmp(prev->f_name, f_name) == 0){
			found = 1;
			node = prev;
		}else{
			if (strcmp(curr->f_name, f_name) == 0){
				found = 1;
				node = curr;
			}
		}
		mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
		mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	}
	
	/*************************************************************************************************/
	//SCRITTURA APPEND (con controllo memoria)
	if (found && (used_mem+dim_f) <= cache_capacity){
		mutex_lock(&(node->mtx), "cache: lock fallita in cache_appendToFile");
		size_t new_size = dim_f + node->f_size;
		ec_null((node->f_data = realloc(node->f_data, sizeof(byte)*new_size)), "cache: calloc cache_writeFile fallita");
		int j = 0, i = node->f_size;
		while (j < dim_f && i < new_size){
			node->f_data[i] = f_data[j];
			j++; i++;
		}
		node->f_size = new_size;
		mutex_unlock(&(node->mtx), "cache: lock fallita in cache_appendToFile");
		cache_capacity_update(dim_f);
		printf("DEBUG: append su %s eseguita\n", node->f_name);
		//printf("DEBUG: eliminato il nodo: %s\n", rep->f_name);

	}else{
		return NULL;
	}
	return rep;
}


int cache_appendToFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id)
{
	file* rep_node; //nodo da restituire al server worker

	//controllo capacità memoria per rimpiazzo
	if(used_mem+dim_f > cache_capacity){
		if((rep_node = replacement_algorithm_append(cache, f_name, f_data, dim_f, dirname, id)) == NULL){
			printf("cache_writeFile: scrittura in append su %s (%zubyte) fallita\n", f_name, dim_f);
			return -1;
		}else{
			printf("cache_appendToFile: append riuscita - file rimpiazzato: %s\n", rep_node->f_name);
			return 0;
		}
	}
	//printf("DEBUG: scrittura in append su %s...(%zu byte)\n", f_name, dim_f);
	file* node;
	size_t found = 0;
	//scrittura senza rimpiazzo
	mutex_lock(&((*cache)->mtx), "cache_appendToFile: lock fallita");
	file* prev = *cache;
	mutex_lock(&((*cache)->next->mtx), "cache_appendToFile: unlock fallita");
	file* curr = (*cache)->next;
	file* aux = prev;
	
	while (curr->next != NULL && strcmp(prev->f_name, f_name) != 0){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache_appendToFile: lock fallita");
		mutex_unlock(&(aux->mtx), "cache_appendToFile: unlock fallita");
	}
		
	if (strcmp(prev->f_name, f_name) == 0){
		found = 1;
		node = prev;
	}else{
		if (strcmp(curr->f_name, f_name) == 0){
			found = 1;
			node = curr;
		}
	}
	mutex_unlock(&(prev->mtx), "cache_appendToFile: unlock fallita");
	mutex_unlock(&(curr->mtx), "cache_appendToFile: unlock fallita");

	if (found){
		mutex_lock(&(node->mtx), "cache_appendToFile: lock fallita");
		size_t new_size = dim_f + node->f_size;
		ec_null((node->f_data = realloc(node->f_data, sizeof(byte)*new_size)), "cache_appendToFile: calloc fallita");
		int j = 0, i = node->f_size;
		while (j < dim_f && i < new_size){
			node->f_data[i] = f_data[j];
			j++; i++;
		}
		node->f_size = new_size;
		mutex_unlock(&(node->mtx), "cache_appendToFile: lock fallita");
		cache_capacity_update(dim_f);
		printf("DEBUG: scrittura in append su %s eseguita\n", node->f_name);
	}
	

	//printf("DEBUG: scrittura in append riuscita - rimpiazzato: %s\n", rep_node->f_name);
	if (found) return 0;
	else return -1;
}

static file* replacement_algorithm_write(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id)
{
	//cerca nodo che rispetti condizione di rimpiazzo (size_old >= size_new)
	//caso peggiore O(n), caso migliore O(1) (testa e coda sono invertite)
	//return ptr al nodo di rimpiazzo
	
	file* new;
	ec_null((new = (file*)malloc(sizeof(file))), "replacement_algorithm_write: malloc fallita");
	file* rep;
	int found = 0;

	//caso cache vuota - utile?
	mutex_lock(&mtx, "replacement_algorithm_write: lock fallita");
	if(*cache == NULL){
		mutex_unlock(&mtx, "replacement_algorithm_write: unlock fallita");
		return NULL;
	}
	mutex_unlock(&mtx, "replacement_algorithm_write: unlock fallita");
	
	
	
	//caso cache con un elemento
	mutex_lock(&((*cache)->mtx) , "replacement_algorithm_write: lock fallita");
	if ((*cache)->next == NULL){
		if((*cache)->f_size >= dim_f){
			rep = *cache;
			*cache = new;
			found = 1;
			mutex_unlock(&((*cache)->mtx), "replacement_algorithm_write: unlock fallita");
			printf("DEBUG: rimpiazzo file: %s\n", (*cache)->f_name);

		}else{
			mutex_unlock(&((*cache)->mtx), "replacement_algorithm_write: unlock fallita");
			free(new);
			return NULL;
		}
	}else{
		//caso coda con due o piu elementi - cerco nodo rimpiazzabile
		file* prev = *cache;
		mutex_lock(&((*cache)->next->mtx), "replacement_algorithm_write: unlock fallita !!");
		file* curr = (*cache)->next;
		//printf("DEBUG DEBUG DEBUG: fino a qui tutto bene\n");
	
		file* aux = prev;
		while (curr->next != NULL){
			//controllo: cond.rimpiazzo + lock + duplicato
			if(prev->f_size >= dim_f && (prev->f_lock == 0 || prev->f_lock == id) && strcmp(prev->f_name, f_name) != 0){
				//trovato il nodo di rimpiazzo
				rep = prev;
				//lo stacco
				if(aux == *cache) *cache = curr; //prev è il primo elem. della coda
				else aux->next = curr;		//prev sta tra aux e curr
				//setto flag per scrittura
				found = 1;
			}
			aux = prev;
			prev = curr;
			curr = curr->next;
			mutex_lock(&(curr->mtx), "cache: lock fallita in replacement_algorithm");
			mutex_unlock(&(aux->mtx), "cache: unlock fallita in replacement_algorithm");
		}
		//il nodo da rimpiazzare è prev
		if(prev->f_size >= dim_f && !found && prev->f_lock == 0){
			//se prev è il primo elemento della lista
			if(aux == prev){
				//printf("DEBUG: rimpiazzo primo elem: %s\n", prev->f_name);
				*cache = curr;
			//prev sta tra aux e curr
			}else{
				mutex_lock(&(aux->mtx), "cache: unlock fallita in replacement_algorithm");
				//printf("DEBUG: rimpiazzo in mezzo: %s\n", prev->f_name);
				aux->next = curr;
				mutex_unlock(&(aux->mtx), "cache: unlock fallita in replacement_algorithm");
			}
			rep = prev;
			curr->next = new;
			found = 1;
		}else{
			//il nodo da rimpiazzare è curr?ss
			if(curr->f_size >= dim_f && !found && curr->f_lock == 0){
				//printf("DEBUG: rimpiazzo ultimo: %s\n", curr->f_name);
				rep = curr;
				prev->next = new;
				found = 1;
			}
		}
		mutex_unlock(&(prev->mtx), "cache: unlock fallita in replacement_algorithm");
		mutex_unlock(&(curr->mtx), "cache: unlock fallita in replacement_algorithm");
	}
	//ho trovato un rimpiazzo -> setta il nuovo elemento in testa
	if(!found){
		free(new);
		return NULL;
	}else{
		if (pthread_mutex_init(&(new->mtx), NULL) != 0){
			LOG_ERR(errno, "replacement_algorithm_write: pthread_mutex_init fallita");
			free(new);
			exit(EXIT_FAILURE);	
		}
		mutex_lock(&(new->mtx), "replacement_algorithm_write: unlock fallita (scrittura file)");
		if(id != 0) new->f_lock = 0;
		else new->f_lock = id;
		size_t len_f_name = strlen(f_name);
		ec_null((new->f_name = calloc(sizeof(char), len_f_name)), "cache: errore calloc");
		strncpy(new->f_name, f_name, len_f_name);
		new->f_size = dim_f;
		ec_null((new->f_data = calloc(sizeof(byte), dim_f)), "cache: calloc cache_writeFile fallita");
		for (int i = 0; i < dim_f; i++)
			new->f_data[i] = f_data[i];
		mutex_unlock(&(new->mtx), "replacement_algorithm_write: unlock fallita (scrittura file)");

		cache_capacity_update(-(rep->f_size));
		cache_capacity_update(dim_f);
	}
	return rep;
}

int cache_writeFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname, int id)
{	
	printf("DEBUG: scrittura %s (%zu byte)... \n", f_name, dim_f);	
	
	//controllo preliminare 1: memoria sufficiente?
	if (cache_capacity < dim_f){
		LOG_ERR(-1, "cache_writeFile: dimensione file maggiore della capacità della cache");
		return -1;
	}

	file* node_rep = NULL; //ptr al nodo eventualmente rimpiazzato
	if (used_mem+dim_f > cache_capacity){
		//capienza non sufficiente ->rimpiazzo
		printf("(!) memoria insufficiente, rimpiazzo necessario\n");
		if ((node_rep = replacement_algorithm_write(cache, f_name, f_data, dim_f, dirname, id)) == NULL){  //se lo trova deve anche scriverlo!
			printf("cache_writeFile: scrittura di %s (%zu) fallita\n", f_name, dim_f);
			return -1;
		}else{
			printf("DEBUG: scrittura %s eseguita on rimpiazzo di %s\n", f_name, node_rep->f_name);
			return 0;
		}
	}
	//normale scrittura in cache tramite enqueue
	if (cache_enqueue(cache, f_name, f_data, dim_f, id) == -1){
		LOG_ERR(-1, "cache_writeFile: enqueue fallita");
		return -1;
	}
	printf("DEBUG: scrittura %s eseguita\n", f_name);
	cache_capacity_update(dim_f);
	return 0;
}


static int cache_enqueue(file** cache, char* f_name, byte* f_data, size_t dim_f, int id)
{
	//creazione nuovo nodo + setting
	file* new;
	ec_null((new = malloc(sizeof(file))), "cache: malloc cache_writeFile fallita");
	new->next = NULL;
	new->f_lock = 0;
	//setting node new;
	size_t len_f_name = strlen(f_name);
	ec_null((new->f_name = calloc(sizeof(char), len_f_name)), "cache: errore calloc");
	strncpy(new->f_name, f_name, len_f_name);
	new->f_size = dim_f;
	ec_null((new->f_data = calloc(sizeof(byte), dim_f)), "cache: calloc cache_writeFile fallita");
	for (int i = 0; i < dim_f; i++)
		new->f_data[i] = f_data[i];

	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in cache_create_file");
		free(new);
		exit(EXIT_FAILURE);	
	}

	//collocazione nodo
	
	mutex_lock(&mtx, "cache: lock fallita in cache_enqueue");
	//caso 1: cache vuota
	if (*cache == NULL){
		(*cache) = new;
		mutex_unlock(&mtx, "cache: unlock fallita in cache_enqueue");
		return 0;
	}
	//caso 2: un elemento
	if((*cache)->next == NULL){
		if (strcmp((*cache)->f_name, f_name) == 0){
			printf("cache_enqueue: il file è un duplicato, scrittura fallita\n");
			mutex_unlock((&mtx), "cache: unlock fallita in cache_enqueue");
			return -1;
		}
		(*cache)->next = new;
		mutex_unlock((&mtx), "cache: unlock fallita in cache_enqueue");
		return 0;
	}
	mutex_unlock((&mtx), "cache: unlock fallita in cache_enqueue");

	//caso cache non vuota con almeno due elementi: 
	//collacazione nodo in testa (alla fine della lista)
	mutex_lock(&((*cache)->mtx), "cache: unlock fallita in cache_enqueue");
	file* prev = *cache;
	mutex_lock(&((*cache)->next->mtx), "cache: unlock fallita in cache_enqueue");
	file* curr = (*cache)->next;

	file* aux;
	while (curr->next != NULL && strcmp(prev->f_name, f_name) != 0){
		//controllo duplicato
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_enqueue");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_enqueue");

	}
	int x = 0;
	if(strcmp(prev->f_name, f_name) == 0 || strcmp(curr->f_name, f_name) == 0){
		printf("cache_enqueue: il file è un duplicato, scrittura fallita\n");
		free(new);
		x = -1;
	}else{
		curr->next = new;
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_enqueue");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_enqueue");
	return x;
}


int cache_lockFile(file* cache, char* f_name, int id)
{

	file* node;
	if ((node = cache_research(cache, f_name)) == NULL){
		printf("file non trovato\n");
		return -1;
	}
	mutex_lock(&(node->mtx), "cache_lockFile: lock fallita");
	if(node->f_lock == 0)
		node->f_lock = id;
	mutex_unlock(&(node->mtx), "cache_lockFile: unlock fallita");
	return 0;
}

int cache_unlockFile(file* cache, char* f_name, int id)
{	
	file* node;
	if ((node = cache_research(cache, f_name)) == NULL){
		printf("file non trovato\n");
		return -1;
	}
	mutex_lock(&(node->mtx), "cache_unlockFile: lock fallita");
	if(node->f_lock == id)
		node->f_lock = 0;
	mutex_unlock(&(node->mtx), "cache_unlockFile: unlock fallita");
	return 0;
}

int cache_removeFile(file** cache, char* f_name, int id)
{
	//caso lista vuota
	mutex_lock(&mtx, "cache_removeFile: lock fallita");
	if(*cache == NULL){
		mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
		return -1;
	}
	mutex_unlock(&mtx, "cache_removeFile: unlock fallita");

	
	//caso un elemento in lista
	mutex_lock(&mtx, "cache_removeFile: lock fallita");
	if((*cache)->next == NULL && strcmp((*cache)->f_name, f_name) == 0){
		if((*cache)->f_lock != 0 || (*cache)->f_lock == id ){
			*cache = NULL;
			free(*cache);
			mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
			return 0;
		}
		mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
		return -1;
	}
	mutex_unlock(&mtx, "cache_removeFile: unlock fallita");

	
	//caso lista con due o piu elementi
	mutex_lock(&((*cache)->mtx), "cache: lock fallita in cache_enqueue");
	file* prev = (*cache);
	mutex_unlock(&((*cache)->next->mtx), "cache: unlock fallita in cache_enqueue");
	file* curr = (*cache)->next;

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
	if (strcmp(prev->f_name, f_name) == 0 && (prev->f_lock == 0 || prev->f_lock == id) ){
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


int cache_readFile(file* cache, char* f_name, byte** buf, size_t* dim_buf, int id)
{
	
	//controllo coda vuota
	mutex_lock(&mtx, "cache: lock fallita in cache_duplicate_control");
	if (cache == NULL){
		mutex_unlock(&mtx, "cache: lock fallita in cache_duplicate_control");
		return -1;
	}
	
	//controllo coda con un elemento
	if (cache->next == NULL && strcmp(cache->f_name, f_name) == 0){
		mutex_unlock(&mtx, "cache: lock fallita in cache_duplicate_control");
		return 0;
	}
	mutex_unlock(&mtx, "cache: lock fallita in cache_duplicate_control");
	
	//due o piu elementi in coda
	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	file* prev = cache;
	mutex_lock(&(cache->next->mtx), "cache: unlock fallita in cache_duplicate_control");
	file* curr = cache->next;

	file* aux;
	while (curr->next != NULL && strcmp(prev->f_name, f_name) != 0){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_duplicate_control");

	}
	file* node = NULL;
	if (strcmp(prev->f_name, f_name) == 0 && (prev->f_lock == 0 || prev->f_lock == id)){
		node = prev;
	}else{
		if (strcmp(curr->f_name, f_name) == 0 && (curr->f_lock == 0 || curr->f_lock == id))
			node = curr;
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");

	mutex_lock(&(node->mtx), "cache: lock in cache_readFile fallita");
	*dim_buf = node->f_size;
	ec_null((*buf = calloc(sizeof(byte), *dim_buf)), "cache: calloc in cache_readFile fallita");
	for (int i = 0; i < *dim_buf; i++){
		(*buf)[i] = node->f_data[i];
	}
	printf("DEBUG: fino a qui tutto bene\n");
	mutex_unlock(&(node->mtx), "cache: unlock in cache_readFile fallita");
	if (node != NULL) return 0;
	else return -1;
}


int cache_readNFile(file* cache, int N, int id, file*** array)
{
	//caso coda vuota
	if(cache == NULL) return 0;

	if (N < 0) N = max_storage_file;
	ec_null((*array = calloc(sizeof(file*), N)), "cache_readNFile: calloc fallita");
	for(int i = 0; i < N; i++){
		(*array)[i] = malloc(sizeof(file));
	}

	//caso coda con un elemento
	mutex_lock(&mtx, "cache_readNFile: lock fallita");
	if(cache->next == NULL && (cache->f_lock == 0 || cache->f_lock == id)){
		size_t len = strlen(cache->f_name);
		(*array)[0]->f_name = calloc(sizeof(char), len);
		strncpy((*array)[0]->f_name, cache->f_name, len);
		//copia data e size data
		(*array)[0]->f_size = cache->f_size;
		(*array)[0]->f_data = calloc(sizeof(byte), cache->f_size);
		for (int j = 0; j < cache->f_size; j++)
			(*array)[0]->f_data[j] = cache->f_data[j];
		mutex_unlock(&mtx, "cache_readNFile: unlock fallita");
		return 1;
	}
	mutex_unlock(&mtx, "cache_readNFile: unlock fallita");

	printf("DEBUGDEBUGDEBUG: fino a qui tutto bene\n");

	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	file* prev = cache;
	mutex_lock(&(cache->next->mtx), "cache: unlock fallita in cache_duplicate_control");
	file* curr = cache->next;

	file* aux = prev;
	size_t scan = 0;
	int i = 0;
	while (curr->next != NULL && N > 0 ){
		if (prev->f_lock == 0 || prev->f_lock == id){
			size_t len = strlen(prev->f_name);
			(*array)[i]->f_name = calloc(sizeof(char), len);
			strncpy((*array)[i]->f_name, prev->f_name, len);
			//copia data e size data
			(*array)[i]->f_size = prev->f_size;
			(*array)[i]->f_data = calloc(sizeof(byte), prev->f_size);
			for (int j = 0; j < prev->f_size; j++)
				(*array)[i]->f_data[j] = prev->f_data[j];
			N--; scan++; i++;
		}
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_duplicate_control");

	}
	if (curr->next == NULL){
		if (--N > 0 && (prev->f_lock == 0 || prev->f_lock == id && ++i)){
			size_t len = strlen(prev->f_name);
			(*array)[i]->f_name = calloc(sizeof(char), len);
			strncpy((*array)[i]->f_name, prev->f_name, len);
			//copia data e size data
			(*array)[i]->f_size = prev->f_size;
			(*array)[i]->f_data = calloc(sizeof(byte), prev->f_size);
			for (int j = 0; j < prev->f_size; j++)
				(*array)[i]->f_data[j] = prev->f_data[j];
			scan++;
		}
		if (--N > 0 && (curr->f_lock == 0 || curr->f_lock == id) && ++i ){
			size_t len = strlen(curr->f_name);
			(*array)[i]->f_name = calloc(sizeof(char), len);
			strncpy((*array)[i]->f_name, curr->f_name, len);
			//copia data e size data
			(*array)[i]->f_size = curr->f_size;
			(*array)[i]->f_data = calloc(sizeof(byte), curr->f_size);
			for (int j = 0; j < curr->f_size; j++)
				(*array)[i]->f_data[j] = curr->f_data[j];
			scan++;
		}
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	printf("DEBUG: file letti = %zu\n", scan);
	return scan;
}

//funzione di deallocazione
file* cache_dealloc(file* cache)
{
	if(cache == NULL) return  NULL;
	if(cache->next == NULL){
		free(cache);
		return NULL;
	}
	
	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
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
		free(aux);
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	free(prev);
	free(curr);
	return NULL;
}

void print_queue(file* cache)
{
	if(cache != NULL){
		printf("%s ", cache->f_name);
		print_queue(cache = cache->next);
	}else{
		printf("\n");
	}
}

int main(){

	file* cache = NULL;
	create_cache(190, 50);
	char dirname[4] = {'a', 'g', 'o', '\0'};

	char f_name1[6] = {'f', 'i', 'l', 'e', '1', '\0'};
	byte data1[6] = {'c', 'i', 'a','c', 'i', '\0'};
	cache_writeFile(&cache, f_name1, data1, 6, dirname, 1996);
	print_queue(cache);
/*
	char f_name2[6] = {'f', 'i', 'l', 'e', '2', '\0'};
	byte data2[8] = {'c', 'o', 'm', 'e', 's', 't', 'a', '\0'};
	cache_writeFile(&cache, f_name2, data2, 8, dirname, 1996);
	print_queue(cache);

	char f_name3[6] = {'f', 'i', 'l', 'e', '3', '\0'};
	byte data3[5] = {'s', 't', 'a', 'i', '\0'};
	cache_writeFile(&cache, f_name3, data3, 5, dirname, 1996);
	print_queue(cache);


	char f_name4[6] = {'f', 'i', 'l', 'e', '4', '\0'};
	byte data4[8] = {'s', 't', 'a', 'i','s', 't', 'a', '\0'};
	cache_writeFile(&cache, f_name4, data4, 8, NULL, 1996);
	print_queue(cache);

	//cache = cache_dealloc(cache);
	byte datax[4] = {'c', 'i', 'a', '\0'};
	cache_appendToFile(&cache, f_name4, datax, 4, NULL, 1996);

	//cache_removeFile(&cache, f_name1, 1996);

	byte* buf;
	size_t dim_buf;
	cache_readFile(cache, f_name4, &buf, &dim_buf, 1996);
	if(buf){
		//printf("leggo: %s\n", f_name4);
		for(int i = 0; i < dim_buf; i++)
			printf("%c", buf[i]);
		printf("\n");
	}
*/
	file** array;
	cache_readNFile(cache, 1, 1996, &array);

	printf("file: %s\n", array[0]->f_name);

/*
	cache_readNFile(cache, 10, "file_letti", 1996);
	cache_lockFile(cache, "file1", 1996);
	cache_research(cache, "file2");
	cache_removeFile(&cache, f_name4, 1997);
*/
	
	return 0;
}