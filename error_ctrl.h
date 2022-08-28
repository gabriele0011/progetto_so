//macro per controllo errori 

//controllo errore mutex
#define mutex_lock(addr_mtx, error_msg) \
	if(pthread_mutex_lock(addr_mtx) != 0){ \
			LOG_ERR(errno, error_msg); \
			exit(EXIT_FAILURE);	\
	}
#define mutex_unlock(addr_mtx, error_msg) \
	if( pthread_mutex_unlock(addr_mtx) != 0){ \
			LOG_ERR(errno, error_msg); \
			exit(EXIT_FAILURE);	\
	}

//macro esplicita controllo errori
#define LOG_ERR(error_code, error_msg) \
	errno = error_code;	\
	fprintf(stderr, "%s: %s\n", error_msg, strerror(error_code));


//macro controllo ritorno funzioni con goto
#define ec_null_c(s, m, c) \
	if(s == NULL){ perror(m); goto c; }

#define ec_meno1_c(s, m, c) \
	if(s == -1){ perror(m); goto c; }

//macro controllo ritorno funzioni void 
#define ec_null_v(s, m) \
	if(s == NULL){ perror(m); return; }

#define ec_meno1_v(s, m) \
	if(s == -1){ perror(m); return; }

//macro controllo ritorno funzioni int
#define ec_null_r(s, m, x) \
	if(s == NULL){ perror(m); return x; }

//macro controllo ritorno -1 
#define ec_meno1_r(s, m, x) \
	if(s == -1){ perror(m); return x; }
