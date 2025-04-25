#include <vx_spawn.h>
#include "common.h"


#define TARGET_ADDR 0x70000000  // address in BRAM device

void kernel_body(kernel_arg_t* __UNIFORM__ arg) {
	auto data_ptr = reinterpret_cast<volatile TYPE*>(TARGET_ADDR);  // cast the fixed address

    // Load the value from BRAM
    TYPE value = *data_ptr;

    // Increment
    value = value + 1;

    // Store back to BRAM
    *data_ptr = value;
}

int main() {
	kernel_arg_t* arg = (kernel_arg_t*)csr_read(VX_CSR_MSCRATCH);
	return vx_spawn_threads(1, &arg->num_tasks, nullptr, (vx_kernel_func_cb)kernel_body, arg);
}
