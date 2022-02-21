#include "err_cleanup.h"


//variabili globali
int flag_D = 0;
int flag_d = 0;
int flag_p = 0;
int time_r;
int flag_h = 0;
char *socket_name = NULL; //controllo memoria


////////////////// ISNUMBER //////////////////
long isNumber(const char* s){
	char* e = NULL;
   	long val = strtol(s, &e, 0);
   	if (e != NULL && *e == (char)0) return val; 
	return -1;
}


////////////////// CASE_H //////////////////
static void case_h(){
	//apertura file
	FILE *fp = fopen("help.txt", "r");
     	//controllo esito apertura
	if (!fp) {
		perror("fopen");
		exit(EXIT_FAILURE);
    	}
	//lettura file in array di n char
    	int n = 10;
	char buf[n];
    	//scandisco il file fino alla fine
	int temp;
	while(!feof(fp) && (temp = fread(buf, sizeof(char), n, fp)) != 0 ){
		for(int i = 0; i < temp; i++)
			printf("%c", buf[i]);
  	}
	fclose(fp);
}


////////////////// IS_OPT //////////////////
static int is_opt( char* arg, char* opt){
	if(strcmp(arg, opt) == 0 && printf("opt %s riconosciuto\n", opt) ) return 1;
	else return 0;
}

////////////////// IS_ARGUMENT //////////////////
static int is_argument(char* c){
	if (c == NULL || (*c == '-') ) return 0;
	else return 1;
}


////////////////// IS_ARGUMENT //////////////////
void mystrtok_r (char* string){
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


////////////////// IS_REGULAR_FILE //////////////////
int is_directory(const char *path)
{
    struct stat path_stat;
    ec_meno1(lstat(path, &path_stat), "errore stat");
    return S_ISDIR(path_stat.st_mode);
}


////////////////// VISIT_AND_REQUEST //////////////////
void visit_folder_send_request(const char* dirname, int* n)
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
		if(is_directory(path)){
			
			//controlla che non si tratti della dir . o ..
			if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
				//visita ricorsiva su nuova directory
				visit_folder_send_request(path, n);
		}else{
			//passo 4 caso 2: è un file
			printf("richiesta di scrittura file: %s\n", entry->d_name);
			(*n)--;
		}
	}
	//controllo su errno
	if (errno != 0){ 
		perror("errore errno"); 
		exit(EXIT_FAILURE);
	}
	//chiudere la directory
	ec_meno1(closedir(d), "errore su closedir");
}


//////////////////PARSER //////////////////
static void parser(int dim, char** array){

	//il parsing è suddiviso in due cicli (si scandisce argv del main)

	//CICLO 1: si gestiscono i comandi di setting -h, -f, -t, -p, -d, -D
	char* arg_D = NULL;
	char* arg_d = NULL;
	int i = 0;

	while (++i < dim){
		
		//CASO -h / NON COMPLETO: gestire uscita con relative procedure di cleanu
		if (is_opt(array[i], "-h")){
			case_h();
			exit(EXIT_FAILURE);
		}
		
		//CASO-f
		if (is_opt(array[i], "-f")){
			//argomento obbligatorio - controlla se esiste
			if (!is_argument(array[i+1])){ ec_meno1(-1, "argomento mancante"); }
			else{
				socket_name = array[++i];
			}
		}
		
		//CASO -t
		if (is_opt(array[i], "-t")){
			if ( !is_argument(array[i+1]) ) time_r = 0;
			else ec_meno1( (time_r = isNumber(array[++i])), "argomento errato");
		}	

		//CASO -p
		if (is_opt(array[i], "-p")) flag_p = 1;

		//CASO -D
		if (is_opt(array[i], "-D")){
			flag_D = 1;
			if (is_argument(array[i+1])){
				arg_D = array[++i];
				//CLEANUP
			}
		}

		//CASO -d
		if (is_opt(array[i], "-d")){
			if (is_argument(array[i+1])){
				flag_d = 1;
				arg_d = array[++i];
				//CLEANUP
			}else{
				printf("errore: argomento -d mancante\n");
			}
		}
	}

	//CICLO 2: si gestiscono i comandi di richiesta al server -w, -W, -R, -r, -l, -u, -c
	i = 0;
	while (++i < dim){	
		//CASO -w
		if (is_opt(array[i], "-w")){		
			
			//1) verifica -d/-D attivo altrimenti errore
			if (!(flag_d || flag_D)){ 
				printf("errore: -d / -D non attivi\n"); 
			}else{
				//2) verifica arg. obbligatorio
				if (is_argument(array[i+1])){
					char* arg_w = array[++i];
					
					//3) verifica arg opzionale 
					char* n = NULL;
					int x;
					//strtok_r restituisce il primo token appena dopo il primo delimitatore
					strtok_r(array[i], ",", &n);
					//se n non è specificata
					if (n == NULL) x = -1;
					else {
						//se n è un numero 
						ec_meno1((x = isNumber(n)), "errore: arg. non intero\n");
						//se zero è come non presente -> setto a -1
						if(x == 0) x = -1; 
					}
					//CLEANUP
					visit_folder_send_request(arg_w, &x);

				}
				else printf("errore: argomento -w mancante\n");
			}	
		}
		
		//CASO -W
		if (is_opt(array[i], "-W")){
			//1) verifica -d o -D attivo, altrimenti errore
			if (!(flag_d || flag_D)){ printf("errore: -d / -D non attivi\n"); }
			else{	
				//argomento obbligatorio	
				if (is_argument(array[i+1]))
					mystrtok_r(array[++i]);
				else printf("errore: argomento obligatorio\n");
			}
		}

/*		
		if (is_opt(array[i], "-r") && (flag_D || flag_d) ){
			printf("opzione -r riconosciuta\n");
		}
		if (is_opt(array[i], "-R") == 0 && (flag_D || flag_d) ){
			printf("opzione -R riconosciuta\n");
		}
		if (is_opt(array[i], "-l") == 0  ){
			printf("opzione -R riconosciuta\n");
		}
		if( strcmp((array[i]), "-l") == 0){
			printf("opzione -L riconosciuta\n");
			//argomento obligatorio
		}
		if( strcmp((array[i]), "-u") == 0 ){
			printf("opzione -u riconosciuta\n");
		}
		if( strcmp((array[i]), "-c") == 0 ){
			printf("opzione -c riconosciuta\n");
		
		//gestione comandi non riconosciuti?
*/
	}
}


////////////////// MAIN //////////////////
int main(int argc, char* argv[]){
	
	//controllo su argc
	if( argc == 1){
		ERR_H(EINVAL, "errore su argomenti\n");
	}
	
	//parser
	parser(argc, argv);

	return 0;
}