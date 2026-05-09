// gpu_host.cu — violet1 x86 host
// Allocates H100 GPU buffer, creates RC QP, registers for RDMA,
// sends rkey + QPN to ARM pipeline, waits for pipeline complete.
// Compile: nvcc gpu_host.cu -o gpu_host -libverbs -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <cuda_runtime.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"

#define CHECK_CUDA(x) do { \
    cudaError_t e=(x); if(e!=cudaSuccess){ \
    fprintf(stderr,"CUDA %s\n",cudaGetErrorString(e)); exit(1);} } while(0)

static double now_ms() {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec*1e3+t.tv_nsec*1e-6;
}

int main(int argc, char** argv) {
    uint32_t count = argc>2 ? atoi(argv[2]) : (1<<18);
    printf("=== gpu_host (violet1 H100) ===\n");
    printf("Count: %u floats (%.2f MB)\n", count, count*4.0f/(1024*1024));

    // ── Allocate GPU buffer ───────────────────────────────────────
    float *d_buf;
    CHECK_CUDA(cudaMalloc(&d_buf, count*sizeof(float)));
    float *h_tmp = (float*)malloc(count*sizeof(float));
    for(uint32_t i=0;i<count;i++) h_tmp[i]=2.0f;
    CHECK_CUDA(cudaMemcpy(d_buf,h_tmp,count*sizeof(float),cudaMemcpyHostToDevice));
    free(h_tmp);
    printf("GPU buffer filled with 2.0\n");

    // ── Open mlx5_0 ──────────────────────────────────────────────
    int ndev;
    struct ibv_device **devs = ibv_get_device_list(&ndev);
    struct ibv_device *dev = NULL;
    for(int i=0;i<ndev;i++)
        if(strstr(ibv_get_device_name(devs[i]),"mlx5_0"))
            { dev=devs[i]; break; }
    if(!dev){fprintf(stderr,"mlx5_0 not found\n");exit(1);}

    struct ibv_context *ctx = ibv_open_device(dev);
    struct ibv_pd      *pd  = ibv_alloc_pd(ctx);
    struct ibv_cq      *cq  = ibv_create_cq(ctx,64,NULL,NULL,0);

    // Register GPU MR
    struct ibv_mr *mr = ibv_reg_mr(pd, d_buf, count*sizeof(float),
        IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_READ|IBV_ACCESS_REMOTE_WRITE);
    if(!mr){perror("ibv_reg_mr — ensure nvidia_peermem loaded");exit(1);}
    printf("GPU MR: rkey=0x%x vaddr=%p\n", mr->rkey, mr->addr);

    // Create RC QP — arm_pipeline will RDMA-read from this QP
    struct ibv_qp_init_attr qa={};
    qa.send_cq=cq; qa.recv_cq=cq; qa.qp_type=IBV_QPT_RC;
    qa.cap.max_send_wr=16; qa.cap.max_recv_wr=16;
    qa.cap.max_send_sge=1; qa.cap.max_recv_sge=1;
    struct ibv_qp *qp = ibv_create_qp(pd,&qa);
    if(!qp){perror("ibv_create_qp");exit(1);}
    printf("Local QPN: 0x%x\n", qp->qp_num);

    // Get GID
    union ibv_gid gid;
    ibv_query_gid(ctx, IB_PORT, GID_INDEX, &gid);

    // Transition to INIT
    struct ibv_qp_attr attr={};
    attr.qp_state=IBV_QPS_INIT; attr.pkey_index=0;
    attr.port_num=IB_PORT;
    attr.qp_access_flags=IBV_ACCESS_REMOTE_READ|IBV_ACCESS_REMOTE_WRITE|IBV_ACCESS_LOCAL_WRITE;
    ibv_modify_qp(qp,&attr,IBV_QP_STATE|IBV_QP_PKEY_INDEX|IBV_QP_PORT|IBV_QP_ACCESS_FLAGS);

    // ── TCP: wait for arm_pipeline, exchange QPN ──────────────────
    printf("Waiting for arm_pipeline on port %d...\n", CTRL_PORT_HOST_TO_ARM);
    int srv = socket(AF_INET,SOCK_STREAM,0);
    int opt=1; setsockopt(srv,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in sa={};
    sa.sin_family=AF_INET; sa.sin_port=htons(CTRL_PORT_HOST_TO_ARM);
    sa.sin_addr.s_addr=INADDR_ANY;
    bind(srv,(struct sockaddr*)&sa,sizeof(sa)); listen(srv,1);
    int cli=accept(srv,NULL,NULL);
    printf("arm_pipeline connected\n");

    // Receive arm_pipeline's QPN first
    uint32_t arm_qpn=0;
    recv(cli, &arm_qpn, sizeof(arm_qpn), MSG_WAITALL);
    printf("arm_pipeline QPN: 0x%x\n", arm_qpn);

    // Transition QP to RTR — connect to arm_pipeline's QP
    memset(&attr,0,sizeof(attr));
    attr.qp_state=IBV_QPS_RTR;
    attr.path_mtu=IBV_MTU_1024;
    attr.dest_qp_num=arm_qpn;
    attr.rq_psn=0; attr.max_dest_rd_atomic=16; attr.min_rnr_timer=12;
    attr.ah_attr.port_num=IB_PORT; attr.ah_attr.is_global=1;
    attr.ah_attr.grh.hop_limit=64; attr.ah_attr.grh.sgid_index=GID_INDEX;
    // arm_pipeline sends its GID — for now use zero GID (will be filled after exchange)
    // We receive arm GID along with QPN
    uint8_t arm_gid[16]={};
    recv(cli, arm_gid, 16, MSG_WAITALL);
    memcpy(&attr.ah_attr.grh.dgid, arm_gid, 16);
    ibv_modify_qp(qp,&attr,IBV_QP_STATE|IBV_QP_AV|IBV_QP_PATH_MTU|
                  IBV_QP_DEST_QPN|IBV_QP_RQ_PSN|IBV_QP_MAX_DEST_RD_ATOMIC|IBV_QP_MIN_RNR_TIMER);

    // Transition to RTS
    memset(&attr,0,sizeof(attr));
    attr.qp_state=IBV_QPS_RTS; attr.timeout=14; attr.retry_cnt=7;
    attr.rnr_retry=7; attr.sq_psn=0; attr.max_rd_atomic=16;
    ibv_modify_qp(qp,&attr,IBV_QP_STATE|IBV_QP_TIMEOUT|IBV_QP_RETRY_CNT|
                  IBV_QP_RNR_RETRY|IBV_QP_SQ_PSN|IBV_QP_MAX_QP_RD_ATOMIC);

    // Send RDMA info to arm_pipeline
    gpu_rdma_info_t info={};
    info.vaddr=(uint64_t)(uintptr_t)mr->addr;
    info.rkey=mr->rkey;
    info.count=count;
    info.lid=0;
    info.qpn=qp->qp_num;
    memcpy(info.gid,&gid,16);
    send(cli, &info, sizeof(info), 0);
    printf("Sent GPU RDMA info to arm_pipeline\n");

    double t_start=now_ms();

    // Wait for pipeline complete signal
    uint32_t done=0;
    recv(cli, &done, sizeof(done), MSG_WAITALL);
    double t_end=now_ms();

    printf("\n=== Timing ===\n");
    printf("Total pipeline time : %.2f ms\n", t_end-t_start);
    printf("Effective throughput: %.1f MB/s\n", (count*4.0)/((t_end-t_start)*1e3));

    ibv_destroy_qp(qp); ibv_destroy_cq(cq);
    ibv_dereg_mr(mr); ibv_dealloc_pd(pd); ibv_close_device(ctx);
    ibv_free_device_list(devs);
    cudaFree(d_buf);
    close(cli); close(srv);
    return 0;
}
