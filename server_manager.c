// nel caso di cache insert si devono controllare i duplicati
// -> facciamo una ricerca preliminare pre inserimento per capire se il file è un duplicato
// sarà una delle condizioni preliminari di inserimento 

//cd /users/gabriele/desktop/progetto_sol/src

#include "server_manager.h"
//#define PIPE_MAX_LEN_MSG sizeof(int)
//#define O_CREATE 1
//#define O_LOCK 2
typedef enum {O_CREATE=1, O_LOCK=2} flags;

static int worker_openFile(int fd_c);
static int worker_writeFile(int x){return 0;}
static int worker_appendToFile(int x){return 0;}
//arrivare qui entro oggi

static int worker_closeFile(int x){return 0;}
static int worker_readFile(int x){return 0;}
static int worker_readNFiles(int x){return 0;}
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

//HANDLERS SEGNALI
static void handler_sigintquit(int signum){
	sig_intquit = 1;
}
static void handler_sighup(int signum){
	sig_hup = 1;
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
		int N;
		if ((N=read(fd_c, buf, sizeof(int))) == -1){
			LOG_ERR(errno, "start_func: read su fd_client fallita -> client disconnesso");
			printf("(SERVER) - start_func:  read fallita -> *buf = 0\n");
			*buf = 0;
		}
		op = *buf;
		printf("(SERVER) - start_func: fd_c= %d / op =%d / byte_read=%d\n", fd_c, op, N); //DEBUG DEBUG DEBUG
		
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
			printf("(SERVER) - start_func: op %d dal client %d terminata\n", op, fd_c); //DEBUG
		}
		//closeFile
		if( op == 2){
			if ( worker_closeFile(fd_c) == -1){ 
				LOG_ERR(errno, "start_func: (2) worker_closeFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "start_func: (2) write su pipe fallita");
			}
		}
		//writeFile
		if (op == 3){
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
		if (op == 4){
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
		if (op == 5){
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
		//if(buf) free(buf); //crea abort 
	}
	//pthread_exit((void*)0);
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
	ec_null((buf = malloc(sizeof(int))), "start_func: malloc fallita");
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
			break;
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
						if ((fd_c = accept(fd_skt, NULL, 0)) == -1){ LOG_ERR(errno, "server_manager: accept fallita"); break; }
						FD_SET(fd_c, &set);
						//si possono contare il numero di client connessi qui
						n_client_conn++; 
						printf("(SERVER) - mainloop:	n_client_conn++ = %d\n", n_client_conn);
						if (fd_c > fd_num) fd_num = fd_c;
                                   	printf("(SERVER) - mainloop:	accettata connessione (con fd = %d) fd_c = %d\n",fd, fd_c); //DEBUG
					}
				}else{
					//caso 2: si tratta del fd della pipe (I/O con client) -> un thread ha un messaggio
					if (fd == fd_pipe_read){
						//printf("(SERVER) - mainloop:	fd pipe =  %d\n", fd);//DEBUG
						*buf = 0;
						if (read(fd_pipe_read, buf, PIPE_MAX_LEN_MSG) == -1){ LOG_ERR(errno, "server_manager: read fallita"); break; }
						if(!buf || !(*buf)) { LOG_ERR(EPIPE, "server_manger: read non valida"); break; }
						// CASO 1: client disconnesso (read legge 0) o thread ritorna -fd
						if (*buf < 0){
							printf("(SERVER) - mainloop:	client %d disconnesso\n", *buf); //DEBUG
							*buf = (*buf)*(-1);
							FD_CLR(*buf, &set);
							close(*buf);
							n_client_conn--;
							printf("(SERVER) - mainloop:	n_client_conn-- = %d\n", n_client_conn);

						}else{
							// CASO 2: richiesta servita -> thread ritorna fd
							FD_SET(*buf, &set);
							printf("(SERVER) - mainloop:	richiesta fd=%d servita \n", *buf);
						}
					//caso 3: inserisce fd client (connesso)in coda concorrente -> nuova richiesta
					}else{
                                    //printf("sto servendo fd = %d\n", fd); //DEBUG
						//lock
						mutex_lock(&g_mtx, "server_manager: lock fallita");
						//inserisci richiesta in coda
						if(enqueue(&conc_queue, fd) == -1){ LOG_ERR(-1, "server_manager: enqueue fallita"); break; }
						//segnale
						if ((err = pthread_cond_signal(&cv)) == -1){ LOG_ERR(err, "server_manager: signal error"); break; }
						//printf("nuovo fd client (%d) da cui si attende richiesta inserito in coda\n",fd ); //DEBUG
						//unlock
						mutex_unlock(&g_mtx, "server_manager: unlock fallita");
						printf("(SERVER) - mainloop:	fd=%d in coda richiesta (espulso da RD_SET)\n\n", fd); //DEBUG
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

//SERVER OPEN FILE
static int worker_openFile(int fd_c)
{
	printf("\n\n"); //DEBUG
	printf("(SERVER) - openFile: INIZIO (fd_c =  %d)\n", fd_c); //DEBUG

	int* buf;
   	ec_null( (buf = (int*)malloc(sizeof(int))), "openFile: malloc fallita");
	*buf = 0;
	int len_pathname;
	char* pathname = NULL;

	//1 reaf -> la prima read viene effettuata nella start_func

	//SETTING RICHIESTA
    	//2 comunica: richiesta openFile (1) accettata (sono dove mi avevi detto di andare)
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 1 fallita");
	//printf("BUG HERE! write w/ *buf = %d / fd_c = %d\n", *buf, fd_c);


	// comunicazione stabilita OK
	// acquisizione dati richiesta dal client

	//LEN PATHNAME
	//5 riceve: lunghezza del pathname
	ec_meno1(read(fd_c, buf, sizeof(int)), "openFile: read fallita");
	len_pathname = *buf;
	//6 risponde: ricevuto lunghezza pathname
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 2 fallita");	//BUG HERE
	printf("(SERVER) - openFile:	len pathname ok (%d)\n", len_pathname); //DEBUG


	//PATHNAME
	//allocazione mem pathname
	ec_null((pathname = (char*)calloc(sizeof(char), len_pathname+1)), "openFile: calloc fallita");
	pathname[len_pathname+1] = '\0';
	//9 riceve: pathname
	read(fd_c, pathname, sizeof(char)*(len_pathname)); //inserisci controllo esito read
	//10 comunica: ricevuto pathname
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 3 fallita");
	printf("(SERVER) - openFile:	pathname ok (%s)\n", pathname); //DEBUG


	//FLAGS
	int flag;
	//13 riceve: flags
	ec_meno1(read(fd_c, buf, sizeof(int)), "openFile: read fallita");
	flag = *buf;
	//14 comunica recevuti flags
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 4 fallita");
	printf("(SERVER) - openFile:	flags ok ("); //DEBUG
	if(flag==(O_CREATE|O_LOCK)) printf("O_CREATE|O_LOCK)\n");
	if(flag==O_LOCK) printf("O_LOCK)\n");


	//IDENTIFICAZIONE PROCESSO CLIENT
	//17 riceve: pid
	ec_meno1(read(fd_c, buf, sizeof(int)), "openFile: read fallita");
	int id = *buf;
	//18 comunica: recevuto pid
	*buf = 0;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 5 fallita");
	printf("(SERVER) - openFile:	id ok\n"); //DEBUG

	//dati ricevuti 

	//ELABORAZIONE RICHIESTA
	file* f = NULL;
	file** file_expelled = NULL;
	size_t file_exist = 0;
	int ret_client = 0;

	//se il file f esiste setta var
	if((f = cache_research(cache, pathname)) != NULL){ file_exist = 1; printf("openFile: file cercato esistente\n"); }

	/*
	//casi di errore - eventualmente attivabili ma non necessari
	if( (flags==O_CREATE || flags==(O_CREATE|O_LOCK)) && file_exist ){
		//LOG_ERR(-1, "server.openFile: condizioni flag O_CREATE non rispettate");
		ret_client = -1;
	}
	if (flags==O_LOCK && !file_exist){
		//LOG_ERR(-1, "server.openFile: condizioni flag O_LOCK non rispettate");
		ret_client = -1;
	}
	*/
	if (flag==(O_CREATE|O_LOCK) && !file_exist){
            if (cache_insert(&cache, pathname, NULL, 0, id, file_expelled) != 0){ 
                  LOG_ERR(-1, "openFile: creazione file non riuscita");
                  ret_client = -1; //se l'inserimento non riesce fallisce tutto
            }
	}else{
		if (file_exist && flag==O_LOCK && (f->f_lock == 0 || f->f_lock == id ) ){
            	cache_lockFile(cache, pathname, id);
			f->f_write = 1;
			f->f_read = 1;
     		}else{
			ret_client = -1;
		}
	}
	//INVIO ESITO OPENFILE
	printf("(SERVER) - openFile:	ESITO FINALE = %d\n", ret_client);
	*buf = ret_client;
	ec_meno1(write(fd_c, buf, sizeof(int)), "openFile: write 6 fallita");
	
	//INVIO FILE ESPULSO EVENTUALMENTE ESPULSO
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
	}
	if(buf) free(buf);
	return 0;
}


