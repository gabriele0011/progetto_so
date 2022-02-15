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
	if(strcmp(arg, opt) == 0) return 1;
	else return 0;
}


////////////////// IS_ARGUMENT //////////////////
static int is_argument(int i, int dim, char* c){
	i++;
	if (i >= dim || (c[i]+0 == '-')) return 0;
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

////////////////// MEM_ARG //////////////////	
static void mem_arg(char** s, char* arg){

	int len = strlen(arg);
	ec_null( (*s = malloc(sizeof(char)*len)), "errore malloc");
	strncpy(*s, arg, len);
}

//////////////////PARSER //////////////////
static void parser(int dim, char** array){

	//il parsing è suddiviso in due cicli (si scandisce argv del main)

	//ciclo 1: si gestiscono i comandi di setting -h, -f, -t, -p, -d, -D
	char* arg_D = NULL;
	char* arg_d = NULL;
	int i = 0;

	while (++i < dim && !flag_h){
		
		//case_h OK -> terima immediatamente
		if (is_opt(array[i], "-h")){
			case_h();
			flag_h = 1;
		}
		
		//caso -f  OK
		if (is_opt(array[i], "-f")){
			//argomento obbligatorio - controlla se esiste
			if (!is_argument(i, dim, array[i])){ ec_meno1(-1, "argomento mancante"); }
			else{
				i++;
				int len = strlen(array[i]);
				ec_null( (socket_name = malloc(sizeof(char)*len)), "errore malloc");
				//CLEANUP
			}
		}

		//caso -t OK
		if (is_opt(array[i], "-t")){
			if ( !is_argument(i, dim, array[i]) ) time_r = 0;
			else ec_meno1( (time_r = isNumber(array[++i])), "argomento errato");
		}	

		//caso -p	OK
		if(is_opt(array[i], "-p")) flag_p = 1;

		//caso -D OK
		if(is_opt(array[i], "-D")){
			flag_D = 1;
			if ( is_argument(i, dim, array[i]) ){
				i++;
				int len = strlen(array[i]);
				ec_null( (arg_D = malloc(sizeof(char)*len)), "errore malloc");
				strncpy(arg_D, array[i], len);
				//CLEANUP
			}
		}

		//caso -d	OK
		if (is_opt(array[i], "-d")){
			flag_d = 1;
			if ( is_argument(i, dim, array[i]) ){
				i++;
				int len = strlen(array[i]);
				ec_null( (arg_d = malloc(sizeof(char)*len)), "errore malloc");	//argomento arg_D = .../esempio/prova
				strncpy(arg_d, array[i], len);
			}else{
				printf("errore: argomento -d mancante\n");
			}
		}
	}
	//quando viene inserito -h il programma termina immediatamente
	//cosa è necessario fare in questo caso? CLEANUP ecc?
	if(flag_h){ printf("programa terminato dopo -h\n"); return; } 


	//ciclo 2: si gestiscono i comandi di richiesta al server -w, -W, -R, -r, -l, -u, -c
	i = 0;
	while (++i < dim){
		
		//caso -w 
		if (is_opt(array[i], "-w")){		
			//verifica che siano attivi -d o -D altrimenti errore
			if (!(flag_d || flag_D)){ printf("errore: -d / -D non attivi\n"); }
			else{
				//passo 1: verifica che sia presente un argomento obbligatorio
				if (is_argument(i, dim, array[i])){
					//memorizza argomento in s
					char* s;
					mem_arg(&s, array[++i]);
					printf("arg di -w = s: %s\n", s);
					
					//passo 2: verifica se presente argomento opzionale
					int n;
					if ( !is_argument(i, dim, array[i]) ) n = 0;
					else ec_meno1( (n = isNumber(array[++i])), "argomento errato");
				}
				//visita la directory ed invia eventuali richieste
			}	//visit_dir(s, n);
		}
		
		//caso -W
		if (is_opt(array[i], "-W") && (flag_D || flag_d)){
			//cosa accade se i flag non sono attivi? da gestire in un altro if questo caso?
			if (is_argument(i, dim, array[i])){
				mystrtok_r(array[++i]);
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
		*/
		//gestione comandi non riconosciuti?
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