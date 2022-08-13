#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"
#include "conc_queue.h"
#include "cache.h"



#define UNIX_PATH_MAX 108
#define PIPE_MAX_LEN_MSG sizeof(int)
typedef enum {O_CREATE=1, O_LOCK=2} flags;

//var. settate mediante file config.txt
static size_t t_workers_num = 0;
static size_t server_mem_size = 0;
static size_t max_storage_file = 0;

//(!) rivedere -> posizionare nei file corretti
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
size_t tot_requests = 0;

//flags segnali di teminazione
volatile sig_atomic_t sig_intquit = 0;	//SIGINT/SIGQUIT: uscita immediata - non si accettano richieste - chiudere connessioni attive
volatile sig_atomic_t sig_hup = 0; 		//SIG_HUP: non si accettano nuove connessioni - si termina una volta concluse quelle attive

//prototipi
static int worker_openFile(int fd_c);
static int worker_writeFile(int fd_c);
static int worker_appendToFile(int fd_c);
static int worker_readFile(int fd_c);
static int worker_readNFiles(int fd_c);
static int worker_lockFile(int fd_c);
static int worker_unlockFile(int fd_c);
static int worker_removeFile(int fd_c);
static int worker_closeFile(int fd_c);
