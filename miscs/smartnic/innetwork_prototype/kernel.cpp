// kernel.cpp — Vortex SIMX kernel, runs on BF3 ARM
// Applies scale to each element: dst[i] = src[i] * scale
// This represents in-network compute: normalize, reduce, transform

#include <vx_spawn.h>
#include "common.h"

void kernel_body(kernel_arg_t* __UNIFORM__ arg) {
    auto src = reinterpret_cast<float*>(arg->src_addr);
    auto dst = reinterpret_cast<float*>(arg->dst_addr);
    dst[blockIdx.x] = src[blockIdx.x] * arg->scale;
}

int main() {
    kernel_arg_t* arg = (kernel_arg_t*)csr_read(VX_CSR_MSCRATCH);
    return vx_spawn_threads(1, &arg->count, nullptr,
                            (vx_kernel_func_cb)kernel_body, arg);
}
