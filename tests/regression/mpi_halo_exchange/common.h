#pragma once
#include <stdint.h>

typedef struct {
    uint32_t count;
    uint64_t local_addr;
    uint64_t halo_addr;
    uint64_t output_addr;
} kernel_arg_t;