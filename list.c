//pila LIFO per memorizzare i node aperti nel client
#include "list.h"

void insert_node(node** list, char* string)
{
      //allocazione
      node* new = NULL;
      new = (node*)malloc(sizeof(node)); //inserire controllo malloc
      //setting
      size_t len_string = strlen(string);
      new->str = (char*)calloc(sizeof(char), len_string+1);
      new->str[len_string+1] = '\0';
      strncpy(new->str, string, len_string);

      if (*list == NULL) new->next = NULL;
      else new->next = *list;
      *list = new;
}

node* extract_node(node** list)
{
      if(*list == NULL) return NULL;
      node* temp;
      temp = *list;
      *list = (*list)->next;
      return temp;
}

void dealloc_list(node** list)
{
      if(*list == NULL) return;
      while(*list != NULL){
            node* temp;
            temp = *list;
            *list = (*list)->next;
            free(temp);
      }
}

//aggiungere procedure di ricerca e rimozione di un nodo

node* search_node(node* list, char* string)
{
      while (list != NULL){
            if (strcmp(list->str, string) == 0) return list;
            list = list->next;
      }
      return NULL;
}

void remove_node(node** list, char* string)
{
      if(*list == NULL) return;
      node* temp = *list;
      node* prev = *list;
      while(temp != NULL){
            if (strcmp(temp->str, string) == 0){
                  prev->next = temp->next;
                  free(temp);
                  break;
            }
            prev = temp;
            temp = temp->next;
      }
}
