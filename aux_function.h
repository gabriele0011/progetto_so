#include "header_file.h"
#include "error_ctrl.h"


//funzioni ausiliarie
long is_number(const char* s);
int is_opt(char* arg, char* opt);
int is_argument(char* c);
int is_directory(const char *path);
void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td);
void reverse(char s[]);
void itoa(int n, char s[]);
