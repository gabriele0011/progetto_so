#include "server_manager.h"

pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

//flags segnali di teminazione
volatile sig_atomic_t sig_int = 0;	//SIGINT/SIGQUIT: uscita immediata - non si accettano richieste - chiudere connessioni attive
volatile sig_atomic_t sig_quit = 0;
volatile sig_atomic_t sig_hup = 0; 	//SIG_HUP: non si accettano nuove connessioni - si termina una volta concluse quelle attive

size_t tot_requests = 0;
file* cache = NULL;
char* socket_name = NULL;
t_queue* conc_queue = NULL;
int fd_pipe_read = 0;
int fd_pipe_write = 0;

int read_config_file(char* f_name) //riscrivere correttamente questa funzione (!)
{
	//apertura del fine
	FILE* fd;
	ec_null_r((fd = fopen(f_name, "r")), "errore fopen in read_config_file", -1);

    //allocazione buf[size] per memorizzare le righe del file
    int len = 256;
    char s[len];
	//memset((void*)s, '\0', sizeof(char)*len);

    //acquisizione delle singole righe del file, aspetto in ordine:
    //t_workers_num, server_mem_size, max_storage_file , socket_file_name;
	while( (fgets(s, len, fd)) != NULL ){
		
		//stringa prima del delimitatore
		char* save = NULL;
		char* token = strtok_r(s, ":", &save); //primo token prima del delimitatore
		size_t len_t = strlen(token); 
		//stringa dopo il delimitatore
		char* token2 = strtok_r(NULL, ":", &save);
		size_t len = strlen(token2)-1;
		char string[len];
		strncpy(string, token2, len);
		string[len] = '\0';

		if (strncmp(token, "t_workers_num", len_t) == 0 ){
			ec_meno1_r((t_workers_num = is_number(string)), "parametro t_workers_num config.txt non valido", -1);
		}

		if (strncmp(token, "server_mem_size", len_t) == 0){
			ec_meno1_r((server_mem_size = is_number(string)), "parametro server_mem_size config.txt non valido", -1);
		}

		if (strncmp(token, "max_storage_file", len_t) == 0){
			ec_meno1_r((max_storage_file = is_number(string)), "parametro max_storage_file config.txt non valido", -1);
		}

		if (strncmp(token, "socket_file_name", len_t) == 0 ){
			size_t L = strlen(token2);
			char str[L];
			strncpy(str, token2, L);
			str[L] = '\0';
			ec_null_r((socket_name = calloc(sizeof(char), L)), "read_config_file", -1)
			memset((void*)socket_name, '\0', L);
			strncpy(socket_name, str, L);
			
			break;
		}
	}
	if (fclose(fd) != 0){ LOG_ERR(errno, "read_config_file: fclose fallita"); return -1; }
	printf("lettura config.txt avvenuta con successo\n"); //DEBUG
	return 0;
}

//HANDLERS SEGNALI
static void handler_sigquit(int signum){
	sig_quit = 1;
	return;
}
static void handler_sigint(int signum){
	sig_int = 1;
	return;
}
static void handler_sighup(int signum){
	sig_hup = 1;
	return;
}

//thread_func
void* thread_func(void *arg)
{
	int fd_c;
	int op;
	int err;
	int* buf = NULL;

	while (1){ // (!) break in caso di EOF o 0?
		
		mutex_lock(&g_mtx, "thread_func: lock fallita");
		//pop richiesta dalla coda concorrente
		while (((buf = (int*)dequeue(&conc_queue)) == NULL) && (!sig_int && !sig_quit)){
			if ( (err = pthread_cond_wait(&cv, &g_mtx)) != 0){
				LOG_ERR(err, "thread_func: phtread_cond_wait fallita");
				exit(EXIT_FAILURE);
			}
		}
		mutex_unlock(&g_mtx, "thread_func: unlock fallita");
		
		if(sig_int || sig_quit ){ 	
			if(buf) free(buf);
			return NULL;
		}
		//salvo il client in fd_c
		fd_c = *buf;
        *buf = 0;
		//leggi la richiesta di fd_c se fallisce torna 0 al manager
		if (read(fd_c, buf, sizeof(int)) == -1){
			LOG_ERR(errno, "thread_func: read su fd_client fallita -> client disconnesso");
			*buf = 0;
		}
		op = *buf;

		printf("\n");
		printf("(SERVER) - thread_func:		elaborazione nuova richiesta di tipo %d\n", op); //DEBUG
		//printf("(SERVER) - thread_func: fd_c= %d / op =%d / byte_read=%d\n", fd_c, op, N); //DEBUG
		
		//client disconnesso
		if (op == EOF || op == 0){
			//fprintf(stderr, "thread_func: client %d disconnesso\n", fd_c);
			*buf = fd_c;
			*buf *=(-1);
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(errno, "thread_func: write su pipe fallita");
			}
		}
		//openFile
		if( op == 1){
			if ( worker_openFile(fd_c) == -1){ //errno da settare nella funzione chiamata
				LOG_ERR(errno, "thread_func: (1) worker_openFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(errno, "thread_func: (1) write su pipe fallita");
			}
			tot_requests++;
		}
		//writeFile
		if (op == 2){
			if (worker_writeFile(fd_c) == -1){
				LOG_ERR(-1, "thread_func: (2) worker_writeFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "thread_func: (2) write su pipe fallita");
			}
			tot_requests++;
		}
		//appendToFile
		if (op == 3){
			if (worker_appendToFile(fd_c) == -1){
				LOG_ERR(-1, "thread_func: (3) worker_appendToFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "thread_func: (3) write su pipe fallita");
			}
			tot_requests++;
		}
		//readFile
		if (op == 4){
			if (worker_readFile(fd_c) == -1){
				LOG_ERR(-1, "thread_func: (4) worker_readFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "thread_func: (4) write su pipe fallita");
			}
			tot_requests++;
		}
		//readNFiles
		if (op == 5){ 
			if (worker_readNFiles(fd_c) == -1){
				LOG_ERR(-1, "thread_func: (5) worker_readNFiles fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "thread_func: (5) write su pipe fallita");
			}
			tot_requests++;
		}	
		//lockFile
		if ( op == 6){
			if (worker_lockFile(fd_c) == -1){
				LOG_ERR(-1, "thread_func: (6) worker_lockFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "thread_func: (6) write su pipe fallita");
			}
			tot_requests++;
		}
		
		//unlockFile
		if (op == 7){
			if (worker_unlockFile(fd_c) == -1){
				LOG_ERR(-1, "thread_func: (7) worker_unlockFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "thread_func: (7) write su pipe fallita");
			}
			tot_requests++;
		}
		//removeFile
		if(op == 8){
			if (worker_removeFile(fd_c) == -1){
				LOG_ERR(-1, "thread_func: (8) worker_removeFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "thread_func: (8) write su pipe fallita");
			}
			tot_requests++;
		}	
		//closeFile
		if( op == 9){
			if ( worker_closeFile(fd_c) == -1){ 
				LOG_ERR(errno, "thread_func: (9) worker_closeFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "thread_func: (9) write su pipe fallita");
			}
			tot_requests++;
		}
		if(buf) free(buf);
		buf = NULL;
	}
}

//MAIN
int main(int argc, char* argv[])
{
	int err;
	int *buf;
	if (argc < 1){
		LOG_ERR(EINVAL, "(SERVER) - main:	file config.txt mancante");
		exit(EXIT_FAILURE);
	}
	
	/////////////////	LETTURA CONFIG.TXT /////////////////
	if (read_config_file(argv[1]) == -1){
		LOG_ERR(ECONNABORTED, "(SERVER) - main:	lettura file di configurazione fallita");
		exit(EXIT_FAILURE);
	}
	///////////////// DICHIARAZIONE FD /////////////////
	int fd_skt = 0;
	int fd_c;			//fd socket e client
	int fd_num = 0;		//num. max fd attivi
	fd_set set;			//insieme fd attivi
	fd_set rdset;		//insieme fd attesi in lettura
    
	///////////////// THREAD POOL  /////////////////
	pthread_t thread_workers_arr[t_workers_num];
	for(int i = 0; i < t_workers_num; i++){
		if ((err = pthread_create(&(thread_workers_arr[i]), NULL, thread_func, NULL)) != 0){    
			LOG_ERR(err, "pthread_create in server_manager");
			goto main_clean;
		}
	}

	///////////////// CACHE /////////////////
	create_cache(server_mem_size, max_storage_file);

	///////////////// PIPE SENZA NOME /////////////////
	int pfd[2];
	if((err = pipe(pfd)) == -1) { LOG_ERR(errno, "server_manager"); goto main_clean; }
	fd_pipe_read = pfd[0];
	fd_pipe_write = pfd[1];

	///////////////// LISTEN SOCKET /////////////////
    struct sockaddr_un sa;
	sa.sun_family = AF_UNIX;
	size_t N = strlen(socket_name);
	//memset((void*)sa.sun_path, '\0', sizeof(char)*N);
	strncpy(sa.sun_path, socket_name, N);
	sa.sun_path[N] = '\0';

    //socket/bind/listen
    ec_meno1_c((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)), "server_manager: socket() fallita", main_clean);
	ec_meno1_c(bind(fd_skt, (struct sockaddr*)&sa, sizeof(sa)), "server_manager: bind() fallita", main_clean);
	ec_meno1_c(listen(fd_skt, SOMAXCONN), "server_manager: listen() fallita", main_clean);
	printf("server in ascolto (socket_name:%s)\n\n", socket_name);
    //aggiornamento fd_num
	if (fd_skt > fd_num) fd_num = fd_skt;
	//inizializzazione e aggiornamento maschera
	FD_ZERO(&set);		
	FD_SET(fd_skt, &set);
	FD_SET(fd_pipe_read, &set);
    int n_client_conn = 0;
	ec_null_c((buf=(int*)malloc(sizeof(int))), "thread_func: malloc fallita", main_clean);
	int fd;
	if(fd_skt>fd_pipe_read) fd_num=fd_skt;     
  	else fd_num=fd_pipe_read;  
	
	///////////////// SEGNALI /////////////////
	struct sigaction s;
	memset(&s, 0, sizeof(struct sigaction));
	s.sa_handler = handler_sigint;
	ec_meno1_c(sigaction(SIGINT, &s, NULL), "server_manager: sigaction fallita", main_clean);
	
	struct sigaction s1;
	memset(&s1, 0, sizeof(struct sigaction));
	s1.sa_handler = handler_sigquit;
	ec_meno1_c(sigaction(SIGQUIT, &s1, NULL), "server_manager: sigaction fallita", main_clean);
	
	struct sigaction s2;
	memset(&s2, 0, sizeof(struct sigaction));
	s2.sa_handler = handler_sighup;
	ec_meno1_c(sigaction(SIGHUP, &s2, NULL), "server_manager: sigaction fallita", main_clean);  

    ///////////////// CICLO /////////////////
	while (!sig_int && !sig_quit){
		if(sig_hup && n_client_conn == 0){
			printf("SIG_HUP: connessione terminate, uscita\n");
			break;
		}
		rdset = set;
		//intercetta fd pronti
		if (select(fd_num+1, &rdset, NULL, NULL, NULL) == -1){
			if(errno=EINTR) continue;
			LOG_ERR(errno, "server_manager: select fallita");
			goto main_clean;
		}
		//controlla fd intercettati da select
		for (fd = 0; fd <= fd_num; fd++){
			//printf("SERVER.mainloop: fd corrente =  %d - fd_max = %d\n", fd, fd_num);// DEBUG
			if (FD_ISSET(fd, &rdset)){
				//printf("(SERVER) - mainloop:	fd ISSET =  %d\n", fd);//DEBUG
				//CASO 1: si tratta del fd_skt -> tentativo di connessione da parte di un client
				if (fd == fd_skt){ 
					//printf("(SERVER) - mainloop:	fd socket =  %d\n", fd);// DEBUG
					//printf("fd = %d è il fd socket connect\n\n", fd); // DEBUG
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
						//printf("(SERVER) - mainloop:	fd pipe =  %d\n", fd); // DEBUG
						*buf = 0;
						if (read(fd_pipe_read, buf, PIPE_MAX_LEN_MSG) == -1){ LOG_ERR(errno, "server_manager: read fallita"); goto main_clean; }
						if(!buf || !(*buf)) { LOG_ERR(EPIPE, "server_manger: read non valida"); goto main_clean; }
						// CASO 1: client disconnesso (read legge 0) o thread ritorna -fd
						if (*buf < 0){
							*buf = (*buf)*(-1);
							printf("(SERVER) - server_manager:	client %d disconnesso\n", *buf); // DEBUG
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
	
	///////////////// CHIUSURA DEL SERVER /////////////////
	mutex_lock(&g_mtx, "server_manager: lock fallita");
	pthread_cond_broadcast(&cv);
	mutex_unlock(&g_mtx, "server_manager: unlock fallita");

	//chiusura server normale
	for (int i = 0; i < t_workers_num; i++) {
		if ((err = pthread_join(thread_workers_arr[i], NULL)) == -1){
        		LOG_ERR(err, "server_manager: pthread_join fallita");
      			goto main_clean;
      		}	
	}
	//if(thread_workers_arr) free(thread_workers_arr);
	close(fd_pipe_read);
	close(fd_pipe_write);
	close(fd_skt);
	//if(socket_name) free(socket_name);
	if(buf) free(buf);
	if(cache) cache_dealloc(&cache);
	if(conc_queue) dealloc_queue(&conc_queue);
	
	exit(EXIT_SUCCESS);

	//chiususa server in caso di errore
	main_clean:
	if(socket_name) free(socket_name);
	if(buf) free(buf);
	if(cache) cache_dealloc(&cache);
	if(conc_queue) dealloc_queue(&conc_queue);
	exit(EXIT_FAILURE);
}

//////////////////////////////////////  SERVER_WORKER  //////////////////////////////////////

//OPEN FILE 1 ok
static int worker_openFile(int fd_c)
{
	char* pathname = NULL;
	int* buf;
   	ec_null_c( (buf = (int*)malloc(sizeof(int))), "malloc in openFile", of_clean);
	*buf = 0;
	size_t len;

	//SETTING RICHIESTA
    //comunica: richiesta openFile (cod.1) accettata
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in openFile", of_clean);

	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	//LEN PATHNAME
	//riceve: lunghezza pathname
	ec_meno1_c(read(fd_c, buf, sizeof(int)), "read in openFile", of_clean);
	len = *buf;
	//comunica: ricevuta lungh. pathname
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in openFile", of_clean);

	//PATHNAME
	//allocazione mem pathname
	ec_null_c((pathname = (char*)calloc(sizeof(char), len)), "calloc in openFile", of_clean);
	memset((void*)pathname, '\0', (sizeof(char)*len));
	//riceve: pathname
	ec_meno1_c(read(fd_c, pathname, sizeof(char)*(len)), "read in openFile", of_clean);
	//comunica: pathname ricevuto
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in openFile", of_clean);

	//FLAGS
	int flag;
	//riceve: flags
	ec_meno1_c(read(fd_c, buf, sizeof(int)), "read in openFile", of_clean);
	flag = *buf;
	//comunica: flags ricevuti
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in openFile", of_clean);

	//IDENTIFICAZIONE PROCESSO CLIENT
	//riceve: pid
	ec_meno1_c(read(fd_c, buf, sizeof(int)), "read in openFile", of_clean);
	int id = *buf;
	//comunica: pid recevuto
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in openFile", of_clean);

	//dati ricevuti 

	printf("(SERVER) - openFile:		INIZIO - path:%s | len_path=%zu |  id=%d | flag:", pathname, len, id); //DEBUG
	if (flag==(O_CREATE|O_LOCK)) printf("O_CREATE|O_LOCK\n"); //DEBUG
	if (flag==O_LOCK) printf("O_LOCK\n"); //DEBUG
	if (flag == 0) printf("/\n"); //DEBUG
	
	/////////////////////////////
	// ELABORAZIONE RICHIESTA //
	///////////////////////////

	file* f = NULL;
	file* file_expelled = NULL;
	size_t file_exist = 0;
	int ret = 0;

	if((f = cache_research(cache, pathname)) != NULL) 
		file_exist = 1;

	
	//CASO CREAZIONE DI UN NUOVO FILE IN LOCK
	if (flag==(O_CREATE|O_LOCK)){
		if(!file_exist){
            	if (cache_insert(&cache, pathname, NULL, 0, id, &file_expelled) != 0)
					goto of_clean;
				//viene aggiornata f->id_list direttamente nella cache
        }else{
				errno = EEXIST;  //condizioni flag non rispettate dal client -> richiesta non valida
				ret = -2;
		}	
	}
	
	//CASO APERTURA IN LOCK  (questo caso non si verifica mai)
	if (flag==O_LOCK){
		if (file_exist){ 
			if (cache_lockFile(cache, f->f_name, id) == -1){ //(!) AGGIUNGERE ATTESA ATTIVA DELLA LOCK
				goto of_clean;
			}
			//aggiorna lista dei processi che hanno aperto il file
			char str_id[101];
			itoa(id, str_id);
			if (search_node(f->id_list, str_id) != NULL) //il file è gia stato aperto?
				insert_node(&(f->id_list), str_id);
		}else{
			errno = EINVAL;
			ret = -2;
		}
	}
	
	//CASO APERTURA SENZA FLAGS
	if (flag==0){
		if (file_exist){ 
			char char_id[101];
			itoa(id, char_id);
			if (search_node(f->id_list, char_id) != NULL) {//il file è gia stato aperto?
				insert_node(&(f->id_list), char_id);
			}
		}else{
			errno = EINVAL;
			ret = -2;
		}
	}
	
	//INVIO ESITO OPENFILE
	//comunica: esito openFile
	if (ret != 0) ret = errno;
	*buf = ret;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in openFile", of_clean);
	//riceve: conferma ricezione
	ec_meno1_c(read(fd_c, buf, sizeof(int)), "read in openFile", of_clean);
	if (*buf != 0 ){ goto of_clean; }
	
	printf("(SERVER) - openFile:		ESITO = %d\n", ret); //DEBUG
	//INVIO FILE EVENTUALMENTE ESPULSO (1) o NULLA (0)
	if (file_expelled == NULL){
		//nessun file espulso
		*buf = 0;
		ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in openFile", of_clean);
	}else{
		//c'è un file espluso
		*buf = 1;
		ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in openFile", of_clean);
		//LEN PATHNAME
		size_t len_path = strlen(file_expelled->f_name);
		*buf =  len_path;
		ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in openFile", of_clean);
		//PATHANAME
		ec_meno1_c(write(fd_c, file_expelled->f_name,  sizeof(char)*len_path), "write in openFile", of_clean);
		//SIZE DATA
		ec_meno1_c(write(fd_c, &(file_expelled->f_size), sizeof(int)), "write in openFile XXX", of_clean);
		//DATA
		ec_meno1_c(write(fd_c, file_expelled->f_data, sizeof(char)*(file_expelled->f_size)), "write in openFile", of_clean);
		
		//rimozione del nodo rimpiazzato dalla cache
		printf("(SERVER) - openFile:		file espulso (path:%s - len_path=%zu- size=%zu) inviato\n", file_expelled->f_name, len_path, file_expelled->f_size); //DEBUG
		
		if (file_expelled != NULL){
			free(file_expelled->f_name);
			free(file_expelled->f_data);
			dealloc_list(&(file_expelled->id_list));
			free(file_expelled);
		}
	}

	if (buf) free(buf);
	if (pathname) free(pathname);
	return ret;
	
	of_clean:
	if (buf) free(buf);
	if (pathname) free(pathname);
	return -1;
}

//WRITE FILE 2 ok
static int worker_writeFile(int fd_c)
{
	char* pathname = NULL;
	char* data = NULL;
	int* buf;
   	ec_null_c((buf = (int*)malloc(sizeof(int))), "malloc in writeFile", wf_clean);
	*buf = 0;
	int ret = 0;
	file* file_expelled = NULL;

	//SETTING RICHIESTA
    //comunica: richiesta openFile accettata
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in writeFile", wf_clean);

	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	//LEN PATHNAME
	size_t len;
	//riceve: lunghezza del pathname
	ec_meno1_c(read(fd_c, buf, sizeof(int)), "read in writeFile", wf_clean);
	len = *buf;
	//risponde: ricevuta
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in writeFile", wf_clean);

	//PATHNAME
	//allocazione mem pathname
	ec_null_c((pathname = calloc(sizeof(char), len)), "calloc in writeFile", wf_clean);
	memset((void*)pathname, '\0', (sizeof(char)*len));
	//pathname[len] = '\0';
	//riceve: pathname
	ec_meno1_c(read(fd_c, pathname, sizeof(char)*(len)), "read in writeFile", wf_clean);
	//comunica: ricevuto
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in writeFile", wf_clean);

	//DIMENSIONE DEL FILE
	size_t file_size;
	//riceve: dimensione file
	ec_meno1_c(read(fd_c, buf, sizeof(int)), "read in writeFile", wf_clean);
	file_size = *buf;
	//risponde: ricevuta
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in writeFile", wf_clean);

	//DATA FILE
	ec_null_c((data = (char*)calloc(sizeof(char), file_size)), "calloc in writeFile", wf_clean);
	ec_meno1_c(read(fd_c, data, sizeof(char)*file_size), "read in writeFile", wf_clean);
	//comunica: ricevuto
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in writeFile", wf_clean);

	//IDENTIFICAZIONE PROCESSO CLIENT
	//riceve: pid
	ec_meno1_c(read(fd_c, buf, sizeof(int)), "write in writeFile", wf_clean);
	int id = *buf;
	//comunica: recevuto
	*buf = 0;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "write in writeFile", wf_clean);

	//dati ricevuti
	//printf("(SERVER) - writeFile:		su path:%s | len_path=%zu | file_size=%zu | id=%d \n", pathname, len, file_size, id); //DEBUG

	////////////////////////////
	// ELABORAZIONE RICHIESTA //
	////////////////////////////

	//printf("ret=%d - path:%s - size:%d - id:%d\n", ret, pathname, file_size, id); //DEBUG
	//se la scrittura fallisce, va eliminato il file vuoto creato nella openFile

	if(cache_appendToFile(&cache, pathname, data, file_size, id, &file_expelled) == 0){
		if (cache_unlockFile(cache, pathname, id) != 0)
			ret = -1;
	}else{
		if(cache_removeFile(&cache, pathname, id) != 0){
			ret = -1;
		}
	}

	//INVIO ESITO WRITE FILE
	//comunica: esito openFile
	if(ret != 0) ret = errno;
	*buf = ret;
	ec_meno1_c(write(fd_c, buf, sizeof(int)), "writeFile: write fallita", wf_clean);
	//riceve: conferma ricezione
	ec_meno1_c(read(fd_c, buf, sizeof(int)), "read in writeFile", wf_clean);
	if(*buf != 0 ){ goto wf_clean; }

	//printf("(SERVER) - writeFile:		ESITO = %d\n", ret); //DEBUG

	//INVIO FILE EVENTUALMENTE ESPULSO
	if (file_expelled == NULL){
		//nessun file espulso
		*buf = 0;
		ec_meno1_c(write(fd_c, buf, sizeof(int)), "writeFile: write fallita", wf_clean);
	}else{
		//c'è un file espluso
		*buf = 1;
		ec_meno1_c(write(fd_c, buf, sizeof(int)), "writeFile: write fallita", wf_clean);
		//LEN PATHNAME
		size_t len_path = strlen(file_expelled->f_name);
		*buf = len_path;
		ec_meno1_c(write(fd_c, buf, sizeof(int)), "writeFile: write fallita", wf_clean);
		//PATHANAME
		ec_meno1_c(write(fd_c, file_expelled->f_name, len_path*sizeof(char)), "writeFile: write fallita", wf_clean);
		//SIZE DATA
		ec_meno1_c(write(fd_c, &(file_expelled->f_size), sizeof(int)), "writeFile: write fallita XXX", wf_clean);
		//DATA
		ec_meno1_c(write(fd_c, file_expelled->f_data, sizeof(char)*(file_expelled->f_size)), "openFile: write fallita", wf_clean);
		//printf("(SERVER) - openFile:		file espulso (apth:%s -len_path=%zu- size=%zu) inviato\n", file_expelled->f_name, len_path, file_expelled->f_size); //DEBUG
		if (file_expelled != NULL){
			free(file_expelled->f_name);
			free(file_expelled->f_data);
			dealloc_list(&(file_expelled->id_list));
			free(file_expelled);
		}

	}

	if(buf) free(buf);
	if(pathname) free(pathname);
	if(data) free(data);
	return ret;
	
 	wf_clean:
	if(buf) free(buf);
	if(pathname) free(pathname);
	if(data) free(data);
	return -1;
}

//APPEND TO FILE 3 ok
static int worker_appendToFile(int fd_c)
{
	int* buffer;
	char* pathname = NULL;
	char* data = NULL;
   	ec_null_c( (buffer = (int*)malloc(sizeof(int))), "malloc in appendToFile", atf_clean);
	*buffer = 0;
	int ret = 0;
	file* file_expelled = NULL;

	//SETTING RICHIESTA
    //comunica: richiesta openFile (cod.1) accettata
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);

	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	//LEN PATHNAME
	size_t len;
	//riceve: lunghezza del pathname
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	len = *buffer;
	//risponde: ricevuta
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);

	//PATHNAME
	//allocazione mem pathname
	ec_null_c((pathname = calloc(sizeof(char), len)), "appendToFile: calloc fallita", atf_clean);
	memset((void*)pathname, '\0', (sizeof(char)*len));
	//riceve: pathname
	ec_meno1_c(read(fd_c, pathname, sizeof(char)*(len)), "appendToFile: read fallita", atf_clean); 
	//comunica: ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);

	//DIMENSIONE DEL FILE
	size_t file_size;
	//riceve: lunghezza del pathname
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	file_size = *buffer;
	//risponde: ricevuta
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);

	//DATA FILE
	ec_null_c((data = (char*)calloc(sizeof(char), file_size)), "appendToFile: calloc fallita", atf_clean);
	ec_meno1_c(read(fd_c, data, sizeof(char)*file_size), "appendToFile: read fallita", atf_clean);
	//comunica: ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);

	//IDENTIFICAZIONE PROCESSO CLIENT
	//riceve: pid
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	int id;
	id = *buffer;
	//comunica: recevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);

	//dati ricevuti
	//printf("(SERVER) - appendToFile:	su path:%s | len_path=%zu | file_size=%zu | id=%d \n", pathname, len, file_size, id); //DEBUG

	/////////////////////////////
	// ELABORAZIONE RICHIESTA //
	///////////////////////////

	if (cache_appendToFile(&cache, pathname, data, file_size, id, &file_expelled) != 0){
		ret = -1;
	}
	
	//INVIO ESITO APPEND TO FILE
	//comunica: esito
	if(ret != 0) ret = errno;
	*buffer = ret;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);
	//riceve: conferma ricezione
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	if(*buffer != 0 ){ LOG_ERR(-1, "writeFile: read non valida fallita"); goto atf_clean; }

	//printf("(SERVER) - appendToFile:	ESITO = %d\n", ret); //DEBUG

	//INVIO FILE EVENTUALMENTE ESPULSO (1) o NULLA (0)
	if (file_expelled == NULL){
		//nessun file espulso
		*buffer = 0;
		ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);
	}else{
		//c'è un file espluso
		*buffer = 1;
		ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);
		//LEN PATHNAME
		size_t len_path = strlen(file_expelled->f_name);
		*buffer = len_path;
		ec_meno1_c(write(fd_c, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);
		//PATHANAME
		ec_meno1_c(write(fd_c, file_expelled->f_name, len_path*sizeof(char)), "appendToFile: write fallita", atf_clean);
		//SIZE DATA
		ec_meno1_c(write(fd_c, &(file_expelled->f_size), sizeof(int)), "appendToFile: write fallita", atf_clean); 
		//DATA
		ec_meno1_c(write(fd_c, file_expelled->f_data, sizeof(char)*(file_expelled->f_size)), "openFile: write fallita", atf_clean);
		//printf("(SERVER) - openFile:		file espulso (apth:%s -len_path=%zu - size=%zu) inviato\n", file_expelled->f_name, len_path, file_expelled->f_size); //DEBUG
		if (file_expelled != NULL){
			free(file_expelled->f_name);
			free(file_expelled->f_data);
			dealloc_list(&(file_expelled->id_list));
			free(file_expelled);
		}
	}

	if(buffer) free(buffer);
	if(pathname) free(pathname);
	if(data) free(data);
	return ret;
	
	atf_clean:
	if(buffer) free(buffer);
	if(pathname) free(pathname);
	if(data) free(data);
	return -1;
}

//READ FILE 4 ok
static int worker_readFile(int fd_c)
{
	char* pathname = NULL;
	int* buffer;
   	ec_null_c( (buffer = (int*)malloc(sizeof(int))), "readFile: malloc fallita", rf_clean);
	*buffer = 0;
	int ret = 0;

	//SETTING RICHIESTA
    //comunica: richiesta accettata
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readFile: write fallita", rf_clean);

	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	//IDENTIFICAZIONE PROCESSO CLIENT
	//riceve: pid
	int id;
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readFile: read fallita", rf_clean);
	id = *buffer;
	//comunica: recevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readFile: write fallita", rf_clean);

	//LEN PATHNAME
	size_t len_path;
	//riceve: lunghezza del pathname
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "appendToFile: read fallita", rf_clean);
	len_path = *buffer;
	//6 risponde: ricevuta
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readFile: write fallita", rf_clean);	//BUG HERE

	//PATHNAME
	//allocazione mem. pathname
	ec_null_c((pathname = calloc(sizeof(char), len_path)), "readFile: calloc fallita", rf_clean);
	memset((void*)pathname, '\0', sizeof(char)*len_path);
	// riceve: pathname
	ec_meno1_c(read(fd_c, pathname, sizeof(char)*len_path), "readFile: read fallita", rf_clean); //inserisci controllo esito read
	//comunica: ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readFile: write fallita", rf_clean);

	/////////////////////////////
	// ELABORAZIONE RICHIESTA //
	///////////////////////////
	
	//1) controllo apertura file
	file* f = NULL;
	if ((f=cache_research(cache, pathname)) != NULL){
		if (search_node(f->id_list, pathname) != NULL){  //controllo file aperto
			if (f->f_lock == 0 || f->f_lock != id){		//controllo lock
				errno = EPERM;
				ret = -1;
			}
		}
	}else{
		errno = ENOENT;
		ret = -1;
	}

	//ESITO
	//comunica: esito
	if(ret == -1) ret = errno;
	*buffer = ret;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readFile: write fallita", rf_clean);
	//23 (riceve): conferma ricezione
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readFile: read fallita", rf_clean);
	if (*buffer != 0){ goto rf_clean; }

	//se il file non esiste, finisce qui
	if(ret == -1){
		goto rf_clean;
	}

	//INVIO DEL FILE DA LEGGERE

	//FILE SIZE
	//comunica: file size
	*buffer = f->f_size;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readFile: write fallita", rf_clean);	
	//riceve: conferma
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readFile: read fallita", rf_clean); 
	if (*buffer != 0){ LOG_ERR(-1, "readFile: read non valida"); goto rf_clean; }

	//DATA FILE
	//comunica: data
	ec_meno1_c(write(fd_c, f->f_data, sizeof(char)*(f->f_size)), "readFile: write fallita", rf_clean);	
	//riceve: conferma
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readFile: read fallita", rf_clean); 
	if (*buffer != 0){ LOG_ERR(-1, "readFile: read non valida"); goto rf_clean; }

	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return 0;

	rf_clean:
	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return -1;

}

//READ N FILES 5
static int worker_readNFiles(int fd_c)
{
	int* buffer;
   	ec_null_c( (buffer = (int*)malloc(sizeof(int))), "readNFiles: malloc fallita", rnf_clean);
	*buffer = 0;
	//int ret; (!)

	//SETTING RICHIESTA
    //comunica: richiesta readNFiles (cod.5) accettata
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita", rnf_clean);

	// comunicazione stabilita
	// acquisizione dati richiesta dal client:
	
	//IDENTIFICAZIONE PROCESSO CLIENT
	//riceve: pid
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita", rnf_clean);
	int id = *buffer;
	//comunica: conferma recezione pid
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita", rnf_clean);

	
	//NUMERO DI FILE DA LEGGERE
	//riceve: numero di file da leggere
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita", rnf_clean);
	int N = *buffer;
	//comunica: conferma ricezione
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita", rnf_clean);

	//FASE 1 -> LOCK DI N FILE (nel caso ottimo)
	int count = N;
	int file_locked = 0;
	file* temp = cache;
	if(count == 0) count = -1; //leggi tutti i file del server
	while (temp != NULL && count != 0){
		if (cache_lockFile(cache, temp->f_name, id) == 0) file_locked++;
		//else se succede qualche errore in cache? errno? ret = errno;
		temp = temp->next;
		count--;
	}

	//file_locked rappresenta il numero di file che verranno letti
	//1. scandisce la lista
	//2. cerca file lockato prima
	//3. invia file al client
	temp = cache;
	int file_letti = 0;

	while(temp != NULL && file_locked > 0){
		//CONDIZIONE DI TERMINAZIONE
		if(temp->f_lock == id){ //se si tratta di uno dei file lockati prima
			//comunica: un file da inviare ora (stato di invio 1)
			*buffer = 1;
			ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita", rnf_clean);
			//riceve: conferma
			ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita", rnf_clean); //inserisci controllo esito read
			if (*buffer != 0){ goto rnf_clean; }

			//invio del file
			//LEN FILE NAME
			int len_name = strlen(temp->f_name);
			*buffer = len_name;
			//comunica:
			ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita", rnf_clean);	
			//riceve: conferma
			ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita", rnf_clean); //inserisci controllo esito read
			if (*buffer != 0){ goto rnf_clean; }

			//FILE NAME
			//comunica: 
			ec_meno1_c(write(fd_c, temp->f_name, sizeof(char)*len_name), "readFile: write fallita", rnf_clean);
			//riceve: conferma
			ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita", rnf_clean); //inserisci controllo esito read
			if (*buffer != 0){ goto rnf_clean; }

			//FILE SIZE
			//comunica: file size
			*buffer = temp->f_size;
			ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita", rnf_clean);
			//riceve: conferma
			ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita", rnf_clean); //inserisci controllo esito read
			if (*buffer != 0){goto rnf_clean; }

			//DATA FILE
			//comunica: data
			ec_meno1_c(write(fd_c, temp->f_data, sizeof(char)*(temp->f_size)), "readNFiles: write fallita", rnf_clean);	//BUG HERE
			//riceve: conferma
			ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita", rnf_clean);
			if (*buffer != 0){ goto rnf_clean; }
			printf("(SERVER) - readNFiles:	file_name:%s (len=%zu) letto e inviato\n", temp->f_name, strlen(temp->f_name)); //DEBUG

			file_locked--;
			file_letti++;
		}else{
			//comunica: nessun file da inviare ora (stato di invio 2)
			*buffer = 2;
			ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita", rnf_clean);
			//riceve: conferma ricezione stato di invio
			ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita", rnf_clean);
			if (*buffer != 0){ goto rnf_clean; }
		}
		//prossimo file
		temp=temp->next;
	}
	//comunica: read terminata
	*buffer = 3;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "readNFiles: write fallita", rnf_clean);
	//riceve: conferma ricezione stato di invio
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "readNFiles: read fallita", rnf_clean);
	if (*buffer != 0){ goto rnf_clean; }

	printf("(SERVER) - readNFiles: FILE LETTI = %d\n", file_letti); //DEBUG

	//COMUNICA ESITO 
	//NUMERO DI FILE LETTI O ERRNO O 0 (!)

	if(buffer) free(buffer);
	return 0;

	rnf_clean:
	if(buffer) free(buffer);
	return -1;
}

//LOCK FILE 6 
static int worker_lockFile(int fd_c)
{
	char* pathname = NULL;
	int* buffer;
   	ec_null_c( (buffer = (int*)malloc(sizeof(int))), "lockFile: malloc fallita", lf_clean);
	*buffer = 0;
	int len;
	int ret;

	//1 -> la prima read viene effettuata nella thread_func

	//SETTING RICHIESTA
    //(comunica): richiesta lockFile (cod.6) accettata
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "lockFile: write fallita", lf_clean);


	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:


	//LEN PATHNAME
	//(riceve): lunghezza del pathname
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "lockFile: read fallita", lf_clean);
	len = *buffer;
	//risponde: ricevuta
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "lockFile: write fallita", lf_clean);


	//PATHNAME
	//allocazione mem pathname
	ec_null_c((pathname = calloc(sizeof(char), len)), "lockFile: calloc fallita", lf_clean);
	pathname[len] = '\0';
	//riceve: pathname
	ec_meno1_c(read(fd_c, pathname, sizeof(char)*(len)), "lockFile: read fallita", lf_clean); 
	//10 (comunica): ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "lockFile: write fallita", lf_clean);


	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 (riceve): pid
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "lockFile: read fallita", lf_clean);
	int id = *buffer;
	//(comunica): recevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "lockFile: write fallita", lf_clean);


	//ELABORAZIONE RICHIESTA

	//controllo esistenza e apertura del file da lockare
	file* f = NULL;
	if( (f = cache_research(cache, pathname)) != NULL)
		ret = cache_lockFile(cache, pathname, id);
	else ret = -1;


	//ESITO
	//(comunica): esito
	if(ret != 0) ret = errno;
	*buffer = ret;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "lockFile: write fallita", lf_clean);
	//(riceve): conferma ricezione
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "lockFile: read fallita", lf_clean);
	if (*buffer != 0){ LOG_ERR(-1, "lockFile: read non valida"); goto lf_clean; } //NESSUN LOG QUI

	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return 0;
	
	lf_clean:
	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return -1;
}

//UNLOCK 7 
static int worker_unlockFile(int fd_c)
{
	char* pathname = NULL;
	int* buffer;
   	ec_null_c( (buffer = (int*)malloc(sizeof(int))), "unlockFile: malloc fallita", uf_clean);
	*buffer = 0;
	int ret;

	//1 -> la prima read viene effettuata nella thread_func

	//SETTING RICHIESTA
    //(comunica): richiesta lockFile (cod.6) accettata
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "unlockFile: write fallita", uf_clean);

	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	//LEN PATHNAME
	size_t len;
	//(riceve): lunghezza del pathname
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "unlockFile: read fallita", uf_clean);
	len = *buffer;
	//risponde: ricevuta
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "unlockFile: write fallita", uf_clean);


	//PATHNAME
	//allocazione mem pathname
	ec_null_c((pathname = calloc(sizeof(char), len)), "unlockFile: calloc fallita", uf_clean);
	pathname[len] = '\0';
	//riceve: pathname
	ec_meno1_c(read(fd_c, pathname, sizeof(char)*(len)), "unlockFile: read fallita", uf_clean); 
	//10 (comunica): ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "unlockFile: write fallita", uf_clean);


	//IDENTIFICAZIONE PROCESSO CLIENT
	//riceve: pid
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "unlockFile: read fallita", uf_clean);
	int id = *buffer;
	//comunica: recevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "unlockFile: write fallita", uf_clean);


	//ELABORAZIONE RICHIESTA
	ret = cache_unlockFile(cache, pathname, id);

	//ESITO
	//comunica: esito
	if(ret != 0) ret = errno;
	*buffer = ret;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "unlockFile: write fallita", uf_clean);
	//riceve: conferma ricezione esito
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "unlockFile: read fallita", uf_clean);
	if (*buffer != 0){ goto uf_clean; }

	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return 0;

	uf_clean:
	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return -1;
}

//REMOVE FILE 8
static int worker_removeFile(int fd_c)
{
	printf("removeFile\n");
	char* pathname = NULL;
	int* buffer;
   	ec_null_c( (buffer = (int*)malloc(sizeof(int))), "malloc in removeFile", rf_clean);
	*buffer = 0;
	int ret = 0;

	//SETTING RICHIESTA
    //comunica: richiesta accettata
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "write in removeFile", rf_clean);

	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	//LEN PATHNAME
	size_t len;
	//riceve: lunghezza del pathname
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "removeFile: read fallita", rf_clean);
	len = *buffer;
	//comunica: ricevuta lungh pathname
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "removeFile: write fallita", rf_clean);

	//PATHNAME
	//allocazione mem pathname
	ec_null_c((pathname = calloc(sizeof(char), len)), "calloc in removeFile", rf_clean);
	//riceve: pathname
	ec_meno1_c(read(fd_c, pathname, sizeof(char)*(len)), "removeFile: read fallita", rf_clean);
	pathname[len] = '\0';
	//comunica: ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "removeFile: write fallita", rf_clean);

	//IDENTIFICAZIONE PROCESSO CLIENT
	//riceve: pid
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "removeFile: read fallita", rf_clean);
	int id = *buffer;
	//comunica: recevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "removeFile: write fallita", rf_clean);

	//ELABORAZIONE RICHIESTA
	//controllo apertura file
	//(!) il controllo effettivo per vedere se un file è aperto da un processo è controllare la lista id_list 
	//	del nodo e vedere se è presente l'id atteso
	file* f = NULL;
	if ((f = cache_research(cache, pathname)) != NULL){
		if (f->f_lock != id){
			errno = EPERM;
			ret = -1;
		}else
			ret = cache_removeFile(&cache, pathname, id);
	}else{
		errno = EINVAL;
		ret = -1;
	}
	
	//ESITO
	//(comunica): esito
	if(ret != 0) ret = errno;
	*buffer = ret;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "removeFile: write fallita", rf_clean);
	//(riceve): conferma ricezione
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "removeFile: read fallita", rf_clean);
	if (*buffer != 0){ goto rf_clean; }

	printf("(SERVER) - removeFile  su %s / esito=%d\n", pathname, ret); //DEBUG

	if(buffer) free(buffer);
	if(pathname)free(pathname);
	return 0;

	rf_clean:
	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return -1;
}

//CLOSE FILE 9
static int worker_closeFile(int fd_c)
{
	char* pathname = NULL;
	int* buffer;
   	ec_null_c((buffer = (int*)malloc(sizeof(int))), "malloc in closeFile", cf_clean);
	*buffer = 0;
	int ret = 0;

	//SETTING RICHIESTA
    //(comunica): richiesta removeFile=9 accettata
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "write in closeFile", cf_clean);

	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client:

	//LEN PATHNAME
	//(riceve): lunghezza del pathname
	size_t len;
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "closeFile: read fallita", cf_clean);
	len = *buffer;
	//risponde: ricevuta
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "write in closeFile", cf_clean);	


	//PATHNAME
	//allocazione mem pathname
	ec_null_c((pathname = calloc(sizeof(char), len)), "calloc in closeFile", cf_clean);
	pathname[len] = '\0';
	//riceve: pathname
	ec_meno1_c(read(fd_c, pathname, sizeof(char)*(len)), "read in closeFile", cf_clean);
	//10 (comunica): ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "write in closeFile", cf_clean);


	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 (riceve): pid
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "read in closeFile", cf_clean);
	int id;
	id = *buffer;
	//(comunica): recevuto
	*buffer = 0;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "write in closeFile", cf_clean);

	//possibilita di aggiornare la LISTA DEI FILE APERTI DA QUI (!) ->
	//aggiornare la lista del file chiuso elminando l'id che sta chiudento il file
	//dalla lista dei processi che hanno aperto il file
	//la lista dei processi che hanno aperto il file è annessa alla struttura del file stesso
	//è una lista interi in cui si dovrà cercare il nodo corrispondente all'id del processo che effettua la closeFile ed eliminarlo

	printf("(SERVER) - closeFile:	su %s (len_path=%zu)\n", pathname, len);
	//ELABORAZIONE RICHIESTA
	file* f;
	if((f = cache_research(cache, pathname)) == NULL){
		errno = EINVAL;
		ret = -1;
	}else{
		char id_str[101];
		itoa(id, id_str);
		remove_node(&(f->id_list), id_str);
		if(f->id_list == NULL){
			f->f_open = 0;
		}
		//unlock del file se lockato da processo che richiede la chiusura
		cache_unlockFile(cache, pathname, id);
	}
	
	//ESITO
	//(comunica): esito
	if(ret != 0) ret = errno;
	*buffer = ret;
	ec_meno1_c(write(fd_c, buffer, sizeof(int)), "closeFile: write fallita", cf_clean);
	//(riceve): conferma ricezione
	ec_meno1_c(read(fd_c, buffer, sizeof(int)), "closeFile: read fallita", cf_clean);
	if (*buffer != 0){ goto cf_clean; } //controlla

	if(buffer) free(buffer);
	if(pathname) free(pathname);
	return ret;

	cf_clean:
	if(pathname) free(pathname);
	if(buffer) free(buffer);
	return -1;
}
