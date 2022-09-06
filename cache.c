#include "cache.h"

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

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
		//printf("(CACHE) - cache_research:		cache vuota\n"); //DEBUG
		return NULL;
	}
	mutex_unlock(&mtx, "lock in cache_appendToFile");
	
	//CODA CON UN ELEMENTO
	mutex_lock(&(cache->mtx), "lock in cache_appendToFile");
	if (cache->next == NULL){
		if(strcmp(cache->f_name, f_name) == 0){
			mutex_unlock(&(cache->mtx), "lock in cache_appendToFile");
			//size_t len_f_name = strlen(f_name);
			//printf("(CACHE) - cache_research:	%s (%zu) trovato\n", f_name, len_f_name); //DEBUG
			return cache;
		}else{
			mutex_unlock(&(cache->mtx), "lock in cache_appendToFile");
			//printf("(CACHE) - cache_research:	non trovato\n"); //DEBUG
			return NULL;
		}
	}
	//print_queue(cache);
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
		printf("(CACHE) - cache_research:	non trovato\n"); //DEBUG 
		return NULL;
	}else{
		size_t len_f_name = strlen(f_name);
		printf("(CACHE) - cache_research:	%s (%zu) trovato\n", f_name, len_f_name); //DEBUG
		return node;
	}
}
void cache_capacity_update(int dim_file, int new_file_or_not)
{
    mutex_lock(&mtx, "lock in cache_capacity_update");
	used_mem = used_mem + dim_file;
	files_in_cache += new_file_or_not;
	printf("(STATO CACHE) : 		used memory = %zu di %zu - n. files = %zu\n", used_mem, cache_capacity, files_in_cache);
	mutex_unlock(&mtx, "unlock in cache_capacity_update");
}
void cache_dealloc(file** cache)
{
	printf("deallocazione cache in corso...\n");
	
	mutex_lock(&mtx, "cache_dealloc: lock fallita");
	if(*cache == NULL){ 
		*cache = NULL;
		mutex_unlock(&mtx, "cache_dealloc: lock fallita");
		printf("(CACHE) - cache_dealloc:	deallocazione cache avvenuta\n"); //DEBUG
		return;
	}
	if((*cache) == NULL){
		file* temp = *cache;
		print_list(temp->id_list);
		dealloc_list(&(temp->id_list));
		free(temp->f_name);
		free(temp->f_data);
		pthread_mutex_destroy(&(temp->mtx));	
		free(temp);
		*cache = NULL;
		mutex_unlock(&mtx, "cache_dealloc: lock fallita");
		printf("(CACHE) - cache_dealloc:	deallocazione cache avvenuta\n"); //DEBUG
		return;
	}
	file* curr = *cache;
	//print_list((*cache)->id_list);

	while (curr != NULL){
		file* temp = curr;
		curr = curr->next;
		dealloc_list(&(temp->id_list));
		free(temp->f_name);
		free(temp->f_data);
		pthread_mutex_destroy(&(temp->mtx));
		temp->next = NULL;
		free(temp);
	}
	*cache = NULL;
	mutex_unlock(&mtx, "cache_dealloc: unlock fallita");
	printf("(CACHE) - cache_dealloc:	deallocazione cache avvenuta\n"); //DEBUG
	print_queue(*cache);
	return;
}
int cache_removeFile(file** cache, const char* f_name, int id)
{
	//caso lista vuota -> ho scelto di dare esito positivo anche se la condizione non dovrebbe verificarsi
	mutex_lock(&mtx, "cache_removeFile: lock fallita");
	if(*cache == NULL){
		mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
		printf("(CACHE) - cache_removeFile:		cache vuota\n"); //DEBUG
		errno = EINVAL;
		return 0;
	}
	//caso un elemento in lista
	if ((*cache)->next == NULL){
		if (strcmp((*cache)->f_name, f_name) == 0 && ((*cache)->f_lock != 0 || (*cache)->f_lock == id) ){
			size_t size = (*cache)->f_size;
			file* temp = *cache;
			free(temp->f_name);
			free(temp->f_data);
			dealloc_list(&(temp->id_list));
			free(temp);
			*cache = NULL; //(!)
			mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
			cache_capacity_update(-size, -1);
			printf("(CACHE) - cache_removeFile:		avvnuta su f_name: %s / id=%d\n", f_name, id); //DEBUG
			print_queue(*cache); //DEBUG
			return 0;
		}else{
			//impossibile eliminare il file
			mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
			errno = EPERM;
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
		if (strcmp(prev->f_name, f_name) == 0 && prev->f_lock == id){
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
	if (strcmp(prev->f_name, f_name) == 0 && prev->f_lock == id){
		//se si tratta del primo nodo della coda
		if (prev == *cache) *cache = prev->next;
		//altrimenti collego aux a curr (prev è in mezzo)
		else aux->next = prev->next;
		//aggiornamento capacità cache
		cache_capacity_update(-(prev->f_size), -1);
		dealloc_list(&(prev->id_list));
		free(prev->f_name);
		free(prev->f_data);
		mutex_unlock(&(prev->mtx), "cache_removeFile: unlock fallita in cache_enqueue"); //(!)
		pthread_mutex_destroy(&(prev->mtx));
		free(prev);
		mutex_unlock(&(curr->mtx), "cache_removeFile: unlock fallita in cache_enqueue");
		return 0;
	}else{
		//se si tratta dell'ultimo elemento della lista
		if (strcmp(curr->f_name, f_name) == 0  && curr->f_lock == id){
				prev->next = NULL; //curr->next = NULL
				size_t size = curr->f_size;
				dealloc_list(&(curr->id_list));
				free(curr->f_name);
			    free(curr->f_data);
				mutex_unlock(&(curr->mtx), "cache_removeFile: unlock fallita in cache_enqueue");
				pthread_mutex_destroy(&(curr->mtx));
				free(curr);
				cache_capacity_update(-size, -1);
				mutex_unlock(&(prev->mtx), "cache_removeFile: unlock fallita in cache_enqueue"); //(!)
				return 0;
		}
	}
	mutex_unlock(&(prev->mtx), "cache_removeFile: unlock fallita in cache_enqueue");
	mutex_unlock(&(curr->mtx), "cache_removeFile: unlock fallita in cache_enqueue");
	if(found){
		printf("(CACHE) - cache_removeFile:		su f_name: %s / id=%d\n", f_name, id); //DEBUG
		print_queue(*cache); //DEBUG
		return 0;
	}else{
		errno = EPERM;
		return -1;
	}
}

//funzioni per scrittura con -w/-W
int cache_insert(file** cache, const char* f_name, char* f_data, const size_t dim_f, int id, file** expelled_file)
{	
	size_t L = strlen(f_name);
	printf("(CACHE) - cache_insert: 	scrittura %s (%zu - %zu byte)... \n", f_name, L, dim_f);	 //DEBUG

    //INSERIMENTO CON RIMPIAZZO
    if (files_in_cache+1 > max_storage_file){
		//printf("CACHE - cache_insert:		(!) rimpiazzo necessario (per num. max file)\n"); //DEBUG
		if ((*expelled_file = replace_to_write(cache, f_name, f_data, dim_f, id)) == NULL){
			printf("(CACHE) - cache_insert: 		scrittura di %s (%zubyte) fallita, rimpiazzo impossibile\n", f_name, dim_f); //DEBUG
			//errno = ?
			return -1;
		}else{
			printf("(CACHE) - cache_insert: 	scrittura %s eseguita con rimpiazzo (espulso %s di %zu byte\n",f_name, (*expelled_file)->f_name, (*expelled_file)->f_size); //DEBUG
			print_queue(*cache);  //DEBUG
			return 0;
		}
	}
	//INSERIMENTO SENZA RIMPIAZZO
	if (cache_enqueue(cache, f_name, f_data, dim_f, id) == -1){
		return -1;
	}
	printf("(CACHE) - cache_insert:		scrittura di %s (%zu byte) eseguita senza rimpiazzo\n", f_name, dim_f); //DEBUG
    cache_capacity_update(dim_f, 1);
	print_queue(*cache);  //DEBUG
	return 0;
}
int cache_enqueue(file** cache,  const char* f_name, char* f_data, const size_t dim_f, int id)
{
	//printf("(CACHE) cache_enqueue:	su %s (%zu)\n", f_name, strlen(f_name)); //DEBUG
	//size_t M = strlen(f_name); //DEBUG
	//printf("(CACHE) - cahe_enqueue: 	scrittura %s (%zu - %zu byte)... \n", f_name, M, dim_f);	 //DEBUG
	//lista invertita cosi da facilitare l'operazione di estrazione dei file in caso di rimpiazzo (FIFO)
	//l'inserimento diventa in coda e l'estrazione in testa in O(1) nel caso ottimo
	
	//CREAZIONE + SETTING
    file* new = NULL;
	ec_null_r((new = (file*)malloc(sizeof(file))), "cache_enqueue: malloc fallita", -1);
	
	//init mutex
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		//LOG_ERR(errno, "cache_enqueue: pthread_mutex_init fallita");
		free(new);
		return -1;
	}
	//lock del file
	new->next = NULL;
	new->f_data = NULL;
	new->id_list = NULL;
	new->f_lock = id;
	new->f_size = dim_f;
	new->f_open = 1;
	//open
	char char_id[101];
	itoa(id, char_id);
	insert_node(&(new->id_list), char_id);
	
	//len pathname + pathname
	size_t len_f_name = strlen(f_name);
	ec_null_r((new->f_name = (char*)calloc(sizeof(char), len_f_name)), "cache_enqueue: calloc fallita", -1);
	memset((void*)new->f_name, '\0', (sizeof(char)*len_f_name));
	strncpy(new->f_name, f_name, len_f_name);


	//COLLOCAZIONE NODO
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
	size_t len_p = strlen(new->f_name);
	printf("(CACHE) cache_enqueue:		new->f_name: %s len_path= %zu\n", new->f_name, len_p); 
	return 0;
}
file* replace_to_write(file** cache,  const char* f_name, char* f_data, const size_t dim_f, int id)
{
	//printf("(CACHE) - replace_to_write: 	scrittura %s (%zu byte)... \n", f_name, dim_f); //DEBUG
	//size_t M = strlen(f_name);
	//printf("(CACHE) - replace_to_write: 	scrittura %s (%zu - %zu byte)... \n", f_name, M, dim_f);	 //DEBUG
	// CREAZIONE DEL NUOVO NODO
    file* new = NULL;
	ec_null_r((new = (file*)malloc(sizeof(file))), "malloc in replace_to_write", NULL);
	//init mutex
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "mutex init in replace_to_write:");
		free(new);
	 	return NULL;	
	}
    //setting
	new->next = NULL;     
	new->id_list = NULL;
	new->f_data = NULL;
	new->f_size = dim_f;
	new->f_lock = id;
	new->f_open = 1;
	//apertura del file
	char char_id[101];
	itoa(id, char_id);
	insert_node(&(new->id_list), char_id);

    size_t len_f_name = strlen(f_name);
	ec_null_r((new->f_name = (char*)calloc(sizeof(char), len_f_name)), "calloc in replace_to_write", NULL);
	memset((void*)new->f_name, '\0', (sizeof(char)*len_f_name));
	strncpy(new->f_name, f_name, len_f_name);


	file* rep = NULL;
	int found = 0;

	//RICERCA DEL NODO DA RIMPIAZZARE
	
	//CASO 1 - un elemento in coda
	mutex_lock(&((*cache)->mtx), "lock in replace_to_write");
	if ((*cache)->next == NULL){
		if( (*cache)->f_lock == 0 || (*cache)->f_lock == id ){
			rep = *cache;
            *cache = new;
			found = 1;
			//printf("(CACHE) - replace_to_write:	rimpiazzo il file: %s\n", (*cache)->f_name); //DEBUG
			mutex_unlock(&((*cache)->mtx), "unlock in replace_to_write");
		}else{	//rimpiazzo impossibile
			pthread_mutex_destroy(&new->mtx);
			dealloc_list(&(new->id_list));
			free(new->f_name);
			free(new);
			mutex_unlock(&((*cache)->mtx), "unlock in replace_to_write");
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
		if (!found && (prev->f_lock == 0 || prev->f_lock == id) ){
			if (aux == prev){      //prev primo elemento lista
				*cache = prev->next;
			}else{                  //prev sta tra aux e curr
				mutex_lock(&(aux->mtx), "lock in replace_to_write");
				aux->next = prev->next;
				mutex_unlock(&(aux->mtx), "unlock in replace_to_write");
			}
			rep = prev;
			found = 1;
            }
		//controllo secondo elem. (no while)/penultimo elem.(si while)
		if (!found && (curr->f_lock == 0 || curr->f_lock == id)){
			rep = curr;
			prev->next = curr->next;
			found = 1;
		}
		//print_queue(*cache);	//DEBUG

		//printf("FILE DA RIMPIAZZARE = %s\n", rep->f_name);
		mutex_unlock(&(prev->mtx), "unlock in replace_to_write");
		mutex_unlock(&(curr->mtx), "unlock in replace_to_write");
	}
	if (!found){
		if (pthread_mutex_destroy(&new->mtx) == -1)
			LOG_ERR(errno, "mutex destroy in replace_to_write");
		dealloc_list(&(new->id_list));
		free(new->f_name);
		dealloc_list(&(new->id_list));
		free(new);
		return NULL;
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

	//AGGIORNAMENTO STATO CACHE
	cache_capacity_update(-(rep->f_size), -1); //eliminato un file
    cache_capacity_update(0, 1); //aggiunto un file vuoto
    return rep;
}

//scrittura in append
int cache_appendToFile(file** cache, const char* f_name, char* f_data, const size_t dim_f, int id, file** expelled_file)
{
	//CONTROLLO PRELIMINARE
	if (cache_capacity < dim_f){
		errno = EFBIG; //da controllare al ritorno per il clien
		return -1;
    }
	printf("(CACHE) - cache_appendToFile:	su file: %s (%zu) | size_file=%zu | id=%d\n", f_name, strlen(f_name), dim_f, id); //DEBUG
	//controllo capacità memoria per rimpiazzo
	if(used_mem+dim_f > cache_capacity){
		printf("cache_appendToFile:	(!) rimpiazzo necessario\n");
		if((*expelled_file = replace_to_append(cache, f_name, f_data, dim_f, id)) == NULL){
			printf("(CACHE) - cache_appendToFile: 		scrittura %s fallita, rimpiazzo impossibile\n", f_name); //DEBUG
			errno = EPERM;
			return -1;
		}else{
			printf("(CACHE) - cache_appendToFile:	scrittura %s avvenuta con rimpiazzo (espulso %s di %zu byte)\n", f_name, (*expelled_file)->f_name, (*expelled_file)->f_size); //DEBUG
			return 0;
		}
	}
	
	size_t found = 0;
	file* node;
	
	//CODA CON UN ELEMENTO
	mutex_lock(&((*cache)->mtx), "lock in cache_appendToFile");
	if ((*cache)->next == NULL && strcmp((*cache)->f_name, f_name) == 0){
		found = 1;
		node = *cache;
		mutex_unlock(&((*cache)->mtx), "lock in cache_appendToFile");
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
			//printf("DEBUG cache_appendToFile: trovato il nodo su cui scrivere -> %s\n", prev->f_name);
		}else{
			if (strcmp(curr->f_name, f_name) == 0 && (curr->f_lock == 0 || curr->f_lock == id)){
				found = 1;
				node = curr;
				//printf("DEBUG cache_appendToFile: trovato il nodo su cui scrivere -> %s\n", curr->f_name);
			}
		}
		mutex_unlock(&(prev->mtx), "unlock in cache_appendToFile");
		mutex_unlock(&(curr->mtx), "unlock in cache_appendToFile");
	}
	if (found){
		//printf("DEBUG cache_appendToFile: setting del nuovo nodo\n");
		mutex_lock(&(node->mtx), "cache_appendToFile: lock fallita");
		size_t new_size = dim_f + node->f_size;
		if (node->f_size == 0){ 
			ec_null_r( (node->f_data = (char*)calloc(sizeof(char), new_size)), "cache_appendToFile: calloc fallita", -1); 
		}else{ 
			ec_null_r( (node->f_data = (char*)realloc(node->f_data, new_size)), "cache_appendToFile: calloc fallita", -1); 
		}
		int j = 0; int i = node->f_size;
		while (j < dim_f && i < new_size){
			node->f_data[i] = f_data[j];
			j++; i++;
		}
		node->f_size = new_size;
		mutex_unlock(&(node->mtx), "cache_appendToFile: lock fallita");
		
		cache_capacity_update(dim_f, 0);
		printf("(CACHE) - cache_appendToFile:	scrittura %s avvenuta\n", f_name); //DEBUG
		print_queue(*cache);  //DEBUG
		return 0;
	}
	errno = EPERM;
	return -1;
}
file* replace_to_append(file** cache, const char* f_name, char* f_data, size_t dim_f, int id)
{
	file* rep = NULL;
	file* node = NULL;

	//RICERCA DEL NODO DA RIMPIAZZARE
	//caso un elem. in lista
	mutex_lock(&((*cache)->mtx), "replace_to_append: lock fallita");
	if ((*cache)->next == NULL){
		mutex_unlock(&((*cache)->mtx), "replace_to_append: unlock fallita");
		return NULL;
	}
	file* prev = *cache;
	mutex_lock(&((*cache)->next->mtx), "replace_to_append: unlock fallita !!");
	file* curr = (*cache)->next;
	size_t delete = 0;
	file* aux = prev;
	//caso due o piu elementi
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
	if (!delete) 
		return NULL;
	else{ 
		printf("(CACHE) - replace_to_append:	rimpiazzato il nodo: %s\n", rep->f_name); //DEBUG
		cache_capacity_update(-(rep->f_size), -1);
		print_queue(*cache);  //DEBUG
	}
      		
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//RICERCA DEL FILE SU SUI SCRVIVERE
	//caso un elem. in lista
	mutex_lock(&((*cache)->mtx), "replace_to_append: lock fallita");
	if ((*cache)->next == NULL){
		node = *cache;
		mutex_unlock(&((*cache)->mtx), "replace_to_append: unlock fallita");
	}else{
		//caso lista con due o piu nodi
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
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//SCRITTURA APPEND (con controllo memoria)
	if ((used_mem+dim_f) <= cache_capacity){
		mutex_lock(&(node->mtx), "replace_to_append: lock fallita");
		size_t new_size = dim_f + node->f_size;
		if(node->f_size == 0){ 
			ec_null_r( (node->f_data = (char*)calloc(sizeof(char), new_size) ), "replace_to_append: calloc fallita", NULL); }
		else{ 
			ec_null_r( (node->f_data = (char*)realloc(node->f_data, new_size)), "replace_to_append: calloc fallita", NULL); }
		int j = 0; int i = node->f_size;
		while (j < dim_f && i < new_size){
			node->f_data[i] = f_data[j];
			j++; i++;
		}
		node->f_size = new_size;
		mutex_unlock(&(node->mtx), "replace_to_append: lock fallita");
		
		printf("(CACHE) - replace_to_append:	append su %s eseguita\n", node->f_name);
		cache_capacity_update(dim_f, 0);
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
		errno = EINVAL;
		return -1;
	}
	mutex_lock(&(node->mtx), "cache_lockFile: lock fallita");
	//file lockato da un altro processo
	if(node->f_lock != 0 && node->f_lock != id){
		errno = EPERM;
		mutex_unlock(&(node->mtx), "cache_lockFile: unlock fallita");
		return -1;
	}
	
	//file non lockato o gia lockato dallo stesso processo
	if(node->f_lock == 0 || node->f_lock == id ){
		node->f_lock = id;
		mutex_unlock(&(node->mtx), "cache_lockFile: unlock fallita"); 
		printf("(CACHE) - lockFile:		file %s lockato da id=%d\n", f_name, id ); //DEBUG
		return 0;
	}

	return 0;
}
int cache_unlockFile(file* cache, const char* f_name, int id)
{	
	file* node;
	if ((node = cache_research(cache, f_name)) == NULL){
		errno = EINVAL;
		printf("cache_unlockFile:	file da unlockare non presente in cache\n"); //DEBUG
		return -1;
	}
	mutex_lock(&(node->mtx), "cache_unlockFile: lock fallita");
	if(node->f_lock == id || node->f_lock == 0){
		node->f_lock = 0;
		mutex_unlock(&(node->mtx), "cache_unlockFile: unlock fallita");
		printf("(CACHE) - unlockFile:		file %s unlockato da id=%d\n", f_name, id ); //DEBUG
		return 0;
	}else{
		if(node->f_lock > 0) printf("cache_unlockFile:	file lockato da un altro processo\n"); //DEBUG
			errno = EPERM;
 			mutex_unlock(&(node->mtx), "cache_unlockFile: unlock fallita");
			return -1;
	}
	return 0;
}

void print_queue(file* cache)
{
	if(cache == NULL ) return;
	file* temp = cache;
	printf("(STAMPA CACHE):			");
	while(temp != NULL){
		//size_t L = strlen(temp->f_name);
		//printf("%s (%zuch/%zub) - ", temp->f_name, L, temp->f_size);
		printf("%s ", temp->f_name);
		printf("(");
		print_list(temp->id_list);
		printf(")\n");
		temp = temp->next;
	}
	printf("\n");
	return;
}

