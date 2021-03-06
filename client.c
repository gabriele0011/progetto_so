
/*
-> implementare lista dei file aperti e procedura di chiusura quando il client si disconnette
-> timespec da implementare nelle richieste
*/

//#include "client.h"
#include "error_ctrl.h"
#include "aux_function.h"
#include "list.h"
typedef unsigned char byte;
enum { NS_PER_SECOND = 1000000000 };
typedef enum { O_CREATE = 1, O_LOCK = 2 }flags;
#define LOCK_TIMER 300

//variabili globali
static char* arg_D = NULL;		//dirname in cui scrivere i file rimossi dal server
static char* arg_d = NULL;		//dirname in cui scrivere i file letti dal server
static byte flag_p = 0;			//abilita stampe sullo std output per ogni operazione
static byte flag_h = 0;			//-h attivo 
static unsigned sleep_time = 0; 	
static int fd_sk; // = -1;
static char *socket_name = NULL;	
static char* path_dir = NULL;		//in caso di scritture (-w dirname), contiene il percorso del file ricavato in send_from_directory
node* open_files_list = NULL;

//prototipi client
static void parser(int dim, char** array);
static void help_message();
static void set_socket(const char* socket_name);
static void send_from_dir(const char* dirname, int* n);
static int append_request(const char* s);
static int write_request(char* arg_W);
static void case_write_w(char* arg_w);
static void case_write_W (char* arg_W);
static void lock_request(char* arg_l);
static void unlock_request(char* arg_u);
static void remove_request(char* arg_c);
static void read_request(char* arg_r);

//prototipi API client
int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);
int openFile(const char* sockaname, int flags);
int writeFile(const char* pathname, char* dirname);
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);
int readFile(const char* pathname, void** buf, size_t* size);
int readNFiles(int n, const char* dirname);
int lockFile(const char* pathname);
int unlockFile(const char* pathname);
int removeFile(const char* pathname);
int closeFile(const char* pathname);


//scritture
static void case_write_W (char* arg_W)
{
	//printf("\n\n"); //DEBUG
	//printf("(CLIENT) - case_write_write_W\n"); //DEBUG
	char* save = NULL;
	char* token = strtok_r(arg_W, ",", &save);
	while (token){
		printf("(CLIENT) - case_write_write_W:	richiesta di scrittura del file %s\n", token); //DEBUG
		if(write_request(token) == -1){
			LOG_ERR(errno, "scrittura fallita\n");
		}
		token = strtok_r(NULL, ",", &save);
	}
	if(closeConnection(socket_name) == -1){
		LOG_ERR(errno, "case_write_write_W: closeConnection fallita\n");
	}else{
		printf("(CLIENT) - case_write_write_W: connessione con server chiusa\n"); //DEBUG
	}
}
static void case_write_w(char* arg_w)
{
		char* dirname; //in seguito qui salvo la dir estratta da arg_w
		//verifica arg opzionale n>0 || n=0 || n = NULL
		char* n = NULL;
		int x;			
		//strtok_r restituisce ind. primo token appena dopo il primo delimitatore
		strtok_r(arg_w, ",", &n);
		//se n non ?? specificata -> x = -1
		if (n == NULL){
			x = -1;
			dirname = NULL;
		}else{
			dirname = strtok(arg_w, ",");
			//controllo che sia un intero e che sia positivo
			if ((x = is_number(n)) == -1){ LOG_ERR(EINVAL, "case_write_w: arg. -w non intero\n"); return; } //in caso di ERRORE?
			if (x < 0){ LOG_ERR(EINVAL, "arg -w optzionale non valido"); return; }			
			//se uguale a zero come non fosse presente -> x = -1
			if (x == 0) x = -1;
		}
		send_from_dir(dirname, &x);
}
static void send_from_dir(const char* dirname, int* n)
{
	//passo 1: apertura directory dirname
	DIR* d;
	ec_null((d = opendir(dirname)), "errore su opendir");
	printf("dir '%s' aperta\n", dirname);

	//passo 2: lettura della directory
	struct dirent* entry;
	while (errno == 0 && ((entry = readdir(d)) != NULL) && *n != 0) {
		//passo 3 : aggiornamento path
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);
		//passo 4 caso 1: controlla che se si tratta di una dir
		if (is_directory(path)){	
			//controlla che non si tratti della dir . o ..
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
				//visita ricorsiva su nuova directory
				send_from_dir(path, n);
		//passo 4 caso 2: richiesta di scrittura
		}else{
			path_dir = path;
			if(strcmp(entry->d_name, ".DS_Store") != 0){ //TERMINALE MACOS
			//printf("(CLIENT) - case_write_w.send_from_dir: richieta di scrittura per %s (path_dir: %s)\n", entry->d_name, path_dir); //DEBUG
			if (write_request(entry->d_name) == -1){  // (!) togliere strcmp errore terminale macos
				LOG_ERR(errno, "errore write_request");
			}
			(*n)--;
			}//TERMINALE MACOS
		}
	}
	//chiudere la directory
	ec_meno1(closedir(d), "closedir");
	//return 0;
}
static int write_request(char* f_name)
{	
	// (!) gestione accurata degli errori
	printf("\n\n"); //DEBUG
	//writeFile
	printf("(CLIENT) - write_request\n"); //DEBUG
	if (openFile(f_name, (O_CREATE|O_LOCK)) != -1 ){
            printf("(CLIENT) - writeFile in corso... \n"); //DEBUG
		writeFile(f_name, arg_d);
		if(errno == ENOENT) removeFile(f_name);
		else insert_node(&open_files_list, f_name);
	}else{
		//appendToFile
		if (openFile(f_name, 0) != -1 ){
			printf("(CLIENT) - appendToFile in corso...\n"); //DEBUG
	      	append_request(f_name);
			insert_node(&open_files_list, f_name);
		}else{
			printf("(CLIENT) - write_request: fallita\n"); //DEBUG
			return -1;
		}
	}
	printf("(CLIENT) - write_request:	funzione terminata con successo\n"); //DEBUG
	return 0;
}
static int append_request(const char* f_name)
{
	//apertura del file
	int fd;
	if(path_dir != NULL){ ec_meno1((fd = open(path_dir, O_RDONLY)), "open in append_request"); }
	else{ ec_meno1((fd = open(f_name, O_RDONLY)), "open in append_request"); }

	//stat per ricavare dimensione file
	struct stat path_stat;
	if(path_dir != NULL){ ec_meno1(lstat(path_dir, &path_stat), "append_request"); }
	else{ ec_meno1(lstat(f_name, &path_stat), "append_request"); }

    	//allocazione buf[size]
    	size_t file_size = path_stat.st_size;
    	char* buf;
 	ec_null((buf = calloc(file_size, sizeof(char))) , "append_request");

 	//lettura file + scrittura del buf
 	ec_meno1(read(fd, buf, file_size), "append_request");
 	ec_meno1(close(fd), "append_request");

 	//appendToFile per scrittura f_name atomica
 	if( appendToFile(f_name, buf, file_size, arg_d) == -1) LOG_ERR(errno, "appendToFile");
    	path_dir = NULL;

 	//cleanup -> con goto? ERRORE?
 	if(!buf) free(buf);
 	return 0;
}

static void writefile_in_dir(char* pathname, size_t size_file, char* data)
{
	//passo 1: apertura directory dirname
	DIR* d;
	ec_null((d = opendir(arg_d)), "writefile_in_dir: errore su opendir");
	printf("dir '%s' aperta\n", arg_d);

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", arg_d, pathname);
	//printf("path: %s\n", path);

	//crea nuovo file in directory e scrivi
	FILE* ifp;
	ifp = fopen(path, "wr");
	if (fwrite( data, sizeof(char), size_file, ifp) == -1){
		LOG_ERR(-1, "writefile_in_dir: scrittura fallita");
		exit(EXIT_FAILURE);
	}
	printf("writefile_in_dir: scrittura di %s avvenuta in %s\n", pathname, arg_d);
	fclose(ifp);
	ec_meno1(closedir(d), "writefile_in_dir: errore su closedir");
}
//read
static void read_request (char* arg_r)
{
	//settare errno
	char* save = NULL;
	char* token = strtok_r(arg_r, ",", &save);
	while (token){
		//su ogni toker che rappresenta il nome del file invio una richiesta di lettura
		void **buf;
		size_t* size;
		if(openFile(token, 0) != -1){
			if(readFile(token, buf, size) == -1)
				LOG_ERR(errno, "readFile: lettura impossibile");
			insert_node(&open_files_list, token);
		}else{
			LOG_ERR(errno, "openFile:");
		}
		token = strtok_r(NULL, ",", &save);
	}
}
//lock/unlock
static void lock_request(char* arg_l)
{
	char* save = NULL;
	char* token = strtok_r(arg_l, ",", &save);
	while (token){
		//su ogni token che rappresenta il nome del file invio una richiesta di lock sul file
		printf("richiesta di lock su: %s\n", token);
		if(openFile(token, 0) != -1){
			if (lockFile(token) == -1){
				LOG_ERR(errno, "lockFile:");
			}
			insert_node(&open_files_list, token);
		}else{
			LOG_ERR(errno, "openFile:");
		}
		token = strtok_r(NULL, ",", &save);
	}
}
static void unlock_request(char* arg_u)
{
	char* save = NULL;
	char* token = strtok_r(arg_u, ",", &save);
	while (token){
		//su ogni token che rappresenta il nome del file invio una richiesta di lock sul file
		printf("richiesta di unlock su: %s\n", token);
		//chiamata;
		token = strtok_r(NULL, ",", &save);
	}
}
//rimozione file
static void remove_request(char* arg_c)
{
	//setta errno
	char* save = NULL;
	char* token = strtok_r(arg_c, ",", &save);
	while (token){
		//su ogni token che rappresenta il nome del file invio una richiesta di lock sul file
		printf("richiesta di cancellazione su: %s\n", token);
		if(openFile(token, 0) != -1){
			if (removeFile(token) == -1){ LOG_ERR(errno, "removeFile:");}
			else{ insert_node(&open_files_list, token);}
		}else{
			LOG_ERR(errno, "openFile:");
		}
		token = strtok_r(NULL, ",", &save);
	}
}
//routine chiusura file aperti
void file_closing(node* open_files_list)
{
	while(open_files_list != NULL){
		closeFile(open_files_list->file_name);
		open_files_list = open_files_list->next;
	}
}
//connessione
static void set_socket(const char* socket_name)
{
	//necessario effettuare un controllo sulla stringa socket_name?
	//deve avere una dimensione specifica?

	//setting parametri di connessione
	double rec_time = 1000; //tempo di riconnessione ogni 1 sec = 1/1000 msec
	struct timespec abstime;
	abstime.tv_sec = 10; //tempo assoluto 10 secondi

	//open connection
   	if(openConnection(socket_name, rec_time, abstime) == -1){
    		LOG_ERR(errno, "set_socket: tentativo di connessione non riuscito");
    		exit(EXIT_FAILURE);
    	}
}
//help
static void help_message()
{
	int fd;
	ec_meno1((fd = open("help.txt", O_RDONLY)), "errore f. open in help_message");
	
	//struttura per acquisire informazioni sul file
	struct stat path_stat;
	ec_meno1(lstat("help.txt", &path_stat), "errore stat");

    	//allocazione buf[size] per la dimensione del file in byte
    	size_t file_size = path_stat.st_size;
   	char* buf;
 	ec_null((buf = calloc(file_size, sizeof(char))) , "malloc in append_request");
	
 	//lettura file + scrittura del buf
 	ec_meno1(read(fd, buf, file_size), "read");
 	
	//stampa buf
 	for(int i = 0; i < file_size; i++) printf("%c", buf[i]);
	//DEBUG: carattere di terminazione presente nel file txt?

 	ec_meno1(close(fd), "errore close in append_request");
 	if(!buf) free(buf);
}
//parser
static void parser(int dim, char** array)
{
	byte flag_wW = 0;			//flag di controllo scritture (extra)
	byte flag_rR = 0;			//flag controllo letture (extra)
	
	//ciclo preliminare per controllare se presente -h e terminare immediatamente 
	int i = 0;
	while (++i < dim){
		//CASO -h
		if (is_opt(array[i], "-h")) {
			help_message();
			goto c_clean;
		}
	}

	//CICLO 1: si gestiscono i comandi di setting del server -f, -t, -p, -d, -D
	i = 0;
	while (++i < dim){
		//CASO -f
		if (is_opt(array[i], "-f")) {
			//argomento obbligatorio
			if (is_argument(array[i+1])){ 
				socket_name = array[++i];
				set_socket(socket_name);
			}else{ 
				LOG_ERR(EINVAL, "client: argomento -f mancante");
				goto c_clean; 
			}
		}
		//CASO -t
		if (is_opt(array[i], "-t")) {
			if ( !is_argument(array[i+1]) ) sleep_time = 0;
			else sleep_time = is_number(array[++i]);
			printf("DEBUG: time_r = %d\n", sleep_time); //DEBUG
		}	
		//CASO -p
		if (is_opt(array[i], "-p")){
			flag_p = 1;
			i++;
		}
		//CASO -D
		if (is_opt(array[i], "-D")) {
			if (is_argument(array[i+1]))
				arg_D = array[++i];
		}
		//CASO -d 
		if (is_opt(array[i], "-d")) {
			if (is_argument(array[i+1]))
				arg_d = array[++i];
		}
		//controllo dipendenza -d (da -w/-W) e -D da (-r/-R)
		if (is_opt(array[i], "-w") || is_opt(array[i], "-W")) flag_wW = 1;
		if (is_opt(array[i], "-r") || is_opt(array[i], "-R")) flag_rR = 1;
	}

	//controllo dipendenze
	if(arg_D!=NULL && !flag_wW){ LOG_ERR(EINVAL, "-D attivo, -w/-W non attivi"); goto c_clean; }
	if(arg_d!=NULL && !flag_rR){ LOG_ERR(EINVAL, "-d attivo, -r/-R non attivi"); goto c_clean; }

	//CICLO 2: si gestiscono i comandi di richiesta al server -w, -W, -R, -r, -l, -u, -c
	i = 0;
	while (++i < dim){	
		//CASO -w
		if (is_opt(array[i], "-w")) {		
			if (is_argument(array[i+1])) {
				case_write_w(array[++i]);
			}else{ 
				LOG_ERR(EINVAL, "argomento mancante -w");
				goto c_clean;
			}	
		}		
		//CASO -W
		if (is_opt(array[i], "-W")) {	
			if (is_argument(array[i+1])){
				case_write_W(array[++i]);
			}else{
				LOG_ERR(EINVAL, "argomento -W mancante");
				goto c_clean;
			}
		}		
		//CASO -r
		if (is_opt(array[i], "-r")) {
			if (is_argument(array[i+1])) {
				//richiesta di lettura al server per ogni file
				read_request(array[++i]);
			}else{
				LOG_ERR(EINVAL, "argomento -r mancante");
				goto c_clean;
			}
		}
		//CASO -R
		if (is_opt(array[i], "-R")){
			//caso in cui ?? presente n
			if (is_argument(array[i+1])) {
				size_t x;
				if ((x = is_number(array[++i])) == -1){
					LOG_ERR(EINVAL, "-R")
					goto c_clean;
				}
				if (x == 0) readNFiles(-1, arg_d);
				else readNFiles(x, arg_d);
			//caso in cui n non ?? presente
			}else readNFiles(-1, arg_d);

		}
		//CASO -l
		if (is_opt(array[i], "-l")){
			if (is_argument(array[i+1])){
				lock_request(array[++i]);
			}else{
				LOG_ERR(EINVAL, "argomento -l mancante");
				goto c_clean;
			}
		}
		//CASO -u
		if (is_opt(array[i], "-u")){
			if (is_argument(array[i+1])){
				unlock_request(array[++i]);
			}else{
				LOG_ERR(EINVAL, "argomento -u mancante");
				goto c_clean;
			}
		}
		//CASO -c
		if (is_opt(array[i], "-c")){
			if (is_argument(array[i+1])){
				remove_request(array[++i]);
			}else{
				LOG_ERR(EINVAL, "argomento -c mancante");
				goto c_clean;
			}
		}
		//tempo di attesa tra una richiesta e l'altra
		if(sleep_time != 0) {
			sleep_time = sleep_time/1000;  //conversione da millisecondi a microsecondi
			usleep(sleep_time);
		}
	}
	closeConnection(socket_name);
	return;
	
	c_clean:
	closeConnection(socket_name);
	return;
}

// MAIN
int main(int argc, char* argv[]){
	
	//controllo su argc
	if(argc < 1){
		LOG_ERR(EINVAL, "main\n");
		exit(EXIT_FAILURE);
	}
	//parser
	parser(argc, argv);
	
	//routine di chiusura dei file aperti
	file_closing(open_files_list);
	dealloc_list(&open_files_list);
	return 0;
}




//////////////////////////////////////  API CLIENT  //////////////////////////////////////
//funzioni aus.
void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td)
{
    td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_nsec < 0)
    {
        td->tv_nsec += NS_PER_SECOND;
        td->tv_sec--;
    }
    else if (td->tv_sec < 0 && td->tv_nsec > 0)
    {
        td->tv_nsec -= NS_PER_SECOND;
        td->tv_sec++;
    }
}

//OPEN CONNECTION
int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	printf("\n\n"); //DEBUG
	//controllo validit?? socket_name
	if(socket_name == NULL) errno=ENOTSOCK; return -1;

	//timer setting
	msec = msec/1000; //conversione da millisecondi a microsecondi per la usleep
	struct timespec time1, time2, delta;
      ec_meno1(clock_gettime(CLOCK_REALTIME, &time1), "openConnection");

       //socket setting
      struct sockaddr_un sa;
	strncpy(sa.sun_path, sockname, strlen(socket_name));
	sa.sun_family = AF_UNIX;
    
	if( (fd_sk = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) return -1;


	//ciclo di connessione
      while(connect(fd_sk, (struct sockaddr*)&sa, sizeof(sa)) == -1){
		//printf("DEBU DEBUG DEBUG sono qui\n");
    	      if(errno == ENOENT){
			usleep(msec);
			//seconda misurazione
			ec_meno1(clock_gettime(CLOCK_REALTIME, &time2), "openConnection");
			//calcola tempo trascorso tra time1 e time2 in delta
			sub_timespec(time1, time2, &delta);
			//controllo che non si sia superato il tempo assoluto dal primo tentativo di connessione
			if( (delta.tv_nsec >= abstime.tv_nsec && delta.tv_sec >= abstime.tv_sec) || delta.tv_sec >= abstime.tv_sec){
				LOG_ERR(ETIME, "openConnection");
				return -1;
			}
       	}else{ 
			return -1;
		}
      }
	if(fd_sk != -1) printf("openConnection: connessione con server stabilita\n\n");;
      return 0;
}

//CLOSE CONNECTION 
int closeConnection(const char* sockname) 
{

	if(!sockname) {
		errno=EINVAL; 
	}
	/*
    	errno=0;
    	if (strcmp(sockname, fd_sk) != 0) {     
      	errno=EINVAL;       			
    	}
	*/
    	if (close(fd_sk) == -1 ){
		LOG_ERR(errno, "client: close fallita");
		return -1;  
	}     			
	/*
   	if(fd_sk) {
		free(fd_sk); 
		fd_sk = NULL;
	}
*/
    	return 0;
}

//OPEN FILE 1
int openFile(const char* pathname, int flag)
{
      //protocollo: C/1(invio dato) -> S/2(ricezione dato) -> S/3 (invio conferma ric. dato) -> C/4(ric. conf. ric dato)
	//printf("\n\n"); //DEBUG
	printf("(CLIENT) - openFile: su %s - %s\n", pathname, path_dir); //DEBUG

	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "openFile: malloc fallita");
	*buffer = 0;

	//SETTING RICHIESTA
      //0 comunica al thread 1 (codifica openFile=1)
	*buffer = 1;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "openFile: write 1 fallita");
      //3 riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "openFile: read non valida") goto of_clean; }

      // comunicazione stabilita OK
      // invio dati richiesta al server
      
      //LEN PATHNAME
	//4 comunica: invia lunghezza pathname
	int len = strlen(pathname);
	*buffer = len;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "openFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "openFile: read non valida"); goto of_clean; }

	//PATHANAME
	//8 comunica: pathname
	ec_meno1(write(fd_sk, pathname, sizeof(char)*len), "openFile: write 3 fallita");
	//11 riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 3 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "openFile: read non valida"); goto of_clean; }


      //FLAGS
      //12 comunica: flags
	*buffer = flag;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "openFile: write 4 fallita");
	//15 riceve: conferma ricezione flags
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 4 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "openFile: read non valida"); goto of_clean; }


      //IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //16 comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "openFile: write 5 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 5 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "openFile: read non valida"); goto of_clean; }

	printf("op_data_receive: %d / %s / flags / %d\n", len, pathname, id); //DEBUG
	//dati inviati al server
	
	//RICEZIONE ESITO OPENFILE
	//21 (riceve): esito openFile
	int ret;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 6 fallita");
	ret = *buffer;
	printf("(CLIENT) - openFile:		ESITO = %d\n", ret); //DEBUG
	//22 (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "openFile: write 6 fallita");
	

	//RICEZIONE DEL FILE EVENTUALMENTE ESPULSO
	//legge 1 se c'?? un file espluso, 0 altrimenti
	*buffer = 0;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "client: read 7 fallita");
	//printf("(CLIENT) - openFile:	file espulso = %d\n", *buf);
	if(*buffer == 1){
		//len pathname
		len = 0;
		ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 8 fallita");
		len = *buffer;
		//pathname
		char path[++len];
		ec_meno1(read(fd_sk, buffer, sizeof(char)*len), "openFile: read 9 fallita");
		path[len+1] = '\0';
		//size data
		size_t size_data;
		ec_meno1(read(fd_sk, buffer, sizeof(char)*len), "openFile: read 10 fallita");
		size_data = *buffer;
		//data
		char data[size_data];
		ec_meno1(read(fd_sk, data, sizeof(char)*size_data), "openFile: read 11 fallita");
		//scrittura file espluso nella directory arg_D
		if(arg_D != NULL)
			writefile_in_dir(path, size_data, data);
		//printf("(CLIENT) - openFile: file espluso = %s\n", pathname);
	}
	
	if(buffer) free(buffer);
	return ret;
	
	of_clean:
	if(buffer) free(buffer);
	return -1;
}
 
//WRITE FILE 2
int writeFile(const char* pathname, char* dirname)
{
	printf("(CLIENT) - writeFile: su: %s\n", pathname);
	//definizioni
	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "openFile: malloc fallita");
	*buffer = 0;
	char* data_file = NULL;
	
	
	//APERTURA E LETTURA FILE
	int fd;
	if (path_dir != NULL){ 
		if ((fd = open(path_dir, O_RDONLY) == -1)){ LOG_ERR(errno, "writeFile"); goto wf_clean; }
	}else{ 
		if((fd = open(pathname, O_RDONLY)) == -1){ LOG_ERR(errno, "writeFile"); goto wf_clean; }
	}
	//struttura per acquisire informazioni sul file
	struct stat path_stat;
	if (path_dir != NULL){ 
		if (lstat(path_dir, &path_stat) == -1){ LOG_ERR(errno, "lstat"); goto wf_clean; }
	}else{ 
		if (lstat(pathname, &path_stat) == -1){ LOG_ERR(errno, "lstat"); goto wf_clean; }
	}

    	//allocazione buf[size] per la dimensione del file in byte
    	size_t file_size = path_stat.st_size;
 	ec_null((data_file = calloc(file_size, sizeof(char))) , "writeFile: malloc fallita");
 	//lettura file + scrittura del buf
 	ec_meno1(read(fd, data_file, file_size), "writeFile: read fallita");
 	ec_meno1(close(fd), "writeFile: close fallita");
	path_dir = NULL;
	///////////////////////////////////////////////////////////////////////////////



	//SETTING RICHIESTA
      //0 comunica al thread 2 (codifica writeFile=2)
	*buffer = 2;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "writeFile: write 1 fallita");
      //3 riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "writeFile: read non valida") goto wf_clean; }
	printf("writeFile: setting richiesta OK\n");

      // comunicazione stabilita OK
      // invio dati richiesta al server 
      

      //LEN PATHNAME
	//4 comunica: invia lunghezza pathname
	int len = strlen(pathname);
	*buffer = len;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "writeFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "writeFile: read non valida"); goto wf_clean; }
	printf("writeFile: len pathname OK\n");

	//PATHANAME
	//8 comunica: pathname
	ec_meno1(write(fd_sk, pathname, sizeof(char)*len), "writeFile: write 3 fallita");
	//11 riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 3 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "writeFile: read non valida"); goto wf_clean; }
	printf("writeFile: pathname OK\n");

	//DIMENSIONE DEL FILE
	*buffer = file_size;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "writeFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "writeFile: read non valida"); goto wf_clean; }
	printf("writeFile: file_size OK\n");

	//DATA FILE
	ec_meno1(write(fd_sk, data_file, sizeof(char)*file_size), "writeFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "writeFile: read non valida"); goto wf_clean; }
	printf("writeFile: data file OK\n");


      //IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "writeFile: write 5 fallita");
      //riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 5 fallita");
	if(*buffer != 0){ LOG_ERR(EBADMSG, "writeFile: read non valida"); goto wf_clean; }
	printf("writeFile: pid OK\n");

	
	
	//printf("op_data_receive: %d / %s / flags / %d\n", len, pathname, id); //DEBUG
	//dati inviati al server
	///////////////////////////////////////////////////////////////////////////////
	

	//RICEZIONE ESITO WRITE FILE 
	//21 (riceve): esito openFile
	int ret;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 6 fallita");
	ret = *buffer;
	printf("(CLIENT) - writeFile:	ESITO = %d\n", ret); //DEBUG
	//22 (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "writeFile: write 6 fallita");


	//RICEZIONE DEL FILE EVENTUALMENTE ESPULSO
	//legge 1 se c'?? un file espluso, 0 altrimenti
	*buffer = 0;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 7 fallita");
	//printf("(CLIENT) - openFile:	file espulso = %d\n", *buf);
	
	if(*buffer == 1){
		//len pathname
		len = 0;
		ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 8 fallita");
		len = *buffer;
		
		//pathname
		char path[++len];
		ec_meno1(read(fd_sk, buffer, sizeof(char)*len), "writeFile: read 9 fallita");
		path[len+1] = '\0';
		
		//size data
		size_t size_data;
		ec_meno1(read(fd_sk, buffer, sizeof(char)*len), "writeFile: read 10 fallita");
		size_data = *buffer;
		
		//data
		char *arr_data = NULL;
		ec_null((arr_data = (char*)calloc(sizeof(char), size_data)), "writeFile: calloc fallita");
		ec_meno1(read(fd_sk, arr_data, sizeof(char)*size_data), "writeFile: read 11 fallita");
		
		//scrittura file espluso nella directory arg_D
		if(dirname != NULL)
			writefile_in_dir(path, size_data, arr_data);
		//printf("(CLIENT) - openFile: file espluso = %s\n", pathname);
		if(arr_data) free(arr_data);
	}
	
	if(buffer) free(buffer);
	if(data_file) free(data_file);
	return ret;
	
	wf_clean:
	if(buffer) free(buffer);
	if(data_file) free(data_file);
	return -1;
}

//APPEND TO FILE 3
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
	printf("(CLIENT) - appendToFile: su: %s\n", pathname);
	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "appendToFile: malloc fallita");
	*buffer = 0;

	//SETTING RICHIESTA
      //0 comunica al thread 3 (codifica appendToFile=2)
	*buffer = 3;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "appendToFile: write 1 fallita");
      //3 riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "appendToFile: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "appendToFile: read non valida") goto atf_clean; }


      // comunicazione stabilita OK
      // invio dati richiesta al server 
      

      //LEN PATHNAME
	//4 comunica: invia lunghezza pathname
	int len = strlen(pathname);
	*buffer = len;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "appendToFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "appendToFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "appendToFile: read non valida"); goto atf_clean; }


	//PATHANAME
	//8 comunica: pathname
	ec_meno1(write(fd_sk, pathname, sizeof(char)*len), "appendToFile: write 3 fallita");
	//11 riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "appendToFile: read 3 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "appendToFile: read non valida"); goto atf_clean; }


	//DIMENSIONE DEL FILE
	*buffer = size;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "appendToFile: write 4 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "appendToFile: read 4 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "appendToFile: read non valida"); goto atf_clean; }


	//DATA FILE
	ec_meno1(write(fd_sk, (char*)buf, sizeof(char)*size), "appendToFile: write 5 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "appendToFile: read 5 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "appendToFile: read non valida"); goto atf_clean; }


      //IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //16 comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "appendToFile: write 6 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "appendToFile: read 6 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "appendToFile: read non valida"); goto atf_clean; }

	
	//printf("op_data_receive: %d / %s / flags / %d\n", len, pathname, id); //DEBUG
	//dati inviati al server
	///////////////////////////////////////////////////////////////////////////////
	
	
	//RICEZIONE ESITO appendToFile
	//21 (riceve): esito openFile
	int ret;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "appendToFile: read 7 fallita");
	ret = *buffer;
	printf("(CLIENT) - appendToFile:	ESITO = %d\n", ret); //DEBUG
	//22 (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "appendToFile: write 7 fallita");


	//RICEZIONE DEL FILE EVENTUALMENTE ESPULSO
	//legge 1 se c'?? un file espluso, 0 altrimenti
	*buffer = 0;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "appendToFile: read 8 fallita");
	//printf("(CLIENT) - openFile:	file espulso = %d\n", *buf);
	char* arr_data = NULL;
	if(*buffer == 1){
		//len pathname
		int len_path;
		ec_meno1(read(fd_sk, buffer, sizeof(int)), "appendToFile: read 9 fallita");
		len_path = *buffer;
		
		//pathname
		char path[len_path+1];
		ec_meno1(read(fd_sk, buffer, sizeof(char)*len), "appendToFile: read 10 fallita");
		path[len_path+1] = '\0';
		
		//size data
		size_t size_data;
		ec_meno1(read(fd_sk, buffer, sizeof(char)*len), "appendToFile: read 11 fallita");
		size_data = *buffer;
		
		//data
		ec_null((arr_data = (char*)calloc(sizeof(char), size_data)), "appendToFile: calloc fallita");
		ec_meno1(read(fd_sk, arr_data, sizeof(char)*size_data), "appendToFile: read 12 fallita");
		
		//scrittura file espluso nella directory arg_D
		if(dirname != NULL)
			writefile_in_dir(path, size_data, arr_data);
		//printf("(CLIENT) - openFile: file espluso = %s\n", path);
	}
	
	if(buffer) free(buffer);
	if(arr_data) free(arr_data);
	return ret;
	
	atf_clean:
	if(buffer) free(buffer);
	if(arr_data) free(arr_data);
	return -1;
}

//READ FILE 4
int readFile(const char* pathname, void** buf, size_t* size)
{

	printf("(CLIENT) - readFile: su: %s\n", pathname);
	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "readFile: malloc fallita");
	*buffer = 0;
	char* data = NULL;
	int id;
	int len_path;
	int ret;

	//SETTING RICHIESTA
      //comunica al thread 4 (codifica readFile=3)
	*buffer = 4;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 1 fallita");
      //riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "readFile: read non valida") goto rf_clean; }
	

      // comunicazione stabilita OK
      // invio dati richiesta al server 

	//IDENTIFICAZIONE PROCESSO
      id = getpid();
      //16 comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 6 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 6 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "readFile: read non valida"); goto rf_clean; }
      
	//LEN PATHNAME
	//comunica: invia lunghezza pathname
	len_path = strlen(pathname);
	*buffer = len_path;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 2 fallita");
      //riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "readFile: read non valida"); goto rf_clean; }


	//PATHANAME
	//comunica: pathname
	ec_meno1(write(fd_sk, pathname, sizeof(char)*len_path), "readFile: write 3 fallita");
	//riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 3 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "readFile: read non valida"); goto rf_clean; }


	//RICEZIONE ESITO readFile
	// riceve: esito openFile
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 7 fallita");
	ret = *buffer;
	// comunica: conferma ricezione
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 7 fallita");

	//se il file non esiste, finisce qui
	if(ret == -1){
		printf("(CLIENT) - readFile:		LETTURA NON RIUSCITA\n"); //DEBUG
		goto rf_clean;
	}

	//FILE SIZE
	//riceve: dimensione
	size_t dim_file;
	size=&dim_file;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 3 fallita");
	dim_file = (size_t)*buffer; //cast dovrebbe essere implicito
	//comunica: conferma ricezione
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 3 fallita");


	//DATA FILE
	//riceve: data
	ec_null((data = calloc(sizeof(char), (*size))), "readFile: calloc fallita");
	ec_meno1(read(fd_sk, data, sizeof(char)*(*size)), "readFile: read 3 fallita");
	//buf = &data;  //BUG CON ASSEGNAMENTO BUFFER (!) ATTENZIONE QUIIIII - SETTA IL PUNTATORE VOID **
	//comunica: conferma ricezione
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 3 fallita");

	printf("(CLIENT) - readFile:	ESITO = %d | letto: %s / %zu bytes\n", ret, pathname, dim_file); //DEBUG
	
	if(buffer) free(buffer);
	if(data) free(data);
	return 0;

	rf_clean:
	if(buffer) free(buffer);
	if(data) free(data);
	return -1;
	
}

//READ N FILES 5
int readNFiles(int N, const char* dirname )
{		
	printf("(CLIENT) - readNFiles: su: %d\n", N);
	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "readNFiles: malloc fallita");
	*buffer = 0;
	char* data_file = NULL;

	//SETTING RICHIESTA
      //comunica al thread 5 (codifica readNFiles=5)
	*buffer = 5;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readNFiles: write 1 fallita");
      //riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "readNFiles: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "readNFiles: read non valida") goto rnf_clean; }
	printf("(CLIENT) - readNFiles: setting richiesta OK\n");
	

      // comunicazione stabilita OK
      // invio dati richiesta al server 

	
	//IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //16 comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readNFiles: write 5 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "readNFiles: read 5 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "readNFiles: read non valida"); goto rnf_clean; }
	printf("(CLIENT) - readNFiles: identificazione processo  OK\n");


      //NUMERO DI FILE DA LEGGERE
	//comunica: n
	*buffer = N;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readNFiles: write 2 fallita");
      //riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "readNFiles: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "readFile: read non valida"); goto rnf_clean; }
	printf("(CLIENT) - readNFiles: numero di file da leggere OK\n");


	while(1) {
		printf("(CLIENT) - BUG HERE?\n");
		//CONDIZIONE DI TERMINAZIONE
		//riceve: condizione di  terminazione
		ec_meno1(read(fd_sk, buffer, sizeof(int)), "readNFiles: read 3 fallita");
		if(*buffer != 1) break;
		*buffer = 0;
		ec_meno1(write(fd_sk, buffer, sizeof(int)), "readNFiles: write 2 fallita");
		printf("(CLIENT) - readNFiles: condizione di terminazione OK\n");


		//CONTROLLO LOCK FILE
		//riceve: flag lock 
		int not_lock;
		ec_meno1(read(fd_sk, buffer, sizeof(int)), "readNFiles: read 3 fallita");
		not_lock = *buffer;
		*buffer = 0;
		ec_meno1(write(fd_sk, buffer, sizeof(int)), "readNFiles: write 2 fallita");
		printf("(CLIENT) - readNFile: controllo lock file OK\n");


		if(not_lock){
			
			//LEN FILE NAME
			//riceve: lunghezza nome file
			int len_name;
			ec_meno1(read(fd_sk, buffer, sizeof(int)), "readNFiles: read 3 fallita");
			len_name = *buffer; //cast dovrebbe essere implicito
			//comunica: conferma ricezione
			*buffer = 0;
			ec_meno1(write(fd_sk, buffer, sizeof(int)), "readNFiles: write 3 fallita");

			//FILE NAME
			//riceve:
			char file_name[len_name];
			file_name[len_name] = '\0';
			ec_meno1(read(fd_sk, file_name, sizeof(char)*len_name), "readNFiles: read 3 fallita");
			//comunica: conferma ricezione
			*buffer = 0;
			ec_meno1(write(fd_sk, buffer, sizeof(int)), "readNFiles: write 3 fallita");

			//FILE SIZE
			//riceve: dimensione
			size_t dim_file;
			ec_meno1(read(fd_sk, buffer, sizeof(int)), "readNFiles: read 3 fallita");
			dim_file = (size_t)*buffer;  //cast implicito?
			//comunica: conferma ricezione
			*buffer = 0;
			ec_meno1(write(fd_sk, buffer, sizeof(int)), "readNFiles: write 3 fallita");
	
			//DATA FILE 
			
			ec_null((data_file = calloc(sizeof(char), dim_file)), "readNFiles: calloc fallita");
			//riceve: data
			ec_meno1(read(fd_sk, data_file, sizeof(char)*dim_file), "readNFiles: read 3 fallita");
			//comunica: conferma ricezione
			*buffer = 0;
			ec_meno1(write(fd_sk, buffer, sizeof(int)), "readNFiles: write 3 fallita");
			printf("(CLIENT) - readFile: letto: %s / %zu bytes\n", file_name, dim_file); //DEBUG

			if (arg_d != NULL)
				writefile_in_dir(file_name, dim_file, data_file);
			
			if(data_file) free(data_file);
		}

	}
	
	if(buffer) free(buffer);
	//if(data_file) free(data_file);
	return 0;

	rnf_clean:
	if(buffer) free(buffer);
	if(data_file) free(data_file);
	return -1;
}

//LOCK FILE 6
int lockFile(const char* pathname)
{

	printf("(CLIENT) - lockFile: su %s \n", pathname); //DEBUG
	int ret;
	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "lockFile: malloc fallita");
	*buffer = 0;

	while(1){
		//SETTING RICHIESTA
      	//0 comunica al thread 6 (codifica lockFile=6)
		*buffer = 6;
		ec_meno1(write(fd_sk, buffer, sizeof(int)), "lockFile: write 1 fallita");
     		//3 riceve: conferma accettazione richiesta openFile (1)
		ec_meno1(read(fd_sk, buffer, sizeof(int)), "lockFile: read 1 fallita");
		if(*buffer != 0){ LOG_ERR(-1, "lockFile: read non valida") goto lf_clean; }

      	// comunicazione stabilita OK
     		// invio dati richiesta al server
      
     		//LEN PATHNAME
		//4 comunica: invia lunghezza pathname
		int len = strlen(pathname);
		*buffer = len;
		ec_meno1(write(fd_sk, buffer, sizeof(int)), "lockFile: write 2 fallita");
     		//7 riceve: conferma ricezione pathname
		ec_meno1(read(fd_sk, buffer, sizeof(int)), "lockFile: read 2 fallita");
		if(*buffer != 0){ LOG_ERR(-1, "lockFile: read non valida"); goto lf_clean; }

		//PATHANAME
		//8 comunica: pathname
		ec_meno1(write(fd_sk, pathname, sizeof(char)*len), "lockFile: write 3 fallita");
		//11 riceve: conferma ricezione pathname
     		ec_meno1(read(fd_sk, buffer, sizeof(int)), "lockFile: read 3 fallita");
		if(*buffer != 0){ LOG_ERR(-1, "lockFile: read non valida"); goto lf_clean; }


    		//IDENTIFICAZIONE PROCESSO
      	int id = getpid();
      	//16 comunica: pid
      	*buffer = id;
		ec_meno1(write(fd_sk, buffer, sizeof(int)), "lockFile: write 5 fallita");
      	//19 riceve: conferma ricezione pid
      	ec_meno1(read(fd_sk, buffer, sizeof(int)), "lockFile: read 5 fallita");
		if(*buffer != 0){ LOG_ERR(-1, "lockFile: read non valida"); goto lf_clean; }

		//dati inviati al server
	
		//RICEZIONE ESITO OPENFILE
		//21 (riceve): esito openFile
		
		ec_meno1(read(fd_sk, buffer, sizeof(int)), "lockFile: read 6 fallita");
		ret = *buffer;
		printf("(CLIENT) - lockFile:		ESITO = %d\n", ret); //DEBUG
		//22 (comunica): ricevuto
		*buffer = 0;
		ec_meno1(write(fd_sk, buffer, sizeof(int)), "lockFile: write 6 fallita");
		if( *buffer == -2 ){
			int msec = LOCK_TIMER/1000;
			usleep(msec);
		}else{
			break;
		}
	}
	printf("(CLIENT) - lockFile:		ESITO = %d\n", ret); //DEBUG

	if(buffer) free(buffer);
	return ret;

	lf_clean:
	if(buffer) free(buffer);
	return -1;
}

//UNLOCK FILE 7
int unlockFile(const char* pathname)
{
	printf("(CLIENT) - unlockFile: su %s\n", pathname); //DEBUG

	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "unlockFile: malloc fallita");
	*buffer = 0;

	//SETTING RICHIESTA
      //0 comunica al thread 7 (codifica unlockFile=7)
	*buffer = 7;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "unlockFile: write 1 fallita");
      //3 riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "unlockFile: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "unlockFile: read non valida") goto lf_clean; }

      // comunicazione stabilita OK
      // invio dati richiesta al server
      
      //LEN PATHNAME
	//4 comunica: invia lunghezza pathname
	int len = strlen(pathname);
	*buffer = len;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "unlockFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "unlockFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "unlockFile: read non valida"); goto lf_clean; }

	//PATHANAME
	//8 comunica: pathname
	ec_meno1(write(fd_sk, pathname, sizeof(char)*len), "unlockFile: write 3 fallita");
	//11 riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "unlockFile: read 3 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "unlockFile: read non valida"); goto lf_clean; }


      //IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //16 comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "unlockFile: write 5 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "unlockFile: read 5 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "unlockFile: read non valida"); goto lf_clean; }

	//dati inviati al server
	
	//RICEZIONE ESITO OPENFILE
	//21 (riceve): esito openFile
	int ret;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 6 fallita");
	ret = *buffer;
	printf("(CLIENT) - unlockFile:		ESITO = %d\n", ret); //DEBUG
	//22 (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "unlockFile: write 6 fallita");

	if(buffer) free(buffer);
	return ret;

	lf_clean:
	if(buffer) free(buffer);
	return -1;
	
}

//UNLOCK FILE 8
int removeFile(const char* pathname)
{
	printf("(CLIENT) - removeFile: su %s\n", pathname); //DEBUG

	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "removeFile: malloc fallita");
	*buffer = 0;

	//SETTING RICHIESTA
      //0 comunica al thread 1 (codifica removeFile=8)
	*buffer = 8;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "removeFile: write 1 fallita");
      //3 riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "removeFile: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "removeFile: read non valida") goto uf_clean; }

      // comunicazione stabilita OK
      // invio dati richiesta al server
      
      //LEN PATHNAME
	//4 comunica: invia lunghezza pathname
	int len = strlen(pathname);
	*buffer = len;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "removeFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "removeFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "removeFile: read non valida"); goto uf_clean; }

	//PATHANAME
	//8 comunica: pathname
	ec_meno1(write(fd_sk, pathname, sizeof(char)*len), "removeFile: write 3 fallita");
	//11 riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "removeFile: read 3 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "removeFile: read non valida"); goto uf_clean; }


      //IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //16 comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "removeFile: write 5 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "removeFile: read 5 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "removeFile: read non valida"); goto uf_clean; }

	//dati inviati al server
	
	//RICEZIONE ESITO OPENFILE
	//21 (riceve): esito openFile
	int ret;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "removeFile: read 6 fallita");
	ret = *buffer;
	printf("(CLIENT) - removeFile:		ESITO = %d\n", ret); //DEBUG
	//22 (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "removeFile: write 6 fallita");

	if(buffer) free(buffer);
	return ret;

	uf_clean:
	if(buffer) free(buffer);
	return -1;
}

//CLOSE FILE 9
int closeFile(const char* pathname)
{
	printf("(CLIENT) - closeFile: su %s\n", pathname); //DEBUG

	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "closeFile: malloc fallita");
	*buffer = 0;

	//SETTING RICHIESTA
      //comunica al thread 1 (codifica closeFile=9)
	*buffer = 9;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "closeFile: write 1 fallita");
      //riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "closeFile: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "closeFile: read non valida") goto cf_clean; }

      // comunicazione stabilita OK
      // invio dati richiesta al server
      
      //LEN PATHNAME
	//comunica: invia lunghezza pathname
	int len = strlen(pathname);
	*buffer = len;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "closeFile: write 2 fallita");
      //riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "closeFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "closeFile: read non valida"); goto cf_clean; }

	//PATHANAME
	//comunica: pathname
	ec_meno1(write(fd_sk, pathname, sizeof(char)*len), "closeFile: write 3 fallita");
	//riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "closeFile: read 3 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "closeFile: read non valida"); goto cf_clean; }


      //IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "closeFile: write 5 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "closeFile: read 5 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "closeFile: read non valida"); goto cf_clean; }

	//dati inviati al server
	

	//ELABORAZIONE
	//unlock e deallocazione nodo dalla open_files_lista dei file aperti?


	//RICEZIONE ESITO OPENFILE
	//21 (riceve): esito openFile
	int ret;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "closeFile: read 6 fallita");
	ret = *buffer;
	printf("(CLIENT) - closeFile:		ESITO = %d\n", ret); //DEBUG
	//22 (comunica): ricevuto
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "closeFile: write 6 fallita");

	if(buffer) free(buffer);
	return ret;

	cf_clean:
	if(buffer) free(buffer);
	return -1;
}
