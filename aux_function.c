#include "aux_function.h"
enum { NS_PER_SECOND = 1000000000 };
int is_opt( char* arg, char* opt)
{
	size_t n = strlen(arg);
	if(strncmp(arg, opt, n) == 0) return 1;
	else return 0;
}
int is_argument(char* c)
{
	if (c == NULL || (*c == '-') ) return 0;
	else return 1;
}
int is_directory(const char *path)
{
    struct stat path_stat;
    if (lstat(path, &path_stat) == -1) return -1;
    return S_ISDIR(path_stat.st_mode);
}

long is_number(const char* s)
{
	char* e = NULL;
   	long val = strtol(s, &e, 0);
   	if (e != NULL && *e == (char)0) return val; 
	return -1;
}
void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td)
{
    td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_nsec < 0)
    {
        td->tv_nsec += NS_PER_SECOND;
        td->tv_sec--;
    }
    else if (td->tv_sec < 0 && td->tv_nsec > 0)
    {
        td->tv_nsec -= NS_PER_SECOND;
        td->tv_sec++;
    }
}
 void reverse(char s[])
 {
     int i, j;
     char c;

     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
}  
//converte un intero n in una stringa
 void itoa(int n, char s[])
 {
     int i, sign;

     if ((sign = n) < 0)  /* record sign */
         n = -n;          /* make n positive */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = n % 10 + '0';   /* get next digit */
     } while ((n /= 10) > 0);     /* delete it */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
} 
