//note:
//1. considera nella procedure se un file è lockato dal client
//2. deallocazione coda 

#include "err_control.h"
typedef unsigned char byte;


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
	byte f_lock;		//il file è in locl
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
	used_mem = 0;
}

//controllo capacità residua se si scrive un file
int cache_capacity_control(int dim_file)
{
	if(used_mem + dim_file > cache_capacity) return -1;
	else return 0;
}

//aggiornamento capacità cache
void cache_capacity_update(int dim_file)
{
	used_mem = used_mem + dim_file;
	printf("cache used memory = %lu\n", used_mem);
}

//controlla se il file è un duplicato
int cache_duplicate_control(file* cache, char* f_name)
{
	//caso cache vuota;
	if(cache == NULL) return 0;
	//caso cache non vuota
	while(cache != NULL){
		if(strcmp(cache->f_name, f_name) == 0) return 1;
		cache = cache->next;
	}
	return 0;
}
//scrittura di un file in cache (usa enqueue)
int cache_writeFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname)
{	


	//caso coda noon vuota -> controllo rimpiazzo
	file* node_rep = NULL;
	if(*cache != NULL && cache_capacity_control(dim_f) == -1){
		//capienza non sufficiente ->rimpiazzo
		printf("memoria cache non sufficiente: rimpiazzo necessario\n");
		if((node_rep = replacement_algorithm(*cache, f_name, dim_f, dirname)) == NULL){
			printf("cache: rimpiazzo fallito, scrittura %s non avvenuta\n", f_name);
			return -1;
		}
	}
	
	file* new = NULL;	
	//se la coda è vuota
	if(*cache  == NULL) *cache = new;
	
	//se rimpiazzo
	if(node_rep != NULL){
		new = node_rep;
	}else{
		//no rimpiazzo	
		new = cache_enqueue(cache);
	}

	mutex_lock(&(new->mtx), "cache: lock in cache_writeFile fallita");
	//inf. setting node new;
	size_t len_f_name = strlen(f_name);
	ec_null((new->f_name = calloc(sizeof(char), len_f_name)), "cache: errore calloc");
	strncpy(new->f_name, f_name, len_f_name);
	new->f_size = dim_f;
	ec_null((new->f_data = calloc(sizeof(byte), dim_f)), "cache: calloc cache_writeFile fallita");
	for (int i = 0; i < dim_f; i++){
		new->f_data[i] = f_data[i];
	}
	//aggiornamento capacità cache post-scrittra
	cache_capacity_update(dim_f);
	
	mutex_unlock(&(new->mtx), "cache: unlock in cache_writeFile fallita");
	
	printf("scrittura %s effettuata\n", new->f_name);	
	printf("fino a qui tutto bene\n");
	return 0;
}

//algoritmo di rimpiazzo
file* replacement_algorithm(file* cache, char* f_name, size_t dim_f, char* dirname)
{
	//cerca nodo che rispetti condizione di rimpiazzo (size_old >= size_new)
	//caso peggiore O(n), caso migliore O(1) (testa e coda sono invertiti)
	//return ptr al nodo di rimpiazzo


	printf("DEBUG: fino a qui tutto bene\n");

	//almeno due nodi in lista
	mutex_lock(&((*cache)->mtx), "cache: lock fallita in replacement_algorithm");
	file* prev = *cache;
	mutex_unlock(&((*cache)->mtx), "cache: unlock fallita in replacement_algorithm");
	file* curr = (*cache)->next;
	
	mutex_lock(&(prev->mtx), "cache: lock fallita in replacement_algorithm");
	mutex_lock(&(prev->mtx), "cache: lock fallita in replacement_algorithm");


	file* aux;
	while (curr->next != NULL){
		aux = prev;
		prev = curr;
		curr = curr->next;
		pthread_mutex_lock(&(curr->mtx));
		pthread_mutex_unlock(&(aux->mtx));

	}
	curr->next = new;
	pthread_mutex_unlock(&(prev->mtx));
	pthread_mutex_unlock(&(curr->mtx));





	//cerca un nodo rimpiazzabile
	file* temp = cache;
	while (temp->next != NULL){
		if (temp->f_size >= dim_f && !temp->f_lock && !temp->f_write) break;
		temp = temp->next;
	}
	//controllo terminazione ricerca 
	if (temp->next == NULL && temp->f_size >= dim_f ){
		printf("rimpiazzo impossibile\n");
		return NULL;
	}else{
		//scrittura del file espulso in dirname
		if (dirname != NULL){
			//se non esiste, creare directory per scrittura file
			if (mkdir(dirname,  S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
				LOG_ERR(errno, "cache replacement_algorithm:");
			}
			//mi sposto nella cartella e scrivo il file
			chdir(dirname);	
			FILE* fd;
			ec_null((fd = fopen(temp->f_name, "wr")), "cache: fopen fallita in replacement_algorithm");
			ec_meno1(fwrite(temp->f_data, sizeof(char), temp->f_size, fd), "cache: fwrite fallita in replacement_algorithm");
			fclose(fd);
			//torno alla directory di partenza
			chdir("../");				
		}
		cache_capacity_update(dim_f);
	}
	return temp;
}	



//inserisce un codo in testa
file* cache_enqueue(file** cache)
{
	printf("DEBUG: enqueue\n");
	
	file* new;
	ec_null((new = malloc(sizeof(file))), "cache: malloc cache_writeFile fallita");
	new->next = NULL;
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in cache_create_file");
		exit(EXIT_FAILURE);	
	}

	//caso 1: cache vuota
	if (*cache == NULL){

		mutex_lock(&(new->mtx), "cache: lock fallita in cache_enqueue");
		*cache = new;
		mutex_unlock(&(new->mtx), "cache: unlock fallita in cache_enqueue");
		printf("DEBUG: enqueue terminata\n");
		return *cache;
	}
	//caso cache con un elemento
	if(*cache->next == NULL){
		*cache->next = new;
	}

	//caso cache non vuota con almeno due elementi: 
	//collacazione nodo in testa (alla fine della lista)
	mutex_lock(&((*cache)->mtx), "cache: lock fallita in cache_enqueue");
	file* prev = *cache;
	pthread_mutex_unlock(&((*cache)->mtx));
	file* curr = (*cache)->next;

	pthread_mutex_lock(&(prev->mtx));
	pthread_mutex_lock(&(curr->mtx));

	file* aux;
	while (curr->next != NULL){
		aux = prev;
		prev = curr;
		curr = curr->next;
		pthread_mutex_lock(&(curr->mtx));
		pthread_mutex_unlock(&(aux->mtx));

	}
	curr->next = new;
	pthread_mutex_unlock(&(prev->mtx));
	pthread_mutex_unlock(&(curr->mtx));

	printf("DEBUG: enqueue terminata\n");
	return new;
}



/*
int cache_appendToFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname)
{

		file* node_rep = NULL;	//puntatore al nodo da rimpiazzare
	//controllo capienza cache pre-scrittura
	if(cache_capacity_control(dim_f) == -1){
		//serve memorizzare il file espulso?
		char* file_rep;
		//CASO 1: capienza non è sufficiente -> si tenta il rimpiazzo
		if((node_rep = replacement_algorithm(*cache, f_name, dim_f, dirname)) == NULL){
			fprintf(stderr, "cache: rimpiazzo impossibile - scrittura %s fallita\n", f_name);
			return -1; //scrittura fallita
		}
	}
	if (pthread_mutex_lock(&(node_rep->mtx)) != 0){
		LOG_ERR(errno, "cache: lock fallita in cache_appendToFile");
		exit(EXIT_FAILURE);
	}	

	//data setting nodo
	strcpy(f_name, node->f_name);
	node->f_size = dim_f;

	ec_null((node_rep->f_data = calloc(sizeof(byte), node_rep->f_size)), "cache: calloc cache_writeFile fallita");
	//new->f_data = calloc(sizeof(byte), f_size);
	for (int i = 0; i < dim_f; i++){
		node_rep->f_data[i] = f_data[i];
	}

	//aggiornamento capacità cache post-scrittra
	cache_capacity_update(dim_f);
	if (pthread_mutex_unlock(&(node_rep->mtx)) != 0){
		LOG_ERR(errno, "cache: unlock fallita in cache_appendToFile");
		exit(EXIT_FAILURE);
	}
	return 0;
}


int cache_lockFile(file* cache, char* f_name)
{
	if (pthread_mutex_lock(&(cache->mtx)) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in lockFile");
		exit(EXIT_FAILURE);
	}
	//ricerca elemento
	file* temp = cache;
	while(temp->next != NULL){
		if(strcmp(temp->f_name, f_name) == 0){
			temp->f_lock == 1;
		}
		temp = temp->next;
	}
	if(temp->next == NULL && strcmp(temp->f_name, f_name) != 0 ){ 
		if (pthread_mutex_lock(&(cache->mtx)) != 0){
			LOG_ERR(errno, "cache: pthread_mutex_init fallita in lockFile");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	
	if (pthread_mutex_lock(&(cache->mtx)) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in lockFile");
		exit(EXIT_FAILURE);
	}
	return 0;
}


int cache_unlockFile(file* cache, char* f_name)
{
	if (pthread_mutex_lock(&(cache->mtx)) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in unlockFile");
		exit(EXIT_FAILURE);
	}
	//ricerca elemento
	file* temp = cache;
	while(temp->next != NULL){
		if(strcmp(temp->f_name, f_name) == 0){
			if(!temp->f_lock && !temp->f_write){
				temp->f_lock == 0;
			}else{
				return -1;
			}
		}
		temp = temp->next;
	}
	
	if(temp->next == NULL && strcmp(temp->f_name, f_name) != 0 ){
		if (pthread_mutex_unlock(&(cache->mtx)) != 0){
			LOG_ERR(errno, "cache: unlock fallita in unlockFile");
			exit(EXIT_FAILURE);
		}
		return -1;
	}
	return 0;
}

int cache_readFile(file* cache, char* f_name, byte** buf, size_t* dim_buf)
{
	if (pthread_mutex_lock(&(cache->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: lock in cache_readFile fallita");
		exit(EXIT_FAILURE);
	}

	file* temp = cache;
	while (temp->next != NULL && strcmp(temp->f_name, f_name) == 0){
		temp->temp->next;	
	}
	if(!temp->f_lock && !temp->f_write){
		*buf = temp->f_data;
		dim_buf = temp->f_size;
	}else{
		return -1;
	}

	if (pthread_mutex_unlock(&(cache->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: unlock in cache_readFile fallita");
		exit(EXIT_FAILURE);
	}
	return 0;

}

int cache_readNFile(file* cache, size_t N, char* dirname)
{
	if (pthread_mutex_lock(&(cache->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: lock in cache_readFile fallita");
		exit(EXIT_FAILURE);
	}


	//creo cartella di scrittura dei file
	if (mkdir(dirname,  S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
		LOG_ERR(errno, "cache replacement_algorithm:");
		return -1;
	}
	//mi sposto nella cartella
	chdir(s);
	int letti = 0;
	//li leggo dalla coda in poi
	file* temp = cache;
	while(N > 0 && temp->next != NULL){
		if(!temp->f_lock && !temp->f_write){
			FILE* fd;
			if (ec_null(fd = fopen(dirname, "wr"), "cache: fopen fallita in replacement_algorithm")) return -1;
			if (ec_meno1(fwrite(temp->f_data, sizeof(char), temp->f_size, fd), "cache: fwrite fallita in replacement_algorithm")) return -1;
			fclose(fd);
			letti++;
		}
		N--;
	}
	//torno alla directory di partenza
	chdir("../");	

	if (pthread_mutex_unlock(&(cache->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: unlock in cache_readFile fallita");
		exit(EXIT_FAILURE);
	}
	return letti;
}


//cache_removeFile
int cache_removeFile(file* cache, char* f_name)
{

	if (pthread_mutex_lock(&(cache->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: lock in cache_removeFile fallita");
		exit(EXIT_FAILURE);
	}
	file* curr = cache;
	file* prev;

	while(temp->next != NULL){
		//nodo trovato 
		if(strcmp(f_name, temp->f_name) == 0){
			break;	
		}
		prev = temp;
		temp = temp->next;
	}
	prev->next = temp->next;
	free(temp);

	if (pthread_mutex_unlock(&(cache->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: unlock in cache_removeFile fallita");
		exit(EXIT_FAILURE);
	}
	return 0;
}
*/


int main(){

	file* cache = NULL;
	create_cache(8, 50);
	char dirname[4] = {'a', 'g', 'o', '\0'};

	char f_name[4] = {'u', 'g', 'o', '\0'};
	byte data[5] = {'c', 'i', 'a', 'o', '\0'};
	cache_writeFile(&cache, f_name, data, 5, dirname);

	//cache = cache_enqueue(&cache);
	//cache = cache_enqueue(&cache);
	while(cache != NULL){
		printf("cache->f_name: %s\n", cache->f_name);
		cache = cache->next;
	}
	char f_name1[2] = {'a', '\0'};
	byte data1[3] = {'c', 'i' ,'\0'};
	cache_writeFile(&cache, f_name1, data1, 3, dirname);
	
	char f_name2[2] = {'b', '\0'};
	byte data2[3] = {'1', '2' , '\0'};
	cache_writeFile(&cache, f_name2, data2, 3, dirname);

	while(cache != NULL){
		printf("cache->f_name: %s\n", cache->f_name);
		cache = cache->next;
	}

	return 0;

}