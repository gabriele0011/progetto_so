//pila LIFO per memorizzare i file aperti dal client e, per ogni elemento della cache, i processi che hanno aperto unfile

#include "list.h"
pthread_mutex_t mtx2 = PTHREAD_MUTEX_INITIALIZER;

//inserimento di un elemento in testa alla lista
void insert_node(node** list, const char* string)
{
      mutex_lock(&mtx2, "lock fallita in ");
      //allocazione
      node* new = NULL;
      new = (node*)malloc(sizeof(node)); //inserire controllo malloc
      //setting
      size_t len_string = strlen(string);
      new->str = (char*)calloc(sizeof(char), len_string);
      new->str[len_string] = '\0';
      strncpy(new->str, string, len_string);

      if (*list == NULL) new->next = NULL;
      else new->next = *list;
      *list = new;
      mutex_unlock(&mtx2, "unlock fallita in");
      return;
}

//estrazione testa della lista
node* extract_node(node** list)
{
      mutex_lock(&mtx2, "lock fallita in ");
      if (*list == NULL){
            mutex_unlock(&mtx2, "unlock fallita in");
            return NULL;
      }
      node* temp;
      temp = *list;
      *list = (*list)->next;
      mutex_unlock(&mtx2, "unlock fallita in");
      return temp;
}

//deallocazione  lista
void dealloc_list(node** list)
{
      mutex_lock(&mtx2, "lock fallita in ");
      if (*list == NULL) return;
      while(*list != NULL){
            node* temp;
            temp = *list;
            *list = (*list)->next;
            free(temp);
      }
      mutex_unlock(&mtx2, "unlock fallita in");
      return;
}

//ricerca di un nodo
node* search_node(node* list, const char* string)
{
      mutex_lock(&mtx2, "lock fallita in ");
      while (list != NULL){
            if (strcmp(list->str, string) == 0){
                  mutex_unlock(&mtx2, "unlock fallita in");
                  return list;
            }
            list = list->next;
      }
      mutex_unlock(&mtx2, "unlock fallita in");
      return NULL;
}

//rimozione di un nodo
void remove_node(node** list, const char* string)
{
      mutex_lock(&mtx2, "lock fallita in ");
      if (*list == NULL){
            mutex_unlock(&mtx2, "unlock fallita in");
            return;
      }
      node* curr = *list;
      node* prev = curr;
      while (curr != NULL){
            if(strcmp(curr->str, string) == 0 ){
                  //se il nodo è quello cercato
                  if(curr == *list){ //un solo elem in lista
                        if((*list)->next == NULL) *list = NULL;
                        else (*list) = (*list)->next;
                  }else
                        prev->next = curr->next;
                  free(curr);
                  mutex_unlock(&mtx2, "unlock fallita in");
                  return;
            }
            prev = curr;
            curr = curr->next;
      }
      mutex_unlock(&mtx2, "unlock fallita in");
      return;
}
void print_list(node* temp)
{
      mutex_lock(&mtx2, "lock fallita in ");
      while(temp != NULL){
            printf("%s ", temp->str);
            temp = temp->next;
      }
      printf("\n");
      mutex_unlock(&mtx2, "unlock fallita in");
      return;

}
