#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "err_control.h"
//tipo nodo coda
typedef struct nodo {
	int data;
	struct nodo* next;
}t_queue;


//mutex
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

//prototipi di funzione
//t_queue* init_queue(t_queue* root, int data);
t_queue* enqueue(t_queue* head, int data);
t_queue* dequeue(t_queue* queue, int* data_eject);
int queueIsEmpty(t_queue* queue);

/*
t_queue* create_queue(t_queue* root, int data)
{
	root = enqueue(root, data);
	return root;
}
*/

t_queue* enqueue(t_queue* queue, int data)
{
	//creazione nodo
	t_queue* new;
	ec_null((new = malloc(sizeof(t_queue))), "malloc enqueue fallita");
	new->data = data;

	//posizionamento nodo
	
	//caso 1 - coda vuota -> nuova testa = coda
	if (queue == NULL){
		if (pthread_mutex_lock(&mtx) != 0){ 
			LOG_ERR(errno, "lock fallita in enqueue");
			exit(EXIT_FAILURE);
		}
		new->next = NULL;
		queue = new;
		if (pthread_mutex_unlock(&mtx) != 0){
			LOG_ERR(errno, "unlock fallita in enqueue");
			exit(EXIT_FAILURE);
		}
	//caso 2 - nuova testa
	}else{
		if (pthread_mutex_lock(&mtx) != 0){
			LOG_ERR(errno, "lock fallita in enqueue");
			exit(EXIT_FAILURE);
		}
		new->next = queue;
		queue = new;
		if (pthread_mutex_unlock(&mtx) != 0){
			LOG_ERR(errno, "lock fallita in enqueue");
			exit(EXIT_FAILURE);
		};
	}	
	return queue;
}

//elimina coda -> restitire il dato del nodo cancellato
t_queue* dequeue(t_queue* queue, int* data_eject)
{

	//caso 0: coda vuota
	if(queueIsEmpty(queue)) return NULL;


	t_queue* temp = queue;
	//caso 1: un elemento in coda -> testa = coda
	if(temp->next == NULL){
		if(pthread_mutex_lock(&mtx) != 0){
			LOG_ERR(errno, "lock fallita in dequeue");
			exit(EXIT_FAILURE);
		}
		*data_eject = queue->data;
		queue = NULL;
		free(temp);
		if(pthread_mutex_unlock(&mtx) != 0){
			LOG_ERR(errno, "lock fallita in dequeue");
			exit(EXIT_FAILURE);
		}
		return queue;
	}
	if(pthread_mutex_lock(&mtx) != 0){
		LOG_ERR(errno, "lock fallita in dequeue");
		exit(EXIT_FAILURE);
	}
	//caso 2: coda con n.elem > 1
	//mi posiziono sull'elemento prima delle coda
	while(temp->next->next != NULL){
		temp = temp->next;
	}
	t_queue* del = temp->next;
	temp->next = NULL;
	*data_eject = del->data;
	free(del);
	if(pthread_mutex_unlock(&mtx) != 0){
		LOG_ERR(errno, "unlock fallita in dequeue");
		exit(EXIT_FAILURE);
	}
	return queue;
}

//controllo coda vuota
int queueIsEmpty(t_queue* queue)
{
	if(queue == NULL) return 1;
	else return 0;
}

void printf_queue(t_queue* queue)
{
	while(queue != NULL){
		printf("%d ", queue->data);
		queue = queue->next;
	}
	printf("\n");
}	

//funzioni di deallocazione coda
void dealloc_queue(t_queue* queue)
{
	//coda vuota
	if(queue == NULL) return;
	//coda non vuota
	while(queue != NULL){
		t_queue* temp = queue;
		queue = queue->next;
		free(temp);
	}
}


//TEST

t_queue* init_queue(t_queue* queue){
	int n;
	while(scanf("%d", &n) && n != 0){
		queue = enqueue(queue, n);
	}
	return queue;
}


//main test
int main(){

	t_queue* queue = NULL;
	queue = init_queue(queue);
	int data_eject;
	printf("init queue:\n");
	printf_queue(queue);
	
	queue = dequeue(queue, &data_eject);
	printf("dequeue queue:\n");
	printf_queue(queue);
	printf("data_eject = %d\n", data_eject);

	dealloc_queue(queue);
	if(!queue)printf_queue(queue);
	return 0;
}

