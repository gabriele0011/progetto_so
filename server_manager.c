//iniziamo a definire lo scheletro del server manager nei suoi punti principali per poi andare ad affrontare 
//un'implementazione piu profonda
#include "err_cleanup.h"
#include "util.h"

int t_workers_num = 0;
int server_mem_size = 0;
int server_file_size = 0;
char* socket_file_name = NULL;


void read_config_file(char* f_name)
{
	//apertura del fine
	FILE* fd;
	ec_null((fd = fopen(f_name, "r")), "errore fopen in read_config_file");


    	//allocazione buf[size] per memorizzare le righe del file
    	int len = 512;
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
		
		if (strncmp(token1, "socket_file_name", len_t) == 0 ){
			socket_file_name = calloc(sizeof(char), len_string);
			strncpy(socket_file_name, string, len_string);
			socket_file_name[len_string] = '\0';
		}
	}

	//chiusura del fd + controllo
	if(fclose(fd) != 0){
		LOG_ERR(errno, "errore fclose in read_config_file");
		exit(EXIT_FAILURE);
	}
}



int main(int argc, char* argv[])
{

	//controllo sui parametri in ingresso -> mi aspetto un argomento, ovvero il file config.txt
	if(argc < 1){
		LOG_ERR(EINVAL, "file config.txt mancante");
		exit(EXIT_FAILURE);
	}
	//apertura del file config.txt da parsare per ottenere le informazioni di setup del server
	read_config_file(argv[1]);

	return 0;
}