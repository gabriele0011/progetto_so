//1 terminare implementazione segnali
//2. implementazione cache e inserimento procedure in server-manager

#include "err_control.h"
#include "function_c.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <string.h>
#define UNIX_PATH_MAX 108


/*********** coda FIFO concorrente ***********/
//tipo nodo coda
typedef struct node {
	int data;
	struct node* next;
}t_queue;

//mutex
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

t_queue* enqueue(t_queue* head, int data);
t_queue* dequeue(t_queue* queue, int* data_eject);
int queueIsEmpty(t_queue* queue);

t_queue* enqueue(t_queue* queue, int data)
{
	//creazione nodo
	t_queue* new;
	ec_null((new = malloc(sizeof(t_queue))), "malloc enqueue fallita");
	new->data = data;

	//posizionamento nodo
	
	//caso 1 - coda vuota -> nuova testa = coda
	if (queue == NULL){
		if (pthread_mutex_lock(&mtx) != 0){ 
			LOG_ERR(errno, "lock fallita in enqueue");
			exit(EXIT_FAILURE);
		}
		new->next = NULL;
		queue = new;
		if (pthread_mutex_unlock(&mtx) != 0){
			LOG_ERR(errno, "unlock fallita in enqueue");
			exit(EXIT_FAILURE);
		}
	//caso 2 - nuova testa
	}else{
		if (pthread_mutex_lock(&mtx) != 0){
			LOG_ERR(errno, "lock fallita in enqueue");
			exit(EXIT_FAILURE);
		}
		new->next = queue;
		queue = new;
		if (pthread_mutex_unlock(&mtx) != 0){
			LOG_ERR(errno, "lock fallita in enqueue");
			exit(EXIT_FAILURE);
		};
	}	
	return queue;
}

t_queue* dequeue(t_queue* queue, int* data_eject)
{

	//caso 0: coda vuota
	if(queueIsEmpty(queue)) return NULL;


	t_queue* temp = queue;
	//caso 1: un elemento in coda -> testa = coda
	if(temp->next == NULL){
		if(pthread_mutex_lock(&mtx) != 0){
			LOG_ERR(errno, "lock fallita in dequeue");
			exit(EXIT_FAILURE);
		}
		*data_eject = queue->data;
		queue = NULL;
		free(temp);
		if(pthread_mutex_unlock(&mtx) != 0){
			LOG_ERR(errno, "lock fallita in dequeue");
			exit(EXIT_FAILURE);
		}
		return queue;
	}
	if(pthread_mutex_lock(&mtx) != 0){
		LOG_ERR(errno, "lock fallita in dequeue");
		exit(EXIT_FAILURE);
	}
	//caso 2: coda con n.elem > 1
	//mi posiziono sull'elemento prima delle coda
	while(temp->next->next != NULL){
		temp = temp->next;
	}
	t_queue* del = temp->next;
	temp->next = NULL;
	*data_eject = del->data;
	free(del);
	if(pthread_mutex_unlock(&mtx) != 0){
		LOG_ERR(errno, "unlock fallita in dequeue");
		exit(EXIT_FAILURE);
	}
	return queue;
}

int queueIsEmpty(t_queue* queue)
{
	if(queue == NULL) return 1;
	else return 0;
}

void printf_queue(t_queue* queue)
{
	while(queue != NULL){
		printf("%d ", queue->data);
		queue = queue->next;
	}
	printf("\n");
}	

//funzioni di deallocazione coda
void dealloc_queue(t_queue* queue)
{
	//coda vuota
	if(queue == NULL) return;
	//coda non vuota
	while(queue != NULL){
		t_queue* temp = queue;
		queue = queue->next;
		free(temp);
	}
}

//var. settate mediante file config.txt
int t_workers_num = 0;
int server_mem_size = 0;
int max_storage_file = 0;
char* socket_file_name = NULL;

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
		printf("n = %d\n", n);
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
	return 0;
}


//flags segnali di teminazione
volatile sig_atomic_t sig_intquit = 0;	//SIGINT/SIGQUIT: uscita immediata - non si accettano richieste - chiudere connessioni attive
volatile sig_atomic_t sig_hup = 0; 		//SIG_HUP: non si accettano nuove connessioni - si termina una volta concluse quelle attive

//handlers segnali di terminazione
static void handler_sigintquit(int signum){
	sig_intquit = 1;
}

static void handler_sighup(int signum){
	sig_hup = 1;
}

int x;
static void* start_func(void* arg){
	printf("ciao\n");
	while(1){ 
		//printf("x = %d\n\n", ++x); 
	}
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
	memset(&s, 0, sizeof(s));
	s.sa_handler = handler_sigintquit;
	ec_meno1(sigaction(SIGINT, &s, NULL), "server_manager: sigaction fallita");

	struct sigaction s1;
	memset(&s, 0, sizeof(s1));
	s1.sa_handler = handler_sigintquit;
	ec_meno1(sigaction(SIGQUIT, &s1, NULL), "server_manager: sigaction fallita");
	
	struct sigaction s2;
	memset(&s, 0, sizeof(s2));
	s2.sa_handler = handler_sighup;
	ec_meno1(sigaction(SIGQUIT, &s2, NULL), "server_manager: sigaction fallita");
	

	//CACHE
	//file* create_cache(server_mem_size, max_storage_file);

	//STRUTTURE DATI
	/*********** buf lettura ***********/
	int buf;

	/*********** coda FIFO concorrente ***********/
	t_queue* conc_queue = NULL;
	//if(conc_queue == NULL){ LOG_ERR(-1, "creazione coda conc. fallita"); exit(EXIT_FAILURE); }

	/*********** pipe senza nome ***********/
	int pfd[2];
	int fd_pipe_read = pfd[0];
	int fd_pipe_write = pfd[1];
	ec_meno1(pipe(pfd), "server_manager: creazione pipe fallita");

	/*********** thread pool ***********/
	pthread_t thread_workers_arr[t_workers_num];
	for(int i = 0; i < t_workers_num; i++){
		if ((err = pthread_create(&(thread_workers_arr[i]), NULL, &start_func, NULL)) != 0){    
			LOG_ERR(err, "server_manager: pthread_create faallita");
			exit(EXIT_FAILURE);
		}
	}

	/*********** listen socket ***********/
	struct sockaddr_un sa;
	strcpy(sa.sun_path, socket_file_name);
	sa.sun_family = AF_UNIX;
	printf("fino a qui tutto bene\n");
	//dichiarazioni fd
	int fd_skt, fd_c;	//fd socket e client
	int fd_num;		//num. max fd attivi
	fd_set set;		//insieme fd attivi
	fd_set rdset;		//insieme fd attesi in lettura
	printf("socket address: %s\n", socket_file_name);
	//creazione listen socket
	ec_meno1((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)), "server_manager: server socket() fallita");
	ec_meno1(bind(fd_skt, (struct sockaddr*)&sa, sizeof(sa)), "server_manager: bind() fallita");
	ec_meno1(listen(fd_skt, SOMAXCONN), "server_manager: listen() fallita");
	//aggiornamento fd_num
	if (fd_skt > fd_num) fd_num = fd_skt;
	//inizializzazione e aggiornamento maschera
	FD_ZERO(&set);		
	FD_SET(fd_skt, &set);

	printf("Server socket fd: %d\n", fd_skt);
    	printf("Pipe read fd: %d\n", fd_pipe_read);
    	printf("Pipe write fd: %d\n\n", fd_pipe_write);

   	int n_client_conn = 0;

	while (!sig_intquit){
		rdset = set;
		//intercetta fd pronti
		if (select(fd_num+1, &rdset, NULL, NULL, NULL) == -1){
			LOG_ERR(-1, "server_manager: select fallita");
			break;
		}else{
			//controlla fd intercettati da select
			for (int fd = 0; fd <= fd_num; fd++){
				if (FD_ISSET(fd, &rdset)){
					
					//caso 1: si tratta del fd_skt
					if (fd == fd_skt){ 
						//CONTROLLO SEGNALE sig_hup
						if (!sig_hup && n_client_conn != 0){
							//stabilisci connessione con un client
							if ((fd_c = accept(fd_skt, NULL, 0)) == -1){
								LOG_ERR(-1, "server_manager: accept fallita");
								break;
							} 
							FD_SET(fd_c, &set);
							if (fd_c > fd_num) fd_num = fd_c;
						}
					}else{ 
							
						//caso 2: si tratta del fd della pipe -> un thread ha un messaggio
						if (fd == fd_pipe_read){
							if(read(fd_pipe_read, &buf, sizeof(int)) == -1){
								LOG_ERR(err, "server_manager: file_manager: readn fallita");
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
							conc_queue = enqueue(conc_queue, fd);
							n_client_conn++;
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
      	}
	}

	//cleanup chiusura server
	if(conc_queue) dealloc_queue(conc_queue);
	close(fd_pipe_read);
	close(fd_pipe_write);
	close(fd_skt);
	exit(0);

	//cleanup_section
	cleanup_section:

	if(conc_queue) dealloc_queue(conc_queue);
	close(fd_pipe_read);
	close(fd_pipe_write);
	exit(EXIT_FAILURE);
	
	return 0;
}