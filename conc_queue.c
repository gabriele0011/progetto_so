#include "conc_queue.h"


pthread_mutex_t mtx1 = PTHREAD_MUTEX_INITIALIZER;

int enqueue(t_queue** queue, int data)
{
	//creazione nuovo nodo + setting
	t_queue* new;
	ec_null_r((new = malloc(sizeof(t_queue))), "cache: malloc cache_writeFile fallita", -1);
	new->next = NULL;
	new->data = data;
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in cache_create_file");
		free(new);
		exit(EXIT_FAILURE);	
	}

	//collocazione nodo
	mutex_lock(&mtx1, "cache: lock fallita in cache_enqueue");
	//caso 1: cache vuota
	if (*queue == NULL){
		*queue = new;
		mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
		return 0;
	}
	mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
	
	mutex_lock(&((*queue)->mtx), "cache: lock fallita in cache_enqueue");
	//caso 2: coda con uno o piu elementi
	if(*queue != NULL){
		new->next = *queue;
		*queue = new;
		mutex_unlock(&((*queue)->mtx), "cache: unlock fallita in cache_enqueue");
		return 0;
	}
	mutex_unlock(&((*queue)->mtx), "cache: unlock fallita in cache_enqueue");
	return 0;
}

int dequeue(t_queue** queue)
{
	mutex_lock(&mtx1, "cache: lock fallita in cache_enqueue");
	//caso 1: cache vuota
	if (*queue == NULL){
		mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
		return -1;
	}
	
	//caso 2: un elemento
	if((*queue)->next == NULL){
		t_queue* aux = *queue;
		*queue = NULL;
		mutex_unlock((&mtx1), "cache: unlock fallita in cache_enqueue");
		return aux->data;
	}
	
	//caso 3: coda con due o piu elementi
	mutex_lock(&((*queue)->mtx), "cache: unlock fallita in cache_enqueue");
	t_queue* prev = *queue;
	mutex_lock(&((*queue)->next->mtx), "cache: unlock fallita in cache_enqueue");
	t_queue* curr = (*queue)->next;

	t_queue* aux;
	while (curr->next != NULL){
		//controllo duplicato
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_enqueue");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_enqueue");

	}
	prev->next = NULL;
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_enqueue");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_enqueue");
	return curr->data;
}

void printf_queue(t_queue* queue)
{	
	mutex_lock(&mtx1, "cache: lock fallita in cache_enqueue");
	while(queue != NULL){
		printf("%d ", queue->data);
		queue = queue->next;
	}
	mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
	printf("\n");
}	

//funzioni di deallocazione coda
void dealloc_queue(t_queue** queue)
{
	mutex_lock(&mtx1, "cache: unlock fallita in cache_enqueue");
	//coda vuota
	if(*queue == NULL){
		mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
		return;
	}
	//coda non vuota
	while(*queue != NULL){
		t_queue* temp = *queue;
		*queue = (*queue)->next;
		free(temp);
	}
	*queue = NULL;
	mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");

}
