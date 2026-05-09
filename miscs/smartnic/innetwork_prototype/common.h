#pragma once
#include <stdint.h>

typedef struct {
    uint32_t count;
    uint64_t src_addr;
    uint64_t dst_addr;
    float    scale;
} kernel_arg_t;

// Exchanged over TCP: gpu_host → arm_pipeline
typedef struct {
    uint64_t vaddr;
    uint32_t rkey;
    uint32_t count;
    uint8_t  gid[16];   // raw GID bytes of gpu_host mlx5_0
    uint16_t lid;
    uint32_t qpn;       // QPN of gpu_host RC QP — arm_pipeline connects to this
} gpu_rdma_info_t;

// Exchanged over TCP: arm_pipeline → arm_receiver
typedef struct {
    uint64_t vaddr;
    uint32_t rkey;
    uint32_t count;
    uint8_t  gid[16];
    uint16_t lid;
    uint32_t qpn;       // QPN of arm_pipeline RC QP
} result_rdma_info_t;

#define CTRL_PORT_HOST_TO_ARM   9001
#define CTRL_PORT_ARM_TO_GPU2   9002
#define CTRL_PORT_ARM_TO_HOST2  9003
#define IB_PORT                 1
#define GID_INDEX               3