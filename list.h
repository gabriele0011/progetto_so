//pila LIFO per memorizzare i file aperti nel client
#include "header_file.h"

typedef struct _node
{
      char* str;
      struct _node* next;
}node;

void insert_node(node** list, char* string);
node* extract_node(node** list);
void dealloc_list(node** list);
node* search_node(node* list, char* string);
void remove_node(node** list, char* string);

