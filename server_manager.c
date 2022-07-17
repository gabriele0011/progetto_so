
// MANCA:
// lista dei file aperti lato client

//cd /users/gabriele/desktop/progetto_sol/src

#include "server_manager.h"
typedef enum {O_CREATE=1, O_LOCK=2} flags;

static int worker_openFile(int fd_c);
static int worker_writeFile(int fd_c);
static int worker_appendToFile(int fd_c);
static int worker_readFile(int fd_c);
static int worker_readNFiles(int fd_c);
//arrivare qui entro oggi

static int worker_lockFile(int x);
static int worker_unlockFile(int x){return 0;}
static int worker_removeFile(int x){return 0;}
static int worker_closeFile(int x){return 0;}

int read_config_file(char* f_name)
{
	//apertura del fine
	FILE* fd;
	ec_null((fd = fopen(f_name, "r")), "errore fopen in read_config_file");

    	//allocazione buf[size] per memorizzare le righe del file
    	int len = 256;
    	char s[len];
	int n = 0;

    	//acquisizione delle singole righe del file, mi aspetto in ordine:
    	//t_workers_num, server_mem_size, max_storage_file , socket_file_name;
	while( (fgets(s, len, fd)) != NULL ){
		char* token2 = NULL;
		char* token1 = strtok_r(s, ":", &token2);
		
		int len_t = strlen(token1);

		//prime tre acquisizioni
		if(n < 3){
			int len_string = strlen(token2);
			len_string--;			 //lunghezza reale stringa
			token2[len_string] = '\0';	 //sovrascrivo carattere di terminazione \n acquisito dal file
		
			//copio token2 in string
			char* string;
			string = calloc(sizeof(char), len_string);
			string[len_string] = '\0';
			strncpy(string, token2, len_string);
		
			if (strncmp(token1, "t_workers_num", len_t) == 0 )
				ec_meno1((t_workers_num = isNumber(string)), "parametro t_workers_num config.txt non valido");

			if (strncmp(token1, "server_mem_size", len_t) == 0)
				ec_meno1((server_mem_size = isNumber(string)), "parametro server_mem_size config.txt non valido");

			if (strncmp(token1, "max_storage_file", len_t) == 0)
				ec_meno1((max_storage_file = isNumber(string)), "parametro max_storage_file config.txt non valido");
		}
		//quarta e ultima acquisizione non ha '\n'
		if (strncmp(token1, "socket_file_name", len_t) == 0 ){
			int len_token2 = strlen(token2);
			socket_name = calloc(sizeof(char), len_token2);
			strncpy(socket_name, token2, len_token2);
			//socket_file_name[len_string] = '\0';
		}
		n++;
	}
	fclose(fd);
	fprintf(stderr, "ho letto config.txt: %s\n", socket_name);
	return 0;
}

//HANDLERS SEGNALI
static void handler_sigintquit(int signum){
	sig_intquit = 1;
}
static void handler_sighup(int signum){
	sig_hup = 1;
}

//start_func
void* start_func(void *arg)
{
	int* buf = NULL;
	ec_null((buf = malloc(sizeof(int))), "start_func: malloc fallita");
	*buf = 0;
	int fd_c;
	int op;
	int err;
	
	while (1){
		mutex_lock(&g_mtx, "start_func: lock fallita");
		//pop richiesta dalla coda concorrente
		while (((*buf = dequeue(&conc_queue)) == -1) && !sig_intquit){
                  //wait
			if ( (err = pthread_cond_wait(&cv, &g_mtx)) != 0){
				LOG_ERR(err, "start_func: phtread_cond_wait fallita");
				exit(EXIT_FAILURE);
			}
		}
		mutex_unlock(&g_mtx, "start_func: unlock fallita");

		//salvo il client in fd_c
		fd_c = *buf;
            *buf = 0;
		//leggi la richiesta di fd_c se fallisce torna 0 al manager
		if (read(fd_c, buf, sizeof(int)) == -1){
			LOG_ERR(errno, "start_func: read su fd_client fallita -> client disconnesso");
			*buf = 0;
		}
		op = *buf;
		printf("\n");
		printf("(SERVER) - start_func:		elaborazione nuova richiesta di tipo %d\n",op); //DEBUG
		//printf("(SERVER) - start_func: fd_c= %d / op =%d / byte_read=%d\n", fd_c, op, N); //DEBUG
		
		//client disconnesso
		if (op == EOF || op == 0){
			fprintf(stderr, "start_func: client %d disconnesso\n", fd_c);
			*buf = fd_c;
			*buf *=(-1);
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) != -1){
				LOG_ERR(errno, "start_func: write su pipe fallita");
			}
		}
		//openFile
		if( op == 1){
			if ( worker_openFile(fd_c) == -1){ //errno da settare nella funzione chiamata
				LOG_ERR(errno, "start_func: (1) worker_openFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(errno, "start_func: (1) write su pipe fallita");
			}
		}
		//writeFile
		if (op == 2){
			if (worker_writeFile(fd_c) == -1){
				LOG_ERR(-1, "start_func: (3) worker_writeFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (3) write su pipe fallita");
			}
		}
		//appendToFile
		if (op == 3){
			if (worker_appendToFile(fd_c) == -1){
				LOG_ERR(-1, "start_func: (4) worker_appendToFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (4) write su pipe fallita");
			}
		}
		//readFile
		if (op == 4){
			if (worker_readFile(fd_c) == -1){
				LOG_ERR(-1, "start_func: (5) worker_readFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (5) write su pipe fallita");
			}
		}
		//readNFiles
		if (op == 5){ 
			if (worker_readNFiles(fd_c) == -1){
				LOG_ERR(-1, "start_func: (6) worker_readNFiles fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (6) write su pipe fallita");
			}
		}	
		//closeFile
		if( op == 6){
			if ( worker_closeFile(fd_c) == -1){ 
				LOG_ERR(errno, "start_func: (2) worker_closeFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (2) write su pipe fallita");
			}
		}
		
		//readNFiles
		if (op == 6){ 
			if (worker_readNFiles(fd_c) == -1){
				LOG_ERR(-1, "start_func: (6) worker_readNFiles fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (6) write su pipe fallita");
			}
		}
		//lockFile
		if ( op == 7){
			if (worker_lockFile(fd_c) == -1){
				LOG_ERR(-1, "start_func: (7) worker_lockFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (7) write su pipe fallita");
			}
		}
		//unlockFile
		if (op == 8){
			if (worker_unlockFile(fd_c) == -1){
				LOG_ERR(-1, "start_func: (8) worker_unlockFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (8) write su pipe fallita");
			}
		}
		//removeFile
		if(op == 9){
			if (worker_removeFile(fd_c) == -1){
				LOG_ERR(-1, "start_func: (9) worker_removeFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (9) write su pipe fallita");
			}
		}	
	}
	if(buf) free(buf); //crea abort ?
	return NULL;
}

//MAIN
int main(int argc, char* argv[])
{
	int err;
	if (argc < 1){
		LOG_ERR(EINVAL, "server_manager: file config.txt mancante");
		exit(EXIT_FAILURE);
	}
	
	/************** LETTURA CONFIG.TXT **************/
	if (read_config_file(argv[1]) != 0){
		LOG_ERR(-1, "server_manager: lettura file configurazione fallita");
		exit(EXIT_FAILURE);
	}

	/************** SEGNALI **************/
	struct sigaction s;
	//memset((&s, 0, sizeof(s));
	s.sa_handler = handler_sigintquit;
	ec_meno1(sigaction(SIGINT, &s, NULL), "server_manager: sigaction fallita");
	
	struct sigaction s1;
	s1.sa_handler = handler_sigintquit;
	ec_meno1(sigaction(SIGQUIT, &s1, NULL), "server_manager: sigaction fallita");
	
	struct sigaction s2;
	s2.sa_handler = handler_sighup;
	ec_meno1(sigaction(SIGHUP, &s2, NULL), "server_manager: sigaction fallita");
	
	/************** CACHE **************/
	create_cache(server_mem_size, max_storage_file);

	/************** PIPE SENZA NOME **************/
	int pfd[2];
	if((err = pipe(pfd)) == -1) { LOG_ERR(errno, "server_manager: creating pipe"); }
	fd_pipe_read = pfd[0];
	fd_pipe_write = pfd[1];

      /************** THREAD POOL  **************/
	pthread_t thread_workers_arr[t_workers_num];
	for(int i = 0; i < t_workers_num; i++){
		if ((err = pthread_create(&(thread_workers_arr[i]), NULL, start_func, NULL)) != 0){    
			LOG_ERR(err, "server_manager: pthread_create fallita");
			exit(EXIT_FAILURE);
		}
	}

      /**************  DICHIARAZIONE FD **************/
	int fd_skt, fd_c;	//fd socket e client
	int fd_num = 0;		//num. max fd attivi
	fd_set set;			//insieme fd attivi
	fd_set rdset;		//insieme fd attesi in lettura

	/************** LISTEN SOCKET **************/
	int l = strlen(socket_name)+1; //non necessario
	socket_name[l] = '\0';         //non necessario
      struct sockaddr_un sa;
	strncpy(sa.sun_path, socket_name, l);
	sa.sun_family = AF_UNIX;

      //socket/bind/listen
      ec_meno1((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)), "server_manager: socket() fallita");
	ec_meno1(bind(fd_skt, (struct sockaddr*)&sa, sizeof(sa)), "server_manager: bind() fallita");
	ec_meno1(listen(fd_skt, SOMAXCONN), "server_manager: listen() fallita");
	printf("server in ascolto\n\n");

      //aggiornamento fd_num
	if (fd_skt > fd_num) fd_num = fd_skt;
	//inizializzazione e aggiornamento maschera
	FD_ZERO(&set);		
	FD_SET(fd_skt, &set);
	FD_SET(fd_pipe_read, &set);
      int n_client_conn = 0;
	int *buf;
	ec_null((buf = (int*)malloc(sizeof(int))), "start_func: malloc fallita");
	int fd;
	if(fd_skt>fd_pipe_read) fd_num=fd_skt;     
  	else fd_num=fd_pipe_read;    

      /************** LOOP **************/
	while (!sig_intquit){
		if(sig_hup && n_client_conn == 0){
			printf("SIG_HUP: connessione terminate, uscita\n");
			break;
		}
		rdset = set;
		//intercetta fd pronti
		if (select(fd_num+1, &rdset, NULL, NULL, NULL) == -1){
			LOG_ERR(errno, "server_manager: select fallita");
			goto main_clean;
		}
		//controlla fd intercettati da select
		for (fd = 0; fd <= fd_num; fd++){
			//printf("SERVER.mainloop: fd corrente =  %d - fd_max = %d\n", fd, fd_num);//DEBUG
			if (FD_ISSET(fd, &rdset)){
				//printf("(SERVER) - mainloop:	fd ISSET =  %d\n", fd);//DEBUG
				//CASO 1: si tratta del fd_skt -> tentativo di connessione da parte di un client
				if (fd == fd_skt){ 
					//printf("(SERVER) - mainloop:	fd socket =  %d\n", fd);//DEBUG
					//printf("fd = %d è il fd socket connect\n\n", fd); DEBUG
					//CONTROLLO SEGNALE sig_hup
					if (!sig_hup){
						//stabilisci connessione con un client ridirigendolo sulla nuova socket dedicata
						if ((fd_c = accept(fd_skt, NULL, 0)) == -1){ LOG_ERR(errno, "server_manager: accept fallita"); goto main_clean; }
						FD_SET(fd_c, &set);
						//si possono contare il numero di client connessi qui
						n_client_conn++; 
						//printf("(SERVER) - mainloop:	n_client_conn++ = %d\n", n_client_conn);
						if (fd_c > fd_num) fd_num = fd_c;
                                   	//printf("(SERVER) - mainloop:	accettata connessione (con fd = %d) fd_c = %d\n",fd, fd_c); //DEBUG
					}
				}else{
					//caso 2: si tratta del fd della pipe (I/O con client) -> un thread ha un messaggio
					if (fd == fd_pipe_read){
						//printf("(SERVER) - mainloop:	fd pipe =  %d\n", fd);//DEBUG
						*buf = 0;
						if (read(fd_pipe_read, buf, PIPE_MAX_LEN_MSG) == -1){ LOG_ERR(errno, "server_manager: read fallita"); goto main_clean; }
						if(!buf || !(*buf)) { LOG_ERR(EPIPE, "server_manger: read non valida"); goto main_clean; }
						// CASO 1: client disconnesso (read legge 0) o thread ritorna -fd
						if (*buf < 0){
							//printf("(SERVER) - mainloop:	client %d disconnesso\n", *buf); //DEBUG
							*buf = (*buf)*(-1);
							FD_CLR(*buf, &set);
							close(*buf);
							n_client_conn--;
							//printf("(SERVER) - mainloop:	n_client_conn-- = %d\n", n_client_conn);

						}else{
							// CASO 2: richiesta servita -> thread ritorna fd
							FD_SET(*buf, &set);
							//printf("(SERVER) - mainloop:	richiesta fd=%d servita \n", *buf);
						}
					//caso 3: inserisce fd client (connesso)in coda concorrente -> nuova richiesta
					}else{
                                    //printf("sto servendo fd = %d\n", fd); //DEBUG
						//lock
						mutex_lock(&g_mtx, "server_manager: lock fallita");
						//inserisci richiesta in coda
						if(enqueue(&conc_queue, fd) == -1){ LOG_ERR(-1, "server_manager: enqueue fallita"); goto main_clean; }
						//segnalek;
						if ((err = pthread_cond_signal(&cv)) == -1){ LOG_ERR(err, "server_manager: signal error"); goto main_clean; }
						//printf("nuovo fd client (%d) da cui si attende richiesta inserito in coda\n",fd ); //DEBUG
						//unlock
						mutex_unlock(&g_mtx, "server_manager: unlock fallita");
						//printf("(SERVER) - mainloop:	fd=%d in coda richiesta (espulso da RD_SET)\n\n", fd); //DEBUG
						//richiesta fd inviata - aggiornamento maschera	
						FD_CLR(fd, &set);
						//si possono contare le richieste qui
					}
				}
			}
		}
	}
	// (!) IMPLEMENTARE CORRETTAMENTE CHIUSURA SERVER!!!!
	/*********** chiusura server ***********/
	for (int i = 0; i < t_workers_num; i++) {
		if ((err = pthread_join(thread_workers_arr[i], NULL)) == -1){
        		LOG_ERR(err, "server_manager: pthread_join fallita");
      			goto main_clean;
      		}	
	}

	//cleanup chiusura serve normale
	if(conc_queue) dealloc_queue(conc_queue);
	if(cache) cache_dealloc(cache);
	close(fd_pipe_read);
	close(fd_pipe_write);
	close(fd_skt);
	exit(EXIT_SUCCESS);

	//chiusa in caso di errore
	main_clean:
	if(conc_queue) dealloc_queue(conc_queue);
	if(cache) cache=cache_dealloc(cache);
	close(fd_pipe_read);
	close(fd_pipe_write);
	close(fd_skt);
	exit(EXIT_FAILURE);
	 
	return 0;
}

//////////////////////////////////////  SERVER_WORKER  \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
//OPEN FILE  1
static int worker_openFile(int fd_c)
{
	
	int* buf;
   	ec_null( (buf = (int*)malloc(sizeof(int))), "openFile: malloc fallita");
	*buf = 0;
	int len_pathname;
	char* pathname = NULL;

	//1 -> la prima read viene effettuata nella start_func

	//SETTING RICHIESTA
    	//2 (comunica): richiesta openFile (cod.1) accettata
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 1 fallita");

	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	//LEN PATHNAME
	//5 (riceve): lunghezza del pathname
	ec_meno1(read(fd_c, buf, sizeof(int)), "openFile: read fallita");
	len_pathname = *buf;
	//6 risponde: ricevuta
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 2 fallita");

	//PATHNAME
	//allocazione mem pathname
	ec_null((pathname = (char*)calloc(sizeof(char), len_pathname+1)), "openFile: calloc fallita");
	pathname[len_pathname+1] = '\0';
	//9 riceve: pathname
	ec_meno1(read(fd_c, pathname, sizeof(char)*(len_pathname)), "openFile: read 3 fallita");
	//10 (comunica): ricevuto
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 3 fallita");


	//FLAGS
	int flag;
	//13 (riceve): flags
	ec_meno1(read(fd_c, buf, sizeof(int)), "openFile: read fallita");
	flag = *buf;
	//14 (comunica): recevuto
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 4 fallita");


	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 (riceve): pid
	ec_meno1(read(fd_c, buf, sizeof(int)), "openFile: read fallita");
	int id = *buf;
	//18 (comunica): recevuto
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 5 fallita");

	//dati ricevuti 

	printf("(SERVER) - openFile:		INIZIO - file_name:%s | id=%d | flag:", pathname, id); //DEBUG
	if (flag==(O_CREATE|O_LOCK)) printf("O_CREATE|O_LOCK\n"); //DEBUG
	if (flag==O_LOCK) printf("O_LOCK\n"); //DEBUG

	//ELABORAZIONE RICHIESTA
	file* f = NULL;
	file** file_expelled = NULL;
	size_t file_exist = 0;
	int ret_client = 0;

	//se il file f esiste setta var / funge anche da controllo duplicati
	if((f = cache_research(cache, pathname)) != NULL)
		file_exist = 1;
	
	//printf("cache print: "); //DEBUG
	//print_queue(cache); //DEBUG

	if (flag==(O_CREATE|O_LOCK) && !file_exist){
            if (cache_insert(&cache, pathname, NULL, 0, id, file_expelled) != 0){ 
                  LOG_ERR(-1, "openFile: creazione file non riuscita");
                  goto of_clean;
            }
		//printf("cache print: "); //DEBUG
		//print_queue(cache); //DEBUG
	}else{
		if (flag==O_LOCK && file_exist && (f->f_lock == 0 || f->f_lock == id ) ){
			if(cache_lockFile(cache, pathname, id) == -1){
				LOG_ERR(-1, "openFile: lock fallita");
				goto of_clean;
			}
			f->f_write = 1;
			f->f_read = 1;
     		}else	ret_client = -1;
	}
	printf("(SERVER) - openFile:		ESITO = %d\n", ret_client); //DEBUG
	
	//INVIO ESITO OPENFILE
	//20 (comunica): esito openFile
	*buf = ret_client;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 6 fallita");
	//23 (riceve): conferma ricezione
	ec_meno1(read(fd_c, buf, sizeof(int)), "openFile: read fallita");
	if(*buf != 0 ){ LOG_ERR(-1, "openFile: read non valida"); goto of_clean; }

	//INVIO FILE EVENTUALMENTE ESPULSO (1) o NULLA (0)
	if (file_expelled == NULL){
		//nessun file espulso
		*buf = 0;
		ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 11 fallita");
	}else{
		//c'è un file espluso
		*buf = 1;
		ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write fallita");
		//LEN PATHNAME
		int len = strlen((*file_expelled)->f_name);
		*buf = len;
		ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 7 fallita");
		//PATHANAME
		ec_meno1(write(fd_c, (*file_expelled)->f_name, len*sizeof(char)), "openFile: write 8 fallita");
		//SIZE DATA
		ec_meno1(write(fd_c, &((*file_expelled)->f_size), sizeof(size_t)), "openFile: write 9 fallita");
		//DATA
		ec_meno1(write(fd_c, (*file_expelled)->f_data, sizeof(char)*(*file_expelled)->f_size), "openFile: write 10 fallita");
		cache_removeFile(&cache, (*file_expelled)->f_name, id);
	}

	if(buf) free(buf);
	if(pathname) free(pathname);
	return 0;
	
	of_clean:
	if(buf) free(buf);
	if(pathname) free(pathname);
	return -1;
}

//WRITE FILE 2
static int worker_writeFile(int fd_c)
{
	int* buf;
   	ec_null( (buf = (int*)malloc(sizeof(int))), "writeFile: malloc fallita");
	*buf = 0;
	int len;
	char* pathname = NULL;
	int ret_client;
	file** file_expelled = NULL;

	//1 -> la prima read viene effettuata nella start_func

	//SETTING RICHIESTA
    	//2 (comunica): richiesta openFile (cod.1) accettata
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write fallita");


	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:


	//LEN PATHNAME
	//5 (riceve): lunghezza del pathname
	ec_meno1(read(fd_c, buf, sizeof(int)), "writeFile: read fallita");
	len = *buf;
	//6 risponde: ricevuta
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write fallita");	//BUG HERE


	//PATHNAME
	//allocazione mem pathname
	ec_null((pathname = calloc(sizeof(char), len+1)), "writeFile: calloc fallita");
	pathname[len+1] = '\0';
	//9 riceve: pathname
	ec_meno1(read(fd_c, pathname, sizeof(char)*(len)), "writeFile: read fallita"); //inserisci controllo esito read
	//10 (comunica): ricevuto
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write fallita");


	//DIMENSIONE DEL FILE
	int file_size;
	//5 (riceve): lunghezza del pathname
	ec_meno1(read(fd_c, buf, sizeof(int)), "writeFile: read fallita");
	file_size = *buf;
	//6 risponde: ricevuta
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write fallita");


	//DATA FILE
	char* data = NULL;
	ec_null((data = (char*)calloc(sizeof(char), file_size)), "writeFile: calloc fallita");
	ec_meno1(read(fd_c, data, sizeof(char)*file_size), "writeFile: read fallita");
	//x (comunica): ricevuto
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write fallita");


	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 (riceve): pid
	ec_meno1(read(fd_c, buf, sizeof(int)), "writeFile: read fallita");
	int id = *buf;
	//18 (comunica): recevuto
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write fallita");

	//dati ricevuti
	printf("(SERVER) - writeFile:		INIZIO - file_name:%s | file_size=%d | id=%d \n", pathname, file_size, id); //DEBUG


	//ELABORAZIONE
	//scrittura del file
	ret_client = cache_appendToFile(&cache, pathname, data, file_size, id, file_expelled);
	
	//se la scrittura fallisce, va eliminato il file vuoto creato nella openFile
	if(ret_client == -1){
		if(cache_removeFile(&cache, pathname, id) == -1){
			LOG_ERR(-1, "writeFile: scrittura fallita e conseguente rimozione file vuoto fallita");
			goto wf_clean;
		}
	}else{
		//unlock del file (precedentemente lockato per essere scritto)
		if (cache_unlockFile(cache, pathname, id) == -1){
			LOG_ERR(-1, "writeFile: cache_unlock fallita");
			goto wf_clean;
		}
	}

	//print_queue(cache); //DEBUG
	//stampa cache //DEBUG
	//printf("(SERVER) - writeFile: stampa cache: "); //DEBUG
	

	//INVIO ESITO WRITE FILE
	//20 (comunica): esito openFile
	*buf = ret_client;
	ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write 6 fallita");
	//23 (riceve): conferma ricezione
	ec_meno1(read(fd_c, buf, sizeof(int)), "writeFile: read fallita");
	if(*buf != 0 ){ printf("non stanno comunicando\n"); }

	printf("(SERVER) - writeFile:		ESITO = %d\n", ret_client); //DEBUG

	//INVIO FILE EVENTUALMENTE ESPULSO (1) o NULLA (0)
	if (file_expelled == NULL){
		//nessun file espulso
		*buf = 0;
		ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write 11 fallita");
	}else{
		//c'è un file espluso
		*buf = 1;
		ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write fallita");
		//LEN PATHNAME
		int len = strlen((*file_expelled)->f_name);
		*buf = len;
		ec_meno1(write(fd_c, buf, sizeof(int)), "writeFile: write 7 fallita");
		//PATHANAME
		ec_meno1(write(fd_c, (*file_expelled)->f_name, len*sizeof(char)), "writeFile: write 8 fallita");
		//SIZE DATA
		ec_meno1(write(fd_c, &((*file_expelled)->f_size), sizeof(size_t)), "writeFile: write 9 fallita");
		//DATA
		ec_meno1(write(fd_c, (*file_expelled)->f_data, sizeof(char)*(*file_expelled)->f_size), "openFile: write 10 fallita");
		cache_removeFile(&cache, (*file_expelled)->f_name, id);
	}

	if(buf) free(buf);
	if(pathname) free(pathname);
	if(data) free(data);
	return 0;
	
	wf_clean:
	if(buf) free(buf);
	if(pathname) free(pathname);
	if(data) free(data);
	return -1;
}

//APPEND TO FILE 3
static int worker_appendToFile(int fd_c)
{
	int* buffer;
   	ec_null( (buffer = (int*)malloc(sizeof(int))), "appendToFile: malloc fallita");
	*buffer = 0;
	int len;
	char* pathname = NULL;
	int ret_client;
	file** file_expelled = NULL;

	//1 -> la prima read viene effettuata nella start_func

	//SETTING RICHIESTA
    	//2 (comunica): richiesta openFile (cod.1) accettata
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita");


	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:


	//LEN PATHNAME
	//5 (riceve): lunghezza del pathname
	ec_meno1(read(fd_c, buffer, sizeof(int)), "appendToFile: read fallita");
	len = *buffer;
	//6 risponde: ricevuta
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita");


	//PATHNAME
	//allocazione mem pathname
	ec_null((pathname = calloc(sizeof(char), len+1)), "appendToFile: calloc fallita");
	pathname[len+1] = '\0';
	//9 riceve: pathname
	ec_meno1(read(fd_c, pathname, sizeof(char)*(len)), "appendToFile: read fallita"); 
	//10 (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita");


	//DIMENSIONE DEL FILE
	int file_size;
	//5 (riceve): lunghezza del pathname
	ec_meno1(read(fd_c, buffer, sizeof(int)), "appendToFile: read fallita");
	file_size = *buffer;
	//6 risponde: ricevuta
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita");


	//DATA FILE
	char* data = NULL;
	ec_null((data = (char*)calloc(sizeof(char), file_size)), "appendToFile: calloc fallita");
	ec_meno1(read(fd_c, data, sizeof(char)*file_size), "appendToFile: read fallita");
	//x (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita");


	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 (riceve): pid
	ec_meno1(read(fd_c, buffer, sizeof(int)), "appendToFile: read fallita");
	int id = *buffer;
	//18 (comunica): recevuto
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita");


	//dati ricevuti
	printf("(SERVER) - appendToFile:	INIZIO - file_name:%s | file_size=%d | id=%d \n", pathname, file_size, id); //DEBUG


	//ELABORAZIONE
	ret_client = cache_appendToFile(&cache, pathname, data, file_size, id, file_expelled);

	//stampa cache
	//printf("(SERVER) - appendToFile: stampa cache: ");
	//print_queue(cache);

	if (cache_unlockFile(cache, pathname, id) == -1){
		LOG_ERR(-1, "writeFile: cache_unlock fallita");
		goto atf_clean;
	}
	
	//INVIO ESITO APPEND TO FILE
	//20 (comunica): esito
	*buffer = ret_client;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write 6 fallita");
	//23 (riceve): conferma ricezione
	ec_meno1(read(fd_c, buffer, sizeof(int)), "appendToFile: read fallita");
	if(*buffer != 0 ){ LOG_ERR(-1, "writeFile: read non valida fallita"); goto atf_clean; }

	printf("(SERVER) - appendToFile:	ESITO = %d\n", ret_client); //DEBUG


	//INVIO FILE EVENTUALMENTE ESPULSO (1) o NULLA (0)
	if (file_expelled == NULL){
		//nessun file espulso
		*buffer = 0;
		ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write 11 fallita");
	}else{
		//c'è un file espluso
		*buffer = 1;
		ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita");
		//LEN PATHNAME
		int len = strlen((*file_expelled)->f_name);
		*buffer = len;
		ec_meno1(write(fd_c, buffer, sizeof(int)), "appendToFile: write 7 fallita");
		//PATHANAME
		ec_meno1(write(fd_c, (*file_expelled)->f_name, len*sizeof(char)), "appendToFile: write 8 fallita");
		//SIZE DATA
		ec_meno1(write(fd_c, &((*file_expelled)->f_size), sizeof(size_t)), "appendToFile: write 9 fallita");
		//DATA
		ec_meno1(write(fd_c, (*file_expelled)->f_data, sizeof(char)*(*file_expelled)->f_size), "openFile: write 10 fallita");
		cache_removeFile(&cache, (*file_expelled)->f_name, id);
	}

	if(buffer) free(buffer);
	if(pathname) free(pathname);
	if(data) free(data);
	return 0;
	
	atf_clean:
	if(buffer) free(buffer);
	if(pathname) free(pathname);
	if(data) free(data);
	return -1;
}

//READ FILE 4 
static int worker_readFile(int fd_c)
{
	int* buffer;
   	ec_null( (buffer = (int*)malloc(sizeof(int))), "readFile: malloc fallita");
	*buffer = 0;
	int len_path;
	char* pathname = NULL;
	int ret_client = 0;

	//1 -> la prima read viene effettuata nella start_func

	//SETTING RICHIESTA
    	// (comunica): richiesta openFile (cod.1) accettata
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "readFile: write fallita");


	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 (riceve): pid
	ec_meno1(read(fd_c, buffer, sizeof(int)), "readFile: read fallita");
	int id = *buffer;
	//18 (comunica): recevuto
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "readFile: write fallita");


	//LEN PATHNAME
	// (riceve): lunghezza del pathname
	ec_meno1(read(fd_c, buffer, sizeof(int)), "appendToFile: read 1 fallita");
	len_path = *buffer;
	//6 risponde: ricevuta
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "readFile: write fallita");	//BUG HERE


	//PATHNAME
	//allocazione mem. pathname
	ec_null((pathname = calloc(sizeof(char), len_path+1)), "readFile: calloc fallita");
	pathname[len_path+1] = '\0';
	// riceve: pathname
	ec_meno1(read(fd_c, pathname, sizeof(char)*(len_path)), "readFile: read 2 fallita"); //inserisci controllo esito read
	// (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "readFile: write fallita");


	//ELABORAZIONE RICHIESTA
	//cerca il file da leggere
	file* f = NULL;
	if ((f = cache_research(cache, pathname)) == NULL){
		ret_client = -1;	
	}

	//ESITO
	//20 (comunica): esito
	*buffer = ret_client;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "readFile: write 6 fallita");
	//23 (riceve): conferma ricezione
	ec_meno1(read(fd_c, buffer, sizeof(int)), "readFile: read 3 fallita");
	if (*buffer != 0){ LOG_ERR(-1, "readFile: read 3 non valida"); goto rf_clean; }

	//se il file non esiste, finisce qui
	if(ret_client == -1){
		goto rf_clean;
	}
	printf("(SERVER) - writeFile:		INIZIO - file_name:%s | id=%d \n", pathname, id); //DEBUG

	//FILE SIZE
	//comunica: file size
	*buffer = (int)f->f_size;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "readFile: write fallita");	
	//riceve: conferma
	ec_meno1(read(fd_c, buffer, sizeof(int)), "readFile: read 4 fallita"); 
	if (*buffer != 0){ LOG_ERR(-1, "readFile: read 4 non valida"); goto rf_clean; }



	//DATA FILE
	//comunica: data
	ec_meno1(write(fd_c, f->f_data, sizeof(char)*f->f_size), "readFile: write fallita");	
	//riceve: conferma
	ec_meno1(read(fd_c, buffer, sizeof(int)), "readFile: read 5 fallita"); 
	if (*buffer != 0){ LOG_ERR(-1, "readFile: read 5 non valida"); goto rf_clean; }

	printf("(SERVER) - writeFile:		LETTURA RIUSCITA - file_name:%s | id=%d \n", pathname, id); //DEBUG


	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return 0;

	rf_clean:
	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return 0;

}

//READ N FILES 5
static int worker_readNFiles(int fd_c)
{
	int* buffer;
   	ec_null( (buffer = (int*)malloc(sizeof(int))), "readNFiles: malloc fallita");
	*buffer = 0;

	//1 -> la prima read viene effettuata nella start_func

	//SETTING RICHIESTA
    	// (comunica): richiesta readNFiles (cod.5) accettata
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita");


	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	
	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 (riceve): pid
	ec_meno1(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita");
	int id = *buffer;
	//18 (comunica): recevuto
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "readNFiles: write 5 fallita");

	
	//NUMERO DI FILE DA LEGGERE
	// (riceve): 
	ec_meno1(read(fd_c, buffer, sizeof(int)), "readNFiles: read 1 fallita");
	int N = *buffer;
	//6 risponde: conferma ricezione
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita");

	int file_letti = 0;
	file* temp = cache;
	N++; //incremento per fermarmi a N!=1 nel while

	while(1){
		//CONDIZIONE DI TERMINAZIONE
		if(temp != NULL && N != 1){
			//invio di un file
			*buffer = 1;
			ec_meno1(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita");
		}else{
			//termina
			*buffer = 0;
			ec_meno1(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita");
			break;
		}
		ec_meno1(read(fd_c, buffer, sizeof(int)), "readNFiles: read 4 fallita");
		if (*buffer != 0){ LOG_ERR(-1, "readNFiles: read 4 non valida"); goto rnf_clean; }


		
		//CONTROLLO LOCK FILE
		int not_lock = 0;
		if(temp->f_lock == 0 || temp->f_lock == id) not_lock = 1;
		// comunica: al client l'esito del controllo
		*buffer = not_lock;
		ec_meno1(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita");
		//riceve: conferma ricezione flag
		ec_meno1(read(fd_c, buffer, sizeof(int)), "readNFiles: read 4 fallita");
		if (*buffer != 0){ LOG_ERR(-1, "readNFiles: read 4 non valida"); goto rnf_clean; }
		
		if(not_lock){
			
			//LEN FILE NAME
			int len_name = strlen(temp->f_name);
			*buffer = len_name;
			//comunica:
			ec_meno1(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita");	
			//riceve: conferma
			ec_meno1(read(fd_c, buffer, sizeof(int)), "readNFiles: read 4 fallita"); //inserisci controllo esito read
			if (*buffer != 0){ LOG_ERR(-1, "readNFiles: read 4 non valida"); goto rnf_clean; }

			//FILE NAME
			//comunica: 
			ec_meno1(write(fd_c, temp->f_name, sizeof(char)*len_name), "readFile: write fallita");
			//riceve: conferma
			ec_meno1(read(fd_c, buffer, sizeof(int)), "readNFiles: read 4 fallita"); //inserisci controllo esito read
			if (*buffer != 0){ LOG_ERR(-1, "readNFiles: read 4 non valida"); goto rnf_clean; }

			//FILE SIZE
			//comunica: file size
			*buffer = temp->f_size;
			ec_meno1(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita");
			//riceve: conferma
			ec_meno1(read(fd_c, buffer, sizeof(int)), "readNFiles: read 4 fallita"); //inserisci controllo esito read
			if (*buffer != 0){ LOG_ERR(-1, "readNFiles: read 4 non valida"); goto rnf_clean; }

			//DATA FILE
			//comunica: data
			ec_meno1(write(fd_c, temp->f_data, sizeof(char)*(temp->f_size)), "readNFiles: write fallita");	//BUG HERE
			//riceve: conferma
			ec_meno1(read(fd_c, buffer, sizeof(int)), "readNFiles: read 5 fallita"); //inserisci controllo esito read
			if (*buffer != 0){ LOG_ERR(-1, "readNFiles: read 5 non valida"); goto rnf_clean; }
			file_letti++;
			printf("(SERVER) - readNFiles:	file_name:%s LETTO E INVIATO\n", temp->f_name); //DEBUG

		}	
		temp=temp->next;
	}
	printf("(SERVER) - writeFile: FILE LETTI = %d\n", file_letti); //DEBUG

	if(buffer) free(buffer);
	return 0;

	rnf_clean:
	if(buffer) free(buffer);
	return -1;
}




//LOCK
static int worker_lockFile(int fd_c)
{
	int* buffer;
   	ec_null( (buffer = (int*)malloc(sizeof(int))), "lockFile: malloc fallita");
	*buffer = 0;
	int len;
	char* pathname = NULL;
	int ret_client;

	//1 -> la prima read viene effettuata nella start_func

	//SETTING RICHIESTA
    	//(comunica): richiesta lockFile (cod.6) accettata
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "lockFile: write fallita");


	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:


	//LEN PATHNAME
	//(riceve): lunghezza del pathname
	ec_meno1(read(fd_c, buffer, sizeof(int)), "lockFile: read fallita");
	len = *buffer;
	//risponde: ricevuta
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "lockFile: write fallita");	//BUG HERE


	//PATHNAME
	//allocazione mem pathname
	ec_null((pathname = calloc(sizeof(char), len+1)), "lockFile: calloc fallita");
	pathname[len+1] = '\0';
	//riceve: pathname
	ec_meno1(read(fd_c, pathname, sizeof(char)*(len)), "lockFile: read fallita"); //inserisci controllo esito read
	//10 (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "lockFile: write fallita");


	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 (riceve): pid
	ec_meno1(read(fd_c, buffer, sizeof(int)), "lockFile: read fallita");
	int id = *buffer;
	//(comunica): recevuto
	*buffer = 0;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "lockFile: write fallita");


	//ELABORAZIONE RICHIESTA
	ret_client = cache_lockFile(pathname, id);

	
	//ESITO
	//(comunica): esito
	*buffer = ret_client;
	ec_meno1(write(fd_c, buffer, sizeof(int)), "lockFile: write 6 fallita");
	//(riceve): conferma ricezione
	ec_meno1(read(fd_c, buffer, sizeof(int)), "lockFile: read 3 fallita");
	if (*buffer != 0){ LOG_ERR(-1, "lockFile: read 3 non valida"); goto rf_clean; }






}

//UNLOCK


//REMOVE FILE


