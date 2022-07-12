#include "err_control.h"
#include "function_c.h"
#include "c_queue.h"
#include "cache.h"


#define UNIX_PATH_MAX 108
#define PIPE_MAX_LEN_MSG sizeof(int)

//var. settate mediante file config.txt
static size_t t_workers_num = 0;
static size_t server_mem_size = 0;
static size_t max_storage_file = 0;

//mutex server_manager/server_worker
pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
//mutex coda concorrente richieste
pthread_mutex_t mtx1 = PTHREAD_MUTEX_INITIALIZER;
//mutex cache
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;


file* cache = NULL; 
char* socket_name = NULL;
t_queue* conc_queue = NULL;
int fd_pipe_read = 0;
int fd_pipe_write = 0;

//flags segnali di teminazione
volatile sig_atomic_t sig_intquit = 0;	//SIGINT/SIGQUIT: uscita immediata - non si accettano richieste - chiudere connessioni attive
volatile sig_atomic_t sig_hup = 0; 		//SIG_HUP: non si accettano nuove connessioni - si termina una volta concluse quelle attive

/*
void* start_func();

static int worker_openFile(int x);
static int worker_closeFile(int x);
static int worker_readFile(int x);
static int worker_readNFiles(int x);
static int worker_writeFile(int x);
static int worker_appendToFile(int x);
static int worker_lockFile(int x);
static int worker_unlockFile(int x);
static int worker_removeFile(int x);
*/

