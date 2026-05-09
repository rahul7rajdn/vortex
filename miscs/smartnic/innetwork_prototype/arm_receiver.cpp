// arm_receiver.cpp — violet2-bf3-1 ARM
// Receives result from arm_pipeline over TCP, validates, forwards to gpu_receiver.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"

static void recv_all(int sock, void* buf, size_t len) {
    size_t got=0; char* p=(char*)buf;
    while(got<len){
        ssize_t n=recv(sock,p+got,len-got,0);
        if(n<=0){perror("recv");exit(1);}
        got+=n;
    }
}
static void send_all(int sock, void* buf, size_t len) {
    size_t sent=0; char* p=(char*)buf;
    while(sent<len){
        ssize_t n=send(sock,p+sent,len-sent,0);
        if(n<=0){perror("send");exit(1);}
        sent+=n;
    }
}

int main(int argc, char** argv) {
    const char* gpu2_ip = argc>1 ? argv[1] : "143.215.138.111";
    printf("=== arm_receiver (violet2-bf3-1) ===\n");
    printf("GPU receiver IP: %s\n", gpu2_ip);

    // Listen for arm_pipeline
    printf("Listening for arm_pipeline on port %d...\n", CTRL_PORT_ARM_TO_GPU2);
    int srv=socket(AF_INET,SOCK_STREAM,0);
    int opt=1; setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in sa={};
    sa.sin_family=AF_INET; sa.sin_port=htons(CTRL_PORT_ARM_TO_GPU2);
    sa.sin_addr.s_addr=INADDR_ANY;
    bind(srv,(struct sockaddr*)&sa,sizeof(sa)); listen(srv,1);
    int psock=accept(srv,NULL,NULL);
    printf("arm_pipeline connected\n");

    // Receive count then result buffer
    uint32_t count=0;
    recv_all(psock,&count,sizeof(count));
    size_t nbytes=count*sizeof(float);
    float* result=(float*)malloc(nbytes);
    recv_all(psock,result,nbytes);
    printf("Received result: %u floats (%.2f MB)\n",count,nbytes/1048576.0);
    printf("Sample: [0]=%.4f [1]=%.4f [last]=%.4f (expected 1.0)\n",
           result[0],result[1],result[count-1]);

    // Validate
    int errors=0;
    for(uint32_t i=0;i<count;i++)
        if(fabsf(result[i]-1.0f)>1e-4f) errors++;
    printf("Validation: %s (%d errors / %u elements)\n",
           errors==0?"PASSED":"FAILED",errors,count);

    // Ack to arm_pipeline
    uint32_t ack=1;
    send(psock,&ack,sizeof(ack),0);

    // Forward to gpu_receiver
    printf("Sending to gpu_receiver %s:%d...\n",gpu2_ip,CTRL_PORT_ARM_TO_HOST2);
    int gsock=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in gsa={};
    gsa.sin_family=AF_INET; gsa.sin_port=htons(CTRL_PORT_ARM_TO_HOST2);
    inet_pton(AF_INET,gpu2_ip,&gsa.sin_addr);
    while(connect(gsock,(struct sockaddr*)&gsa,sizeof(gsa))!=0)
        { printf("Waiting for gpu_receiver...\n"); sleep(1); }

    send(gsock,&count,sizeof(count),0);
    send(gsock,&errors,sizeof(errors),0);
    send_all(gsock,result,nbytes);

    uint32_t gpu_ack=0;
    recv_all(gsock,&gpu_ack,sizeof(gpu_ack));
    printf("gpu_receiver acknowledged — pipeline COMPLETE\n");

    free(result);
    close(psock); close(srv); close(gsock);
    return 0;
}