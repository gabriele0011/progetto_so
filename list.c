//pila LIFO per memorizzare i file aperti nel client
#include "list.h"

void insert_node(node** list, char* f_name)
{
      //allocazione
      node* new = NULL;
      new = (node*)malloc(sizeof(node)); //inserire controllo malloc
      //setting
      size_t len_f_name = strlen(f_name);
      new->file_name = (char*)calloc(sizeof(char), len_f_name+1);
      new->file_name[len_f_name+1] = '\0';
      strncpy(new->file_name, f_name, len_f_name);

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
