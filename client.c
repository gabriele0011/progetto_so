#include "err_cleanup.h"


//variabili globali
int flag_D = 0;
int flag_d = 0;
int flag_p = 0;
int flag_rR = 0;
int flag_wW = 0;
int time_r = 0;
int flag_h = 0;
char *socket_name = NULL; //controllo memoria
char* arg_D = NULL;
char* arg_d = NULL;


static void case_h();
static long isNumber(const char* s);
static int is_opt( char* arg, char* opt);
static int is_argument(char* c);
static int is_directory(const char *path);
static void visit_folder_send_request(const char* dirname, int* n);
static void case_w(char* arg_w);
static void parser(int dim, char** array);
static int append_file(const char* s);


////////////////// isNumber //////////////////
static long isNumber(const char* s)
{
	char* e = NULL;
   	long val = strtol(s, &e, 0);
   	if (e != NULL && *e == (char)0) return val; 
	return -1;
}


////////////////// case_h //////////////////
static void case_h()
{
	//apertura file
	FILE *fp;
	ec_null((fp = fopen("help.txt", "r")), "errore su fopen in case_h");
    /*
     	//controllo esito apertura
	if (!fp){
		perror("fopen");
		exit(EXIT_FAILURE);
    */
	//lettura file in array di n char
    	size_t n = 10;
	char buf[n];
    	//scandisco il file fino alla fine
	size_t temp;
	while (!feof(fp) && (temp = fread(buf, sizeof(char), n, fp)) != 0 ){
		for (int i = 0; i < temp; i++)
			printf("%c", buf[i]);
  	}
	if (fclose(fp) != 0){
		perror("errore fclose in case_h"); 
		exit(EXIT_FAILURE); 
	}
}


////////////////// is_opt //////////////////
static int is_opt( char* arg, char* opt)
{
	if(strcmp(arg, opt) == 0 && printf("opt %s riconosciuto\n", opt) ) return 1;
	else return 0;
	//elimina la stampa di opt -> qui solo per debug
}


////////////////// is_argument //////////////////
static int is_argument(char* c)
{
	if (c == NULL || (*c == '-') ) return 0;
	else return 1;
}


////////////////// mystrtok_r //////////////////
static void mystrtok_r (char* string)
{
	//(!)problema: per ogni parola tokenizzata va effettuata una richiesta di scrittura al server
	//si puo gestire direttamente da qui?
	//rivaluta dopo l'implementazione dell'API

	char* save = NULL;
	char* token = strtok_r(string, ",", &save );
	while (token){
		printf("%s\n", token);
		token = strtok_r(NULL, ",", &save);
	}
}


////////////////// is_directory //////////////////
static int is_directory(const char *path)
{
    struct stat path_stat;
    ec_meno1(lstat(path, &path_stat), "errore stat");
    return S_ISDIR(path_stat.st_mode);
}

int appendToFile(const char* f_name, char* buf, int dim_buf, char* arg){ int d; scanf("appendToFile: 0/1\n%d", &d); return d;}
int writeFile(const char* f_name, char* arg){ int d; scanf("wrteFile: 0/1\n%d", &d); return d;}
int openFile(const char* fname, int n){ int d; scanf("openFile: 0/1\n%d", &d); return d;}


////////////////// append_file //////////////////
static int append_file(const char* f_name)
{
	//necess. allocare un buf(array di char) che contiene il testo del file e ha quindi dimensione in byte del file
	//ricavare le dimensioni in byte del file con stat
	//scrittura del buf e conseguente appendToFile

	//apertura del file
	int fd;
	ec_meno1( (fd = open(f_name, O_RDONLY)), "open in append_file");

	//stat per ricavare dimensione file
	struct stat path_stat;
    	ec_meno1(lstat(f_name, &path_stat), "errore stat");

    	//allocazione buf[size]
    	size_t file_size = path_stat.st_size;;
    	printf("file_size = %zu\n", file_size);
    	char* buf;
 	ec_null( (buf = calloc(file_size, sizeof(char))) , "malloc in append_file");

 	//scrittura del buf
 	ec_meno1(read(fd, buf, file_size), "read");
 	for(int i = 0; i < file_size; i++) printf("%c", buf[i]);

 	ec_meno1(close(fd), "errore close in append_file");

 	//appendToFile per scrittura f_name atomica
 	ec_meno1(appendToFile(f_name, buf, file_size, arg_d), "appendToFile in append_file");

 	printf("fine append_file\n");
 	if(!buf) free(buf);
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
		}else{
			send_write_request(entry->d_name)
			//passo 4 caso 2: è un file
			printf("richiesta di scrittura file: %s\n", entry->d_name);

			//openFile/appendToFile/writeFile funzionamento:
			//openFile apre un file -> il file puo essere gia presente sul server oppure va creato
			//se esiste gia lo si scrive (aggiorna) con appendToFile atomicamente
			//se si tratta di un nuovo file, lo si scrive in lock con writeFile


			if (/*openFile(entry->d_name, O_CREATE||O_LOCK)*/ openFile(entry->d_name, 1) != 0){
				printf("file esistente -> scrittura in append\n");
				
				//file già esistente -> scrive (aggiorna) con append_file -> appendToFile
				if (errno == 0 && append_file(entry->d_name) != 0){ 
					LOG_ERR(-1, "append_file fallita"); 
				}
			}else{
				//file non presente sul server -> nuovo file da scrivere in lock
				printf("file non esistente -> scrittura con writeFile\n");
				
				ec_meno1(/*openFile(entry->d_name, O_LOCK)*/openFile(entry->d_name, 2), "openFile fallita");
				//file aperto o creato in mod locked
				ec_meno1(writeFile(entry->d_name, arg_d), "writeFile error");						//SONO QUI
				printf("WriteFile effettuata\n");
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
		//in seguito in dirname salvo la directory contenuta in arg_w
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

////////////////// parser //////////////////
static void parser(int dim, char** array){

	//il parsing è suddiviso in due cicli (si scandisce argv del main)

	//CICLO 1: si gestiscono i comandi di setting -h, -f, -t, -p, -d, -D
	
	int i = 0;

	while (++i < dim){
		
		//CASO -h / NON COMPLETO: gestire uscita con relative procedure di cleanu
		if (is_opt(array[i], "-h")) {
			case_h();
			exit(EXIT_FAILURE);
		}
		
		//CASO-f
		if (is_opt(array[i], "-f")) {
			//argomento obbligatorio - controlla se esiste
			if (!is_argument(array[i+1])){ 
				LOG_ERR(EINVAL, "argomento -t mancante");
				exit(EXIT_FAILURE); 
			}else{ socket_name = array[++i]; }
		}
		
		//CASO -t
		if (is_opt(array[i], "-t")) {
			if ( !is_argument(array[i+1]) ) time_r = 0;
			else ec_meno1( (time_r = isNumber(array[++i])), "argomento -t errato");
		}	

		//CASO -p
		if (is_opt(array[i], "-p")) flag_p = 1;

		//CASO -D
		if (is_opt(array[i], "-D")) {
			flag_D = 1;
			if (is_argument(array[i+1])) arg_D = array[++i];
		}

		//CASO -d 
		if (is_opt(array[i], "-d")) {
			if (is_argument(array[i+1])) {
				flag_d = 1;
				arg_d = array[++i];
				//CLEANUP
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
			if (is_argument(array[i+1]))
				mystrtok_r(array[++i]);
			else{
				LOG_ERR(EINVAL, "argomento -W mancante");
				exit(EXIT_FAILURE);
			}
		}
		
		//CASO -r
		if (is_opt(array[i], "-r")) {
			if (is_argument(array[i+1])) {
				//altera la stringa?
				//mystrtok_r(array[++i]);
				//lista di nomi da leggere nel server
				//richiesta di lettura al server per ogni file

			}else{
				LOG_ERR(EINVAL, "argomento -r mancante");
				exit(EXIT_FAILURE);
			}
		}
	

		//CASO -R
		if (is_opt(array[i], "-R")){
			//puoi chiamare readfile o readnfile rispetto al paramentro n che riceve
			//se n=0 o non presente legge tutti i file del server
			//altrimenti ne legge n
			//caso in cui n è presente: n=0 : n>0?
			if (is_argument(array[i+1])) {
				size_t x;
				ec_meno1((x = isNumber(array[++i])), "errore: arg. -w n non intero\n");
				if (x == 0) printf("rchiedere lettura di tutti i file nel server\n");
				else printf("richiedere lettura di n file nel server\n");
			}else{
				//caso in cui n non è presente
				printf("rchiedere lettura di tutti i file nel server\n");
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
			}else{
				LOG_ERR(EINVAL, "argomento -u mancante");
				exit(EXIT_FAILURE);
			}
		}

		//CASO -c
		if (is_opt(array[i], "-c")){
			if (is_argument(array[i+1])){
				printf("lista di file da rimuovere dal server se presenti\n");
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
