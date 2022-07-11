//1 ricontrollare la terminazione dei segnali

#include "server_manager.h"
#define O_CREATE 10
#define O_LOCK 11

static int worker_openFile(int fd_c);
static int worker_closeFile(int x){return 0;}
static int worker_readFile(int x){return 0;}
static int worker_readNFiles(int x){return 0;}
static int worker_writeFile(int x){return 0;}
static int worker_appendToFile(int x){return 0;}
static int worker_lockFile(int x){return 0;}
static int worker_unlockFile(int x){return 0;}
static int worker_removeFile(int x){return 0;}

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

//HANDLERS SEGNALI - forse ne manca uno?
static void handler_sigintquit(int signum){
	sig_intquit = 1;
}
static void handler_sighup(int signum){
	sig_hup = 1;
}


//DEBUG
//start function thread di prova
void* start_func2(void *arg){
      printf("start_func2 debug\n");
}
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
                  printf("DEBUG.start_func: buf dequeue = %d\n", *buf); //DEBUG DEBUG DEBUG
                  //wait
			if ( (err = pthread_cond_wait(&cv, &g_mtx)) == -1){
				LOG_ERR(err, "start_func: phtread_cond_wait fallita");
				exit(EXIT_FAILURE);
			}
		}
		mutex_unlock(&g_mtx, "start_func: lock fallita");
		
            
            //salvo il client
		fd_c = *buf;
		//ripristino buf
            *buf = 0;

		//leggi la richiesta di fd_c
		if (read(fd_c, buf, sizeof(int)) == -1){
			LOG_ERR(-1, "start_func: read su client");
			*buf = 0;
		}

		op = *buf;
            printf("DEBUG.start_func: op = %d\n", op); //DEBUG DEBUG DEBUG

            //DEBUG: cosa accade se op == EOF? si entra comunque nel caso default dello switch?
		
		//client disconnesso
		if (op == EOF || op == 0){
			fprintf(stderr, "start_func: client %d disconnesso\n", fd_c);
			*buf = fd_c;
			*buf = fd_c*(-1);
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) != -1)
				LOG_ERR(-1, "start_func: write su pipe fallita");
		}
		switch(op){
			//openFile
			case 1:
				if (worker_openFile(fd_c) == -1){
					LOG_ERR(-1, "start_func: (1) worker_openFile fallita");
					exit(EXIT_FAILURE);
				}
				printf("op %d dal client %d terminata\n", op, fd_c);
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (1) write su pipe fallita");
				break;
			
			//closeFile
			case 2:
				if (worker_closeFile(fd_c) == -1){
					LOG_ERR(-1, "start_func: (2) worker_closeFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (2) write su pipe fallita");
				break;
			
			//writeFile
			case 3:
				if (worker_writeFile(fd_c) == -1){
					LOG_ERR(-1, "start_func: (3) worker_writeFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (3) write su pipe fallita");
				break;
			
			//appendToFile
			case 4:
				if (worker_appendToFile(fd_c) == -1){
					LOG_ERR(-1, "start_func: (4) worker_appendToFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (4) write su pipe fallita");
				break;
			
			//readFile
			case 5:
				if (worker_readFile(fd_c) == -1){
					LOG_ERR(-1, "start_func: (5) worker_readFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (5) write su pipe fallita");
				break;
			
			//readNFiles
			case 6: 
				if (worker_readNFiles(fd_c) == -1){
					LOG_ERR(-1, "start_func: (6) worker_readNFiles fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (6) write su pipe fallita");
				break;
			
			//lockFile
			case 7: 
				if (worker_lockFile(fd_c) == -1){
					LOG_ERR(-1, "start_func: (7) worker_lockFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (7) write su pipe fallita");
				break;
			
			//unlockFile
			case 8: 
				if (worker_unlockFile(fd_c) == -1){
					LOG_ERR(-1, "start_func: (8) worker_unlockFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (8) write su pipe fallita");
				break;
			
			//removeFile
			case 9: 
				if (worker_removeFile(fd_c) == -1){
					LOG_ERR(-1, "start_func: (9) worker_removeFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (9) write su pipe fallita");
				break;
			//default
			default:
 				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "start_func: (default) write su pipe fallita");
				}
				*buf = fd_c;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "start_func: (default) richiesta client non valida");
				break;
		}
	}
      printf("richiesta di tipo %d dal client %d\n", op, fd_c);
	if(buf) free(buf);
	pthread_exit((void*)0);
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
	ec_meno1(sigaction(SIGQUIT, &s2, NULL), "server_manager: sigaction fallita");
	
	/************** CACHE **************/
	create_cache(server_mem_size, max_storage_file);
      //printf("server.DEBUG: creazione cache riusciata\n");

	//BUF LETTURA
	int buf;

	/************** PIPE SENZA NOME **************/
	int pfd[2];
	ec_meno1(pipe(pfd), "server_manager: creazione pipe fallita");
	int fd_pipe_read = pfd[0];
	int fd_pipe_write = pfd[1];
      //printf("Pipe write fd: %d\n\n", fd_pipe_write);
	//printf("Pipe read fd: %d\n", fd_pipe_read);
	
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
      //printf("socket_name: %s / strlen(socket_name) = %zu / l = %d\n", socket_name, strlen(socket_name), l);
      //printf("sa.sun_path: %s / strlen(sa.sun_path) = %zu\n", sa.sun_path, strlen(sa.sun_path));

      //socket/bind/listen
      ec_meno1((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)), "server_manager: socket() fallita");
	ec_meno1(bind(fd_skt, (struct sockaddr*)&sa, sizeof(sa)), "server_manager: bind() fallita");
	ec_meno1(listen(fd_skt, SOMAXCONN), "server_manager: listen() fallita");
	printf("server in ascolto\n");
	
      //aggiornamento fd_num
	if (fd_skt > fd_num) fd_num = fd_skt;
	//inizializzazione e aggiornamento maschera
	FD_ZERO(&set);		
	FD_SET(fd_skt, &set);
      int n_client_conn = 0;
	
      /************** LOOP **************/
	while (!sig_intquit){
		if(sig_hup && n_client_conn == 0){
			printf("SIG_HUP: connessione terminate, uscita\n");
			break;
		}
		rdset = set;
		//intercetta fd pronti
		if (select(fd_num+1, &rdset, NULL, NULL, NULL) == -1){
			LOG_ERR(-1, "server_manager: select fallita");
			break;
		}else{
			//controlla fd intercettati da select
			for (int fd = 0; fd <= fd_num; fd++){
				if (FD_ISSET(fd, &rdset)){
					printf("intercettato fd = %d\n",fd);
					//CASO 1: si tratta del fd_skt -> tentativo di connessione da parte di un client
					if (fd == fd_skt){ 
						printf("fd = %d è il fd socket connect\n\n", fd);
						//CONTROLLO SEGNALE sig_hup
						if (!sig_hup){
							//stabilisci connessione con un client ridirigendolo sulla nuova socket dedicata
							if ((fd_c = accept(fd_skt, NULL, 0)) == -1){ LOG_ERR(-1, "server_manager: accept fallita"); break; }
							FD_SET(fd_c, &set);
							if (fd_c > fd_num) fd_num = fd_c;
                                          printf("DEBUG.loop: fd_c = %d aggiunto nel rdset\n", fd_c);
						}
					}else{ 
						//caso 2: si tratta del fd della pipe (I/O con client) -> un thread ha un messaggio
						if (fd == fd_pipe_read){
							if (read(fd_pipe_read, &buf, sizeof(int)) == -1){ LOG_ERR(-1, "server_manager: readn fallita"); break; }
							//si hanno due casi, rispettivamente due messaggi possibili:
							//richiesta servita -> thread ritorna fd
							if (buf == fd) FD_SET(buf, &set);
							//client disconnesso (read legge 0) -> thread ritorna -fd
							if (buf <= 0){
								buf = buf*(-1);
								FD_CLR(buf, &set);
								close(fd);
								n_client_conn--;
							}
						//caso 3: inserisce fd client (connesso)in coda concorrente -> nuova richiesta
						}else{
                                          printf("sto servendo fd = %d\n", fd);
							//lock
							mutex_lock(&g_mtx, "server_manager: lock fallita");
							//inserisci richiesta in coda
							if(enqueue(&conc_queue, fd) == -1){ LOG_ERR(err, "server_manager: enqueue fallita"); break; }
							//segnale
							if ((err = pthread_cond_signal(&cv)) == -1){ LOG_ERR(err, "server_manager: signal error"); break; }
							printf("nuovo fd client (%d) da cui si attende richiesta inserito in coda\n",fd );
							//unlock
							mutex_unlock(&g_mtx, "server_manager: unlock fallita");
							
							n_client_conn++; 
                                          printf("n_client_conn = %d\n", n_client_conn);
							FD_CLR(fd, &set);
							fd_num--;
							close(fd);	
						}
					}
				}
			}
		}
	}

	/*********** chiusura server ***********/
	for (int i = 0; i < t_workers_num; i++) {
		if ((err = pthread_join(thread_workers_arr[i], NULL)) == -1){
        		LOG_ERR(err, "server_manager: pthread_join fallita");
      			exit(EXIT_FAILURE);
      		}	
	}

	//cleanup chiusura server
	if(conc_queue) dealloc_queue(conc_queue);
	close(fd_pipe_read);
	close(fd_pipe_write);
	close(fd_skt);
	exit(0);

	//cleanup_section -> (!) eliminabile?
	//cleanup_section:

	if(conc_queue) dealloc_queue(conc_queue);
	close(fd_pipe_read);
	close(fd_pipe_write);
	exit(EXIT_FAILURE);
	
	return 0;
}

//////////////////////////////////////  SERVER_WORKER  \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

//OPEN FILE
static int worker_openFile(int fd_c)
{
	int* buf;
   	ec_null( (buf = malloc(sizeof(int))), "server_worker: malloc fallita");
    	*buf = 0;
	char* pathname;
	size_t len_pathname;
	int flags;

	//SETTING RICHIESTA
    	//2 comunica: richiesta openFile (1) accettata
	*buf = 1;
	ec_meno1(write(fd_c, buf, sizeof(int)), "server_worker: write fallita");
	
	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client

	//LEN PATHNAME
	//5 riceve: lunghezza del pathname
	ec_meno1(read(fd_c, buf, sizeof(int)), "server_worker: read fallita");
	len_pathname = *buf;
	//6 risponde: ricevuto lunghezza pathname
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "server_worker: write fallita");	

	//PATHNAME
	//allocazione mem pathname
	ec_null((pathname = calloc(sizeof(char), ++len_pathname)), "server_worker: calloc fallita");
	//9 riceve: pathname
	read(fd_c, pathname, sizeof(char)*(len_pathname-1)); //inserisci controllo esito read
	pathname[len_pathname+1] = '\0';
	//10 comunica: ricevuto pathname
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "server_worker: write fallita");

	//FLAGS
	//13 riceve: flags
	ec_meno1(read(fd_c, buf, sizeof(int)), "server_worker: read fallita");
	flags = *buf;
	//14 comunica recevuti flags
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "server_worker: write fallita");

	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 riceve: pid
	ec_meno1(read(fd_c, buf, sizeof(int)), "server_worker: read fallita");
	int id = *buf;
	//18 comunica: recevuti flags
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "server_worker: write fallita");

	//dati ricevuti 

	//ELABORAZIONE RICHIESTA
	file* f;
	size_t file_exist = 0;
	int ret_client = 0;
	file** file_expelled = NULL;

	//se il file f esiste setta var
	if((f = cache_research(cache, pathname)) != NULL) file_exist = 1;
	
	//casi di errore 
	if((flags==O_CREATE || flags==(O_CREATE|O_LOCK)) && file_exist ){
		LOG_ERR(-1, "server.openFile: condizioni flag O_CREATE non rispettate");
		ret_client = -1;
	}
	if (flags==O_LOCK && !file_exist){
		LOG_ERR(-1, "server.openFile: condizioni flag O_LOCK non rispettate");
		ret_client = -1;
	}

	//crea nuovo file in lock
	if (flags==(O_CREATE|O_LOCK) /*|| flag=O_CREATE*/){ 
      	//creazione di un file vuoto lockato
            if(cache_insert(&cache, pathname, NULL, 0, id, 0, file_expelled) != 0){ 
                  LOG_ERR(-1, "server.openFile: creazione file non riuscita");
                  ret_client = -1;
            }
	}
	//O_LOCK -> file aperto in lock e in scrittura/lettura
	if (flags==O_LOCK && (f->f_lock == 0 || f->f_lock == id ) ){
            cache_lockFile(cache, pathname, id);
		f->f_write = 1;
		f->f_read = 1;
      }

	//INVIO ESITO OPENFILE
	*buf = ret_client;
	ec_meno1(write(fd_c, buf, sizeof(int)), "server_worker: write fallita");
	
	//INVIO FILE ESPULSO EVENTUALMENTE ESPULSO
	*buf = 0;
	if (file_expelled != NULL){
		*buf = 1;
		ec_meno1(write(fd_c, buf, sizeof(int)), "server_worker: write fallita");
	}else{
		ec_meno1(write(fd_c, buf, sizeof(int)), "server_worker: write fallita");
	}

	//LEN PATHNAME -> controlla tipi
	int len = strlen((*file_expelled)->f_name);
	*buf = len;
	ec_meno1(write(fd_c, buf, sizeof(int)), "client: write fallita");
	//PATHANAME
	ec_meno1(write(fd_c, (*file_expelled)->f_name, len*sizeof(char)), "client: write fallita");
	//SIZE DATA
	ec_meno1(write(fd_c, &((*file_expelled)->f_size), sizeof(size_t)), "client: write fallita");
	//DATA
	ec_meno1(write(fd_c, (*file_expelled)->f_data, sizeof(char)*(*file_expelled)->f_size), "client: write fallita");

	return 0;
}


