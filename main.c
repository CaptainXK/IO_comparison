#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/uio.h>//writev
#include <sys/time.h>//gettimeofday
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

//#include <linux/aio_abi.h>
#include <libaio.h>
#include <sys/syscall.h>

#define BUF_SZ (1 << 20)  
#define BUF_NB  (26)

#define TIMER_INIT()\
        struct timeval begin, end;\
        int64_t dura;\

#define TIMER_UP()\
        gettimeofday(&begin, NULL);\

#define TIMER_DOWN()\
        gettimeofday(&end, NULL);\
        dura = ((int64_t)end.tv_sec*1000000 + end.tv_usec)-((int64_t)begin.tv_sec*1000000 + begin.tv_usec);\
        fprintf(stdout, "Time elapsed %ld us, ", dura);\
        fprintf(stdout, "speed=%lf MBps...\n", (double)(((BUF_SZ*BUF_NB) / dura ) * 1000000) / 1024 / 1024 );\

#define NOR_MODE (S_IRUSR | S_IWUSR)


int main(void)
{
    char **buff_lists = NULL;

    buff_lists = (char**)malloc(sizeof(char*) * BUF_NB);

    for(int i = 0; i < BUF_NB; ++i){
        buff_lists[i] = calloc(BUF_SZ, sizeof(char));

        memset(buff_lists[i], (int)('a')+i, sizeof(char)*BUF_SZ);
        buff_lists[i][BUF_SZ - 1] = '\n';
    }

    TIMER_INIT()
    
    /*
     * normal write test
     * */
    fprintf(stdout, "[Normak write test]\n");
    int fd_1 = open("test1.txt", O_CREAT | O_RDWR, NOR_MODE);
    int nb_written = 0;
    assert(fd_1 != -1 );
    //do test
    TIMER_UP();
    for(int i = 0; i < BUF_NB; ++i){
        nb_written += write(fd_1, buff_lists[i], BUF_SZ);
        lseek(fd_1, nb_written, SEEK_SET);
    }
    TIMER_DOWN();
    close(fd_1);
    nb_written = 0;

    /*
     * iov test
     * */
    fprintf(stdout, "[IOV write test]\n");
    int fd_2 = open("test2.txt", O_CREAT | O_RDWR, NOR_MODE);
    assert(fd_2 != -1);

    //init iov
    struct iovec *iov_list;
    iov_list = (struct iovec*)malloc(sizeof(struct iovec) * BUF_NB);
    assert(iov_list != NULL);
    for(int i = 0; i < BUF_NB; ++i){
        iov_list[i].iov_base = buff_lists[i];
        iov_list[i].iov_len = BUF_SZ;
    }
    //do test
    TIMER_UP();
    nb_written = writev(fd_2, iov_list, BUF_NB);
    TIMER_DOWN();
    close(fd_2);
        
    //libaio test
    int fd_3 = open("test3.txt", O_CREAT | O_RDWR, NOR_MODE);
    assert(fd_3 != -1);
    io_context_t ctx;
    struct iocb ** cb_list;
    struct io_event * e_list;
    
    /* init io_context  */
    memset(&ctx, 0, sizeof(ctx));
    e_list = (struct io_event *)calloc(BUF_NB, sizeof(struct io_event));

    /* io setup */
    if(io_setup(BUF_NB, &ctx) !=0 ){
        fprintf(stdout, "io_setup error\n");
        close(fd_3);
        goto OUT;
    }

    /* init iocb */
    cb_list = (struct iocb **)calloc(BUF_NB, sizeof(struct iocb*));
    for(int i = 0; i < BUF_NB; ++i){
        cb_list[i] = (struct iocb*)calloc(1, sizeof(struct iocb));
        io_prep_pwrite(cb_list[i], fd_3, (void*)buff_lists[i], BUF_SZ, BUF_SZ * i);
        cb_list[i]->data = (void*)buff_lists[i];
    }
    
    /* submit */
    int ret = io_submit(ctx, BUF_NB, cb_list);
    if( ret != BUF_NB){
        perror("io submit error\n");

        free(cb_list);
        close(fd_3);
        goto OUT;
    }

    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 500000000;

    fprintf(stdout, "[libaio test]\n");
    TIMER_UP();
    while(1){
        if(io_getevents(ctx, BUF_NB, BUF_NB, e_list, NULL) == BUF_NB){
            close(fd_3);
            break;
        }
    }
    TIMER_DOWN();
    io_destroy(ctx);

OUT:
    //clean all resource
    for(int i = 0; i < BUF_NB; ++i){
        free(buff_lists[i]);
    }
    free(buff_lists);
    free(iov_list);

    return 0;
}
