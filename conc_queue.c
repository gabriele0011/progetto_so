#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


typedef struct nodo {
	int data;
	struct nodo* next;
}node;

pthread_mutex_t head = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tail = PTHREAD_MUTEX_INITIALIZER;


node* init_queue(node* root, int data);
node* enqueue(node* head, int data);
void dequeue(node* queue);
int queueIsEmpty(node* queue);


node* create_queue(node* root, int data)
{
	root = enqueue(root, data);
	return root;
}


node* enqueue(node* queue, int data)
{
	//creazione nodo
	node* new = malloc(sizeof(node));
	new->data = data;

	//posizionamento nodo
	//caso 1 - coda vuota -> nuova testa
	if (queue == NULL){
		pthread_mutex_lock(&head);
		new->next = NULL;
		queue = new;
		pthread_mutex_unlock(&head);
	//caso 2 - nuova testa
	}else{
		pthread_mutex_lock(&head);
		new->next = queue;
		queue = new;
		pthread_mutex_unlock(&head);
	}	
	return queue;
}

//elimina coda
void dequeue(node* queue)
{
	node* temp = queue;

	//controllo che la lista non sia vuota
	if (queueIsEmpty(queue))
		return;

	//mi posiziono sul penultimo elemento della coda
	while (temp->next != NULL){
		temp = temp->next;
	}
	pthread_mutex_lock(&tail);
	temp->next = NULL;
	free(temp->next);
	pthread_mutex_unlock(&tail);
}	

//controllo coda vuota
int queueIsEmpty(node* queue)
{
	if(queue == NULL) return 0;
	else return 1;
}

int main(){

	node* queue = NULL;
	queue = create_queue(queue, 5);
	printf("root\n", queue->data);
	//printq(queue);
	return 0;
}


