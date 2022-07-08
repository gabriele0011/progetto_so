
/*
-> implementare lista dei file aperti e procedura di chiusura quando il client si disconnette
-> rivedi tipi variabili globali
-> controlla l'errore sulle directory lato server 
-> timespec va nel file header del client
*/

#include "err_control.h"
#include "function_c.h"
typedef unsigned char byte;
#define O_CREATE 10
#define O_LOCK 11
enum { NS_PER_SECOND = 1000000000 };

//variabili globali
byte flag_D = 0;
byte flag_d = 0;
byte flag_p = 0;
byte flag_rR = 0;
byte flag_wW = 0;
byte flag_h = 0;
char* arg_d = NULL; //liberare questa memoria o allocarla dinamicamente (!)
char* arg_D = NULL;
unsigned sleep_time = 0; 
int fd_sk; // = -1;
char *socket_name = NULL;


static void case_h();
static long isNumber(const char* s);
static int is_opt( char* arg, char* opt);
static int is_argument(char* c);
static int is_directory(const char *path);
static void write_in_directory(const char* dirname, int* n);
static void parser(int dim, char** array);
static int append_file(const char* s);
static int writeFile_request(const char* arg_W);
static void case_w(char* arg_w);
static void case_l( char* arg_l);
static void case_u( char* arg_u);
static void case_c( char* arg_c);
static void set_socket(const char* socket_name);


//prototipi API client
int openConnection(const char* sockname, int msec, const struct timespec abstime);
int openFile(const char* sockaname, int flags);
//da implementare:
int closeConnection(const char* sockname){return 0;};
int readFile(const char* f_name, void** buf, size_t* size){return 0;};
int readNFiles(int n, const char* dirname ){return 0;};
int writeFile(const char* f_name, char* arg){return 0;};
int appendToFile(const char* f_name, char* buf, int dim_buf, char* arg){return 0;};
int lockFile(const char* f_name){return 0;};
int unlockFile(const char* f_name){return 0;};
int remvoveFile(const char* f_name){return 0;};

/*_____________________ FUNZIONI AUSILIARIE _____________________*/
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


/*_____________________ SCRITTURE _____________________*/

// CASE_W 
static void case_W (char* arg_W)
{
	//(!) problema: per ogni parola tokenizzata va effettuata una richiesta di scrittura al server
	//si puo gestire direttamente da qui?
	//rivaluta dopo l'implementazione dell'API

	char* save = NULL;
	char* token = strtok_r(arg_W, ",", &save);
	while (token){
		//su ogni token che rappresenta il nome del file invio una richiesta di scrittura
		printf("richiesta di scrittura su %s\n", token);
		writeFile_request(token);
		token = strtok_r(NULL, ",", &save);
	}
}

// CASE_w
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
		write_in_directory(dirname, &x);
}

// write_in_directory -> ausiliaria di case_w
static void write_in_directory(const char* dirname, int* n)
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
				write_in_directory(path, n);
		//passo 4 caso 2: richiesta di scrittura
		}else{
			if (writeFile_request(entry->d_name) != 0){ 
				LOG_ERR(-1, "errore write_request"); 
				exit(EXIT_FAILURE); 
			}
			(*n)--;
		}
	}
	//chiudere la directory
	ec_meno1(closedir(d), "errore su closedir");
}

// WRITE_REQUEST 
static int writeFile_request(const char* f_name)
{
 
      // sulla base dell'esito di openFile si dovrà chiamare la rispettiva procedura dell'API
      // casi:
      // 1. scrittura di un file non esistente  -> writeFile API     -> openFile con O_CREATE
      // 2. scrittura di un file esistente      -> appendToFile API  -> openFile senza O_CREATE 

      //O_CREATE -> si vuole creare un nuovo file -> fallisce se il file già esiste
      //O_LOCK -> si vuole aprire o creare in modalità locket -> fallisce se il file è gia lockato

	if (openFile(f_name, O_CREATE|O_LOCK) != -1 ){
            //writeFile(f_name, arg_d);					
		printf("DEBUG CLIENT: openFile scrittura nuovo file\n");
	}else{
		if (openFile(f_name, O_LOCK) != -1){
		      printf("DEBUG CLIENT: openFile scrttura file esistente\n");
		      //append_file(f_name);
           }
      }
	return 0;
}

// append_file chiama appedToFile
static int append_file(const char* f_name)
{
	// necess. allocare un buf(array di char) che contiene il testo del file e ha quindi dimensione in byte del file
	// ricavare le dimensioni in byte del file con stat
	// scrittura del buf e conseguente appendToFile

	//apertura del file
	int fd;
	ec_meno1((fd = open(f_name, O_RDONLY)), "open in append_file");

	//stat per ricavare dimensione file
	struct stat path_stat;
    	ec_meno1(lstat(f_name, &path_stat), "errore stat");

    	//allocazione buf[size]
    	size_t file_size = path_stat.st_size;
    	printf("file_size = %zu\n", file_size);
    	char* buf;
 	ec_null((buf = calloc(file_size, sizeof(char))) , "malloc in append_file");

 	//lettura file + scrittura del buf
 	ec_meno1(read(fd, buf, file_size), "read");
 	ec_meno1(close(fd), "errore close in append_file");

 	//appendToFile per scrittura f_name atomica
 	ec_meno1(appendToFile(f_name, buf, file_size, arg_d), "appendToFile in append_file");

 	//cleanup
 	if(!buf) free(buf);
 	return 0;
}



/*_____________________ ALTRO _____________________*/

// CASE_R
static void case_r (char* arg_r)
{
	//(!)problema: per ogni parola tokenizzata va effettuata una richiesta di scrittura al server
	//si puo gestire direttamente da qui?
	//rivaluta dopo l'implementazione dell'API

	char* save = NULL;
	char* token = strtok_r(arg_r, ",", &save);
	while (token){
		//su ogni toker che rappresenta il nome del file invio una richiesta di scrittura
		printf("richiesta di lettura su: %s\n", token);
		void **buf;
		size_t* size;
		ec_meno1(readFile(token, buf, size), "readFile fallita");
		token = strtok_r(NULL, ",", &save);
	}
}

// CASE_L
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

// CASE_U
static void case_u(char* arg_u)
{
	char* save = NULL;
	char* token = strtok_r(arg_u, ",", &save);
	while (token){
		//su ogni token che rappresenta il nome del file invio una richiesta di lock sul file
		printf("richiesta di unlock su: %s\n", token);
		ec_meno1(unlockFile(token), "unlockFile fallita");
		token = strtok_r(NULL, ",", &save);
	}
}

// CASE_C 
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

// SET_SOCKET
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

// CASE_H  
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

// PARSER
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



/*_____________________ API CLIENT _____________________*/

//FUNZIONE AUSILIARIA API (tempo trascorso tra t1,t2)
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
    
	fd_sk = socket(AF_UNIX, SOCK_STREAM, 0); 
	printf("fd_socket client = %d\n", fd_sk);
	
	//ciclo di connessione
      while(connect(fd_sk, (struct sockaddr*)&sa, sizeof(sa)) == -1){
    	      if(errno == ENOENT){
			usleep(msec);
			//seconda misurazione
			ec_meno1(clock_gettime(CLOCK_REALTIME, &time2), "client: clock_gettime fallita");
			//calcola tempo trascorso tra time1 e time2 in delta
			sub_timespec(time1, time2, &delta);
			//controllo che non si sia superato il tempo assoluto dal primo tentativo di connessione
			if( (delta.tv_nsec >= abstime.tv_nsec && delta.tv_sec >= abstime.tv_sec) || delta.tv_sec >= abstime.tv_sec){
				printf("APIclient.OpenConnection: abstime scaduto, connessione fallita\n");
				return -1;
			}
       	}
      	else return -1; 
      }
	//write(fd_sk, "hello!", 6);
      //size_t N = 100;
      //char buf[N];
      //read(fd_sk, buf, N);
	//close(fd_skt);
	//if(fd_skt != -1) printf("DEBUG: connessione con server stabilita\n");
   	//printf("DEBUG: fd_client = %d\n", fd_skt);
      return 0;
}

//OPEN FILE
int openFile(const char* pathname, int flags)
{
      //nota: ogni volta che si invia un'informazione lato client, questa deve essere letta e 
      //confermata la ricezione lato server, e infine ricevuta la conferma lato client
      //protocollo: C/1(invio dato) -> S/2(ricezione dato) S/3 (invio conferma ric. dato) -> C/4(ric. conf. ric dato)
	int* buf;
	ec_null( (buf = malloc(sizeof(int))), "client: malloc fallita");
	*buf = 0;
	
      //SETTING RICHIESTA
      //1 comunica: il tipo di richiesta -> 1 per OpenFile
	*buf = 1;
	ec_meno1(write(fd_sk, buf, sizeof(int)), "client: write fallita");
      //3 riceve: conferma accettazione richiesta openFile (1)
	*buf = 0;
	ec_meno1(read(fd_sk, buf, sizeof(int)), "client: read fallita");
	if(*buf != 1) return -1;

      // comunicazione stabilita OK
      // invio dati richiesta al server
      
      //LEN PATHNAME
	//4 comunica: invia lunghezza pathname
	int len = strlen(pathname);
	*buf = len;
	ec_meno1(write(fd_sk, buf, sizeof(int)), "client: write fallita");
      //7 riceve: conferma ricezione pathname
	ec_meno1(read(fd_sk, buf, sizeof(int)), "client: read fallita");
	if(buf != 0) return -1;

	//PATHANAME
	//8 comunica: pathname
	ec_meno1(write(fd_sk, pathname, sizeof(int)), "client: write fallita");
	//11 riceve: conferma ricezione pathname
      ec_meno1(read(fd_sk, buf, sizeof(int)), "client: read fallita");
	if(buf != 0) return -1;

      //FLAGS
      //12 comunica: flags
	*buf = flags;
	ec_meno1(write(fd_sk, buf, sizeof(int)), "client: write fallita");
	//15 riceve: conferma ricezione flags
      ec_meno1(read(fd_sk, buf, sizeof(int)), "client: read fallita");
	if(buf != 0) return -1;

      //IDENTIFICAZIONE PROCESSO
      int id = getpid();
      //16 comunica: pid
      *buf = id;
	ec_meno1(write(fd_sk, buf, sizeof(int)), "client: write fallita");
      //19 riceve: conferma ricezione pid
      ec_meno1(read(fd_sk, buf, sizeof(int)), "client: read fallita");
	if(buf != 0) return -1;

      //ORA? ASPETTO CHE ELABORI LA RICHIESTA E CHE RITORNI L'ESITO
	
      return 0;
}