#include "server_worker.h"

void start_func(char* arg)
{
	int* buf = NULL;
	int fd_client;
	int  op;
	int err;
	
	while (!sig_intquit){
		
		//pop richiesta dalla coda concorrente
		while (!(buf = dequeue(&conc_queue))){
			
			//lock	
			mutex_unlock(&g_mtx, "server_worker: lock fallita");
			//wait
			if ( (err = phtread_cond_wait(&cv, &g_mtx)) == -1){
				LOG_ERR(err, "server_worker: phtread_cond_wait fallita");
				exit(EXIT_FAILURE);
				}
			}
		}
		//unlock
		mutex_unlock(&g_mtx, "server_worker: lock fallita");

		//salvo il client
		fd_client = *buf;
		
		//libero il buffer e rialloco per riasarlo in lettura
		if (buf){ 
			free(buf);
			buf = NULL;
		}
		//ec_null(buf = malloc(sizeof(int)), "server_worker: malloc fallita");
		int* buf = 0;

		//leggi la richiesta dal client
		if (err = readn(fd_client, buf, sizeof(int)) == -1){
			LOG_ERR(-1, "server_worker: readn su client fallita");
			exit(EXIT_FAILURE);
		}
		printf("DEBUG: nuovo valore buf (richiesta): %d\n", *buf);
		
		//a seguire codifica di op e identificazione del comando
		op = *buf;
		printf("richiesta di tipo %d dal client %d\n", *buf, fd_client);

		//client disconnesso
		if (op == EOF){
			fprintf(stderr, "client %d disconnesso\n", fd_client);
			*buf = fd_client;
			*buf = fd_client*(-1);
			if ((err = writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG)) != -1)
				LOG_ERR(-1, "server_worker: write su pipe fallita");
			
		}

		switch(op){
			//client disconnesso
			case 0:
				fprintf(stderr, "client %d disconnesso\n", fd_client);
				*buf = fd_client;
				*buf = fd_client*(-1);
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			//openFile
			case 1:
				if(worker_openFile(fd_client) == -1){
					LOG_ERR(-1, "server_worker: worker_openFile fallita");
					exit(EXIT_FAILURE);
				}
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//closeFile
			case 2:
				if(worker_closeFile(fd_client); == -1){
					LOG_ERR(-1, "server_worker: worker_closeFile fallita");
					exit(EXIT_FAILURE);
				}
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//writeFile
			case 3:
				if(worker_writeFile(fd_client); == -1){
					LOG_ERR(-1, "server_worker: worker_writeFile fallita");
					exit(EXIT_FAILURE);
				}
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//appendToFile
			case 4:
				if(worker_appendToFile(fd_client); == -1){
					LOG_ERR(-1, "server_worker: worker_appendToFile fallita");
					exit(EXIT_FAILURE);
				}
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//readFile
			case 5:
				if(worker_readFile(fd_client); == -1){
					LOG_ERR(-1, "server_worker: worker_readFile fallita");
					exit(EXIT_FAILURE);
				}
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//readNFiles
			case 6: 
				if(worker_readNFiles(fd_client); == -1){
					LOG_ERR(-1, "server_worker: worker_readNFiles fallita");
					exit(EXIT_FAILURE);
				}
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//lockFile
			case 7: 
				if(worker_lockFile(fd_client); == -1){
					LOG_ERR(-1, "server_worker: worker_lockFile fallita");
					exit(EXIT_FAILURE);
				}
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//unlockFile
			case 8: 
				if(worker_unlockFile(fd_client); == -1){
					LOG_ERR(-1, "server_worker: worker_unlockFile fallita");
					exit(EXIT_FAILURE);
				}
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			
			//removeFile
			case 9: 
				if(worker_removeFile(fd_client); == -1){
					LOG_ERR(-1, "server_worker: worker_removeFile fallita");
					exit(EXIT_FAILURE);
				}
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				break;
			//default
			default:
 				if (writen(fd_pipe_write, EBADR, PIPE_MAX_LEN_MSG) == -1){
					LOG_ERR(EPIPE, "server_worker: write su pipe fallita");
				buf = fd_client;
				if (writen(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1)
					LOG_ERR(EPIPE, "server_worker: richiesta client non valida");
				break;
	}
	if(buf) free(buf);
	pthred_exit(0);
}