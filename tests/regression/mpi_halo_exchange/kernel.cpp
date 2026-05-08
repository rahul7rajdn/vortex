#include <vx_spawn.h>
#include "common.h"

void kernel_body(kernel_arg_t* __UNIFORM__ arg) {
    auto local_ptr  = reinterpret_cast<float*>(arg->local_addr);
    auto halo_ptr   = reinterpret_cast<float*>(arg->halo_addr);
    auto output_ptr = reinterpret_cast<float*>(arg->output_addr);

    // output[i] = local[i] + halo[i]
    output_ptr[blockIdx.x] = local_ptr[blockIdx.x] + halo_ptr[blockIdx.x];
}

int main() {
    kernel_arg_t* arg = (kernel_arg_t*)csr_read(VX_CSR_MSCRATCH);
    return vx_spawn_threads(1, &arg->count, nullptr, (vx_kernel_func_cb)kernel_body, arg);
}