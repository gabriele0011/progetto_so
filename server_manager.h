#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"
#include "conc_queue.h"
#include "cache.h"
//#include "list.h"



#define UNIX_PATH_MAX 108
#define PIPE_MAX_LEN_MSG sizeof(int)
typedef enum {O_CREATE=1, O_LOCK=2} flags;

//var. settate mediante file config.txt
static size_t t_workers_num;
static size_t server_mem_size;
static size_t max_storage_file;

//(!) rivedere -> posizionare nei file corretti
//mutex server_manager/server_worker


file* cache = NULL;
char* socket_name = NULL;
t_queue* conc_queue = NULL;
int fd_pipe_read;
int fd_pipe_write;
size_t tot_requests;

//prototipi
int read_config_file(char* f_name);
void* thread_func(void *arg);
static int worker_openFile(int fd_c);
static int worker_writeFile(int fd_c);
static int worker_appendToFile(int fd_c);
static int worker_readFile(int fd_c);
static int worker_readNFiles(int fd_c);
static int worker_lockFile(int fd_c);
static int worker_unlockFile(int fd_c);
static int worker_removeFile(int fd_c);
static int worker_closeFile(int fd_c);
