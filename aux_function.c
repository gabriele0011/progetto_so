#include "aux_function.h"
static int is_opt( char* arg, char* opt)
{
	size_t n = strlen(arg);
	if(strncmp(arg, opt, n) == 0) return 1;
	else return 0;
}
static int is_argument(char* c)
{
	if (c == NULL || (*c == '-') ) return 0;
	else return 1;
}
static int is_directory(const char *path)
{
    struct stat path_stat;
    ec_meno1(lstat(path, &path_stat), "errore stat");
    return S_ISDIR(path_stat.st_mode);
}

static long is_number(const char* s)
{
	char* e = NULL;
   	long val = strtol(s, &e, 0);
   	if (e != NULL && *e == (char)0) return val; 
	return -1;
}
