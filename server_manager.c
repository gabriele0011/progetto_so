//1 ricontrollare la terminazione dei segnali
//2. 


#include "server_manager.h"
#define O_CREATE 10
#define O_LOCK 11

static int worker_openFile(int fd_client);
static int worker_closeFile(int x){return 0;}
static int worker_readFile(int x){return 0;}
static int worker_readNFiles(int x){return 0;}
static int worker_writeFile(int x){return 0;}
static int worker_appendToFile(int x){return 0;}
static int worker_lockFile(int x){return 0;}
static int worker_unlockFile(int x){return 0;}
static int worker_removeFile(int x){return 0;}


/*********** read_config_file ***********/
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
			socket_file_name = calloc(sizeof(char), len_token2);
			strncpy(socket_file_name, token2, len_token2);
			//socket_file_name[len_string] = '\0';
		}
		n++;
	}
	fclose(fd);
	fprintf(stderr, "ho letto config.txt: %s\n", socket_file_name);
	return 0;
}

//handlers segnali di terminazione
static void handler_sigintquit(int signum){
	sig_intquit = 1;
}

static void handler_sighup(int signum){
	sig_hup = 1;
}
void* start_func2(void *arg){
}

//MAIN
int main(int argc, char* argv[])
{

	int err;
	//controllo argomenti main
	if (argc <= 1){
		LOG_ERR(EINVAL, "server_manager: file config.txt mancante");
		exit(EXIT_FAILURE);
	}
	
	//PARSING config.txt
	if (read_config_file(argv[1]) != 0){
		LOG_ERR(-1, "server_manager: lettura file configurazione fallita");
		exit(EXIT_FAILURE);
	}

	//SEGNALI 
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
	
	//CACHE
	create_cache(server_mem_size, max_storage_file);

	//STRUTTURE DATI
	/*********** buf lettura ***********/
	int buf;

	/*********** pipe senza nome ***********/
	int pfd[2];
	int fd_pipe_read = pfd[0];
	int fd_pipe_write = pfd[1];
	ec_meno1(pipe(pfd), "server_manager: creazione pipe fallita");
	printf("Pipe read fd: %d\n", fd_pipe_read);
    	printf("Pipe write fd: %d\n\n", fd_pipe_write);
	
	/*********** thread pool ***********/
	pthread_t thread_workers_arr[t_workers_num];
	for(int i = 0; i < t_workers_num; i++){
		if ((err = pthread_create(&(thread_workers_arr[i]), NULL, start_func, NULL)) != 0){    
			LOG_ERR(err, "server_manager: pthread_create fallita");
			exit(EXIT_FAILURE);
		}
	}

	/*********** listen socket ***********/
	int lenlen = strlen(socket_file_name)+1;
	socket_file_name[lenlen] = '\0';
	
	struct sockaddr_un sa;
	strncpy(sa.sun_path, socket_file_name, strlen(socket_file_name));
	sa.sun_family = AF_UNIX;
	//dichiarazioni fd
	int fd_skt, fd_c;	//fd socket e client
	int fd_num = 0;		//num. max fd attivi
	fd_set set;		//insieme fd attivi
	fd_set rdset;		//insieme fd attesi in lettura
	
	//printf("sun path %s\n", sa.sun_path);
	//printf("socket address: %s\n", socket_file_name);
	//creazione listen socket
	fprintf(stderr, "DEBUG: %s\n", socket_file_name);
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
					//printf("intercettato fd nuov = %d\n",fd);
					//caso 1: si tratta del fd_skt
					if (fd == fd_skt){ 
						printf("fd = %d è il fd socket\n",fd);
						//CONTROLLO SEGNALE sig_hup
						if (!sig_hup){
							//stabilisci connessione con un client
							if ((fd_c = accept(fd_skt, NULL, 0)) == -1){
								LOG_ERR(-1, "server_manager: accept fallita");
								break;
							}
							printf("fd_client %d accettato\n",fd_c); 
							FD_SET(fd_c, &set);
							if (fd_c > fd_num) fd_num = fd_c;
						}
					}else{ 
							
						//caso 2: si tratta del fd della pipe -> un thread ha un messaggio
						if (fd == fd_pipe_read){
							if(read(fd_pipe_read, &buf, sizeof(int)) == -1){
								LOG_ERR(-1, "server_manager: readn fallita");
								break;
							}
							//richiesta servita -> thread ritorna fd
							if( buf == fd ) FD_SET(buf, &set);
							//client disconnesso (read legge 0) -> thread ritorna -fd
							if(buf <= 0){
								buf = buf*(-1);
								FD_CLR(buf, &set);
								close(fd);
								n_client_conn--;
							}	
						//caso 3: inserisce fd client (connesso)in coda concorrente -> nuova richiesta
						}else{	
							//lock
							mutex_lock(&g_mtx, "server_manager: lock fallita");
							//inserisci
							if(enqueue(&conc_queue, fd) == -1){
								LOG_ERR(err, "server_manager: enqueue fallita");
								break;
							}
							//segnale
							if ((err = pthread_cond_signal(&cv)) == -1){
								LOG_ERR(err, "server_manager: signal error");
								break;
							}
							printf("nuova richiesta in coda da client %d\n",fd );
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
	cleanup_section:

	if(conc_queue) dealloc_queue(conc_queue);
	close(fd_pipe_read);
	close(fd_pipe_write);
	exit(EXIT_FAILURE);
	
	return 0;
}

void* start_func(void *arg)
{
	int* buf = NULL;
	ec_null((buf = malloc(sizeof(int))), "start_func: malloc fallita");
	*buf = 0;
	int fd_client;
	int op;
	int err;
	
	while (1){
		mutex_lock(&g_mtx, "start_func: lock fallita");
		//pop richiesta dalla coda concorrente
		while (((*buf = dequeue(&conc_queue)) == -1) && !sig_intquit){
			//wait
			if ( (err = pthread_cond_wait(&cv, &g_mtx)) == -1){
				LOG_ERR(err, "start_func: phtread_cond_wait fallita");
				exit(EXIT_FAILURE);
			}
		}
		mutex_unlock(&g_mtx, "start_func: lock fallita");
		//salvo il client
		fd_client = *buf;
		//libero il buffer e rialloco per riasarlo in lettura
		if (buf){ 
			free(buf); 
			buf = NULL; 
		}
		ec_null((buf = malloc(sizeof(int))), "start_func: malloc fallita");
		*buf = 0;

		//leggi la richiesta dal client
		if (read(fd_client, buf, sizeof(int)) == -1){
			LOG_ERR(-1, "start_func: read su client");
			*buf = 0;
		}

		//a seguire codifica di op e identificazione del comando
		op = *buf;
		printf("richiesta di tipo %d dal client %d\n", op, fd_client);

		//client disconnesso
		if (op == EOF){
			fprintf(stderr, "client %d disconnesso\n", fd_client);
			*buf = fd_client;
			*buf = fd_client*(-1);
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) != -1)
				LOG_ERR(-1, "start_func: write su pipe fallita");
		}

		switch(op){
			//client disconnesso
			case 0:
				fprintf(stderr, "client %d disconnesso\n", fd_client);
				*buf = fd_client;
				*buf = fd_client*(-1);
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			//openFile
			case 1:
				if (worker_openFile(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_openFile fallita");
					exit(EXIT_FAILURE);
				}
				printf("op %d dal client %d terminata\n", op, fd_client);
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//closeFile
			case 2:
				if (worker_closeFile(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_closeFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//writeFile
			case 3:
				if (worker_writeFile(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_writeFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//appendToFile
			case 4:
				if (worker_appendToFile(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_appendToFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//readFile
			case 5:
				if (worker_readFile(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_readFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//readNFiles
			case 6: 
				if (worker_readNFiles(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_readNFiles fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//lockFile
			case 7: 
				if (worker_lockFile(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_lockFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//unlockFile
			case 8: 
				if (worker_unlockFile(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_unlockFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//removeFile
			case 9: 
				if (worker_removeFile(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_removeFile fallita");
					exit(EXIT_FAILURE);
				}
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			//default
			default:
 				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				}
				*buf = fd_client;
				if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: richiesta client non valida");
				break;
		}
	}
	if(buf) free(buf);
	pthread_exit((void*)0);
}


static int worker_openFile(int fd_client)
{
	int* buf = malloc(sizeof(int));
	char* pathname;
	size_t len_pathname;
	int flags;

	*buf = 0;
	//avviso richiesta accettata al client 
	ec_meno1(write(fd_client, buf, sizeof(int)), "server_worker: write fallita");
	
	//lettura della lunghezza del pathname
	ec_meno1(read(fd_client, buf, sizeof(int)), "server_worker: read fallita");
	len_pathname = *buf;
	*buf = 0;
	ec_meno1(write(fd_client, buf, sizeof(int)), "server_worker: write fallita");	

	//allocazione mem pathname
	ec_null((pathname = calloc(sizeof(char), ++len_pathname)), "server_worker: calloc fallita");
	memset(pathname, '\0', sizeof(char)*len_pathname);

	//lettura pathname
	read(fd_client, pathname, sizeof(char)*(len_pathname-1));
	*buf = 0;
	ec_meno1(write(fd_client, buf, sizeof(int)), "server_worker: write fallita");

	//lettura flags
	read(fd_client, buf, sizeof(char)*(len_pathname-1));
	flags = *buf;
	*buf = 0;
	ec_meno1(write(fd_client, buf, sizeof(int)), "server_worker: write fallita");

	printf("pathname: %s flags = %d\n", pathname, flags);
	return 0;


}