
#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"
#include "list.h"

typedef unsigned char byte;

typedef enum { O_CREATE = 1, O_LOCK = 2 }flags;
#define LOCK_TIMER 300

//variabili globali
static char* arg_D = NULL;		//dirname in cui scrivere i file rimossi dal server
static char* arg_d = NULL;		//dirname in cui scrivere i file letti dal server
static byte flag_p = 0;			//abilita stampe sullo std output per ogni operazione
static unsigned sleep_time = 0; 	
static int fd_sk = -1;
static char* path_dir = NULL;		//in caso di scritture (-w dirname), contiene il percorso del file ricavato in send_from_directory
node* open_files_list = NULL;

//prototipi client
static void parser(int dim, char** array);
static void help_message();
static int set_socket(const char* socket_name);
static void send_from_dir(const char* dirname, int* n);
static int append_request(const char* s);
static void write_request(char* arg_W);
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