//pila LIFO per memorizzare i file aperti nel client
typedef struct Node{
      char* file_name;
      struct Node* next;
}node;

void insert_node(node** list, char* f_name);
node* extract_node(node** list);
void dealloc_list(node** list);