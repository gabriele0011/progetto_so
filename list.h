//pila LIFO per memorizzare i file aperti nel client
#include "header_file.h"
#include "error_ctrl.h"


typedef struct _node
{
      char* str;
      struct _node* next;
}node;

void insert_node(node** list, const char* string);
node* extract_node(node** list);
void dealloc_list(node** list);
node* search_node(node* list, const char* string);
void remove_node(node** list, const char* string);
void print_list(node* temp);

