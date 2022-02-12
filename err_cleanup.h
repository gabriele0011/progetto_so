/* file include e define che permettono la gestione degli errori e dei clean-up */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SUCCES (0)
#define ERR (-1)
#define MEMERR (-2)

//macro che permette di stampare gli errori in stderr
#define ERR_H(error_code, error_msg) \
	errno = error_code;	\
	fprintf(stderr, "%s: %s\n", error_msg, strerror(error_code)); 

//macro che permettono di controllare l'esito di funzioni ed chiamare proc. di cleanup
#define ec_null(s, m) \
	if(s == NULL){ perror(m); exit(EXIT_FAILURE); }
//error clean up
#define ec_meno1(s, m) \
	if(s == -1){ perror(m); exit(EXIT_FAILURE); }
//con procedura/funzione cleanup
#define ec_meno1_c(s, m, c) \
	if(s == -1){ perror(m); c; }