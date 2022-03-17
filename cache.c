// 1.	in seguito ad una scrittura (writeFile o appendToFile)i file eventualmente rimossi per via di cache piena dall'algoritmo di rimpiazzo
//	se la dirname != NULL dovranno essere scritti in dirname
//	questa operazione è specificata alla chiamata delle funzioni di scrittura considerando dirname e passandola all'algoritmo di rimpiazza
// 	nelle attuali procedure di scrittura non si considerano i rimpiazzi -> implementa
//	scrittura -> algoritmo di rimpiazzo


static size_t cache_capacity;
static size_t max_storage_file;
static size_t used_mem;

//struct per definire tipo di un nodo
typedef struct _file
{
	char* f_name;		//nome del file
	size_t f_size;		//lung. in byte
	byte* f_data		//array di byte di lung. f_size
	byte f_write;		//il file è in scrittura
	byte f_lock;		//il file è in locl
	pthread_mutex_t mtx;	//mutex nodo
	struct _file* next;	//nodo successivo
}file;


//prototipi
void create_cache(int mem_size, int max_file_in_cache);
int cache_writeFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname);
int cache_capacity_control(int dim_file);
int cache_duplicate_control(file* cache, char* f_name);


//inizializza cache
void create_cache(int mem_size, int max_file_in_cache)
{
	cache_capacity = mem_size;
	max_storage_file = max_file_in_cache;
	used_mem = 0;
}

//scrittura di un file in cache (usa enqueue)
int cache_writeFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname)
{	

	pthread_mutex_lock(&(*cache->mtx));
	*cache->lock = 1;
	//controllo capienza cache pre-scrittura
	if(cache_capacity_control(dim_f) == -1){

		char* file_rep;//serve memorizzare il file espulso?
		
		//CASO 1: capienza non è sufficiente -> si tenta il rimpiazzo
		file* node_rep;
		if((node_rep = replacement_algorithm(cache, f_name, dim_f, dirname)) == NULL){
			fprintf(stderr, "cache: rimpiazzo impossibile - scrittura %s fallita\n", f_name);
			*cache->lock = 0;
			pthread_mutex_unlock(&(*cache->mtx));
			return -1;
		}
	}

	file* new;
	if(node_rep != NULL){ 		//se ho un rimpiazzo setto il nodo rimpiazzato
		new = node_rep;
	}else{
		new = enqueue(cache);	//altrimenti alloco un nuovo nodo e setto
	}

	//data setting nodo
	strcpy(f_name, new->f_name);
	new->f_size = dim_f;
	ec_null((new->f_data = calloc(sizeof(byte), f_size)), "cache: calloc cache_writeFile fallita");
	for (int i = 0; i < f_size; i++){
		new->f_data[i] = f_data[i];
	}

	//aggiornamento capacità cache post-scrittra
	cache_capacity_update(dim_f);
	
	pthread_mutex_unlock(&(*cache->mtx));
	*cache->lock = 0;
	return 0;
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
	used_mem = used_mem - dim_file;
}

//controlla se il file è un duplicato
int cache_duplicate_control(file* cache, char* f_name)
{
	//caso cache vuota;
	if(cache == NULL) return 0;
	//caso cache non vuota
	while(cache->next != NULL){
		if(strcmp(cache->f_name, f_name) == 0) return 1
		cache = cache->next;
	}
	return 0;
}

//inserisce un codo in testa
file* cache_enqueue(file** cache)
{
	//allocazione
	file* new;
	ec_null((new = malloc(sizeof(file))), "cache: malloc cache_writeFile fallita");

	//setting
	new->next = NULL;
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in cache_create_file");
		exit(EXIT_FAILURE);
	}

	//lock
	if (pthread_mutex_lock(&(new->mtx)) != 0){
		LOG_ERR(errno, "cache: unlock fallita in cache_create_file");
		exit(EXIT_FAILURE);
	}
	new->f_lock = 1;
	//caso cache vuota
	if (*cache == NULL){
		*cache = new;
		//unlock1
		if (pthread_mutex_unlock(&(new->mtx)) != 0){
			LOG_ERR(errno, "cache: lock fallita in cache_create_file");
			exit(EXIT_FAILURE);
		}
		new->f_lock = 1;	
		return NULL;
	}

	//caso cache non vuouta: collacazione nodo
	file* temp = *cache;
	while (temp->next == NULL){
		temp = temp->next;
	}
	temp->next = new;
	new->next = NULL;
	//unlock2
	if (pthread_mutex_unlock(&(new->mtx)) != 0){
			LOG_ERR(errno, "cache: lock fallita in cache_create_file");
			exit(EXIT_FAILURE);
	}
	return new;
}




file* replacement_algorithm(file* cache, char* f_name, size_t dim_f, char* dirname)
{
	//cerca nodo che rispetti condizione di rimpiazzo (size_old >= size_new)
	//caso peggiore O(n), caso migliore O(1) (testa e coda sono invertiti)
	//return ptr al nodo di rimpiazzo

	if (pthread_mutex_lock(&(cache->mtx)) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in cache_create_file");
		exit(EXIT_FAILURE);
	}

	//cerca un nodo rimpiazzabile
	file* temp = cache;
	while(temp->f_size >= dim_f && temp->next != NULL){
		temp = temp->next;
	}
	//controllo terminazione ricerca 
	if(temp->next == NULL && temp->f_size >= dim_f ){
		printf("rimpiazzo impossibile\n");
		if (pthread_mutex_unlock(&(cache->mtx)) != 0){
			LOG_ERR(errno, "cache: lock fallita in replacement_algorithm");
			exit(EXIT_FAILURE);
		}
		return NULL;
	}else{
		//nodo trovato: si procede con il rimpiazzo -> il file sostituito si scrive in dirname se != NULL
		//salvo nome file da espellere (!) serve?
		char* rep_file;
		size_t len = stelen(rep_file);
		strcpy(rep_file, temp->f_name);
		
		//scrittura del file espulso in dirname
		if (dirname != NULL){
			//se non esiste, creare directory per scrittura file
			if (mkdir(dirname,  S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
				LOG_ERR(errno, "cache replacement_algorithm:");
			}
			//mi sposto nella cartella e scrivo il file
			chdir(s);	
			FILE* fd;
			ec_null(fd = fopen(dirname, "wr"), "cache: fopen fallita in replacement_algorithm");
			ec_meno1(fwrite(rep_file, sizeof(char), rep_len, fd), "cache: fwrite fallita in replacement_algorithm");
			fclose(fd);
			//torno alla directory di partenza
			chdir("../");				
		}
		cache_capacity_update(dim_f);
	}
	if (pthread_mutex_unlock(&(cache->mtx)) != 0){
			LOG_ERR(errno, "cache: lock fallita in replacement_algorithm");
			exit(EXIT_FAILURE);
	}
	return temp;
}	





int cache_appendToFile(file** cache, char* f_name, byte* f_data, size_t dim_f, char* dirname)
{
	//controllo capienza cache pre-scrittura
	if(cache_capacity_control(dim_f) == -1){
		//serve memorizzare il file espulso?
		char* file_rep;
		//CASO 1: capienza non è sufficiente -> si tenta il rimpiazzo
		file* node_rep = NULL;	//puntatore al nodo da rimpiazzare
		if((node_rep = replacement_algorithm(cache, f_name, dim_f, dirname)) == NULL){
			fprintf(stderr, "cache: rimpiazzo impossibile - scrittura %s fallita\n", f_name);
			return -1; //scrittura fallita
		}
	}

	//data setting nodo
	strcpy(f_name, node->f_name);
	node->f_size = dim_f;

	ec_null((node->f_data = calloc(sizeof(byte), f_size)), "cache: calloc cache_writeFile fallita");
	//new->f_data = calloc(sizeof(byte), f_size);
	for (int i = 0; i < f_size; i++){
		node->f_data[i] = f_data[i];
	}

	//aggiornamento capacità cache post-scrittra
	cache_capacity_update(dim_f);
	return 0;
}

}




int cache_lockFile(file* cache, char* f_name)
{
	//ricerca elemento
	file* temp = cache;
	while(temp->next != NULL){
		if(strcmp(temp->f_name, f_name) == 0){
			temp->f_lock == 1;
		}
		temp = temp->next;
	}
	if(temp->next == NULL && strcmp(temp->f_name, f_name) != 0 ) return -1;
	return 0;
}

int cache_lockFile(file* cache, char* f_name)
{
	//ricerca elemento
	file* temp = cache;
	while(temp->next != NULL){
		if(strcmp(temp->f_name, f_name) == 0){
			temp->f_lock == 0;
		}
		temp = temp->next;
	}
	if(temp->next == NULL && strcmp(temp->f_name, f_name) != 0 ) return -1;
	return 0;
}

// operazioni supportate:

//cache_appendToFile

//cache_readFile
//cache_readNFiles

//cache_removeFile

