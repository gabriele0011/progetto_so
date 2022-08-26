#include "cache.h"

//funzioni cache 
void create_cache(int mem_size, int max_file_in_cache)
{
	cache_capacity = mem_size;
	max_storage_file = max_file_in_cache;
	//memoria attualmente occupata
	used_mem = 0;
	//numero di file memorizzati in cache
	files_in_cache = 0;
}
file* cache_research(file* cache, const char* f_name)
{
	printf("(CACHE) - cache_research:	cerco %s\n", f_name); //DEBUG
	//print_queue(cache); //DEBUG
	file* node = NULL;
	//CODA VUOTA
	mutex_lock(&mtx, "lock in cache_appendToFile");
	if (cache == NULL){
		mutex_unlock(&mtx, "lock in cache_appendToFile");
		//printf("(CACHE) - cache_research:	cache vuota\n"); //DEBUG
		return NULL;
	}
	mutex_unlock(&mtx, "lock in cache_appendToFile");
	
	//CODA CON UN ELEMENTO
	mutex_lock(&(cache->mtx), "lock in cache_appendToFile");
	if (cache->next == NULL){
		if(strcmp(cache->f_name, f_name) == 0){
			mutex_unlock(&(cache->mtx), "lock in cache_appendToFile");
			//printf("(CACHE) - cache_research:	trovato\n"); //DEBUG
			return cache;
		}else{
			mutex_unlock(&(cache->mtx), "lock in cache_appendToFile");
			//printf("(CACHE) - cache_research:	non trovato\n"); //DEBUG
			return NULL;
		}
	}
	
	//printf("BUG HERE?!\n");
	//CODA CON DUE O PIU ELEMENTI
	file* prev = cache;
	mutex_lock(&(cache->next->mtx), "lock in cache_appendToFile");
	file* curr = cache->next;
	file* aux;
	
	int found = 0;
	while (curr->next != NULL){
		if(strcmp(prev->f_name, f_name) == 0){
			found = 1;
			node = prev;
			break;
		}
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "lock in cache_appendToFile");
		mutex_unlock(&(aux->mtx), "unlock in cache_appendToFile");
	}
	
	//CONTROLLO PRIMI/ULTIMI DUE ELEMENTI
	if(!found){
		if (strcmp(prev->f_name, f_name) == 0){
			node = prev;
			found = 1;
		}
		if (strcmp(curr->f_name, f_name) == 0){
			node = curr;
			found = 1;
		}
	}
	mutex_unlock(&(prev->mtx), "unlock in cache_appendToFile");
	mutex_unlock(&(curr->mtx), "unlock in cache_appendToFile");
	//printf("CACHE - cache_research: fine ricerca %s\n", f_name); //DEBUG
	if (!found){ 
		//printf("(CACHE) - cache_research:	non trovato\n"); //DEBUG 
		return NULL;
	}else{
		//printf("(CACHE) - cache_research:	trovato\n"); //DEBUG
		return node;
	}
}
void cache_capacity_update(int dim_file, int new_file_or_not)
{
      mutex_lock(&mtx, "lock in cache_capacity_update");
	used_mem = used_mem + dim_file;
	files_in_cache += new_file_or_not;
	printf("(STATO CACHE) : 		used memory = %lu di %zu - n. files = %zu\n", used_mem, cache_capacity, files_in_cache);
	mutex_unlock(&mtx, "unlock in cache_capacity_update");
}
void cache_dealloc(file** cache)
{
	mutex_lock(&mtx, "cache_dealloc: lock fallita");
	if(*cache == NULL){ 
		*cache = NULL;
		mutex_unlock(&mtx, "cache_dealloc: lock fallita");
		printf("(CACHE) - cache_dealloc:	deallocazione cache avvenuta\n"); //DEBUG
		return;
	}
	if((*cache)->next == NULL){
		free(*cache);
		*cache = NULL;
		mutex_unlock(&mtx, "cache_dealloc: lock fallita");
		printf("(CACHE) - cache_dealloc:	deallocazione cache avvenuta\n"); //DEBUG
		return;
	}

	file* curr = *cache;
	file* prev;
	while (curr->next != NULL){
		prev = curr;
		curr = curr->next;
		free(prev);
	}
	*cache = NULL;
	mutex_unlock(&mtx, "cache_dealloc: unlock fallita");
	printf("(CACHE) - cache_dealloc:	deallocazione cache avvenuta\n"); //DEBUG
	return;
}
int cache_removeFile(file** cache, const char* f_name, int id)
{
	printf("(CACHE) - cache_removeFile:		su f_name: %s / id=%d\n", f_name, id); //DEBUG
	//caso lista vuota -> ho scelto di dare esito positivo anche se la condizione non dovrebbe verificarsi
	mutex_lock(&mtx, "cache_removeFile: lock fallita");
	if(*cache == NULL){
		mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
		printf("(CACHE) - cache_removeFile: cache vuota\n"); //DEBUG
		return 0;
	}
	//caso un elemento in lista
	if ((*cache)->next == NULL){
		if (strcmp((*cache)->f_name, f_name) == 0 && ((*cache)->f_lock != 0 || (*cache)->f_lock == id) ){
			int size = (*cache)->f_size;
			file* temp = *cache;
			*cache = NULL;
			free(temp);
			mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
			cache_capacity_update(-size, -1);
			printf("(CACHE) - cache_removeFile: unico elemento eliminato\n"); //DEBUG
			printf("(STAMPA CACHE) :		"); //DEBUG
			print_queue(*cache); //DEBUG
			return 0;
		}else{
			//impossibile eliminare il file
			mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
			return -1;
		}
	}
	mutex_unlock(&mtx, "cache_removeFile: unlock fallita");

	
	//caso lista con due o piu elementi
	mutex_lock(&((*cache)->mtx), "cache_removeFile: lock fallita");
	file* prev = (*cache);
	mutex_lock(&((*cache)->next->mtx), "cache_removeFile: unlock fallita");
	file* curr = (*cache)->next;

	file* aux;
	int found = 0;
	while (curr->next != NULL){
		if (strcmp(prev->f_name, f_name) == 0 && (prev->f_lock != 0  || prev->f_lock == id)){
			found = 1;
			break;
		}
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache_removeFile: lock fallita in cache_enqueue");
		mutex_unlock(&(aux->mtx), "cache_removeFile: unlock fallita in cache_enqueue");

	}
	//se il nodo da rimuovere è prev (puo essere il primo e in mezzo alla lista)
	if (strcmp(prev->f_name, f_name) == 0 && (prev->f_lock != 0 || prev->f_lock == id) ){
		//se si tratta del primo nodo della coda
		if (prev == *cache) *cache = prev->next;
		//altrimenti collego aux a curr (prev è in mezzo)
		else aux->next = prev->next;
		//aggiornamento capacità cache
		cache_capacity_update(-(prev->f_size), -1);
		printf("(STAMPA CACHE) :		"); //DEBUG
		print_queue(*cache); //DEBUG
		pthread_mutex_destroy(&prev->mtx);
		free(prev);
		found = 1;
	}else{
		//se si tratta dell'ultimo elemento della lista
		if (strcmp(curr->f_name, f_name) == 0 && (curr->f_lock != 0 || curr->f_lock == id)){
				prev->next = curr->next; //curr->next = NULL
				pthread_mutex_destroy(&curr->mtx);
				free(curr);
				//aggiornamento capacità cache
				cache_capacity_update(-(curr->f_size), -1);
				printf("(STAMPA CACHE) :		"); //DEBUG
				print_queue(*cache); //DEBUG
				found = 1;
		}
	}
	mutex_unlock(&(prev->mtx), "cache_removeFile: unlock fallita in cache_enqueue");
	mutex_unlock(&(curr->mtx), "cache_removeFile: unlock fallita in cache_enqueue");
	if(found) printf("(CACHE) - cache_removeFile: eliminazione riuscita\n"); //DEBUG
	return found;
}

//funzioni per scrittura con -w/-W
int cache_insert(file** cache, const char* f_name, char* f_data, size_t dim_f, int id, file** expelled_file)
{	
      printf("(CACHE) - cache_insert: 	scrittura %s (%zu - %zu byte)... \n", f_name, strlen(f_name), dim_f);	 //DEBUG

      //INSERIMENTO CON RIMPIAZZO
      if (files_in_cache+1 > max_storage_file){
		printf("CACHE - cache_insert:		(!) rimpiazzo necessario (per num. max file)\n"); //DEBUG
		if ((*expelled_file = replace_to_write(cache, f_name, f_data, dim_f, id)) == NULL){
			printf("(CACHE) - cache_insert: 		scrittura di %s (%zubyte) fallita, rimpiazzo impossibile\n", f_name, dim_f); //DEBUG
			return -1;
		}else{
			printf("(CACHE) - cache_insert: 	scrittura %s eseguita con rimpiazzo di %s\n",f_name, (*expelled_file)->f_name); //DEBUG
			return 0;
		}
	}
	//INSERIMENTO SENZA RIMPIAZZO
	if (cache_enqueue(cache, f_name, f_data, dim_f, id) == -1){
		LOG_ERR(-1, "cache_writeFile: enqueue fallita");
		return -1;
	}
	printf("(CACHE) - cache_insert:		scrittura di %s eseguita senza rimpiazzo\n", f_name); //DEBUG
      cache_capacity_update(dim_f, 1);
      printf("(STAMPA CACHE) :		"); //DEBUG
	print_queue(*cache);  //DEBUG
	return 0;
}
int cache_enqueue(file** cache,  const char* f_name, char* f_data, size_t dim_f, int id)
{
	printf("(CACHE) cache_enqueue:	su %s (%zu)\n", f_name, strlen(f_name)); //DEBUG
	
	//lista invertita cosi da facilitare l'operazione di estrazione dei file in caso di rimpiazzo (FIFO)
	//l'inserimento diventa in coda e l'estrazione in testa in O(1) nel caso ottimo
	//CREAZIONE + SETTING
      file* new = NULL;
	ec_null_r((new = malloc(sizeof(file))), "cache_enqueue: malloc fallita", -1);
	new->next = NULL;
	new->f_size = dim_f;
	//len pathname + pathname
	size_t len_f_name = strlen(f_name);
	ec_null_r((new->f_name = calloc(sizeof(char), len_f_name)), "cache_enqueue: calloc fallita", -1);
	strncpy(new->f_name, f_name, len_f_name);
	(new->f_name)[len_f_name] = '\0';

	//permessi di scrittura/lettura
	new->f_read = 1;
	new->f_write = 1;
	new->f_open = 1; //se f_open = 1 sappiamo che il file è stato aperto da qualcuno - è un flag indicativo, forse inutile (!)
      new->f_lock = id;
	//aggiunge/aggiorna lista dei processi che hanno aperto il file
	new->id_list = NULL;
	char char_id[101];
	itoa(id, char_id);
	insert_node(&(new->id_list), char_id);
	//print_list(new->id_list);

	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "cache_enqueue: pthread_mutex_init fallita");
		free(new);
		exit(EXIT_FAILURE);	
	}

	//COLLOCAZIONE NODO (in coda)
	mutex_lock(&mtx, "cache: lock fallita");
	//caso 1: cache vuota
	if (*cache == NULL){
		(*cache) = new;
		mutex_unlock(&mtx, "cache_enqueue: unlock fallita");
		return 0;
	}
	mutex_unlock(&mtx, "cache_enqueue: unlock fallita");

	//caso 2: un elemento
	mutex_lock(&((*cache)->mtx), "cache_enqueue: unlock fallita");	
	if((*cache)->next == NULL){
		(*cache)->next = new;
		mutex_unlock(&((*cache)->mtx), "cache_enqueue: unlock fallita");
		return 0;
	}
	
	//caso 3: almeno due elementi (collacazione nodo in testa=coda)
	file* prev = *cache;
	mutex_lock(&((*cache)->next->mtx), "cache_enqueue: unlock fallita");
	file* curr = (*cache)->next;
	file* aux;
	while (curr->next != NULL){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache_enqueue: lock fallita ");
		mutex_unlock(&(aux->mtx), "cache_enqueue: unlock fallita");

	}
	curr->next = new;
	mutex_unlock(&(prev->mtx), "cache_enqueue: unlock fallita");
	mutex_unlock(&(curr->mtx), "cache_enqueue: unlock fallita");
	return 0;
}
file* replace_to_write(file** cache,  const char* f_name, char* f_data, size_t dim_f, int id)
{
	printf("(CACHE) - replace_to_write: 	scrittura %s (%zu byte)... \n", f_name, dim_f); //DEBUG
	printf("(STAMPA CACHE) :		"); //DEBUG
	print_queue(*cache); //DEBUG
	// CREAZIONE DEL NUOVO NODO
      file* new = NULL;
	ec_null_r((new = (file*)malloc(sizeof(file))), "malloc in replace_to_write", NULL);
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		 LOG_ERR(errno, "mutex init in replace_to_write:");
		 free(new);
		 return NULL;	
	}
      //setting
	//apertura del file
	new->id_list = NULL;
	char char_id[101];
	itoa(id, char_id);
	insert_node(&(new->id_list), char_id);
	new->f_open = 1;
	//altri parametri
	new->f_size = dim_f;
	new->f_read = 1; 	//utile?
      new->f_write = 1; //utile?
	new->f_lock = id;
	new->next = NULL;     

      size_t len_f_name = strlen(f_name);
	ec_null_r((new->f_name = (char*)calloc(sizeof(char), len_f_name)), "calloc in replace_to_write", NULL);
	strncpy(new->f_name, f_name, len_f_name);
	(new->f_name)[len_f_name] = '\0';

	file* rep = NULL;
	int found = 0;

	//RICERCA DEL NODO DA RIMPIAZZARE
	//CASO 1 - un elemento in coda
	mutex_lock(&((*cache)->mtx), "lock in replace_to_write");
	if ((*cache)->next == NULL){
		if( (*cache)->f_lock == 0 || (*cache)->f_lock == id ){ //file non lockato
			rep = *cache;
                  *cache = new;
			found = 1;
			printf("(CACHE) - replace_to_write:	rimpiazzo il file: %s\n", (*cache)->f_name); //DEBUG
			mutex_unlock(&((*cache)->mtx), "unlock in replace_to_write");
		}else{	//l'unico elem. non rispetta le cond. di rimpiazzo
			mutex_unlock(&((*cache)->mtx), "unlock in replace_to_write");
			pthread_mutex_destroy(&new->mtx);
			free(new);
			return NULL;
		}
	}else{
		//CASO 2 - due o piu elementi
		file* prev = *cache;
		mutex_lock(&((*cache)->next->mtx), "unlock in replace_to_write");
		file* curr = (*cache)->next;	
		file* aux = prev;

		while (curr->next != NULL){ 
			//printf("nodo corrente: prev->f_name = %s | prev->f_lock = %d\n",prev->f_name, prev->f_lock); //DEBUG
			if (prev->f_lock == 0 || prev->f_lock == id){
				//printf("nodo scelto per rimpiazzo: prev->f_name = %s\n",prev->f_name); //DEBUG
				found = 1;
				break;
			}
			aux = prev;
			prev = curr;
			curr = curr->next;
			mutex_lock(&(curr->mtx), "lock in replace_to_write");
			mutex_unlock(&(aux->mtx), "unlock in replace_to_write");
		}
		//controllo degli ultimi o primi due elementi
		if (prev->f_lock == 0 || prev->f_lock == id ){
			rep = prev;
			if (aux == prev){      //prev primo elemento lista
				*cache = prev->next;
			}else{                  //prev sta tra aux e curr
				mutex_lock(&(aux->mtx), "lock in replace_to_write");
				aux->next = prev->next;
				mutex_unlock(&(aux->mtx), "unlock in replace_to_write");
			}
			found = 1;
            }
		//controllo secondo elem. (no while)/penultimo elem.(si while)
		if (!found && (curr->f_lock == 0 || curr->f_lock == id)){
			rep = curr;
			prev->next = curr->next;
			found = 1;
		}
		printf("(STAMPA CACHE) :		"); //DEBUG
		print_queue(*cache);	//DEBUG

		//printf("FILE DA RIMPIAZZARE = %s\n", rep->f_name);
		mutex_unlock(&(prev->mtx), "unlock in replace_to_write");
		mutex_unlock(&(curr->mtx), "unlock in replace_to_write");
	}
	if (!found){ //in writeFile il nodo va cercato ed eliminato
		if (pthread_mutex_destroy(&new->mtx) != -1){ 
			free(new);
			return NULL;
		}else{
			LOG_ERR(errno, "mutex destroy in replace_to_write");
			return NULL;
		}
	}
	//COLLOCAZIONE NODO IN CODA
	mutex_lock(&((*cache)->mtx) , "lock in replace_to_write");
	file* prev = *cache;
	mutex_lock(&((*cache)->next->mtx), "lock in replace_to_write");
	file* curr = (*cache)->next;	
	file* aux = prev;
	while (curr->next != NULL ){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "lock in replace_to_write");
		mutex_unlock(&(aux->mtx), "unlock in replace_to_write");
	} 
	curr->next = new;
	mutex_unlock(&(prev->mtx), "unlock in replace_to_write");
	mutex_unlock(&(curr->mtx), "unlock in replace_to_write");

	cache_capacity_update(-(rep->f_size), -1); //eliminato un file
	//printf("(CACHE) - replace_to_write:	rimpiazzato il nodo: %s\n", rep->f_name);
	printf("(STAMPA CACHE) :		"); //DEBUG
	print_queue(*cache);  //DEBUG
	
	//AGGIORNAMENTO STATO CACHE
      cache_capacity_update(0, 1); //aggiunto un file vuoto
      return rep;
}

//scrittura in append
int cache_appendToFile(file** cache, const char* f_name, char* f_data, size_t dim_f, int id, file** expelled_file)
{
	//CONTROLLO PRELIMINARE
	if (cache_capacity < dim_f){
		errno = EFBIG; //da controllare al ritorno per il client
		//LOG_ERR(EFBIG, "cache_appendToFile: "); //DEBUG
		return errno;
      }
	printf("(CACHE) - cache_appendToFile:	su file: %s (%zu) | size_file=%zu | id=%d\n", f_name, strlen(f_name), dim_f, id); //DEBUG
	//controllo capacità memoria per rimpiazzo
	if(used_mem+dim_f > cache_capacity){
		printf("cache_appendToFile:	(!) rimpiazzo necessario\n");
		if((*expelled_file = replace_to_append(cache, f_name, f_data, dim_f, id)) == NULL){
			printf("(CACHE) - cache_appendToFile: 		scrittura fallita, rimpiazzo impossibile\n"); //DEBUG
			return -1;
		}else{
			printf("(CACHE) - cache_appendToFile:	scrittura riuscita con rimpiazzo di %s (%zubyte)\n", (*expelled_file)->f_name, (*expelled_file)->f_size); //DEBUG
			return 0;
		}
	}
	
	printf("(CACHE) - cache_appendToFile: 	scrittura in append senza rimpiazzo su %s...(%zu - %zu byte)\n", f_name, strlen(f_name), dim_f); //DEBUG

	size_t found = 0;
	file* node;
	
	//CODA CON UN ELEMENTO
	mutex_lock(&((*cache)->mtx), "lock in cache_appendToFile");
	if ((*cache)->next == NULL && strcmp((*cache)->f_name, f_name) == 0){
		mutex_unlock(&((*cache)->mtx), "lock in cache_appendToFile");
		found = 1;
		node = *cache;
	}else{
		//CODA CON DUE O PIU ELEMENTI
		file* prev = *cache;
		mutex_lock(&((*cache)->next->mtx), "lock in cache_appendToFile");
		file* curr = (*cache)->next;
		file* aux = prev;
	
		while (curr->next != NULL && strcmp(prev->f_name, f_name) != 0){
			aux = prev;
			prev = curr;
			curr = curr->next;
			mutex_lock(&(curr->mtx), "lock in cache_appenToFile");
			mutex_unlock(&(aux->mtx), "unlock in cache_appendToFile");
		}
		
		if (strcmp(prev->f_name, f_name) == 0 && (prev->f_lock == 0 || prev->f_lock == id)){
			found = 1;
			node = prev;
			printf("DEBUG cache_appendToFile: trovato il nodo su cui scrivere -> %s\n", prev->f_name);
		}else{
			if (strcmp(curr->f_name, f_name) == 0 && (curr->f_lock == 0 || curr->f_lock == id)){
				found = 1;
				node = curr;
				printf("DEBUG cache_appendToFile: trovato il nodo su cui scrivere -> %s\n", curr->f_name);
			}
		}
		mutex_unlock(&(prev->mtx), "unlock in cache_appendToFile");
		mutex_unlock(&(curr->mtx), "unlock in cache_appendToFile");
	}
	if (found){
		//printf("DEBUG cache_appendToFile: setting del nuovo nodo\n");
		mutex_lock(&(node->mtx), "cache_appendToFile: lock fallita");
		size_t new_size = dim_f + node->f_size;
		if(node->f_size == 0){ ec_null_r( (node->f_data = (char*)calloc(sizeof(char), new_size)), "cache_appendToFile: calloc fallita", -1); }
		else{ ec_null_r( (node->f_data = (char*)realloc(node->f_data, new_size)), "cache_appendToFile: calloc fallita", -1); }
		int j = 0; int i = node->f_size;
		while (j < dim_f && i < new_size){
			node->f_data[i] = f_data[j];
			j++; i++;
		}
		node->f_size = new_size;
		mutex_unlock(&(node->mtx), "cache_appendToFile: lock fallita");
		
		cache_capacity_update(dim_f, 0);
		printf("(CACHE) - appendToFile: 	scrittura in append su %s eseguita senza rimpiazzo\n", node->f_name);
		printf("(STAMPA CACHE) :		");  //DEBUG
		print_queue(*cache);  //DEBUG
	}

	if (found) return 0;
	else return -1;
}
file* replace_to_append(file** cache, const char* f_name, char* f_data, size_t dim_f, int id)
{
	file* rep = NULL;
	file* node = NULL;

	//CACHE CON UN ELEMENTO -> rimpiazzo impossibile (append)
	mutex_lock(&((*cache)->mtx), "replace_to_append: lock fallita");
	if ((*cache)->next == NULL){
		mutex_unlock(&((*cache)->mtx), "replace_to_append: unlock fallita");
		return NULL;
	}
	//RICERCA DEL NODO DA RIMPIAZZARE

	file* prev = *cache;
	mutex_lock(&((*cache)->next->mtx), "replace_to_append: unlock fallita !!");
	file* curr = (*cache)->next;
	size_t delete = 0;
	file* aux = prev;

	while (curr->next != NULL ){
		if (prev->f_size >= dim_f && (prev->f_lock == 0 || prev->f_lock == id) && strcmp(prev->f_name, f_name) != 0)
			break;
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "replace_to_append: lock fallita");
		mutex_unlock(&(aux->mtx), "replace_to_append: unlock fallita");
	}
	//il nodo da rimpiazzare è prev?
	if (prev->f_size >= dim_f && (prev->f_lock == 0 || prev->f_lock == id) && strcmp(prev->f_name, f_name) != 0){
		
		if (aux == prev){ //se prev è il primo elemento della lista
			//printf("DEBUG: rimpiazzo primo elem: %s\n", prev->f_name);
			*cache = curr;
		}else{ //prev sta tra aux e curr
			mutex_lock(&(aux->mtx), "replace_to_append: lock fallita");
			aux->next = prev->next;
			mutex_unlock(&(aux->mtx), "replace_to_append: unlock fallita");
		}
		rep = prev;
		delete = 1;
	}else{
		//il nodo da rimpiazzare è curr?
		if (curr->f_size >= dim_f && !delete && (curr->f_lock == 0 || curr->f_lock == id) && strcmp(curr->f_name, f_name) != 0){
			//printf("DEBUG: rimpiazzo ultimo: %s\n", curr->f_name);
			rep = curr;
			prev->next = curr->next;
			delete = 1;
		}
	}
	mutex_unlock(&(prev->mtx), "replace_to_append: unlock fallita");
	mutex_unlock(&(curr->mtx), "replace_to_append: unlock fallita");
	if (!delete) return NULL;
	else{ 
		printf("(CACHE) - replace_to_append:	rimpiazzato il nodo: %s\n", rep->f_name);
		cache_capacity_update(-(rep->f_size), -1);
		printf("(STAMPA CACHE) :		");  //DEBUG
		print_queue(*cache);  //DEBUG
	}
      		
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//RICERCA DEL FILE SU SUI SCRVIVERE
	mutex_lock(&((*cache)->mtx), "replace_to_append: lock fallita");
	prev = *cache;
	mutex_lock(&((*cache)->next->mtx), "replace_to_append: unlock fallita");
	curr = (*cache)->next;
	
	while (curr->next != NULL && strcmp(prev->f_name, f_name) != 0){
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "replace_to_append: lock fallita");
		mutex_unlock(&(aux->mtx), "replace_to_append: unlock fallita");
	}
		
	if (strcmp(prev->f_name, f_name) == 0)
		node = prev;
	else{
		if (strcmp(curr->f_name, f_name) == 0)
			node = curr;
	}
	mutex_unlock(&(prev->mtx), "replace_to_append: unlock fallita");
	mutex_unlock(&(curr->mtx), "replace_to_append: unlock fallita");
	
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//SCRITTURA APPEND (con controllo memoria)
	if ((used_mem+dim_f) <= cache_capacity){
		mutex_lock(&(node->mtx), "replace_to_append: lock fallita");
		size_t new_size = dim_f + node->f_size;
		if(node->f_size == 0){ ec_null_r( (node->f_data = (char*)calloc(sizeof(char), new_size) ), "replace_to_append: calloc fallita", NULL); }
		else{ ec_null_r( (node->f_data = (char*)realloc(node->f_data, new_size)), "replace_to_append: calloc fallita", NULL); }
		int j = 0; int i = node->f_size;
		while (j < dim_f && i < new_size){
			node->f_data[i] = f_data[j];
			j++; i++;
		}
		node->f_size = new_size;
		mutex_unlock(&(node->mtx), "replace_to_append: lock fallita");
		
		printf("(CACHE) - replace_to_append:	append su %s eseguita\n", node->f_name);
		cache_capacity_update(dim_f, 0);
		printf("(STAMPA CACHE) :		");  //DEBUG
		print_queue(*cache);  //DEBUG
	}
	//in ogni caso il rimpiazzo è avvenuto
	return rep;
}

//lock e unlock
int cache_lockFile(file* cache, const char* f_name, int id)
{
	file* node;
	if ((node = cache_research(cache, f_name)) == NULL){
		//settare errno per i vari casi !!
		//printf("cache_lockFile:		file da lockare non trovato\n"); // DEBUG
		errno = ENOLCK;
		return -1;
	}
	mutex_lock(&(node->mtx), "cache_lockFile: lock fallita");
	//file non lockato o gia lockato dallo stesso processo
	if(node->f_lock == 0 || node->f_lock == id ){
		node->f_lock = id;
		mutex_unlock(&(node->mtx), "cache_lockFile: unlock fallita"); 
		printf("(CACHE) - lockFile:		file %s lockato da id=%d\n", f_name, id ); //DEBUG
		return 0;
	}
	//file lockato da un altro processo
	if(node->f_lock != 0 && node->f_lock != id){
		mutex_unlock(&(node->mtx), "cache_lockFile: unlock fallita");
		errno = ENOLCK;
		return -2;
	}
	return 0;
}
int cache_unlockFile(file* cache, const char* f_name, int id)
{	
	file* node;
	//printf("(CACHE) - cache_unlockFile: cerco %s / %zu\n", f_name, strlen(f_name)); //DEBUG
	//printf("(CACHE) - cache_unlockFile: lista -> "); //DEBUG
	//print_queue(cache); //DEBUG
	if ((node = cache_research(cache, f_name)) == NULL){
		//LOG_ERR(errno, "cache_unlockFile:")
		printf("cache_unlockFile:	file da unlockare non presente in cache\n"); //DEBUG
		
		return -1;
	}
	mutex_lock(&(node->mtx), "cache_unlockFile: lock fallita");
	if(node->f_lock == id){
		node->f_lock = 0;
		mutex_unlock(&(node->mtx), "cache_unlockFile: unlock fallita");
		printf("(CACHE) - unlockFile:		file %s unlockato da id=%d\n", f_name, id ); //DEBUG
		return 0;
	}else{
		if(node->f_lock > 0) printf("cache_unlockFile:	file lockato da un altro processo\n"); //DEBUG
 		mutex_unlock(&(node->mtx), "cache_unlockFile: unlock fallita");
		return -2;
	}
	return 0;
}

void print_queue(file* cache)
{
	file* temp = cache;
	while(temp != NULL){
		printf("%s (%zu-%zu) ", temp->f_name, strlen(temp->f_name), temp->f_size);
		temp = temp->next;
	}
	printf("\n");
	return;
}
