
/*
-> implementare lista dei file aperti e procedura di chiusura quando il client si disconnette
-> timespec da implementare nelle richieste
*/


#include "err_control.h"
#include "function_c.h"
typedef unsigned char byte;
enum { NS_PER_SECOND = 1000000000 };
typedef enum {O_CREATE=1, O_LOCK=2} flags;
//variabili globali
byte flag_D = 0;			//arg_D attivo RIDONDANTE -> SI PUO USARE NULL
char* arg_D = NULL;		//dirname in cui scrivere i file rimossi dal server
byte flag_d = 0;			//arg_d attivo  RIDONDANTE -> SI PUO USARE NULL
char* arg_d = NULL;		//dirname in cui scrivere i file letti dal server
byte flag_wW = 0;			//flag di controllo scritture
byte flag_rR = 0;			//flag controllo letture
byte flag_p = 0;			//abilita stampe sullo std output per ogni operazione
byte flag_h = 0;			//-h attivo 
unsigned sleep_time = 0; 	
int fd_sk; // = -1;
char *socket_name = NULL;	
char* path_dir = NULL;		//in caso di scritture (-w dirname), contiene il percorso del file ricavato in send_from_directory

//prototipi client
static void case_h();
static long isNumber(const char* s);
static int is_opt(char* arg, char* opt);
static int is_argument(char* c);
static int is_directory(const char *path);
static void send_from_dir(const char* dirname, int* n);
static void parser(int dim, char** array);
static int append_file(const char* s);
static int write_request(const char* arg_W);
static void case_w(char* arg_w);
static void case_l(char* arg_l);
static void case_u(char* arg_u);
static void case_c(char* arg_c);
static void set_socket(const char* socket_name);


//prototipi API client
//connessione
int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);
//scrittura
int openFile(const char* sockaname, int flags);
int writeFile(const char* f_name, char* dirname);
int appendToFile(const char* f_name, void* buf, size_t size, const char* dirname);
//lettura
int readFile(const char* f_name, void** buf, size_t* size);
int readNFiles(int n, const char* dirname);
//operazioni su file
int lockFile(const char* f_name){return 0;};
int unlockFile(const char* f_name){return 0;};
int remvoveFile(const char* f_name){return 0;};


static int is_opt( char* arg, char* opt)
{
	size_t n = strlen(arg);
	if(strncmp(arg, opt, n) == 0) return 1;
	else return 0;
}
static int is_argument(char* c)
{
	if (c == NULL || (*c == '-') ) return 0;
	else return 1;
}
static int is_directory(const char *path)
{
    struct stat path_stat;
    ec_meno1(lstat(path, &path_stat), "errore stat");
    return S_ISDIR(path_stat.st_mode);
}

//scritture

static void case_W (char* arg_W)
{
	//printf("\n\n"); //DEBUG
	//printf("(CLIENT) - case_W\n"); //DEBUG
	char* save = NULL;
	char* token = strtok_r(arg_W, ",", &save);
	while (token){
		printf("(CLIENT) - case_W:	richiesta di scrittura del file %s\n", token); //DEBUG
		if(write_request(token) == -1){
			LOG_ERR(errno, "scrittura fallita\n");
			exit(EXIT_FAILURE);
		}
		token = strtok_r(NULL, ",", &save);
	}
	if(closeConnection(socket_name) == -1){
		LOG_ERR(errno, "case_W: closeConnection fallita\n");
		exit(EXIT_FAILURE);
	}else{
		printf("(CLIENT) - case_W: connessione con server chiusa\n"); //DEBUG
	}
	exit(EXIT_SUCCESS);
}
static void case_w(char* arg_w)
{
		//in seguito in dirname salvo la directory estratta da arg_w
		char* dirname;
		//verifica arg opzionale n>0 || n=0 || n = NULL
		char* n = NULL;
		int x;			
		//strtok_r mi da il primo token appena dopo il primo delimitatore usando il puntatore n
		strtok_r(arg_w, ",", &n);
		//se n non è specificata -> x = -1
		if (n == NULL){
			x = -1;
			dirname = arg_w;
		}else{
			dirname = strtok(arg_w, ",");
			//controllo che sia un intero e che sia positivo
			ec_meno1((x = isNumber(n)), "errore: arg. -w n non intero\n");
			if (x < 0){ LOG_ERR(EINVAL, "arg -w optzionale non valido"); exit(EXIT_FAILURE); }
			
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
			if(strcmp(entry->d_name, ".DS_Store") != 0){
			//printf("(CLIENT) - case_w.send_from_dir: richieta di scrittura per %s (path_dir: %s)\n", entry->d_name, path_dir); //DEBUG
			if (write_request(entry->d_name) == -1){  // (!) togliere strcmp errore terminale macos
				LOG_ERR(-1, "errore write_request"); 
				exit(EXIT_FAILURE); 
			}
			(*n)--;
			}
		}
	}
	//chiudere la directory
	ec_meno1(closedir(d), "errore su closedir");
	//return 0;
}
static int write_request(const char* f_name)
{	
	// gestione accurata degli errori
	printf("\n\n"); //DEBUG
	// openFile -> richiesta di apertura o creazione del file da scrivere
	// predispone la successiva scrittura e controlla le condizioni necessarie
	//writeFile
	printf("(CLIENT) - write_request\n");
	if (openFile(f_name, (O_CREATE|O_LOCK)) != -1 ){
            printf("(CLIENT) - writeFile in corso...\n");
		writeFile(f_name, arg_d);					
		//errno = ENOENT; ad esempio cosi si setta errno che poi viene utilizzato nella funzione di ritorno
	}
	else{
		//appendToFile
		if (openFile(f_name, O_LOCK) != -1 ){
			printf("(CLIENT) - appendToFile in corso...\n");
	      	append_file(f_name);
			printf("(CLIENT) - write_request:	appendToFile in progress su %s\n", f_name);
		}else{
			printf("(CLIENT) - write_request:	funzione terminata con fallimento\n");
			return -1;
		}
	}
	printf("(CLIENT) - write_request:	funzione terminata con successo\n");
	return 0;
}
static int append_file(const char* f_name)
{
	// necess. allocare un buf(array di char) che contiene il testo del file e ha quindi dimensione in byte del file
	// ricavare le dimensioni in byte del file con stat
	// scrittura del buf e conseguente appendToFile

	//apertura del file
	int fd;
	if(path_dir != NULL){ ec_meno1((fd = open(path_dir, O_RDONLY)), "open in append_file"); }
	else{  ec_meno1((fd = open(f_name, O_RDONLY)), "open in append_file"); }

	//stat per ricavare dimensione file
	struct stat path_stat;
	if(path_dir != NULL){ ec_meno1(lstat(path_dir, &path_stat), "errore stat"); }
	else{ ec_meno1(lstat(f_name, &path_stat), "errore stat"); }

    	//allocazione buf[size]
    	size_t file_size = path_stat.st_size;
    	char* buf;
 	ec_null((buf = calloc(file_size, sizeof(char))) , "malloc in append_file");

 	//lettura file + scrittura del buf
 	ec_meno1(read(fd, buf, file_size), "read");
 	ec_meno1(close(fd), "errore close in append_file");

 	//appendToFile per scrittura f_name atomica
 	ec_meno1(appendToFile(f_name, buf, file_size, arg_d), "appendToFile in append_file");
    	path_dir = NULL;

 	//cleanup
 	if(!buf) free(buf);
 	return 0;
}

//read
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
static void case_r (char* arg_r)
{


	char* save = NULL;
	char* token = strtok_r(arg_r, ",", &save);
	while (token){
		//su ogni toker che rappresenta il nome del file invio una richiesta di lettura
		printf("richiesta di lettura su: %s\n", token);
		void **buf;
		size_t* size;
		ec_meno1(readFile(token, buf, size), "readFile fallita");
		token = strtok_r(NULL, ",", &save);
	}
}


//lock/unlock
static void case_l(char* arg_l)
{
	char* save = NULL;
	char* token = strtok_r(arg_l, ",", &save);
	while (token){
		//su ogni token che rappresenta il nome del file invio una richiesta di lock sul file
		printf("richiesta di lock su: %s\n", token);
		ec_meno1(lockFile(token), "lockFile fallita");
		token = strtok_r(NULL, ",", &save);
	}
}
static void case_u(char* arg_u)
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
static void case_c(char* arg_c)
{
	char* save = NULL;
	char* token = strtok_r(arg_c, ",", &save);
	while (token){
		//su ogni token che rappresenta il nome del file invio una richiesta di lock sul file
		printf("richiesta di cancellazione su: %s\n", token);
		ec_meno1(remvoveFile(token), "removeFile fallita");
		token = strtok_r(NULL, ",", &save);
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
    	LOG_ERR(errno, "client: -f tentativo di connessione non riuscito");
    	exit(EXIT_FAILURE);
    }
}

//help
static void case_h()
{
	int fd;
	ec_meno1((fd = open("help.txt", O_RDONLY)), "errore f. open in case_h");
	
	//struttura per acquisire informazioni sul file
	struct stat path_stat;
	ec_meno1(lstat("help.txt", &path_stat), "errore stat");

    	//allocazione buf[size] per la dimensione del file in byte
    	size_t file_size = path_stat.st_size;
   	char* buf;
 	ec_null((buf = calloc(file_size, sizeof(char))) , "malloc in append_file");
	
 	//lettura file + scrittura del buf
 	ec_meno1(read(fd, buf, file_size), "read");
 	
	//stampa buf
 	for(int i = 0; i < file_size; i++) printf("%c", buf[i]);
	//DEBUG: carattere di terminazione presente nel file txt?

 	ec_meno1(close(fd), "errore close in append_file");
 	if(!buf) free(buf);
}

//parser
static void parser(int dim, char** array){

	//ciclo preliminare per controllare se presente -h e terminare immediatamente 
	int i = 0;
	while (++i < dim){
		//CASO -h
		if (is_opt(array[i], "-h")) {
			case_h();
			exit(0);
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
				printf("client:DEBUG: socket name acquisito: %s\n", socket_name);
				set_socket(socket_name);
			}else{ 
				LOG_ERR(EINVAL, "client: argomento -f mancante");
				exit(EXIT_FAILURE); 
			}
		}
		//CASO -t
		if (is_opt(array[i], "-t")) {
			if ( !is_argument(array[i+1]) ) sleep_time = 0;
			else ec_meno1( (sleep_time = isNumber(array[++i])), "client: argomento -t errato");
			printf("DEBUG: time_r = %d\n", sleep_time);
		}	
		//CASO -p
		if (is_opt(array[i], "-p")){
			flag_p = 1;
			i++;
		}
		//CASO -D
		if (is_opt(array[i], "-D")) {
			flag_D = 1;
			if (is_argument(array[i+1]))
				arg_D = array[++i];
		}
		//CASO -d 
		if (is_opt(array[i], "-d")) {
			flag_d = 1;
			if (is_argument(array[i+1]))
				arg_d = array[++i];
		}
		//controllo dipendenza -d (da -w/-W) e -D da (-r/-R)
		if (is_opt(array[i], "-w") || is_opt(array[i], "-W")) flag_wW = 1;
		if (is_opt(array[i], "-r") || is_opt(array[i], "-R")) flag_rR = 1;
	}

	//controllo dipendeza
	if(flag_D && !flag_wW){ LOG_ERR(EINVAL, "-D attivo -w/-W non attivi"); exit(EXIT_FAILURE); }
	if(flag_d && !flag_rR){ LOG_ERR(EINVAL, "-d attivo -r/-R non attivi"); exit(EXIT_FAILURE); }

	//CICLO 2: si gestiscono i comandi di richiesta al server -w, -W, -R, -r, -l, -u, -c
	i = 0;
	while (++i < dim){	
		//CASO -w
		if (is_opt(array[i], "-w")) {		
			if (is_argument(array[i+1])) {
				case_w(array[++i]);
			}else{ 
				LOG_ERR(EINVAL, "argomento mancante -w");
				exit(EXIT_FAILURE);
			}	
		}		
		//CASO -W
		if (is_opt(array[i], "-W")) {	
			if (is_argument(array[i+1])){
				case_W(array[++i]);
			}else{
				LOG_ERR(EINVAL, "argomento -W mancante");
				exit(EXIT_FAILURE);
			}
		}		
		//CASO -r
		if (is_opt(array[i], "-r")) {
			if (is_argument(array[i+1])) {
				//richiesta di lettura al server per ogni file
				case_r(array[++i]);

			}else{
				LOG_ERR(EINVAL, "argomento -r mancante");
				exit(EXIT_FAILURE);
			}
		}
		//CASO -R
		if (is_opt(array[i], "-R")){
			//puoi chiamare readfile o readnfile rispetto al paramentro n che riceve
			//se n=0 o non presente legge tutti i file del server
			//altrimenti ne legge n e li momorizza in arg_d
			
			//caso in cui è presente n
			if (is_argument(array[i+1])) {
				size_t x;
				ec_meno1((x = isNumber(array[++i])), "errore: arg. -R n non intero\n");
				if (x == 0){ ec_meno1(readNFiles(-1, arg_d), "readNFiles fallita"); }
				else{ ec_meno1(readNFiles(x, arg_d), "readNFiles fallita"); }
			//caso in cui n non è presente
			}else{
				ec_meno1(readNFiles(-1, arg_d), "readNFiles fallita");
			}

		}
		//CASO -l
		if (is_opt(array[i], "-l")){
			if (is_argument(array[i+1])){
				case_l(array[++i]);
				printf("lista di nomi di file su cui acquisire la mutua esclusione\n");
			}else{
				LOG_ERR(EINVAL, "argomento -l mancante");
				exit(EXIT_FAILURE);
			}
		}
		//CASO -u
		if (is_opt(array[i], "-u")){
			if (is_argument(array[i+1])){
				printf("lista di nomi di file su cui rilasciare la mutua esclusione\n");
				case_u(array[++i]);
			}else{
				LOG_ERR(EINVAL, "argomento -u mancante");
				exit(EXIT_FAILURE);
			}
		}
		//CASO -c
		if (is_opt(array[i], "-c")){
			if (is_argument(array[i+1])){
				printf("lista di file da rimuovere dal server se presenti\n");
				case_c(array[++i]);
			}else{
				LOG_ERR(EINVAL, "argomento -c mancante");
				exit(EXIT_FAILURE);
			}
		}
		//tempo di attesa tra una richiesta e l'altra
		if(sleep_time != 0) {
			sleep_time = sleep_time/1000;  //conversione da millisecondi a microsecondi
			usleep(sleep_time);
		}
	}
}

// MAIN
int main(int argc, char* argv[]){
	
	//controllo su argc
	if(argc < 1){
		LOG_ERR(EINVAL, "errore su argomenti\n");
		exit(EXIT_FAILURE);
	}
	//parser
	parser(argc, argv);
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
//connessione
int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	printf("\n\n"); //DEBUG
	//controllo validità socket_name
	if(socket_name == NULL) return -1;

	//timer setting
	msec = msec/1000; //conversione da millisecondi a microsecondi per la usleep
	struct timespec time1, time2, delta;
      ec_meno1(clock_gettime(CLOCK_REALTIME, &time1), "client: clock_gettime fallita");

       //socket setting
      struct sockaddr_un sa;
	strncpy(sa.sun_path, sockname, strlen(socket_name));
	sa.sun_family = AF_UNIX;
    
	if( (fd_sk = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){ 
		LOG_ERR(errno, "client.openConnection: socket fallita");
		exit(EXIT_FAILURE);
	}
	//printf("DEBUG: fd_socket client = %d\n", fd_sk); //DEBUG
	//ciclo di connessione
      while(connect(fd_sk, (struct sockaddr*)&sa, sizeof(sa)) == -1){
		//printf("DEBU DEBUG DEBUG sono qui\n");
    	      if(errno == ENOENT){
			usleep(msec);
			//seconda misurazione
			ec_meno1(clock_gettime(CLOCK_REALTIME, &time2), "client: clock_gettime fallita");
			//calcola tempo trascorso tra time1 e time2 in delta
			sub_timespec(time1, time2, &delta);
			//controllo che non si sia superato il tempo assoluto dal primo tentativo di connessione
			if( (delta.tv_nsec >= abstime.tv_nsec && delta.tv_sec >= abstime.tv_sec) || delta.tv_sec >= abstime.tv_sec){
				printf("CLIENT.OpenConnection: abstime scaduto, connessione fallita\n");
				return -1;
			}
       	}else{ 
			LOG_ERR(errno, "CLIENT.openConnection: connect fallita");
			exit(EXIT_FAILURE);
		}
      }
	if(fd_sk != -1) printf("CLIENT.openConnection: connessione con server stabilita\n\n");;
      return 0;
}
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
//scrittura
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
	if(*buffer != 0){ LOG_ERR(-1, "openFile: read non valida") goto of_clean; }

      // comunicazione stabilita OK
      // invio dati richiesta al server
      
      //LEN PATHNAME
	//4 comunica: invia lunghezza pathname
	int len = strlen(pathname);
	*buffer = len;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "openFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "openFile: read non valida"); goto of_clean; }

	//PATHANAME
	//8 comunica: pathname
	ec_meno1(write(fd_sk, pathname, sizeof(char)*len), "openFile: write 3 fallita");
	//11 riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 3 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "openFile: read non valida"); goto of_clean; }


      //FLAGS
      //12 comunica: flags
	*buffer = flag;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "openFile: write 4 fallita");
	//15 riceve: conferma ricezione flags
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 4 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "openFile: read non valida"); goto of_clean; }


      //IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //16 comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "openFile: write 5 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "openFile: read 5 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "openFile: read non valida"); goto of_clean; }

	//printf("op_data_receive: %d / %s / flags / %d\n", len, pathname, id); //DEBUG
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
	//legge 1 se c'è un file espluso, 0 altrimenti
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
int writeFile(const char* f_name, char* dirname)
{
	
	printf("(CLIENT) - writeFile: su: %s\n", f_name);
	//APERTURA E LETTURA FILE
	int fd;
	if(path_dir != NULL){ ec_meno1((fd = open(path_dir, O_RDONLY)), "writeFile: errore open");}
	else{ ec_meno1((fd = open(f_name, O_RDONLY)), "writeFile: errore open");}
	//struttura per acquisire informazioni sul file
	struct stat path_stat;
	if(path_dir != NULL){ ec_meno1(lstat(path_dir, &path_stat), "writeFile: errore stat");}
	else{ ec_meno1(lstat(f_name, &path_stat), "writeFile: errore stat");}

    	//allocazione buf[size] per la dimensione del file in byte
    	size_t file_size = path_stat.st_size;
   	char* data_file = NULL;
 	ec_null((data_file = calloc(file_size, sizeof(char))) , "writeFile: malloc fallita");
 	//lettura file + scrittura del buf
 	ec_meno1(read(fd, data_file, file_size), "writeFile: read fallita");
 	ec_meno1(close(fd), "writeFile: close fallita");
	path_dir = NULL;
	///////////////////////////////////////////////////////////////////////////////

	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "openFile: malloc fallita");
	*buffer = 0;

	//SETTING RICHIESTA
      //0 comunica al thread 2 (codifica writeFile=2)
	*buffer = 2;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "writeFile: write 1 fallita");
      //3 riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "writeFile: read non valida") goto wf_clean; }


      // comunicazione stabilita OK
      // invio dati richiesta al server 
      

      //LEN PATHNAME
	//4 comunica: invia lunghezza pathname
	int len = strlen(f_name);
	*buffer = len;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "writeFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "writeFile: read non valida"); goto wf_clean; }


	//PATHANAME
	//8 comunica: pathname
	ec_meno1(write(fd_sk, f_name, sizeof(char)*len), "writeFile: write 3 fallita");
	//11 riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 3 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "writeFile: read non valida"); goto wf_clean; }


	//DIMENSIONE DEL FILE
	*buffer = file_size;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "writeFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "writeFile: read non valida"); goto wf_clean; }


	//DATA FILE
	ec_meno1(write(fd_sk, data_file, sizeof(char)*file_size), "writeFile: write 2 fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 2 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "writeFile: read non valida"); goto wf_clean; }


      //IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //16 comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "writeFile: write 5 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 5 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "writeFile: read non valida"); goto wf_clean; }
	
	
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
	//legge 1 se c'è un file espluso, 0 altrimenti
	*buffer = 0;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "writeFile: read 7 fallita");
	//printf("(CLIENT) - openFile:	file espulso = %d\n", *buf);
	char *arr_data = NULL;
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
		ec_null((arr_data = (char*)calloc(sizeof(char), size_data)), "writeFile: calloc fallita");
		ec_meno1(read(fd_sk, arr_data, sizeof(char)*size_data), "writeFile: read 11 fallita");
		
		//scrittura file espluso nella directory arg_D
		if(dirname != NULL)
			writefile_in_dir(path, size_data, arr_data);
		//printf("(CLIENT) - openFile: file espluso = %s\n", pathname);
	}
	
	if(buffer) free(buffer);
	if(data_file) free(data_file);
	if(arr_data) free(arr_data);
	return ret;
	
	wf_clean:
	if(buffer) free(buffer);
	if(data_file) free(data_file);
	if(arr_data) free(arr_data);
	return -1;
}
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
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
	//legge 1 se c'è un file espluso, 0 altrimenti
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

//lettura
int readFile(const char* pathname, void** buf, size_t* size)
{
	int* buffer;
	ec_null( (buffer = malloc(sizeof(int))), "readFile: malloc fallita");
	*buffer = 0;

	//SETTING RICHIESTA
      //comunica al thread 4 (codifica readFile=3)
	*buffer = 4;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 1 fallita");
      //riceve: conferma accettazione richiesta openFile (1)
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 1 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "readFile: read non valida") goto rf_clean; }
	

      // comunicazione stabilita OK
      // invio dati richiesta al server 
      printf("(CLIENT) - readFile: fino a qui tutto bene\n");

	//IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //16 comunica: pid
      *buffer = id;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 6 fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 6 fallita");
	if(*buffer != 0){ LOG_ERR(-1, "readFile: read non valida"); goto rf_clean; }
      
	//LEN PATHNAME
	//comunica: invia lunghezza pathname
	int len_path = strlen(pathname);
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
	int ret;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 7 fallita");
	ret = *buffer;
	printf("(CLIENT) - readFile:	ESITO = %d\n", ret); //DEBUG
	// comunica: conferma ricezione
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 7 fallita");

	//se il file non esiste, finisce qui
	if(ret == -1){
		goto rf_clean;
	}

	printf("(SERVER) - readFile: fino a qui tutto bene\n");

	//FILE SIZE
	//riceve: dimensione
	size_t dim_file;
	size=&dim_file;
	ec_meno1(read(fd_sk, buffer, sizeof(int)), "readFile: read 3 fallita");
	dim_file = (size_t)*buffer; //cast dovrebbe essere implicito
	printf("(CLIENT) - readFile:	*size = %zu\n", *size); //DEBUG
	//comunica: conferma ricezione
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 3 fallita");


	//DATA FILE
	//riceve: data
	char* data = NULL;
	ec_null((data = calloc(sizeof(char), (*size))), "readFile: calloc fallita");
	ec_meno1(read(fd_sk, data, sizeof(char)*(*size)), "readFile: read 3 fallita");
	//buf = &data;  //BUG CON ASSEGNAMENTO BUFFER
	//comunica: conferma ricezione
	*buffer = 0;
	ec_meno1(write(fd_sk, buffer, sizeof(int)), "readFile: write 3 fallita");

	
	if(buffer) free(buffer);
	if(data) free(data);
	return 0;

	rf_clean:
	if(buffer) free(buffer);
	if(data) free(data);
	return -1;
	
}

//READ N FILES
int readNFiles(int n, const char* dirname )
{
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
	*buffer = n;
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

