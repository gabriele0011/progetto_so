/*
-> timespec da implementare nelle richieste -> inserire una usleep controllata dal flag_p in ogni funzione API 
*/

#include "client.h"
static int status_conn = 0;

//scritture
static void case_write_W (char* arg_W)
{
	//printf("(CLIENT) - case_write_W\n"); //DEBUG
	char* save = NULL;
	char* token = strtok_r(arg_W, ",", &save);
	while (token){
		write_request(token);
		token = strtok_r(NULL, ",", &save);
	}
	return;
}
static void case_write_w(char* arg_w)
{	
		char* token;
		token = strtok(arg_w, ",");
		if (token == NULL) return;

		char* dir = token;

		token = strtok(NULL, ",");
		int N;
		if(token != NULL){
			if ( (N = is_number(token)) == -1 || N < 0){ LOG_ERR(EINVAL, "case_write_w: arg. -w non valido\n"); return; }
			if(N == 0) N = -1;	
		}else{
			N = -1;
		}
		//printf("DEBUG - case_write_w con paramentri: dir:%s n=%d\n", dir, N); //DEBUG
		send_from_dir(dir, &N);
		return;
}
static void send_from_dir(const char* dirname, int* n)
{
	//passo 1: apertura directory dirname
	DIR* d;
	if ((d = opendir(dirname)) == NULL){ LOG_ERR(errno, "opendir in send_from_dir"); return; }
	printf("(CLIENT) - send_from_dir:	dir '%s' aperta\n", dirname); //DEBUG
	//passo 2: lettura della directory
	struct dirent* entry;
	while (errno == 0 && ((entry = readdir(d)) != NULL) && *n != 0) {
		
		//passo 3 : aggiornamento path
		char path[PATH_MAX];
		memset((void*)path, '\0', (sizeof(char)*PATH_MAX));
		snprintf(path, PATH_MAX , "%s/%s", dirname, entry->d_name);
		//passo 4 caso 1: controlla che se si tratti di una dir
		if (is_directory(path)){	
			//controlla che non si tratti della dir . oppure ..
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
				//visita ricorsiva su nuova directory
				send_from_dir(path, n);
		//passo 4 caso 2: richiesta di scrittura
		}else{
			//write_request
			printf("send_from_dir su %s\n", path);
			write_request(path);
			errno = 0;
			(*n)--;
		}
	}
	//chiudere la directory
	if (closedir(d) == -1){ LOG_ERR(errno, "closedir in send_from_dir"); }
	return;
}
static void write_request(const char* f_name)
{
	printf("(CLIENT) - write_request:	su %s \n", f_name); //DEBUG

	//openFile + writeFile
	if (openFile(f_name, (O_CREATE|O_LOCK)) != 0){
		LOG_ERR(errno, "openFile");
	}else{
		if (writeFile(f_name, arg_D) != 0){
			if(errno == ENOENT){
				removeFile(f_name);	//elimina file vuoto creato con openFile
				remove_node(&open_files_list, f_name);
			}
			LOG_ERR(errno, "writeFile");
		}
		return;
	}

	//openFile + appendToFile
	if (openFile(f_name, 0) != 0){
		LOG_ERR(errno, "openFile");
	}else
		append_request(f_name);

	//printf("(CLIENT) - write_request:	terminata\n\n"); //DEBUG
	return;
}
static int append_request(const char* f_name)
{
	//apertura del file
	int fd;
    char* buf = NULL;
	ec_meno1_c((fd = open(f_name, O_RDONLY)), "open in append_request", ar_clean);
	
	//stat per ricavare dimensione file
	struct stat path_stat;
    ec_meno1_c(lstat(f_name, &path_stat), "lstat in append_request", ar_clean);
	
	//allocazione buf[size]
    size_t file_size = path_stat.st_size;
 	ec_null_c((buf = calloc(file_size, sizeof(char))) , "append_request", ar_clean);

 	//lettura file + scrittura del buf
	ec_meno1_c(read(fd, buf, file_size), "read in append_request", ar_clean);
	ec_meno1_c(close(fd), "close in append_request", ar_clean);

    	path_dir = NULL;

 	//appendToFile per scrittura f_name atomica
 	if (appendToFile(f_name, buf, file_size, arg_D) != 0){
		LOG_ERR(errno, "appendToFile");
		goto ar_clean;
	}
	//printf("(CLIENT) - append_request	avvenuta su %s\n", f_name); //DEBUG
 	if (buf) free(buf);
 	return 0;

	ar_clean:
	if (buf) free(buf);
 	return -1;
}

//scrittura di un file in una directory specificata in pathname
static void writefile_in_dir(char* path, size_t len_path, size_t size_data, char* data, const char* dest_dir)
{
	char pathname[len_path];
	//memset((void*)pathname, '\0', sizeof(char)*len_path);
	strncpy(pathname, path, len_path);
	pathname[len_path] = '\0';
	// apertura directory dirname
	DIR* d;
	if ((d = opendir(dest_dir)) == NULL){
		if (errno == ENOENT){
			mkdir(dest_dir, 0777);
		}
	}

	ec_meno1_v(chdir(dest_dir), "chdir 1 in writefile_in_dir");
	
	//tokenizzazione path + creazione percorso file
	char* token = strtok(pathname, "/");
	int hop_start_dir = 1;
	char* temp = NULL;
	
	while (token != NULL){
		temp = token;
		token = strtok(NULL, "/");
		if(token != NULL){
			mkdir(temp, 0777);
			/*
			if (mkdir(temp, 0777) != 0){
				 if(errno == EEXIST)
					LOG_ERR(errno, "mkdir in writefile_in_dir");
			}
			*/
			hop_start_dir++;
			ec_meno1_v(chdir(temp), "chdir in writefile_in_dir");
		}
	}
	if(temp == NULL) return;
	char string[PATH_MAX];
	memset((void*)string, '\0', sizeof(char)*PATH_MAX);
	strcpy(string, temp);

	//crea nuovo file nella dir corrente e scrivi (!)
	FILE* ifp;
	if ( (ifp = fopen(string, "w")) == NULL){
		LOG_ERR(errno, "fopen in writefile_in_dir");
		return;  
	}
	if (fwrite(data, sizeof(char), size_data, ifp) == -1){
		LOG_ERR(errno, "fwrite in writefile_in_dir");
		return;
	}
	if (fclose(ifp) == -1){ LOG_ERR(errno, "fclose in writefile_in_dir"); return; }
	//printf("SCRITTURA DI: string: %s - len_string= %zu\n", string, strlen(string));
	//ritorno alla directory di partenza
	if(flag_p) printf("(CLIENT) - writefile_in_dir:	su %s avvenuta in %s\n", string, dest_dir); //DEBUG
	while(hop_start_dir > 0){
		ec_meno1_v(chdir(".."), "chdir in writefile_in_dir");
		hop_start_dir--;
	}

	if (d != NULL && closedir(d) == -1){ LOG_ERR(errno, "closedir in writefile_in_dir"); return; }
	return;
}

//read
static void read_request (char* arg_r)
{
	//settare errno
	size_t size; //
	void *buf = NULL;
	char* token = strtok(arg_r, ",");

	while (token){
		if (openFile(token, 0) != 0){
			LOG_ERR(errno, "openFile:");
		}else{
			if (readFile(token, &buf, &size) != 0){
				LOG_ERR(errno, "readFile");
			}else{
				if(arg_d != NULL){
					writefile_in_dir(token, strlen(token), size, (char*)buf, arg_d);
					if(buf) free(buf);
				}
			}
		}
		token = strtok(NULL, ",");
	}
	return;
}

//lock/unlock
static void lock_request(char* arg_l)
{
	char* token = strtok(arg_l, ",");
	while (token){
		//su ogni token che rappresenta il nome del file invio una richiesta di lock sul file
		if(openFile(token, 0) != 0){
			LOG_ERR(errno, "openFile");
		}else{
			if (lockFile(token) != 0){
				LOG_ERR(errno, "lockFile");
			}
		}
		token = strtok(NULL, ",");
	}
	return;
}

static void unlock_request(char* arg_u)
{
	char* token = strtok(arg_u, ",");
	while (token){
		if ( openFile(token, 0) != 0){ 
			LOG_ERR(errno, "openFile");
		}else{
			if (unlockFile(token) != 0){
				LOG_ERR(errno, "lockFile");
			}
		}
		token = strtok(NULL, ",");
	}
	return;
}
//rimozione file
static void remove_request(char* arg_c)
{
	//setta errno
	char* token = strtok(arg_c, ",");
	while (token != NULL){
		//su ogni token che rappresenta il nome del file invio una richiesta di lock sul file
		printf("richiesta di cancellazione su: %s\n", token);
		if(openFile(token, 0) != 0){
			LOG_ERR(errno, "openFile:");
		}else{
			if (removeFile(token) != 0){ LOG_ERR(errno, "removeFile");}
		}
		token = strtok(NULL, ",");
	}
}
//routine chiusura file aperti
void file_closing(node* open_files_list)
{
	while(open_files_list != NULL){
		if(closeFile(open_files_list->str) != 0 ){ LOG_ERR(errno, "closeFile"); }
		open_files_list = open_files_list->next;
	}
	return;
}
//connessione
static int set_socket(const char* socket_name)
{
	//necessario effettuare un controllo sulla stringa socket_name?
	//deve avere una dimensione specifica?

	//setting parametri di connessione
	double rec_time = 1000; //tempo di riconnessione ogni 1 sec = 1/1000 msec
	struct timespec abstime;
	abstime.tv_sec = 10; //tempo assoluto 10 secondi

	//open connection
   	if(openConnection(socket_name, rec_time, abstime) == -1){
    		LOG_ERR(errno, "openConnection:");
			return -1;
    }
	status_conn = 1;
	return 0;
}
//help
static void help_message()
{
	int fd;
	if ((fd = open("help.txt", O_RDONLY)) == -1){ LOG_ERR(errno, "open in help_message"); return; }
	
	//struttura per acquisire informazioni sul file
	struct stat path_stat;
	if (lstat("help.txt", &path_stat) == -1){ LOG_ERR(errno, "lstat in help_message"); return; }

    //allocazione buf[size] per la dimensione del file in byte
    size_t file_size = path_stat.st_size;
   	char* buf = NULL;
 	ec_null_c((buf = calloc(file_size, sizeof(char))) , "malloc in append_request", hm_clean);
	
 	//lettura file + scrittura del buf
 	if (read(fd, buf, file_size) == -1){ LOG_ERR(errno, "read in help_message"); goto hm_clean; }
 	
	//stampa buf
 	for (int i = 0; i < file_size; i++) printf("%c", buf[i]);

	if (close(fd) == -1){ LOG_ERR(errno, "close in help_message"); goto hm_clean; }

	if (buf) free(buf);
	hm_clean:
	if (buf) free(buf);
}
//parser
static void parser(int dim, char** array)
{
	byte flag_wW = 0;			//flags controllo dipendenze
	byte flag_rR = 0;			
	char* socket_name = NULL;	
	//ciclo preliminare per controllare se presente -h e terminare immediatamente 
	int i = 0;
	while (++i < dim){
		//CASO -h
		if (is_opt(array[i], "-h")) {
			help_message();
			goto c_clean;
		}
		//CASO -p
		if (is_opt(array[i], "-p")){
			flag_p = 1;
			//i++;
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
		if(!status_conn){
			LOG_ERR(ECONNABORTED, "connessione non attiva, impossibile effettuare richieste al server");
			goto c_clean;
		}
		//CASO -t
		if (is_opt(array[i], "-t")) {
			if ( !is_argument(array[i+1]) ) sleep_time = 0;
			else sleep_time = is_number(array[++i]);
			printf("DEBUG: time_r = %d\n", sleep_time); //DEBUG
		}	

		//CASO -D
		if (is_opt(array[i], "-D")) {
			if (is_argument(array[i+1]))
				arg_D = array[++i];
			else{ 
				LOG_ERR(EINVAL, "argomento mancante -D");
				goto c_clean;
			}
		}	
		//CASO -d 
		if (is_opt(array[i], "-d")) {
			if (is_argument(array[i+1]))
				arg_d = array[++i];
			else{ 
				LOG_ERR(EINVAL, "argomento mancante -d");
				goto c_clean;
			}
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
			//caso in cui è presente n
			int x;
			if (is_argument(array[i+1])) { //con argomento
				if ((x=is_number(array[++i])) == -1 || x<0 ){
					LOG_ERR(EINVAL, "argomento -R non valido");
					goto c_clean;
				}
			}else{ //no argomento
				x = -1;
			}
			//printf("arg_d:%s && x=%d\n", arg_d, x);
			if (readNFiles(x, arg_d) != 0){ LOG_ERR(errno, "readNFiles"); }
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
	if (flag_p){
		printf("lista file aperti: ");
		print_list(open_files_list);
	}
	file_closing(open_files_list);
	dealloc_list(&open_files_list);
	closeConnection(socket_name);
	return;
	
	c_clean:
	if(status_conn){
		if (flag_p){
			printf("lista file aperti: ");
			print_list(open_files_list);
		}
		file_closing(open_files_list);
		dealloc_list(&open_files_list);
		closeConnection(socket_name);
	}
	return;
}

// MAIN
int main(int argc, char* argv[])
{	
	if(argc < 1){
		LOG_ERR(EINVAL, "main\n");
		exit(EXIT_FAILURE);
	}
	//parser
	parser(argc, argv);
	return 0;
}


//////////////////////////////////////  API CLIENT  //////////////////////////////////////

//OPEN CONNECTION
int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	//controllo sockname
	if(sockname == NULL){ errno=ENOTSOCK; return -1; }

	//timer setting
	msec = msec/1000; //conversione da millisecondi a microsecondi per la usleep
	struct timespec time1, time2, delta;
    ec_meno1_r(clock_gettime(CLOCK_REALTIME, &time1), "openConnection", -1);

    //socket setting
	struct sockaddr_un sa;
	size_t len_sockname =  strlen(sockname);
	memset((void*)sa.sun_path, '\0', len_sockname);
	strncpy(sa.sun_path, sockname, len_sockname);
	sa.sun_family = AF_UNIX;
	if(flag_p) printf("(CLIENT) - openConnection:	tentativo di connessione (socket: %s)...\n", sockname);
    
	if ((fd_sk = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) return -1;

	//ciclo di connessione
      while(connect(fd_sk, (struct sockaddr*)&sa, sizeof(sa)) == -1){
    	    if (errno == ENOENT){
				usleep(msec);
				//seconda misurazione
				ec_meno1_r(clock_gettime(CLOCK_REALTIME, &time2), "openConnection", -1);
				//calcola tempo trascorso tra time1 e time2 in delta
				sub_timespec(time1, time2, &delta);
				//controllo che non si sia superato il tempo assoluto dal primo tentativo di connessione
				if ((delta.tv_nsec >= abstime.tv_nsec && delta.tv_sec >= abstime.tv_sec) || delta.tv_sec >= abstime.tv_sec){
					errno = ETIMEDOUT;
					return -1;
				}
       		}else{
				errno = ECONNABORTED; // (!) errno adatta?
				return -1;
			}
    }
	if (fd_sk != -1 && flag_p) printf("(CLIENT) - openConnection:	connessione con server stabilita\n"); //DEBUG
    return 0;
}

//CLOSE CONNECTION 
int closeConnection(const char* sockname) 
{
	if (!sockname){
		errno = EINVAL;
		return -1;
	}
    	if (close(fd_sk) == -1 ){
		return -1;  
	}
	if (flag_p) printf("(CLIENT) - closeConnection:		connessione chiusa\n");	
    	return 0;
}

//OPEN FILE 1
int openFile(const char* pathname, int flag)
{ 
    
	//protocollo: C/1(invio dato) -> S/2(ricezione dato) -> S/3 (invio conferma ric. dato) -> C/4(ric. conf. ric dato)
	int* buffer;
	ec_null_c((buffer = malloc(sizeof(int))), "malloc in openFile", of_clean);
	*buffer = 0;

	//SETTING RICHIESTA
    //comunica: codifica openFile=1
	*buffer = 1;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write 1 in openFile", of_clean);
    //riceve: conferma accettazione richiesta
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read 1 in openFile", of_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto of_clean; }

    // comunicazione stabilita OK
    // invio dati richiesta al server
    //LEN PATHNAME
	//comunica: lunghezza pathname
	size_t len = strlen(pathname);
	*buffer = len;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write 2 in openFile", of_clean);
	//7 riceve: conferma ricezione pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read 2 in openFile", of_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto of_clean; }

	//PATHANAME
	//comunica: pathname
	ec_meno1_c(write(fd_sk, pathname, sizeof(char)*len), "write 3 in openFile", of_clean);
	//11 riceve: conferma ricezione pathname
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read 3 in openFile", of_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto of_clean; }

    //FLAGS
	//comunica: flags
	*buffer = flag;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write 4 in openFile", of_clean);
	//riceve: conferma ricezione flags
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read 4 in openFile", of_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto of_clean; }

    //IDENTIFICAZIONE PROCESSO
    int id = getpid();
    //comunica: pid
    *buffer = id;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write 5 in openFile", of_clean);
    //riceve: conferma ricezione pid
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read 5 in openFile", of_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto of_clean; }

	//dati inviati al server
	
	//RICEZIONE ESITO OPENFILE
	//riceve: esito openFile
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read 6 in openFile", of_clean);
	int ret = *buffer;
	//comunica: esito ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write 6 in openFile", of_clean);
	
	if(ret == 0 && search_node(open_files_list, pathname) == NULL) insert_node(&open_files_list, pathname);

	if(flag_p){
		printf("(CLIENT) - openFile	su file: %s / esito: ", pathname); //DEBUG
		if(ret != 0) printf("FALLITA\n"); //DEBUG
		else printf("SUCCESSO\n"); //DEBUG
	}
	

	//RICEZIONE DEL FILE EVENTUALMENTE ESPULSO
	//legge 1 se c'è un file espluso, 0 altrimenti
	*buffer = 0;
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read 7 in openFile", of_clean);
	//printf("(CLIENT) - openFile:	file espulso = %d\n", *buf); //DEBUG
	if(*buffer == 1){
		//len pathname
		size_t len_path;
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read 8 in openFile", of_clean);
		len_path = *buffer;
		//file name
		char path[len_path];
		memset((void*)path, '\0', sizeof(char)*len_path);
		ec_meno1_c(read(fd_sk, path, sizeof(char)*len_path), "read 9 in openFile", of_clean);
		//size data
		size_t size_data;
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read 10 in openFile", of_clean);
		size_data = *buffer;
		//data
		char arr_data[size_data];
		ec_meno1_c(read(fd_sk, arr_data, sizeof(char)*size_data), "read 11 in openFile", of_clean);
		//scrittura file espluso nella directory arg_D

		if(flag_p) printf("file espluso: %s - size_file: %zu\n", path, size_data); //DEBUG
		if(arg_D != NULL)
			writefile_in_dir(path, len_path, size_data, arr_data, arg_D);
	}

	if(buffer) free(buffer);
	if(ret != 0) errno = ret;
	return ret;
	
	of_clean:
	if(buffer) free(buffer);
	return -1;
}
 
//WRITE FILE 2
int writeFile(const char* pathname, const char* dirname)
{
	char* data_file = NULL;
	int* buffer;
	ec_null_c( (buffer = malloc(sizeof(int))), "writeFile: malloc fallita", wf_clean);
	*buffer = 0;

	//APERTURA E LETTURA FILE
	int fd;
	struct stat path_stat;
	//casi possibili
	if ((fd = open(pathname, O_RDONLY)) == -1){ goto wf_clean; }
	if (stat(pathname, &path_stat) == -1){ goto wf_clean; }

    //allocazione buf[size] per la dimensione del file in byte
    size_t file_size = path_stat.st_size;
 	ec_null_c((data_file = (char*)calloc(file_size, sizeof(char))) , "writeFile: calloc fallita", wf_clean)
	//lettura file + scrittura del buf
 	ec_meno1_c(read(fd, data_file, sizeof(char)*file_size), "writeFile: read fallita", wf_clean); //BUG QUI
 	ec_meno1_c(close(fd), "writeFile: close fallita", wf_clean);
	
	///////////////////////////////////////////////////////////////////////////////
	
	//SETTING RICHIESTA
	//comunica: codifica writeFile=2
	*buffer = 2;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "writeFile: write fallita", wf_clean);
    //riceve: conferma accettazione richiesta
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read fallita", wf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto wf_clean; }

    // invio dati richiesta al server 
      
    //LEN PATHNAME
	//comunica: lunghezza pathname
	size_t len = strlen(pathname);
	*buffer = len;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "writeFile: write fallita", wf_clean);
    //riceve: conferma ricezione pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read fallita", wf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto wf_clean; }

	//PATHANAME
	//comunica: pathname
	ec_meno1_c(write(fd_sk, pathname, sizeof(char)*len), "writeFile: write fallita", wf_clean);
	//riceve: conferma ricezione pathname
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read fallita", wf_clean);
	if(*buffer != 0){ goto wf_clean; }

	//DIMENSIONE DEL FILE
	//comunica: file size
	*buffer = file_size;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "writeFile: write fallita", wf_clean);
    //riceve: conferma ricezione pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read 2 fallita", wf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto wf_clean; }

	//DATA FILE
	//comunica: data file
	ec_meno1_c(write(fd_sk, data_file, sizeof(char)*file_size), "writeFile: write fallita", wf_clean);
    //riceve: conferma ricezione pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read fallita", wf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto wf_clean; }

    //IDENTIFICAZIONE PROCESSO
    int id = getpid();
    //comunica: pid
    *buffer = id;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "writeFile: write fallita", wf_clean);
    //riceve: conferma ricezione pid
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read fallita", wf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto wf_clean; }

	//dati inviati al server
	//printf("DEBUG writeFile operation data-> pathname: %s - len_p=%zu - file_size: %zu\n", pathname, len, file_size); //DEBUG
	///////////////////////////////////////////////////////////////////////////////
	
	//RICEZIONE ESITO WRITE FILE 
	//riceve: esito openFile
	int ret;
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read fallita", wf_clean);
	ret = *buffer;
	//22 (comunica): ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "writeFile: write 6 fallita", wf_clean);


	//RICEZIONE DEL FILE EVENTUALMENTE ESPULSO
	//legge 1 se c'è un file espluso, 0 altrimenti
	*buffer = 0;
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read fallita", wf_clean);
	
	if(flag_p){
		printf("(CLIENT) - writeFile	su file: %s / esito: ", pathname); //DEBUG
		if(ret != 0) printf("FALLITA\n"); //DEBUG
		else printf("SUCCESSO\n"); //DEBUG
	}


	if(*buffer == 1){
		//len pathname
		size_t len_path;
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read fallita", wf_clean);
		len_path = *buffer;
		
		//pathname
		char path[len_path];
		memset((void*)path, '\0', sizeof(char)*len_path);
		ec_meno1_c(read(fd_sk, path, sizeof(char)*len_path), "writeFile: read fallita", wf_clean);
		
		//size data
		size_t size_data;
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "writeFile: read fallita", wf_clean);
		size_data = *buffer;
		
		//data
		char arr_data[size_data];
		//ec_null_c((arr_data = (char*)calloc(sizeof(char), size_data)), "writeFile: calloc fallita", wf_clean);
		ec_meno1_c(read(fd_sk, arr_data, sizeof(char)*size_data), "writeFile: read fallita", wf_clean);
		
		if (flag_p) printf("file espluso: %s - size_file: %zu\n", path, size_data); //DEBUG
		//printf("(CLIENT) - writeFile: 	file espluso:%s - len=%zu - size_data: %zu\n", path, len_path, size_data); //DEBUG
		//scrittura file espluso nella directory arg_D
		if(dirname != NULL)
			writefile_in_dir(path, len_path, size_data, arr_data, dirname);
	}

	if(buffer) free(buffer);
	if(data_file) free(data_file);
	if(ret != 0) errno = ret;
	return ret;
	
	wf_clean:
	if(buffer) free(buffer);
	if(data_file) free(data_file);
	return -1;
}

//APPEND TO FILE 3
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
	int* buffer;
	ec_null_c( (buffer = malloc(sizeof(int))), "appendToFile: malloc fallita", atf_clean);
	*buffer = 0;

	//SETTING RICHIESTA
    //comunica al thread 3 (codifica appendToFile=2)
	*buffer = 3;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);
    //3 riceve: conferma accettazione richiesta openFile (1)
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto atf_clean; }

    // comunicazione stabilita
    // invio dati richiesta al server 
      
    //LEN PATHNAME
	//comunica: lunghezza pathname
	size_t len = strlen(pathname);
	*buffer = len;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);
    //riceve: conferma ricezione pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto atf_clean; }


	//PATHANAME
	//comunica: pathname
	ec_meno1_c(write(fd_sk, pathname, sizeof(char)*len), "appendToFile: write fallita", atf_clean);
	//11 riceve: conferma ricezione pathname
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto atf_clean; }


	//DIMENSIONE DEL FILE
	//comunica: file size
	*buffer = size;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);
    //riceve: conferma ricezione file size
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto atf_clean; }


	//DATA FILE
	//comunica: data file
	ec_meno1_c(write(fd_sk, (char*)buf, sizeof(char)*size), "appendToFile: write fallita", atf_clean);
    //riceve: conferma ricezione data file
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto atf_clean; }


    //IDENTIFICAZIONE PROCESSO
    int id = getpid();
    //comunica: pid
    *buffer = id;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);
    //riceve: conferma ricezione pid
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	if(*buffer != 0){ /*errno = EBADE;*/ goto atf_clean; }

	
	//printf("op_data_receive: %d / %s / flags / %d\n", len, pathname, id); //DEBUG
	//dati inviati al server
	///////////////////////////////////////////////////////////////////////////////
	
	//RICEZIONE ESITO
	//riceve: esito openFile
	int ret;
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	ret = *buffer;
	//comunica: ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "appendToFile: write fallita", atf_clean);

	if(flag_p){
		printf("(CLIENT) - appendToFile		su file: %s / esito: ", pathname); //DEBUG
		if(ret != 0) printf("FALLITA\n"); //DEBUG
		else printf("SUCCESSO\n"); //DEBUG
	}
	//RICEZIONE DEL FILE EVENTUALMENTE ESPULSO
	//legge 1 se c'è un file espluso, 0 altrimenti
	*buffer = 0;
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
	//printf("(CLIENT) - openFile:	file espulso = %d\n", *buf);
	
	if(*buffer == 1){
		//len pathname
		size_t len_path;
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
		len_path = *buffer;
		
		//pathname
		char path[len_path];
		memset((void*)path, '\0', sizeof(char)*len_path);
		ec_meno1_c(read(fd_sk, path, sizeof(char)*len_path), "appendToFile: read fallita", atf_clean);

		//size data
		
		size_t size_data;
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "appendToFile: read fallita", atf_clean);
		size_data = *buffer;
		
		//data
		char arr_data[size_data];
		ec_meno1_c(read(fd_sk, arr_data, sizeof(char)*size_data), "appendToFile: read fallita", atf_clean);
		
		printf("file espluso: %s - size_file: %zu\n", path, size_data); //DEBUG
		//printf("(CLIENT) - appendToFile: 	file espluso:%s - len_p=%zu - size_data: %zu\n", path, len_path, size_data); //DEBUG
		//scrittura file espluso nella directory arg_D
		if(dirname != NULL)
			writefile_in_dir(path, len_path, size_data, arr_data, dirname);
			//printf("(CLIENT) - openFile: file espluso = %s\n", path); //DEBUG
	}
	

	if(buffer) free(buffer);
	if(ret != 0) errno = ret;
	return ret;
	
	atf_clean:
	if(buffer) free(buffer);
	return -1;
}

//READ FILE 4
int readFile(const char* pathname, void** buf, size_t* size)
{
	int* buffer;
	ec_null_c( (buffer = malloc(sizeof(int))), "readFile: malloc fallita", rf_clean);
	*buffer = 0;

	//SETTING RICHIESTA
    //comunica: codifica readFile=4
	*buffer = 4;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "readFile: write fallita", rf_clean);
    //riceve: conferma accettazione richiesta
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "readFile: read fallita", rf_clean);
	if(*buffer != 0){  goto rf_clean; }
	
    // comunicazione stabilita OK
    // invio dati richiesta al server 

	//IDENTIFICAZIONE PROCESSO
    int id = getpid();
    //comunica: pid
    *buffer = id;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "readFile: write fallita", rf_clean);
    //riceve: conferma ricezione pid
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "readFile: read fallita", rf_clean);
	if(*buffer != 0){ goto rf_clean; }
      
	//LEN PATHNAME
	//comunica: invia lunghezza pathname
	size_t len_path = strlen(pathname);
	*buffer = len_path;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "readFile: write fallita", rf_clean);
    //riceve: conferma ricezione pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "readFile: read fallita", rf_clean);
	if(*buffer != 0){ goto rf_clean; }

	//PATHANAME
	//comunica: pathname
	ec_meno1_c(write(fd_sk, pathname, sizeof(char)*len_path), "readFile: write fallita", rf_clean);
	//riceve: conferma ricezione pathname
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "readFile: read fallita", rf_clean);
	if(*buffer != 0){ goto rf_clean; }

	//RICEZIONE ESITO readFile
	// riceve: esito
	int ret;
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "readFile: read fallita", rf_clean);
	ret = *buffer;
	// comunica: conferma ricezione esito
	*buffer = 0;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "readFile: write fallita", rf_clean);

	//se il file non esiste, finisce qui
	if(ret != 0){
		errno = ret;
		if(flag_p) printf("(CLIENT) - readFile		su %s / esito: FALLITA\n", pathname); //DEBUG
		goto rf_clean;
	}else{

		//FILE SIZE
		//riceve: dimensione
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "readFile: read fallita", rf_clean);
		*size = (size_t)(*buffer); //cast dovrebbe essere implicito
		//comunica: conferma ricezione
		*buffer = 0;
		ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "readFile: write fallita", rf_clean);

		//DATA FILE
		//riceve: data
		ec_null_c((*buf = (char*)calloc(sizeof(char), *size)), "readFile: calloc fallita", rf_clean);
		ec_meno1_c(read(fd_sk, *buf, sizeof(char)*(*size)), "readFile: read fallita", rf_clean);
		//comunica: conferma ricezione
		*buffer = 0;
		ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "readFile: write fallita", rf_clean);

		if(flag_p) printf("(CLIENT) - readFile		lettura %s (%zu bytes) avvenuta\n", pathname, *size); //DEBUG
	}
	
	if(buffer) free(buffer);
	return 0;

	rf_clean:
	if(buffer) free(buffer);
	return -1;
	
}

//READ N FILES 5
int readNFiles(int N, const char* dirname)
{
	char* data_file = NULL;
	int* buffer;
	ec_null_c( (buffer = malloc(sizeof(int))), "malloc in readNFiles", rnf_clean);
	*buffer = 0;
	//int ret;

	//SETTING RICHIESTA    //comunica al thread 5 (codifica readNFiles=5)
	*buffer = 5;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);
    //riceve: conferma accettazione richiesta openFile (1)
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in readNFiles", rnf_clean);
	if(*buffer != 0){ goto rnf_clean; }

    // comunicazione stabilita OK
    // invio dati richiesta al server 

	//IDENTIFICAZIONE PROCESSO
    int id = getpid();
    //16 comunica: pid
    *buffer = id;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);
    //19 riceve: conferma ricezione pid
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in readNFiles", rnf_clean);
	if(*buffer != 0){ goto rnf_clean; }

    //NUMERO DI FILE DA LEGGERE
	//comunica: n
	*buffer = N;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);
    //riceve: conferma ricezione pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in readNFiles", rnf_clean);
	if(*buffer != 0){ goto rnf_clean; }

	int file_letti = 0;
	while(1){
		//stato di invio: 0 -> nessun file da ricevere ora | 1 -> un file da ricevere ora | 2 -> lettura terminata
		//printf("DIRNAME: %s\n", dirname);
		//riceve: stato di invio
		*buffer = 0;
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in readNFiles", rnf_clean);
		
		if(*buffer == 1){	//RICEVI UN FILE
			//comunica: ricevuto stato di invio
			*buffer = 0;
			ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);
			
			//LEN FILE NAME
			//riceve: lunghezza nome file
			size_t len_path;
			ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in readNFiles", rnf_clean);
			len_path = *buffer;
			//comunica: conferma ricezione
			*buffer = 0;
			ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);

			//FILE NAME
			//riceve: path del file
			char path[len_path];
			//memset((void*)path, '\0', sizeof(char)*len_path);
			ec_meno1_c(read(fd_sk, path, sizeof(char)*len_path), "read in readNFiles", rnf_clean);
			path[len_path] = '\0';
			//comunica: conferma ricezione
			*buffer = 0;
			ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);
			
			//FILE SIZE
			//riceve: dimensione
			size_t size_data;
			ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in readNFiles", rnf_clean);
			size_data = (size_t)*buffer;
			//comunica: conferma ricezione size_data
			*buffer = 0;
			ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);
	
			//DATA FILE 
			ec_null_c((data_file = (char*)calloc(sizeof(char), size_data)), "calloc in readNFiles", rnf_clean); //definizione diretta senza malloc?
			memset((void*)data_file, '\0', sizeof(char)*size_data);
			//riceve: data
			ec_meno1_c(read(fd_sk, data_file, sizeof(char)*size_data), "read in readNFiles", rnf_clean);
			//comunica: conferma ricezione data
			*buffer = 0;
			ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);
		
			//if(flag_p){ printf("(CLIENT) - readNFiles: letto: %s / %zu bytes / len_p %zu / dir:%s\n", path, size_data, len_path, dirname); } //DEBUG
			//scrittura del file in directory
			if (dirname != NULL){
				int N = strlen(dirname);
				char dir[N];
				strncpy(dir, dirname, N);
				dir[N] = '\0';
				writefile_in_dir(path, len_path, size_data, data_file, dir);
			}
			if (search_node(open_files_list, path) == NULL) insert_node(&open_files_list, path);
			file_letti++;
			if(data_file) free(data_file);
			data_file = NULL;
		}
	
		if(*buffer == 2){ //ATTENDI
			//comunica: ricezione stato di invio
			*buffer = 0;
			ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);
		}

		if(*buffer == 3){ //STOP
			//comunica: conferma ricezione fine procedura
			*buffer = 0;
			ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in readNFiles", rnf_clean);
			break;
		}

	}
	if(flag_p) printf("(CLIENT) - readNFiles	num. file letti = %d\n", file_letti); //DEBUG

	if(buffer) free(buffer);
	if(data_file) free(data_file);
	return 0;

	rnf_clean:
	if(buffer) free(buffer);
	if(data_file) free(data_file);
	return -1;
}

//LOCK FILE 6
int lockFile(const char* pathname)
{
	int ret;
	int* buffer;
	ec_null_c((buffer = malloc(sizeof(int))), "lockFile: malloc fallita", lf_clean);
	*buffer = 0;

	while(1){
		//SETTING RICHIESTA
      	//comunica: codifica lockFile=6
		*buffer = 6;
		ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "lockFile: write fallita", lf_clean);
     	//riceve: conferma accettazione richiesta openFile (1)
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "lockFile: read fallita", lf_clean);
		if(*buffer != 0){ goto lf_clean; }

      	// comunicazione stabilita OK
     	// invio dati richiesta al server
      
     	//LEN PATHNAME
		//comunica: invia lunghezza pathname
		size_t len = strlen(pathname);
		*buffer = len;
		ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "lockFile: write fallita", lf_clean);
     	//riceve: conferma ricezione pathname
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "lockFile: read fallita", lf_clean);
		if(*buffer != 0){ goto lf_clean; }

		//PATHANAME
		//comunica: pathname
		ec_meno1_c(write(fd_sk, pathname, sizeof(char)*len), "lockFile: write fallita", lf_clean);
		//riceve: conferma ricezione pathname
     	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "lockFile: read fallita", lf_clean);
		if(*buffer != 0){  goto lf_clean; }


    	//IDENTIFICAZIONE PROCESSO
      	int id = getpid();
      	//comunica: pid
      	*buffer = id;
		ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "lockFile: write fallita", lf_clean);
      	//riceve: conferma ricezione pid
      	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "lockFile: read fallita", lf_clean);
		if(*buffer != 0){ goto lf_clean; }

		//dati inviati al server
	
		//RICEZIONE ESITO
		//riceve: esito lockFile
		ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "lockFile: read fallita", lf_clean);
		ret = *buffer;
		//comunica: ricevuto
		*buffer = 0;
		ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "lockFile: write fallita", lf_clean);
		
		//stato di acquisizione lock
		if( *buffer == -2 ){
			int msec = LOCK_TIMER/1000;
			usleep(msec);
		}else{
			break;
		}
	}
	
	if(flag_p){
		printf("(CLIENT) - lockFile	su %s / esito: ", pathname); //DEBUG
		if(ret != 0) printf("FALLITA\n"); //DEBUG
		else printf("SUCCESSO\n"); //DEBUG
	}

	if(buffer) free(buffer);
	return ret;

	lf_clean:
	if(buffer) free(buffer);
	return -1;
}

//UNLOCK FILE 7
int unlockFile(const char* pathname)
{
	int* buffer;
	ec_null_c( (buffer = malloc(sizeof(int))), "unlockFile: malloc fallita", uf_clean);
	*buffer = 0;

	//SETTING RICHIESTA
    //comunica: codifica unlockFile=7
	*buffer = 7;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "unlockFile: write fallita", uf_clean);
    //riceve: conferma accettazione richiesta
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "unlockFile: read fallita", uf_clean);
	if(*buffer != 0){ goto uf_clean; }

    // comunicazione stabilita OK
    // invio dati richiesta al server
      
    //LEN PATHNAME
	//comunica: lunghezza pathname
	size_t len = strlen(pathname);
	*buffer = len;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "unlockFile: write fallita", uf_clean);
    //riceve: conferma ricezione pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "unlockFile: read fallita", uf_clean);
	if(*buffer != 0){ goto uf_clean; }

	//PATHANAME
	//comunica: pathname
	ec_meno1_c(write(fd_sk, pathname, sizeof(char)*len), "unlockFile: write fallita", uf_clean);
	//riceve: conferma ricezione pathname
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "unlockFile: read fallita", uf_clean);
	if(*buffer != 0){ goto uf_clean; }


    //IDENTIFICAZIONE PROCESSO
    int id = getpid();
    //comunica: pid
    *buffer = id;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "unlockFile: write fallita", uf_clean);
    //riceve: conferma ricezione pid
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "unlockFile: read fallita", uf_clean);
	if(*buffer != 0){ goto uf_clean; }

	//dati inviati al server
	
	//RICEZIONE ESITO OPENFILE
	//riceve: esito openFile
	int ret;
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "openFile: read fallita", uf_clean);
	ret = *buffer;
	//comunica: ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "unlockFile: write fallita", uf_clean);
	
	if(flag_p){
		printf("(CLIENT) - unlockFile	su %s / esito: ", pathname); //DEBUG
		if(ret != 0) printf("FALLITA\n"); //DEBUG
		else printf("SUCCESSO\n"); //DEBUG
	}

	if(ret != 0) errno = ret;
	if(buffer) free(buffer);
	return 0;

	uf_clean:
	if(buffer) free(buffer);
	return -1;
	
}

//UNLOCK FILE 8
int removeFile(const char* pathname)
{
	int* buffer;
	ec_null_c( (buffer = malloc(sizeof(int))), "removeFile: malloc fallita", rf_clean);
	*buffer = 0;

	//SETTING RICHIESTA
    //comunica: codifica removeFile=8
	*buffer = 8;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "removeFile: write fallita", rf_clean);
    //riceve: conferma accettazione richiesta
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "removeFile: read fallita", rf_clean);
	if(*buffer != 0){ goto rf_clean; }

    // comunicazione stabilita OK
    // invio dati richiesta al server
      
    //LEN PATHNAME
	//comunica: lunghezza pathname
	size_t len = strlen(pathname);
	*buffer = len;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "removeFile: write fallita", rf_clean);
    //riceve: conferma ricezione pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "removeFile: read fallita", rf_clean);
	if(*buffer != 0){  goto rf_clean; }

	//PATHANAME
	//comunica: pathname
	ec_meno1_c(write(fd_sk, pathname, sizeof(char)*len), "removeFile: write fallita", rf_clean);
	//riceve: conferma ricezione pathname
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "removeFile: read fallita", rf_clean);
	if(*buffer != 0){ goto rf_clean; }

    //IDENTIFICAZIONE PROCESSO
    int id = getpid();
    //comunica: pid
    *buffer = id;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "removeFile: write fallita", rf_clean);
    //riceve: conferma ricezione pid
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "removeFile: read fallita", rf_clean);
	if(*buffer != 0){ goto rf_clean; }

	//dati inviati al server
	
	//RICEZIONE ESITO OPENFILE
	//riceve: esito openFile
	int ret;
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "removeFile: read fallita", rf_clean);
	ret = *buffer;
	if(ret != 0) errno = ret;	//comunica: ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "removeFile: write fallita", rf_clean);
	
	if (flag_p){
		printf("(CLIENT) - removeFile	su %s / esito: ", pathname); //DEBUG
		if(ret != 0) printf("FALLITA\n"); //DEBUG
		else printf("SUCCESSO\n"); //DEBUG
	}	

	if(buffer) free(buffer);
	return ret;

	rf_clean:
	if(buffer) free(buffer);
	return -1;
}

//CLOSE FILE 9
int closeFile(const char* pathname)
{
	int* buffer;
	ec_null_c( (buffer = malloc(sizeof(int))), "malloc in closeFile", cf_clean);
	*buffer = 0;

	//SETTING RICHIESTA
    //comunica: codifica closeFile=9
	*buffer = 9;

	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in closeFile", cf_clean);
    //riceve: conferma accettazione richiesta 
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in closeFile", cf_clean);
	if(*buffer != 0){  goto cf_clean; }

    // comunicazione stabilita OK
    // invio dati richiesta al server
      
    //LEN PATHNAME
	//comunica: lunghezza pathname
	size_t len = strlen(pathname);
	*buffer = len;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in closeFile", cf_clean);
    //riceve: conferma ricezione lunghezza pathname
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in closeFile", cf_clean);
	if(*buffer != 0){  goto cf_clean; }

	//PATHANAME
	//comunica: pathname
	ec_meno1_c(write(fd_sk, pathname, sizeof(char)*len), "write in closeFile", cf_clean);
	//riceve: conferma ricezione pathname
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in closeFile", cf_clean);
	if(*buffer != 0){ goto cf_clean; }


    //IDENTIFICAZIONE PROCESSO
    int id = getpid();
    //comunica: pid
    *buffer = id;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in closeFile", cf_clean);
    //riceve: conferma ricezione pid
    ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in closeFile", cf_clean);
	if(*buffer != 0){ goto cf_clean; }

	//dati inviati al server

	//RICEZIONE ESITO
	//riceve: esito openFile
	int ret;
	ec_meno1_c(read(fd_sk, buffer, sizeof(int)), "read in closeFile", cf_clean);
	ret = *buffer;
	
	if(flag_p){
		printf("(CLIENT) - closeFile:	su %s / esito: ", pathname); //DEBUG
		if(ret != 0) printf("FALLITA\n"); //DEBUG
		else printf("SUCCESSO\n"); //DEBUG
	}

	//comunica: ricevuto
	*buffer = 0;
	ec_meno1_c(write(fd_sk, buffer, sizeof(int)), "write in closeFile", cf_clean);

	if(buffer) free(buffer);
	if(ret != 0) errno = ret;
	return ret;

	cf_clean:
	if(buffer) free(buffer);
	return -1;
}
