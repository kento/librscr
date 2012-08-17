#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "ibtls.h"
#include "common.h"
#include "transfer.h"
#include "scr_list_queue.h"
#include "pgz.h"


#define COMPRESS 1
#define BUF_SIZE (2 * 1024 * 1024 * 1024L)

struct write_args {
  int id;
  char *path;
  char *addr;
  uint64_t size; 
  uint64_t offset;;
};

int dump_ckpt(struct write_args *wa);
ssize_t write_ckpt(const char* file, int fd, const void* buf, size_t size);
int simple_write(struct write_args *wa);
int compress_write(struct write_args *wa) ;
static struct write_args* get_write_arg(scr_lq *q, int id);

pthread_mutex_t compress_mutex = PTHREAD_MUTEX_INITIALIZER;
int comp_count = 0;
pthread_mutex_t dump_mutex = PTHREAD_MUTEX_INITIALIZER;
int dump_count = 0;
pthread_t dump_thread;

uint64_t buff_size = BUF_SIZE;
uint64_t chunk_size = CHUNK_SIZE;
uint64_t allocated_size = 0;

int main(int argc, char **argv) 
{
  struct RDMA_communicator comm;
  struct RDMA_request *req1, *req2;
  struct scr_transfer_ctl ctl[NUM_BUFF];
  struct scr_transfer_ctl file_info;
  file_info.size = 0;

  char *data[NUM_BUFF];
  int i;
  //  uint64_t  buff_size = BUF_SIZE;

  int buff_index = 0;
  uint64_t recv_size = -1;
  uint64_t ckpt_size = 0;
  pthread_t thread;
  scr_lq wq; /*enqueue incomming PFS chkp data*/

  //  struct write_args wa[NUM_BUFF];
  struct write_args *pendding_write_args = NULL;

  int req_num = 2;
  struct RDMA_request req[req_num];
  int req_id = 0;
  int ctl_tag;

  double rdma_s, rdma_e;
  int count = 0;
  int request_status = 0;

  RDMA_Passive_Init(&comm);
  scr_lq_init(&wq);
  //  for (i = 0; i < NUM_BUFF; i++) {  
  //    data[i] = RDMA_Alloc(size);
  //  }

  while(1) {
    /*Colloect RDMA read requests until buffer exceeds the */
    usleep(1000);

    //    fprintf (stderr, "1\n");
    //  fprintf (stderr, "3: %p %d %lu %lu %lu\n", pendding_write_args, request_status, allocated_size, file_info.size, buff_size) ;
    //    fprintf(stderr, "%lu %lu\n", allocated_size, buf_size);
    while (allocated_size < buff_size) {
      struct write_args *wa;
      //      fprintf(stderr, "aa\n");
      request_status = RDMA_Iprobe(RDMA_ANY_SOURCE, 0, &comm);
      //      fprintf(stderr, "bb\n");
      if (!request_status) {
	//	fprintf(stderr, "cc\n");
	//	fprintf (stderr, "4: %p %d %lu %lu %lu\n", pendding_write_args, request_status, allocated_size, file_info.size, buff_size) ;
	//	sleep(1);
	break;
      }
      //      fprintf(stderr, "dd\n");
      //fprintf (stderr, "1: %p %d %lu %lu %lu\n", pendding_write_args, request_status, allocated_size, file_info.size, buff_size) ;
      RDMA_Recv(&file_info, sizeof(file_info), NULL, RDMA_ANY_SOURCE, 0, &comm);
      fprintf(stderr, "PATH: %s, ID: %d size:%lu\n", file_info.path, file_info.id, file_info.size);
      // ==== Init
      wa = (struct write_args*)malloc(sizeof(struct write_args));
      wa->id = file_info.id;
      wa->path = (char *)malloc (128);
      sprintf(wa->path, "%s", file_info.path);
      wa->size = file_info.size;
      wa->offset = 0;
      allocated_size += file_info.size;
      wa->addr = RDMA_Alloc(file_info.size);
      fprintf(stderr, "%lu/%lu\n", allocated_size, buff_size );
      //  fprintf(stderr, "====> %p\n", wa->addr);
      
      scr_lq_enq(&wq, wa);
      //   fprintf(stderr, "ENQ(%d:%p)\n", file_info.id, get_write_arg(&wq, file_info.id));
    }

    while (RDMA_Iprobe(RDMA_ANY_SOURCE, 1, &comm)) {
      struct write_args* recv_wa;
      req_id = RDMA_Reqid(&comm, 1);
      recv_wa = get_write_arg(&wq, req_id);
      
      // rdma_s = get_dtime();
      //      fprintf(stderr, "recv_wa: %p (%d)\n", recv_wa, req_id);
      //      fprintf(stderr, "wa:%p (%d)\n", recv_wa, req_id);
      //fprintf(stderr, "%d: Offset: %lu, size: %lu, req_id:%d (%s)\n", req_id, recv_wa->offset, recv_wa->size, req_id, recv_wa->path);
      //      fprintf(stderr, "addr:%p, offset:%lu\n", recv_wa->addr, recv_wa->offset);
      RDMA_Recv(recv_wa->addr + recv_wa->offset, 0, NULL, req_id, 1, &comm);
      recv_wa->offset += chunk_size;
      if (recv_wa->offset > recv_wa->size) {
	recv_wa->offset = recv_wa->size;
      }
     
      //      fprintf(stderr, ":RDMA_Recv ends\n");
      //     memcpy(&recv_size, data[buff_index], sizeof(int));
      //      memcpy(&id, data[buff_index] + sizeof(int), sizeof(int));
      //      fprintf(stderr, "ID: %d\n", id);
      //      fprintf(stderr, "resv_size: %d\n", recv_size);
      fprintf(stderr, "%d: Offset: %lu, size: %lu, req_id:%d\n", req_id, recv_wa->offset, recv_wa->size, req_id);
      if (recv_wa->size == recv_wa->offset) {

#if COMPRESS == 1
	pthread_mutex_lock(&compress_mutex);
	if (pthread_create(&thread, NULL, (void *)compress_write, recv_wa)) {
	  fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
	  exit(1);
	}
#else
	pthread_mutex_lock(&dump_mutex);
	if (pthread_create(&thread, NULL, (void *)simple_write, recv_wa)) {
	  fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
	  exit(1);
	}
#endif
	
	if (pthread_detach(thread)) {
	  fprintf(stderr, "RDMA lib: SEND: ERROR: pthread detach failed @ %s:%d", __FILE__, __LINE__);
	  exit(1);
	}
	scr_lq_remove(&wq, recv_wa);
	//	fprintf(stderr, "DEQ(%d)\n", file_info.id);
	/*
	  fprintf(stderr, "1.Frees\n");
	  allocated_size -= recv_wa->size;
	  fprintf(stderr, "2.Frees\n");
	  free(recv_wa->path);
	  fprintf(stderr, "3.Frees\n");
	  fprintf(stderr, "====> %p\n", recv_wa->addr);
	  //	sleep(10);
	  RDMA_Free(recv_wa->addr);
	  fprintf(stderr, "4.Frees\n");
	  scr_lq_remove(&wq, recv_wa);
	  fprintf(stderr, "5.Frees\n");
	  free(recv_wa);
	  fprintf(stderr, "6.Frees\n");
	*/
	/*
	  wa[buff_index].path = ctl[buff_index].path;
	  wa[buff_index].addr = data[buff_index] + 2 * sizeof(int);
	  wa[buff_index].size = recv_size;
	  rdma_e = get_dtime();
	  fprintf(stderr, "OVLP: RDMA %f %d %f %f %s\n",rdma_s, count, rdma_e, rdma_e - rdma_s, wa[buff_index].path);
	  fprintf(stderr, "OVLP: RDMA %f %d\n", rdma_e, count++);
	  fprintf(stderr, "OVLP: RDMA \n");
	*/
      } else if  (recv_wa->size <= recv_wa->offset) {
	fprintf(stderr, "Received size exceeded the actual file size: file size=%lu, Received size=%lu\n", recv_wa->size, recv_wa->offset);
      }
    }
  }
  return 1;
  /*
  while(1) {
    ckpt_size = 0;
    recv_size = -1;

    RDMA_Recv(&ctl[buff_index], sizeof(ctl[buff_index]), NULL, RDMA_ANY_SOURCE, 0, &comm);

    fprintf(stderr, "PATH: %s, ID: %d \n", ctl[buff_index].path, ctl[buff_index].id);
    while (recv_size > 0 || recv_size == -1) {
      int id;
      //      fprintf(stderr, ":RDMA_Recv started\n");
       rdma_s = get_dtime();
      RDMA_Recv(data[buff_index], size, NULL, RDMA_ANY_SOURCE, 1, &comm);

      //      fprintf(stderr, ":RDMA_Recv ends\n");
     memcpy(&recv_size, data[buff_index], sizeof(int));
      memcpy(&id, data[buff_index] + sizeof(int), sizeof(int));
      //      fprintf(stderr, "ID: %d\n", id);

      //      fprintf(stderr, "resv_size: %d\n", recv_size);
      if (recv_size == 0) break;


      wa[buff_index].path = ctl[buff_index].path;
      wa[buff_index].addr = data[buff_index] + sizeof(int);
      wa[buff_index].size = recv_size;
      rdma_e = get_dtime();
      fprintf(stderr, "OVLP: RDMA %f %d %f %f %s\n",rdma_s, count, rdma_e, rdma_e - rdma_s, wa[buff_index].path);
      fprintf(stderr, "OVLP: RDMA %f %d\n", rdma_e, count++);
      fprintf(stderr, "OVLP: RDMA \n");
#if COMPRESS == 1
      pthread_mutex_lock(&compress_mutex);
      if (pthread_create(&thread, NULL, (void *)compress_write, &wa[buff_index])) {
#else
      pthread_mutex_lock(&dump_mutex);
      if (pthread_create(&thread, NULL, (void *)simple_write, &wa[buff_index])) {
#endif
	fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
	exit(1);
      }
      if (pthread_detach(thread)) {
	fprintf(stderr, "RDMA lib: SEND: ERROR: pthread detach failed @ %s:%d", __FILE__, __LINE__);
	exit(1);
      }
      buff_index = (buff_index + 1) % NUM_BUFF;
    }
  */
    //    fprintf(stderr, " Done\n");

  /*
  RDMA_Irecv(data[req_id], size, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req[req_id]);  
  req_id = (req_id + 1) % req_num;
  fprintf(stderr, "%p: size=%lu: \n", data, size);
  while (1) {
    //RDMA_Recv(data, size, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm);  
    ss = get_dtime();
    //    RDMA_Recv(data, 1, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm);  
    RDMA_Irecv(data[req_id], size, NULL, RDMA_ANY_SOURCE, RDMA_ANY_TAG, &comm, &req[req_id]);  
    req_id = (req_id + 1) % req_num;
    fprintf(stderr, "req_id:%d\n", req_id);
    RDMA_Wait(&req[req_id]);
    ee = get_dtime();
    fprintf(stderr, "Latency: %d\n", ee - ss);
    free(req1);
  }
  return 0;
  */
  //  }
}

static struct write_args* get_write_arg(scr_lq *q, int id)
{
  struct write_args* wa;
  struct write_args* war = NULL;
  scr_lq_init_it(q);
  while ((wa = (struct write_args*)scr_lq_next(q)) != NULL) {
    if (wa->id == id) {
      war = wa;
      break;
    }
  }
  scr_lq_fin_it(q);
  return war;
}


int free_write_args(struct write_args *wa) {
  //  fprintf(stderr, "1.Frees\n");                                                                                                      
  free(wa->path);                                                                                                                
  //  fprintf(stderr, "3.Frees\n");                                                                                                       
  //    sleep(10);                                                                                                                    
  RDMA_Free(wa->addr);                                                                                                           
  //  fprintf(stderr, "4.Frees\n");                                                                                                       
  //  scr_lq_remove(&wq, recv_wa);                                                                                                        
  //  fprintf(stderr, "5.Frees\n");  
  allocated_size -= wa->size;
  free(wa);                                                                                                                      
  //  fprintf(stderr, "6.Frees\n"); 

  return 1;
}

int simple_write(struct write_args *wa) 
{
  int fd;
  int n_write = 0;
  int n_write_sum = 0;
  double write_s, write_e;
  char *path = wa->path;
  char *addr = wa->addr;
  int size   = wa->size;


  write_s = get_dtime();

  fd = open(path, O_WRONLY | O_APPEND | O_CREAT, S_IREAD | S_IWRITE); 
  if (fd <= 0) {
    fprintf(stderr, "error(%d): path: |%s|, size: %d, fd=%d\n",  errno, path, size, fd);
    exit(1);
  }
  do {
    n_write = write(fd, (char*)addr + n_write_sum, size - n_write_sum);
    if (n_write == -1) {
      fprintf(stderr, "Write error: %d\n", errno);
      exit(1);
    }
    n_write_sum += n_write;
    if (n_write_sum >= size) break;
  } while(n_write > 0);
  fsync(fd);
  close(fd);
  write_e = get_dtime();
  free_write_args(wa);
  pthread_mutex_unlock(&dump_mutex);
  //  fprintf(stderr, "OVLP: WRIT %f %d %f %f\n", write_s, dump_count, write_e, write_e - write_s);
  //  fprintf(stderr, "OVLP: WRIT %f %d\n", write_e, dump_count++);
  //  fprintf(stderr, "OVLP: WRIT \n");
}

int compress_write(struct write_args *wa) 
{
  struct write_args *cwa;
  double comp_s, comp_e;


  //  fprintf(stderr, "Checkpoint path: %s, %p, %d\n", wa->path, wa->addr, wa->size);
  //  fprintf(stderr, "ST.");
  cwa = (struct write_args*)malloc(sizeof(struct write_args));
  // =======
  comp_s = get_dtime();
  cwa->size = pcompress(&(cwa->addr), wa->addr, wa->size);
  comp_e = get_dtime();
  //  fprintf(stderr, "OVLP: COMP %f %d %f %f\n", comp_s, comp_count, comp_e, comp_e - comp_s);
  //  fprintf(stderr, "OVLP: COMP %f %d\n", comp_e, comp_count++);
  //  fprintf(stderr, "OVLP: COMP \n");
  //  cwa->size = wa->size;
  //  cwa->addr = wa->addr;
  // =======
  cwa->path = (char *)malloc(256);
  sprintf(cwa->path, "%s.gz", wa->path);
  
  {
    pthread_mutex_lock(&dump_mutex);
    //  dump(wa->path, wa->addr, wa->size);
    if (pthread_create(&dump_thread, NULL, (void *)dump_ckpt, cwa)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: pthread create failed @ %s:%d", __FILE__, __LINE__);
      exit(1);
    }
    if (pthread_detach(dump_thread)) {
      fprintf(stderr, "RDMA lib: SEND: ERROR: pthread detach failed @ %s:%d", __FILE__, __LINE__);
      exit(1);
    }
  }
  
  free_write_args(wa);
  pthread_mutex_unlock(&compress_mutex);
}

int dump_ckpt(struct write_args *cwa)
{
  int fd;
  int n_write = 0;
  int n_write_sum = 0;
  double write_s, write_e;
  char *path = cwa->path;
  char *addr = cwa->addr;
  int size   = cwa->size;


  write_s = get_dtime();
  fd = open(path, O_WRONLY | O_APPEND | O_CREAT, S_IREAD | S_IWRITE); 
  //  fprintf(stderr, "path: |%s|, size: %d, fd=%d\n",  path, size, fd);
  //fd = open("/scr//test", O_WRONLY | O_APPEND | O_CREAT, S_IREAD | S_IWRITE); 
  if (fd <= 0) {
    fprintf(stderr, "error(%d): path: |%s|, size: %d, fd=%d\n",  errno, path, size, fd);
    exit(1);
  }
  //  return  write_ckpt(path, fd, addr, size);

  do {
    //    fprintf(stderr, "Write: addr:%lu, off:%lu\n", addr, n_write_sum);
    n_write = write(fd, (char*)addr + n_write_sum, size - n_write_sum);
    if (n_write == -1) {
      fprintf(stderr, "Write error: %d\n", errno);
      exit(1);
    }
    n_write_sum += n_write;
    //    fprintf(stderr, "Write: %d sum=%d, size=%d\n", n_write, n_write_sum, size);
    if (n_write_sum >= size) break;
  } while(n_write > 0);
  fsync(fd);
  close(fd);
  write_e = get_dtime();
  //  fprintf(stderr, "OVLP: WRIT %f %d %f %f\n", write_s, dump_count, write_e, write_e - write_s);
  //  fprintf(stderr, "OVLP: WRIT %f %d\n", write_e, dump_count++);
  //  fprintf(stderr, "OVLP: WRIT \n");
  //  printf("dumping...ends\n");
  //
  //TODO: free
  free(cwa->path);
  free(cwa->addr);
  free(cwa);
  pthread_mutex_unlock(&dump_mutex);
}


/* reliable write to opened file descriptor (retries, if necessary, until hard error) */
/*
ssize_t write_ckpt(const char* file, int fd, const void* buf, size_t size)
{
  ssize_t n = 0;
  int retries = 10;
  while (n < size)
  {
     ssize_t rc = write(fd, (char*) buf + n, size - n);
     if (rc > 0) {
       n += rc;
     } else if (rc == 0) {
       fprintf(stderr, "rc=0:\n");
       exit(1);
     } else {
       if (errno == EINTR || errno == EAGAIN) {
	 
         continue;
       }
       retries--;
       if (retries) {
	 fprintf(stderr, "retry:\n");
       } else {
	 fprintf(stderr, "many retry:\n");
         exit(1);
       }
     }
   }
   return n;
}
*/
