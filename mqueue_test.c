#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    mqd_t mq;
    struct mq_attr attr;
    
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 256;
    attr.mq_curmsgs = 0;
    
    // Try to create a test queue
    mq_unlink("/test_queue");
    mq = mq_open("/test_queue", O_CREAT | O_RDWR, 0644, &attr);
    
    if (mq == -1) {
        perror("mq_open failed");
        return 1;
    }
    
    printf("Message queue created successfully!\n");
    
    mq_close(mq);
    mq_unlink("/test_queue");
    
    return 0;
}