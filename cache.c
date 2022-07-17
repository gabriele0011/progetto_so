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
file* cache_research(file* cache, char* f_name)
{
	printf("(CACHE) - cache_research: cerco %s\n", f_name); //DEBUG
	//printf("(CACHE) - cache_research: lista -> "); //DEBUG
	//print_queue(cache); //DEBUG
	file* node = NULL;
	//CODA VUOTA
	mutex_lock(&mtx, "cache: lock fallita in cache_duplicate_control");
	if (cache == NULL){
		mutex_unlock(&mtx, "cache: lock fallita in cache_duplicate_control");
		printf("(CACHE) - cache_research: cache vuota\n"); //DEBUG
		return NULL;
	}
	mutex_unlock(&mtx, "cache: lock fallita in cache_duplicate_control");
	
	//CODA CON UN ELEMENTO
	mutex_lock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
	if (cache->next == NULL){
		if(strcmp(cache->f_name, f_name) == 0){
			mutex_unlock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
			printf("(CACHE) - cache_research: trovato\n"); //DEBUG
			return cache;
		}else{
			mutex_unlock(&(cache->mtx), "cache: lock fallita in cache_duplicate_control");
			printf("(CACHE) - cache_research: non trovato\n"); //DEBUG
			return NULL;
		}
	}
	
	//CODA CON DUE O PIU ELEMENTI
	file* prev = cache;
	mutex_lock(&(cache->next->mtx), "cache: unlock fallita in cache_duplicate_control");
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
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_duplicate_control");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_duplicate_control");
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
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	//printf("CACHE - cache_research: fine ricerca %s\n", f_name); //DEBUG
	if (!found){ 
		printf("(CACHE) - cache_research: non trovato\n"); //DEBUG 
		return NULL;
	}else{
		printf("(CACHE) - cache_research: non trovato\n"); //DEBUG
		return node;
	}
}
void cache_capacity_update(int dim_file, int new_file_or_not)
{
      mutex_lock(&mtx, "cache: lock fallita in cache_capacity_update");
	used_mem = used_mem + dim_file;
	files_in_cache += new_file_or_not;
	printf("(CACHE) - cache used memory = %lu di %zu - files in cache = %zu\n", used_mem, cache_capacity, files_in_cache);
	mutex_unlock(&mtx, "cache: unlock fallita in cache_capacity_update");
}
file* cache_dealloc(file* cache)
{
	mutex_lock(&mtx, "cache_dealloc: lock fallita");
	if(cache == NULL){ 
		mutex_unlock(&mtx, "cache_dealloc: lock fallita");
		return  NULL;
	}
	if(cache->next == NULL){
		free(cache);
		mutex_unlock(&mtx, "cache_dealloc: lock fallita");
		return NULL;
	}

	file* curr = cache;
	file* prev;
	while (curr->next != NULL){
		prev = curr;
		curr= curr->next;
		free(prev);
	}
	mutex_unlock(&mtx, "cache_dealloc: unlock fallita");
	return NULL;
}
int cache_removeFile(file** cache, char* f_name, int id)
{
	printf("(CACHE) - cache_removeFile su f_name: %s / id=%d\n", f_name, id);
	//caso lista vuota -> ho scelto di dare esito positivo anche se la condizione non dovrebbe verificarsi
	mutex_lock(&mtx, "cache_removeFile: lock fallita");
	if(*cache == NULL){
		mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
		printf("(CACHE) - cache_removeFile: cache vuota\n");
		return 0;
	}
	//caso un elemento in lista
	if ((*cache)->next == NULL){
		if (strcmp((*cache)->f_name, f_name) == 0 && ((*cache)->f_lock == 0 || (*cache)->f_lock == id) ){
			int size = (*cache)->f_size;
			file* temp = *cache;
			*cache = NULL;
			free(temp);
			mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
			cache_capacity_update(-size, -1);
			printf("(CACHE) - cache_removeFile: unico elemento eliminato\n");
			return 0;
		}else{
			//impossibile eliminare il file
			mutex_unlock(&mtx, "cache_removeFile: unlock fallita");
			return -1;
		}
	}
	mutex_unlock(&mtx, "cache_removeFile: unlock fallita");

	
	//caso lista con due o piu elementi
	mutex_lock(&((*cache)->mtx), "cache: lock fallita in cache_enqueue");
	file* prev = (*cache);
	mutex_unlock(&((*cache)->next->mtx), "cache: unlock fallita in cache_enqueue");
	file* curr = (*cache)->next;

	file* aux;
	int found = 0;
	while (curr->next != NULL){
		if (strcmp(prev->f_name, f_name) == 0 && (prev->f_lock == 0 || prev->f_lock == id)){
			found = 1;
			break;
		}
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_enqueue");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_enqueue");

	}
	//se il nodo da rimuovere è prev (puo essere il primo e in mezzo alla lista)
	if (strcmp(prev->f_name, f_name) == 0 && (prev->f_lock == 0 || prev->f_lock == id) ){
		//se si tratta del primo nodo della coda
		if (prev == *cache) *cache = prev->next;
		//altrimenti collego aux a curr (prev è in mezzo)
		else aux->next = prev->next;
		//aggiornamento capacità cache
		cache_capacity_update(-(prev->f_size), -1);
		free(prev);
		found = 1;
	}else{
		//se si tratta dell'ultimo elemento della lista
		if (strcmp(curr->f_name, f_name) == 0 && (curr->f_lock == 0 || curr->f_lock == id)){
				prev->next = curr->next; //curr->next = NULL
				free(curr);
				//aggiornamento capacità cache
				cache_capacity_update(-(curr->f_size), -1);
				found = 1;
		}
	}
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_enqueue");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_enqueue");
	if(found) printf("(CACHE) - cache_removeFile: eliminazione riuscita\n");
	return found;
}

//funzioni per scrittura con -w/-W
int cache_insert(file** cache, char* f_name, char* f_data, size_t dim_f, int id, file** expelled_file)
{	
      printf("(CACHE) - cache_insert: 	scrittura %s (%zu byte)... \n", f_name, dim_f);	 //DEBUG
	/*
      //CONTROLLO PRELIMINARE -> INUTILE i file qui arrivano sempre con dimensione 0
	if (cache_capacity < dim_f){
		LOG_ERR(-1, "cache_insert: dimensione file maggiore della capacità della cache");
		return -1;
      }
	*/
      //INSERIMENTO CON RIMPIAZZO -> gestire puntatore del file rimpiazzato -> è necessario controllare il numero di file poiche dim_f = 0
      if (used_mem+dim_f > cache_capacity || files_in_cache+1 > max_storage_file){
		printf("CACHE - cache_insert:		rimpiazzo necessario\n");
		if ((*expelled_file = replace_to_write(cache, f_name, f_data, dim_f, id)) == NULL){
			printf("cache_insert: 		scrittura di %s fallita - rimpiazzo fallito\n", f_name);
			return -1;
            }
            printf("CACHE - cache_insert: 	scrittura %s eseguita con rimpiazzo di %s\n", f_name, (*expelled_file)->f_name); //DEBUG
	}

	//INSERIMENTO SENZA RIMPIAZZO
	printf("CACHE - cahce_insert:		enqueue (rimpiazzo non necessario)\n"); //DEBUG
	if (cache_enqueue(cache, f_name, f_data, dim_f, id) == -1){
		LOG_ERR(-1, "cache_writeFile: enqueue fallita");
		return -1;
	}
	printf("(CACHE) - cache_insert:	scrittura %s eseguita\n", f_name); //DEBUG
      cache_capacity_update(dim_f, 1);
      return 0;
}
int cache_enqueue(file** cache, char* f_name, char* f_data, size_t dim_f, int id)
{
	//printf("cache_enqueue: sono qui\n"); //DEBUG
	
	//lista invertita cosi da facilitare l'operazione di estrazione dei file in caso di rimpiazzo (FIFO)
	//l'inserimento diventa in coda e l'estrazione in testa in O(1) nel caso ottimo
	//CREAZIONE + SETTING
      file* new = NULL;
	ec_null((new = malloc(sizeof(file))), "cache: malloc cache_writeFile fallita");
	new->next = NULL;
	new->f_size = dim_f;
	//len pathname + pathname
	size_t len_f_name = strlen(f_name);
	ec_null((new->f_name = calloc(sizeof(char), len_f_name+1)), "cache: errore calloc");
	new->f_name[len_f_name+1] = '\0';
	strncpy(new->f_name, f_name, len_f_name);
	//permessi di scrittura/lettura
	new->f_read = 1;
	new->f_write = 1;
      //lock file
      new->f_lock = id;

	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in cache_create_file");
		free(new);
		exit(EXIT_FAILURE);	
	}

	//COLLOCAZIONE NODO (in coda)
	mutex_lock(&mtx, "cache: lock fallita in cache_enqueue");
	//caso 1: cache vuota
	if (*cache == NULL){
		(*cache) = new;
		mutex_unlock(&mtx, "cache: unlock fallita in cache_enqueue");
		return 0;
	}
	mutex_unlock(&mtx, "cache: unlock fallita in cache_enqueue");

	//caso 2: un elemento
	mutex_lock(&((*cache)->mtx), "cache: unlock fallita in cache_enqueue");	
	if((*cache)->next == NULL){
		(*cache)->next = new;
		mutex_unlock(&((*cache)->mtx), "cache: unlock fallita in cache_enqueue");
		return 0;
	}
	
	//caso 3: almeno due elementi (collacazione nodo in testa=coda)
	file* prev = *cache;
	mutex_lock(&((*cache)->next->mtx), "cache: unlock fallita in cache_enqueue");
	file* curr = (*cache)->next;
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
	return 0;
}
file* replace_to_write(file** cache, char* f_name, char* f_data, size_t dim_f, int id)
{
	//cerca nodo che rispetti condizione di rimpiazzo (size_old >= size_new && file non lockato o lockato dallo stesso processo)
	//caso peggiore O(n), caso migliore O(1) (testa e coda sono invertite)
	//return ptr al nodo di rimpiazzo
	
	// CREAZIONE DEL NUOVO NODO
      file* new;
	ec_null((new = (file*)malloc(sizeof(file))), "replacement_algorithm_write: malloc fallita");
      file* rep = NULL;
	int found = 0;

	//RICERCA DEL NODO DA RIMPIAZZARE -> operazione valida sempre ma si suppone che new sia gia in coda?
	//caso cache con un elemento
	mutex_lock(&((*cache)->mtx) , "replacement_algorithm_write: lock fallita");
	if ((*cache)->next == NULL){
		if((*cache)->f_size >= dim_f && ((*cache)->f_lock == 0 || (*cache)->f_lock == id) ){ //l'unico elem. rispetta le cond. di rimpiazzo
			rep = *cache;
                  *cache = new;
			found = 1;
			printf("DEBUG: rimpiazzo file: %s\n", (*cache)->f_name);
			mutex_unlock(&((*cache)->mtx), "replacement_algorithm_write: unlock fallita");
		}else{	//l'unico elem. non rispetta le cond. di rimpiazzo
			mutex_unlock(&((*cache)->mtx), "replacement_algorithm_write: unlock fallita");
			free(new);
			return NULL;
		}
	}else{
		//caso coda con due o piu elementi - cerco nodo rimpiazzabile
		file* prev = *cache;
		mutex_lock(&((*cache)->next->mtx), "replacement_algorithm_write: unlock fallita !!");
		file* curr = (*cache)->next;	
		file* aux = prev;

		while (curr->next != NULL){ //arrivo alla fine cosi da collocare anche new in testa(=coda)
			//controllo nodo rimpiazzabile
			if (!found && prev->f_size >= dim_f && (prev->f_lock == 0 || prev->f_lock == id)){
				//trovato il nodo di rimpiazzo
				rep = prev;
				//lo stacco
				if (aux == *cache) *cache = curr; //se prev è il primo elem. della coda
				else aux->next = curr;		//se prev sta tra aux e curr
				//setto flag per scrittura
				found = 1;
			}
			aux = prev;
			prev = curr;
			curr = curr->next;
			mutex_lock(&(curr->mtx), "cache: lock fallita in replacement_algorithm");
			mutex_unlock(&(aux->mtx), "cache: unlock fallita in replacement_algorithm");
		}
		//colloco new alla fine della coda
            if(curr->next == NULL && found) curr->next = new;
		
		//controllo primo(no while)/terzultimo(while) elem
		if (!found && prev->f_size >= dim_f && (prev->f_lock == 0 || prev->f_lock == id) ){
			if (aux == prev){       //prev primo elemento lista
				*cache = curr;
			}else{                  //prev sta tra aux e curr
				mutex_lock(&(aux->mtx), "cache: unlock fallita in replacement_algorithm");
				aux->next = curr;
				mutex_unlock(&(aux->mtx), "cache: unlock fallita in replacement_algorithm");
			}
			rep = prev;
			found = 1;
            }
		//controllo secondo elem. (no while)/penultimo elem.(si while)
		if (!found && curr->f_size >= dim_f && (curr->f_lock == 0 || curr->f_lock == id)){
			//printf("DEBUG: rimpiazzo ultimo: %s\n", curr->f_name);
			rep = curr;
			prev->next = curr->next;
			found = 1;
		}

		mutex_unlock(&(prev->mtx), "cache: unlock fallita in replacement_algorithm");
		mutex_unlock(&(curr->mtx), "cache: unlock fallita in replacement_algorithm");
	}

	if (!found){ //in writeFile il nodo va cercato ed eliminato
		free(new);
		return NULL;
	}		
	//SETTING DEL NUOVO NODO
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		 LOG_ERR(errno, "replacement_algorithm_write: pthread_mutex_init fallita");
		 free(new);
		 exit(EXIT_FAILURE);	
	 }
	mutex_lock(&(new->mtx), "replacement_algorithm_write: unlock fallita (scrittura file)");
	new->f_read = 1;
      new->f_write = 1;
	new->f_lock = id;	      
      size_t len_f_name = strlen(f_name);
	ec_null((new->f_name = calloc(sizeof(char), len_f_name)), "cache: errore calloc");
	strncpy(new->f_name, f_name, len_f_name);
      mutex_unlock(&(new->mtx), "replacement_algorithm_write: unlock fallita (scrittura file)");

	//AGGIORNAMENTO STATO CACHE
      cache_capacity_update(0, 1); //aggiunto un file vuoto
	cache_capacity_update(-(rep->f_size), -1); //eliminato un file
      return rep;
}

//scrittura in append
int cache_appendToFile(file** cache, char* f_name, char* f_data, size_t dim_f, int id, file** expelled_file)
{
	//CONTROLLO PRELIMINARE
	if (cache_capacity < dim_f){
		LOG_ERR(EFBIG, "cache_appendToFile: ");
		return -1;
      }

	printf("(CACHE) - cache_appendToFile:	su file_name:%s | size_file=%d | id=%d\n", f_name, dim_f, id);
	//controllo capacità memoria per rimpiazzo
	if(used_mem+dim_f > cache_capacity){
		if((*expelled_file = replace_to_append(cache, f_name, f_data, dim_f, id)) == NULL){
			printf("cache_appendToFile: 		scrittura in append su %s (%zubyte) fallita - rimpiazzo impossibile\n", f_name, dim_f);
			return -1;
		}else{
			printf("cache_appendToFile: 		append riuscita - file rimpiazzato: %s - scrittura avvenuta\n", (*expelled_file)->f_name);
			return 0;
		}
	}
	printf("(CACHE) - cache_appendToFile: 		scrittura in append senza rimpiazzo su %s...(%zu byte)\n", f_name, dim_f);
	
	size_t found = 0;
	file* node;
	
	//CODA CON UN ELEMENTO
	mutex_lock(&((*cache)->mtx), "cache: lock fallita in cache_duplicate_control");
	if ((*cache)->next == NULL && strcmp((*cache)->f_name, f_name) == 0){
		mutex_unlock(&((*cache)->mtx), "cache: lock fallita in cache_duplicate_control");
		found = 1;
		node = *cache;
	}else{
		//CODA CON DUE O PIU ELEMENTI
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
	}
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
		cache_capacity_update(dim_f, 0);
		printf("(CACHE) - appendToFile: 		scrittura in append su %s eseguita senza rimpiazzo\n", node->f_name);
	}

	if (found) return 0;
	else return -1;
}

file* replace_to_append(file** cache, char* f_name, char* f_data, size_t dim_f, int id)
{
	file* rep = NULL;
	file* node = NULL;

	//CACHE CON UN ELEMENTO -> RIMPIAZZO IMPOSSIBILE
	mutex_lock(&((*cache)->mtx), "replacement_algorithm_append: lock fallita");
	if ((*cache)->next == NULL){
		mutex_unlock(&((*cache)->mtx), "replacement_algorithm_append: unlock fallita");
		return NULL;
	}else{
		//CACHE CON DUE O PIU ELEMENTI		
		
		//CICLO 1: eliminazione del file da rimpiazzare
		file* prev = *cache;
		mutex_lock(&((*cache)->next->mtx), "replacement_algorithm_write: unlock fallita !!");
		file* curr = (*cache)->next;
	
		size_t delete = 0;
		file* aux = prev;

		while (curr->next != NULL ){
			if (prev->f_size >= dim_f && (prev->f_lock == 0 || prev->f_lock == id) && strcmp(prev->f_name, f_name) != 0)
				break;
			aux = prev;
			prev = curr;
			curr = curr->next;
			mutex_lock(&(curr->mtx), "replacement_algorithm_append: lock fallita");
			mutex_unlock(&(aux->mtx), "replacement_algorithm_append: unlock fallita");
		}
		//il nodo da rimpiazzare è prev?
		if (prev->f_size >= dim_f && (prev->f_lock == 0 || prev->f_lock == id) && strcmp(prev->f_name, f_name) != 0){
			//se prev è il primo elemento della lista
			if (aux == prev){
				//printf("DEBUG: fino a qui tutto bene\n");
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
				prev->next = curr->next;
				delete = 1;
			}
		}
		mutex_unlock(&(prev->mtx), "replacement_algorithm_append: unlock fallita");
		mutex_unlock(&(curr->mtx), "replacement_algorithm_append: unlock fallita");
		if (!delete){
			return NULL;
		}else{
			cache_capacity_update(-(rep->f_size), -1);
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
		
		if (strcmp(prev->f_name, f_name) == 0)
			node = prev;
		else{
			if (strcmp(curr->f_name, f_name) == 0)
				node = curr;
		}
		mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_duplicate_control");
		mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_duplicate_control");
	}
	
	/*************************************************************************************************/
	//SCRITTURA APPEND (con controllo memoria)
	if ((used_mem+dim_f) <= cache_capacity){
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
		cache_capacity_update(dim_f, 0);
		printf("DEBUG: append su %s eseguita\n", node->f_name);
		//printf("DEBUG: eliminato il nodo: %s\n", rep->f_name);

	}else{
		return NULL;
	}
	return rep;
}

//lock e unlock
int cache_lockFile(file* cache, char* f_name, int id)
{
	file* node;
	if ((node = cache_research(cache, f_name)) == NULL){
		printf("cache_lockFile:		file da lockare non trovato\n"); // DEBUG
		return -1;
	}
	mutex_lock(&(node->mtx), "cache_lockFile: lock fallita");
	if(node->f_lock == 0)
		node->f_lock = id;
	mutex_unlock(&(node->mtx), "cache_lockFile: unlock fallita"); 
	printf("(CACHE) - lockFile:		file %s lockato da id=%d\n", f_name, id ); //DEBUG
	return 0;
}
int cache_unlockFile(file* cache, char* f_name, int id)
{	
	file* node;
	//printf("(CACHE) - cache_unlockFile: cerco %s / %zu\n", f_name, strlen(f_name)); //DEBUG
	//printf("(CACHE) - cache_unlockFile: lista -> "); //DEBUG
	//print_queue(cache); //DEBUG
	if ((node = cache_research(cache, f_name)) == NULL){
		printf("cache_unlockFile:	file da unlockare non trovato\n");
		return -1;
	}
	mutex_lock(&(node->mtx), "cache_unlockFile: lock fallita");
	if(node->f_lock == id)
		node->f_lock = 0;
	mutex_unlock(&(node->mtx), "cache_unlockFile: unlock fallita");
	printf("(CACHE) - unlockFile:		file %s unlockato da id=%d\n", f_name, id ); //DEBUG
	return 0;
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
