#include "err_cleanup.h"
#include "util.h"
#include <sys/socket.h>
#include <sys/un.h>

int err;

//var. settate mediante file config.txt (!) aggiorna
int t_workers_num = 0;
int server_mem_size = 0;
int server_file_size = 0;
char* socket_addr = NULL;

#define UNIX_PATH_MAX 108

//flags segnali di teminazione 
//SIGINT/SIGQUIT: uscita prima possibile. non si accettano richieste, chiudere connessioni attive
//SIG_HUP: non si accettano nuove connessioni e si termina una volta esaurite quelle attive
int flag_sig_intquit = 0;
int flag_sig_hup = 0;

//prototipi di funzione
void read_config_file(char* f_name);



int main(int argc, char* argv[])
{

	//controllo argomenti main
	if (argc <= 1){
		LOG_ERR(EINVAL, "file config.txt mancante");
		exit(EXIT_FAILURE);
	}
	
	//parsing del file config.txt
	if (read_config_file(argv[1]) != 0){
		LOG_ERR(-1, "lettura file configurazione fallita");
		exit(EXIT_FAILURE);
	}


	//STRUTTURE DATI

	//buffer di lettura -> che dimensione ha?
	int N = 256;
	char buf[N];

	//coda FIFO concorrente per comunicazione M -> Ws
	t_queue conc_queue;
	//if(conc_queue == NULL){ LOG_ERR(-1, "creazione coda conc. fallita"); exit(EXIT_FAILURE); }

	//pipe senza nome per comunicazione Ws -> M
	int pfd[2];
	int fd_pipe_read = pfd[0];
	int fd_pipe_write = pfd[1];
	ec_meno1(pipe(pfd), "creazione pipe fallita");

	//pool di thread Ws
	pthread_t thread_workers_arr[t_workers_num];
	for(int i = 0; i < t_workers_num; i++){
		if (err = pthread_create(&(thread_workers_arr[i]), NULL, worker_func, NULL) != 0){    //worker_func?
			LOG_ERR(err, "pthread_create fallita");
			exit(EXIT_FAILURE);
	}

	//listen socket
	struct sockaddr_un sa;
	strncpy(sa.sun_path, socket_addr, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	//dichiarazioni fd
	int fd_skt, fd_c;	//fd socket e client
	int fd_num;		//num. max fd attivi
	fd_set set;		//insieme fd attivi
	fd_set rdset;		//insieme fd attesi in lettura
	//creazione listen socket
	ec_meno1(fd_skt = socket(AF_UNIX, SOCK_STREAM, 0), "server socket() fallita");
	ec_meno1(bind(fd_skt, (struct sockaddr_un*)sa, sizeof(*sa)), "bind() fallita");
	ec_meno1(listen(fd_skt, SOMAXCONN), "listen() fallita");
	//aggiornamento fd_num
	if (fd_skt > fd_num) fd_num = fd_skt;
	//inizializzazione e aggiornamento maschera
	FD_ZERO(&set);		
	FD_SET(fd_skt, &set);


	//guardia del while da definire con controlli di terminazione signal
	while(1){
		rdset = set;
		if (select(fd_num+1, &rdset, NULL, NULL, NULL) == -1){
			LOG_ERR(-1, "select fallita");
			exit(EXIT_FAILURE);
		}else{
			//scorre tra gli fd da testare
			for (fd = 0; fd <= fd_num; fd++){
				if (FD_ISSET(fd, &rdset)){ //trovato un fd pronto
					
					//si tratta del fd_skt (socket connect)
					if (fd == fd_skt){ 
						//si, stabilisci connessione con client (sovrascrive fd_c)
						if ((fd_c = accept(fd_skt, NULL, 0)) == -1){
							LOG_ERR(-1, "accept fallita");
							exit(EXIT_FAILURE);
						} 
						fd_set(fd_c, &set);
						if (fd_c > fd_num) fd_num = fd_c;


					//se il fd pronto è un socket di I/O con client gia connesso
					}else{	
						nread = read(fd, buf, N);
						//EOF client
						if (nread == 0){	
							FD_CLR(fd, &set);
							fd_num = aggiorna(&set);
							close(fd);

						//socket di I/O attivo -> si puo comunicare	
						}else{
							printf("server got: %s\n", buf);
							write(fd, "bye\n", 5);
						}
					}
				}
			}

		}
	}

	return 0;
}


void read_config_file(char* f_name)
{
	//apertura del fine
	FILE* fd;
	ec_null((fd = fopen(f_name, "r")), "errore fopen in read_config_file");


    	//allocazione buf[size] per memorizzare le righe del file
    	int len = 256;
    	char s[len];

    	//acquisizione delle singole righe del file, mi aspetto in ordine:
    	//t_workers_num, server_mem_size, socket_file_name ,server_size_file;	
	while( (fgets(s, len, fd)) != NULL ){			
		
		char* token2 = NULL;
		char* token1 = strtok_r(s, ":", &token2);

		//string che prende token2 ovvero il valore da assegnare alle variabili
		int len_string = strlen(token2)-1; //si esclude '\n'
		char* string;
		string = calloc(sizeof(char), len_string);
		string[len_string] = '\0';
		strncpy(string, token2, len_string);
		
		//printf("string: %s len_string = %d\n", string, len_string);

		int len_t = strlen(token1);
		if (strncmp(token1, "server_file_size", len_t) == 0)
			ec_meno1((server_file_size = isNumber(string)), "parametro server_file_size config.txt non valido");

		if (strncmp(token1, "t_workers_num", len_t) == 0 )
			ec_meno1((t_workers_num = isNumber(string)), "parametro t_workers_num config.txt non valido");


		if (strncmp(token1, "server_mem_size", len_t) == 0)
			ec_meno1((server_mem_size = isNumber(string)), "parametro server_mem_size config.txt non valido");
		
		if (strncmp(token1, "socket_addr", len_t) == 0 ){
			socket_addr = calloc(sizeof(char), len_string);
			strncpy(socket_addr, string, len_string);
			socket_addr[len_string] = '\0';
		}
	}

	//chiusura del fd + controllo
	if(fclose(fd) != 0){
		LOG_ERR(errno, "errore fclose in read_config_file");
		exit(EXIT_FAILURE);
	}
	return 0;
}
