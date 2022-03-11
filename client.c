#include "err_cleanup.h"


//variabili globali
int flag_D = 0;
int flag_d = 0;
int flag_p = 0;
int flag_rR = 0;
int flag_wW = 0;
int time_r = 0;
int flag_h = 0;
char *socket_name = NULL;
char* arg_d = NULL;
char* arg_D = NULL;



static void case_h();
static long isNumber(const char* s);
static int is_opt( char* arg, char* opt);
static int is_argument(char* c);
static int is_directory(const char *path);
static void visitFolder_sendRequest(const char* dirname, int* n);
static void parser(int dim, char** array);
static int append_file(const char* s);
static int write_request(const char* arg_W);
static void case_w(char* arg_w);
static void case_l( char* arg_l);
static void case_u( char* arg_u);
static void case_c( char* arg_c);
static void set_socket(const char* socket_name);


//funzioni test per append_file -  esito positivo 0 / errore -1
int appendToFile(const char* f_name, char* buf, int dim_buf, char* arg){ return 0;}
int writeFile(const char* f_name, char* arg){ return 0; }
int openFile(const char* f_name, int n){ int d; return 0; }
int readFile(const char* f_name, void** buf, size_t* size){ return 0; }
int lockFile(const char* f_name){ return 0; }
int unlockFile(const char* f_name){ return 0; }
int remvoveFile(const char* f_name){ return 0; }
int readNFiles(int n, const char* dirname ){ return 0; }
int openConnection(const char* sockname, int msec, const struct timespec abstime){ return 0; }



////////////////// isNumber //////////////////
static long isNumber(const char* s)
{
	char* e = NULL;
   	long val = strtol(s, &e, 0);
   	if (e != NULL && *e == (char)0) return val; 
	return -1;
}

////////////////// is_opt //////////////////
static int is_opt( char* arg, char* opt)
{
	if(strcmp(arg, opt) == 0) return 1;
	else return 0;
}


////////////////// is_argument //////////////////
static int is_argument(char* c)
{
	if (c == NULL || (*c == '-') ) return 0;
	else return 1;
}

////////////////// is_directory //////////////////
static int is_directory(const char *path)
{
    struct stat path_stat;
    ec_meno1(lstat(path, &path_stat), "errore stat");
    return S_ISDIR(path_stat.st_mode);
}

////////////////// case_h //////////////////
static void case_h()
{
	int fd;
	ec_meno1((fd = open("help.txt", O_RDONLY)), "errore open in case_h");
	
	struct stat path_stat;
    	ec_meno1(lstat("help.txt", &path_stat), "errore stat");

    	//allocazione buf[size]
    	size_t file_size = path_stat.st_size;
    	char* buf;
 	ec_null((buf = calloc(file_size, sizeof(char))) , "malloc in append_file");

 	//lettura file + scrittura del buf
 	ec_meno1(read(fd, buf, file_size), "read");
 	//stampa buf
 	for(int i = 0; i < file_size; i++) printf("%c", buf[i]);
 	
 	ec_meno1(close(fd), "errore close in append_file");
 	if(!buf) free(buf);
}

////////////////// case_W //////////////////
static void case_W (char* arg_W)
{
	//(!)problema: per ogni parola tokenizzata va effettuata una richiesta di scrittura al server
	//si puo gestire direttamente da qui?
	//rivaluta dopo l'implementazione dell'API

	char* save = NULL;
	char* token = strtok_r(arg_W, ",", &save);
	while (token){
		//su ogni toker che rappresenta il nome del file invio una richiesta di scrittura
		printf("richiesta di scrittura su %s\n", token);
		write_request(token);
		token = strtok_r(NULL, ",", &save);
	}
}

////////////////// case_r //////////////////
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


////////////////// append_file //////////////////
static int append_file(const char* f_name)
{
	//necess. allocare un buf(array di char) che contiene il testo del file e ha quindi dimensione in byte del file
	//ricavare le dimensioni in byte del file con stat
	//scrittura del buf e conseguente appendToFile

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

////////////////// write_request //////////////////
static int write_request(const char* f_name)
{
	//openFile/appendToFile/writeFile funzionamento:
	//openFile apre un file -> il file puo essere gia presente sul server oppure va creato
	//se esiste gia lo si scrive (aggiorna) con appendToFile atomicamente
	//se si tratta di un nuovo file, lo si scrive in lock con writeFile

	if (/*openFile(entry->d_name, O_CREATE||O_LOCK)*/ openFile(f_name, 1) != 0){
		printf("file esistente -> scrittura in append\n");
				
		//file già esistente -> scrive (aggiorna) con append_file -> appendToFile
		//(!) errno dovrebbe indicare che la open ha fallito perché il file esiste gia e questa condizione si usa nell'if
		if (append_file(f_name) != 0){ 
			LOG_ERR(-1, "append_file fallita"); 
		}
	}else{
		//file non presente sul server -> nuovo file da scrivere in lock
		printf("file non esistente -> scrittura con writeFile\n");
		//apertura file in lock		
		ec_meno1(/*openFile(entry->d_name, O_LOCK)*/openFile(f_name, 2), "openFile fallita");
		//file aperto o creato in mod locked
		ec_meno1(writeFile(f_name, arg_d), "writeFile error");						
	}
	return 0;
}

////////////////// visit_folder_send_requst //////////////////
static void visitFolder_sendRequest(const char* dirname, int* n)
{
	//passo 1: apertura directory dirname
	DIR* d;
	ec_null((d = opendir(dirname)), "errore su opendir");
	printf("dir '%s' aperta\n", dirname);

	//passo 2: lettura della directory
	struct dirent* entry;
	while (errno == 0 && ((entry = readdir(d)) != NULL) && *n != 0) {
		//passo 3 : aggiornamento path
		char path[1024];
		snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);
		
		//passo 4 caso 1: controlla che se si tratta di una dir
		if (is_directory(path)){
			
			//controlla che non si tratti della dir . o ..
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
				//visita ricorsiva su nuova directory
				visitFolder_sendRequest(path, n);
		//passo 4 caso 2: richiesta di scrittura
		}else{
			if (write_request(entry->d_name) != 0){ 
				LOG_ERR(-1, "errore write_request"); 
				exit(EXIT_FAILURE); 
			}
			(*n)--;
		}
	}
	//chiudere la directory
	ec_meno1(closedir(d), "errore su closedir");
}


////////////////// case_h //////////////////
static void case_w(char* arg_w)
{
		//in seguito in dirname salvo la directory estratta da arg_w
		char* dirname;
		//verifica arg opzionale 
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
		visitFolder_sendRequest(dirname, &x);
}


////////////////// case_l //////////////////
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

////////////////// case_u //////////////////
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

////////////////// case_c //////////////////
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

static void set_socket(const char* socket_name)
{
	int rec_time = 500; //richiesta di riconnessione ogni 500 millisecondi = 0.5 secondi
	int time_out = 10; //10 secondi prima di terminare

	struct timespec timer; // struttura che memorizza i sec/ns passati da quando si chiama clock_gettime

	//prelevo l'orario di sistema salvato in ts
        ec_meno1(clock_gettime(CLOCK_REALTIME, &timer), "clock clock_gettime fallita");


        //tempo attuale + time_out
        timer.tv_sec += time_out;  

        if(openConnection(socket_name, rec_time, timer) == -1){
        	LOG_ERR(errno, "-f tentativo di connessione non riuscito");
        }
}

////////////////// parser //////////////////
static void parser(int dim, char** array){

	//ciclo preliminare per controllare -h e terminare immediatamente se presente
	int i = 0;
	while (++i < dim){
		//CASO -h
		if (is_opt(array[i], "-h")) {
			case_h();
			exit(0);
		}
	}
	
	//il parsing è suddiviso in due cicli (si scandisce argv del main)
	//CICLO 1: si gestiscono i comandi di setting -f, -t, -p, -d, -D
	i = 0;
	while (++i < dim){
		
		//CASO-f
		if (is_opt(array[i], "-f")) {
			//argomento obbligatorio
			if (is_argument(array[i+1])){ 
				socket_name = array[++i];
				set_socket(socket_name);
			}else{ 
				LOG_ERR(EINVAL, "argomento -f mancante");
				exit(EXIT_FAILURE); 
			}
		}
		
		//CASO -t
		if (is_opt(array[i], "-t")) {
			if ( !is_argument(array[i+1]) ) time_r = 0;
			else ec_meno1( (time_r = isNumber(array[++i])), "argomento -t errato");
		}	

		//CASO -p
		if (is_opt(array[i], "-p")){
			flag_p = 1;
			i++;
		}


		//CASO -D
		if (is_opt(array[i], "-D")) {
			if (is_argument(array[i+1])) {
				flag_D = 1;
				arg_D = array[++i];
			}else{
				LOG_ERR(EINVAL, "argomento -D mancante");
				exit(EXIT_FAILURE);
			}
		}
		//CASO -d 
		if (is_opt(array[i], "-d")) {
			if (is_argument(array[i+1])) {
				flag_d = 1;
				arg_d = array[++i];
			}else{
				LOG_ERR(EINVAL, "argomento -d mancante");
				exit(EXIT_FAILURE);
			}
		}

		//controllo dipendenza -d da -w/-W e -D da -r/-R
		if (is_opt(array[i], "-w") || is_opt(array[i], "-W")) flag_wW = 1;
		if (is_opt(array[i], "-r") || is_opt(array[i], "-R")) flag_rR = 1;
	}

	//controllo dipendeza
	if(flag_D && !flag_wW){ LOG_ERR(EINVAL, "-d attivo -w/-W non attivi"); exit(EXIT_FAILURE); }
	if(flag_d && !flag_rR){ LOG_ERR(EINVAL, "-D attivo -r/-R non attivi"); exit(EXIT_FAILURE); }

	//CICLO 2: si gestiscono i comandi di richiesta al server -w, -W, -R, -r, -l, -u, -c
	i = 0;

	while (++i < dim){	

		//CASO -w
		if (is_opt(array[i], "-w")) {		
			//2) verifica arg. obbligatorio
			if (is_argument(array[i+1])) {
				case_w(array[++i]);
			}else{ 
				LOG_ERR(EINVAL, "argomento mancante -w");
				exit(EXIT_FAILURE);
			}	
		}
		
		//CASO -W
		if (is_opt(array[i], "-W")) {
			//argomento obbligatorio	
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

		//CASO -t
		if (is_opt(array[i], "-t")){
			//default: time_r=0 altrimenti time_r = x
			if (is_argument(array[i+1]))
				ec_meno1((time_r = isNumber(array[i+1])), "errore: arg. -w n non intero\n");
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
		
	}
}


////////////////// main //////////////////
int main(int argc, char* argv[]){
	
	//controllo su argc
	if( argc == 1){
		LOG_ERR(EINVAL, "errore su argomenti\n");
	}
	
	//parser
	parser(argc, argv);

	return 0;
}
