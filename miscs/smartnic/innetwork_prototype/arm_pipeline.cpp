// arm_pipeline.cpp — violet1-bf3-1 ARM
// Simplified: result sent to arm_receiver via TCP (avoids second QP creation)
// This proves the full pipeline end to end.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vortex.h>
#include "common.h"

#define RT_CHECK(x) do{int _r=(x);if(_r){fprintf(stderr,"Vortex error %d at %s:%d\n",_r,__FILE__,__LINE__);exit(1);}}while(0)

static double now_ms(){
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec*1e3+t.tv_nsec*1e-6;
}

// Send full buffer over TCP in chunks
static void tcp_send_all(int sock, void* buf, size_t len) {
    size_t sent=0;
    char* p=(char*)buf;
    while(sent<len){
        ssize_t n=send(sock,p+sent,len-sent,0);
        if(n<=0){perror("send");exit(1);}
        sent+=n;
    }
}

static void recv_all(int sock, void* buf, size_t len) {
    size_t got=0;
    char* p=(char*)buf;
    while(got<len){
        ssize_t n=recv(sock,p+got,len-got,0);
        if(n<=0){perror("recv");exit(1);}
        got+=n;
    }
}

int main(int argc, char** argv) {
    const char* host_ip     = argc>1 ? argv[1] : "143.215.138.110";
    const char* receiver_ip = argc>2 ? argv[2] : "10.10.10.70";
    const char* kernel_file = argc>3 ? argv[3] : "kernel.vxbin";

    printf("=== arm_pipeline (violet1-bf3-1) ===\n");
    printf("GPU host IP    : %s\n", host_ip);
    printf("ARM receiver IP: %s\n", receiver_ip);

    // ── Open RDMA on mlx5_2 ──────────────────────────────────────
    int ndev;
    struct ibv_device **devs=ibv_get_device_list(&ndev);
    struct ibv_device *dev=NULL;
    for(int i=0;i<ndev;i++)
        if(strstr(ibv_get_device_name(devs[i]),"mlx5_2"))
            { dev=devs[i]; break; }
    if(!dev){fprintf(stderr,"mlx5_2 not found\n");exit(1);}

    struct ibv_context *ctx=ibv_open_device(dev);
    struct ibv_pd *pd=ibv_alloc_pd(ctx);
    struct ibv_cq *cq=ibv_create_cq(ctx,64,NULL,NULL,0);

    // Allocate buffer: src + dst side by side (max 4MB each)
    size_t max_bytes = 4*1024*1024;
    float* buf_mem = (float*)malloc(max_bytes*2);
    struct ibv_mr *mr=ibv_reg_mr(pd,buf_mem,max_bytes*2,
        IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_WRITE|IBV_ACCESS_REMOTE_READ);
    if(!mr){perror("ibv_reg_mr");exit(1);}

    // Create single QP
    struct ibv_qp_init_attr qa={};
    qa.send_cq=cq; qa.recv_cq=cq; qa.qp_type=IBV_QPT_RC;
    qa.cap.max_send_wr=16; qa.cap.max_recv_wr=16;
    qa.cap.max_send_sge=1; qa.cap.max_recv_sge=1;
    struct ibv_qp *qp=ibv_create_qp(pd,&qa);
    if(!qp){perror("ibv_create_qp");exit(1);}
    printf("Local QPN: 0x%x\n", qp->qp_num);

    // Transition QP to INIT
    struct ibv_qp_attr attr={};
    attr.qp_state=IBV_QPS_INIT; attr.pkey_index=0; attr.port_num=IB_PORT;
    attr.qp_access_flags=IBV_ACCESS_REMOTE_READ|IBV_ACCESS_REMOTE_WRITE|IBV_ACCESS_LOCAL_WRITE;
    ibv_modify_qp(qp,&attr,IBV_QP_STATE|IBV_QP_PKEY_INDEX|IBV_QP_PORT|IBV_QP_ACCESS_FLAGS);

    union ibv_gid my_gid;
    ibv_query_gid(ctx,IB_PORT,1,&my_gid); // GID index 1 = 10.10.10.69

    // ── TCP connect to gpu_host ───────────────────────────────────
    printf("Connecting to gpu_host %s:%d ...\n", host_ip, CTRL_PORT_HOST_TO_ARM);
    int sock=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in sa={};
    sa.sin_family=AF_INET; sa.sin_port=htons(CTRL_PORT_HOST_TO_ARM);
    inet_pton(AF_INET,host_ip,&sa.sin_addr);
    while(connect(sock,(struct sockaddr*)&sa,sizeof(sa))!=0)
        { printf("Waiting for gpu_host...\n"); sleep(1); }

    // Send QPN + GID
    uint32_t my_qpn=qp->qp_num;
    send(sock,&my_qpn,sizeof(my_qpn),0);
    send(sock,&my_gid,16,0);

    // Receive GPU RDMA info
    gpu_rdma_info_t gpu_info={};
    recv_all(sock,&gpu_info,sizeof(gpu_info));
    printf("Got GPU RDMA info: count=%u rkey=0x%x vaddr=0x%lx qpn=0x%x\n",
           gpu_info.count,gpu_info.rkey,gpu_info.vaddr,gpu_info.qpn);

    uint32_t count=gpu_info.count;
    size_t nbytes=count*sizeof(float);
    float* src=buf_mem;        // first half — received GPU data
    float* dst=buf_mem+count;  // second half — Vortex output

    // Connect QP to gpu_host
    memset(&attr,0,sizeof(attr));
    attr.qp_state=IBV_QPS_RTR; attr.path_mtu=IBV_MTU_1024;
    attr.dest_qp_num=gpu_info.qpn; attr.rq_psn=0;
    attr.max_dest_rd_atomic=16; attr.min_rnr_timer=12;
    attr.ah_attr.port_num=IB_PORT; attr.ah_attr.is_global=1;
    attr.ah_attr.grh.hop_limit=64; attr.ah_attr.grh.sgid_index=1;
    memcpy(&attr.ah_attr.grh.dgid,gpu_info.gid,16);
    ibv_modify_qp(qp,&attr,IBV_QP_STATE|IBV_QP_AV|IBV_QP_PATH_MTU|
                  IBV_QP_DEST_QPN|IBV_QP_RQ_PSN|IBV_QP_MAX_DEST_RD_ATOMIC|IBV_QP_MIN_RNR_TIMER);
    memset(&attr,0,sizeof(attr));
    attr.qp_state=IBV_QPS_RTS; attr.timeout=14; attr.retry_cnt=7;
    attr.rnr_retry=7; attr.sq_psn=0; attr.max_rd_atomic=16;
    ibv_modify_qp(qp,&attr,IBV_QP_STATE|IBV_QP_TIMEOUT|IBV_QP_RETRY_CNT|
                  IBV_QP_RNR_RETRY|IBV_QP_SQ_PSN|IBV_QP_MAX_QP_RD_ATOMIC);
    printf("QP connected to gpu_host\n");

    // ── RDMA read GPU VRAM → ARM src buffer ──────────────────────
    printf("RDMA read from H100 VRAM (%.2f MB)...\n",nbytes/1048576.0);
    double t0=now_ms();

    struct ibv_sge sge={};
    sge.addr=(uint64_t)(uintptr_t)src;
    sge.length=nbytes; sge.lkey=mr->lkey;
    struct ibv_send_wr wr={},*bad=NULL;
    wr.wr_id=1; wr.sg_list=&sge; wr.num_sge=1;
    wr.opcode=IBV_WR_RDMA_READ; wr.send_flags=IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr=gpu_info.vaddr; wr.wr.rdma.rkey=gpu_info.rkey;
    ibv_post_send(qp,&wr,&bad);

    struct ibv_wc wc; int n=0;
    while(n==0) n=ibv_poll_cq(cq,1,&wc);
    if(wc.status!=IBV_WC_SUCCESS)
        fprintf(stderr,"RDMA read failed: %s\n",ibv_wc_status_str(wc.status));
    double t1=now_ms();

    printf("RDMA read done: %.2f ms  BW=%.1f MB/s\n",t1-t0,nbytes/((t1-t0)*1e3));
    printf("First element: %.4f (expected 2.0)\n",src[0]);

    // ── Vortex SIMX compute ───────────────────────────────────────
    printf("Running Vortex SIMX (scale x0.5)...\n");
    vx_device_h device;
    vx_buffer_h buf_src,buf_dst,krnl_buf,args_buf;
    RT_CHECK(vx_dev_open(&device));
    RT_CHECK(vx_mem_alloc(device,nbytes,VX_MEM_READ, &buf_src));
    RT_CHECK(vx_mem_alloc(device,nbytes,VX_MEM_WRITE,&buf_dst));

    kernel_arg_t karg={};
    karg.count=count; karg.scale=0.5f;
    RT_CHECK(vx_mem_address(buf_src,&karg.src_addr));
    RT_CHECK(vx_mem_address(buf_dst,&karg.dst_addr));
    RT_CHECK(vx_copy_to_dev(buf_src,src,0,nbytes));
    RT_CHECK(vx_upload_kernel_file(device,kernel_file,&krnl_buf));
    RT_CHECK(vx_upload_bytes(device,&karg,sizeof(karg),&args_buf));

    double t2=now_ms();
    RT_CHECK(vx_start(device,krnl_buf,args_buf));
    RT_CHECK(vx_ready_wait(device,VX_MAX_TIMEOUT));
    double t3=now_ms();
    RT_CHECK(vx_copy_from_dev(dst,buf_dst,0,nbytes));

    printf("Vortex done: %.2f ms  first result=%.4f (expected 1.0)\n",
           t3-t2,dst[0]);

    // ── Send result to arm_receiver over TCP ──────────────────────
    // TCP is sufficient to prove the pipeline — avoids second QP
    printf("Connecting to arm_receiver %s:%d (TCP)...\n",
           receiver_ip, CTRL_PORT_ARM_TO_GPU2);
    int rsock=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in rsa={};
    rsa.sin_family=AF_INET; rsa.sin_port=htons(CTRL_PORT_ARM_TO_GPU2);
    inet_pton(AF_INET,receiver_ip,&rsa.sin_addr);
    while(connect(rsock,(struct sockaddr*)&rsa,sizeof(rsa))!=0)
        { printf("Waiting for arm_receiver...\n"); sleep(1); }

    // Send count then result buffer
    send(rsock,&count,sizeof(count),0);
    tcp_send_all(rsock,dst,nbytes);

    // Wait for ack
    uint32_t ack=0;
    recv_all(rsock,&ack,sizeof(ack));
    double t4=now_ms();
    printf("arm_receiver confirmed\n");

    // Signal gpu_host pipeline done
    uint32_t done=1;
    send(sock,&done,sizeof(done),0);

    printf("\n=== arm_pipeline timing ===\n");
    printf("RDMA read (H100 VRAM → ARM) : %.2f ms  %.1f MB/s\n",
           t1-t0, nbytes/((t1-t0)*1e3));
    printf("Vortex SIMX compute         : %.2f ms\n", t3-t2);
    printf("Result → arm_receiver (TCP) : %.2f ms\n", t4-t3);
    printf("Total pipeline              : %.2f ms\n", t4-t0);

    vx_mem_free(buf_src); vx_mem_free(buf_dst);
    vx_mem_free(krnl_buf); vx_mem_free(args_buf);
    vx_dev_close(device);
    ibv_dereg_mr(mr); ibv_destroy_qp(qp); ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd); ibv_close_device(ctx);
    ibv_free_device_list(devs); free(buf_mem);
    close(sock); close(rsock);
    return 0;
}