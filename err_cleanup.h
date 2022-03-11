//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>


//macro che permette di stampare gli errori in stderr
#define LOG_ERR(error_code, error_msg) \
	errno = error_code;	\
	fprintf(stderr, "%s: %s\n", error_msg, strerror(error_code)); 

//macro controllo ritorno null
#define ec_null(s, m) \
	if(s == NULL){ perror(m); exit(EXIT_FAILURE); }

//macro controllo ritorno -1
#define ec_meno1(s, m) \
	if(s == -1){ perror(m); exit(EXIT_FAILURE); }

//macro controllo -1 con procedura di cleanup
#define ec_meno1_c(s, m, c) \
	if(s == -1){ perror(m); c; }