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
	byte writefile;		//il file è in scrittura
	byte lockfile;		//il file è in locl
	pthread_mutex_t mtx;	//mutex nodo
	struct _file* next;	//nodo successivo
}file;


//prototipi
void create_cache(int mem_size, int max_file_in_cache);
void cache_capacity_control(int dim_file);
int cache_duplicate_control(file* cache, char* f_name);
void cache_capacity_update(int dim_file);
file* cache_writeFile(file* cache, char* f_name, byte* f_data, size_t dim_f, char* dirname);
void replacement_algorithm(file* cache, char* f_name, byte* f_data, size_t dim_f, char* dirname);





//funzionamento: cerca un nodo la cui f_size sia >= dimf per poter effettuare il rimpiazzo del file
//partendo dalla coda poiché si adotta lo schema FIFO
//caso peggiore O(n), caso migliore O(1)
void replacement_algorithm(file* cache, char* f_name, byte* f_data, size_t dim_f, char* dirname)
{
	file* temp = cache;

	while(temp->f_size >= dim_f && temp->next != NULL){
		temp = temp->next;
	}
	//controllo terminazione
	if(temp->next == NULL && temp->f_size >= dim_f ){
		printf("rimpiazzo impossibile\n");
	}else{
		//si procede con il rimpiazzo -> il file in temp viene sostituito ma va prima scritto in dirname se != NULL;
		if(temp->f_size >= dim_f){
			//salvo nome file da espellere
			char* rep_file;
			strcpy(rep_file, temp->f_name);
			//scrittura del file espulso in dirname
			if(dirname != NULL){
				//scrittura del file in dirname
			}
			temp->f_size = dim_f;
			cache_capacity_update(dim_f)
7
		}

	}

}	





//inizializzare la cache
void create_cache(int mem_size, int max_file_in_cache)
{
	cache_capacity = mem_size;
	max_storage_file = max_file_in_cache;
	used_mem = 0;
}


//controllo della capacità rispetto ad un nuovo file che deve essere scritto
//se si sfora è necessario un rimpiazzo prima della scrittura
void cache_capacity_control(int dim_file)
{
	if(used_mem + dim_file > cache_capacity) return -1;
	else return 0;
}

void cache_capacity_update(int dim_file)
{
	used_mem = used_mem - dim_file;
}

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


//(!) il file eventualmente espulso dall'algoritmo di rimpiazzo?
//equivalente a una enqueue in O(n) (come fosse un inserimento in coda, dove coda = testa)
file* cache_writeFile(file* cache, char* f_name, byte* f_data, size_t dim_f)
{	

	if(cache_capacity_control(dim_f) == -1){
		char* file_rep;
		replacement_algorithm
		return cache;
	}

	//procedere in primis con l'algoritmo di rimpiazzo


	//creazione e setting paramentri nodo
	file* new;
	ec_null((new = malloc(sizeof(file))), "cache: malloc cache_writeFile fallita");
	
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: unlock fallita in cache_create_file");
		exit(EXIT_FAILURE);
	}
	new->next = NULL;
	strcpy(f_name, new->f_name);
	new->f_size = dim_f;

	//data set
	ec_null((new->f_data = calloc(sizeof(byte), f_size)), "cache: calloc cache_writeFile fallita");
	//new->f_data = calloc(sizeof(byte), f_size);
	for (int i = 0; i < f_size; i++){
		new->f_data[i] = f_data[i];
	}

	cache_capacity_update(dim_f);
	
	if (pthread_mutex_lock(&(new->mtx) != 0){
		LOG_ERR(errno, "cache: unlock fallita in cache_create_file");
		exit(EXIT_FAILURE);
	}

	//caso cache vuota
	if (cache == NULL){
		cache = new;
		if (pthread_mutex_unlock(&(new->mtx)) != 0){
			LOG_ERR(errno, "cache: lock fallita in cache_create_file");
			exit(EXIT_FAILURE);
		}	
		return cache;
	}

	//caso cache non vuouta: collacazione nodo
	file* temp = cache;
	while (temp->next == NULL){
		temp = temp->next;
	}
	temp->next = new;
	new->next = NULL;

	if (pthread_mutex_unlock(&(new->mtx)) != 0){
		LOG_ERR(errno, "cache: unlock fallita in cache_create_file");
		exit(EXIT_FAILURE);
	}
	
	return cache;
}




// operazioni supportate:


//cache_readFile
//cache_readNFiles

//cache_lockFile
//cache_unlockFile

//cache_removeFile

//cache_capacity_update

// procedure di controllo di stato

//cache_fileIsDulicate
//cacheIsEmpty
//cacheIsFull
