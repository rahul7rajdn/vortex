# Makefile for in-network compute prototype
# Run `make help` to see targets

# ── violet1 host (x86) paths ─────────────────────────────────────
CUDA_HOME   ?= /usr/local/cuda-13.1
MPI_HOME    ?= /projects/tools/x86_64/ubuntu-22.04/hpcx-v2.21-gcc-doca_ofed-ubuntu22.04-cuda12-x86_64/ompi

# ── BF3 ARM paths (inside apptainer) ─────────────────────────────
VORTEX_HOME ?= /net/netscratch/rn84/may_code/vortex
VORTEX_RT   ?= $(VORTEX_HOME)/build/runtime
LLVM_VORTEX ?= /home/tools/llvm-vortex
RISCV_TOOLCHAIN ?= /home/tools/riscstar-toolchain-15.2-r1-aarch64-riscv32-none-elf

# ── Output dir ───────────────────────────────────────────────────
BUILD       ?= ./build

.PHONY: all host arm kernel help clean

all: host arm kernel

# ── Build gpu_host and gpu_receiver on violet1/violet2 (x86) ─────
host: $(BUILD)/gpu_host $(BUILD)/gpu_receiver

$(BUILD)/gpu_host: gpu_host.cu common.h | $(BUILD)
	nvcc $< -o $@ \
	    -I$(CUDA_HOME)/include \
	    -L$(CUDA_HOME)/lib64 \
	    -libverbs -lpthread \
	    -std=c++17
	@echo "Built: $@"

$(BUILD)/gpu_receiver: gpu_receiver.cu common.h | $(BUILD)
	nvcc $< -o $@ \
	    -I$(CUDA_HOME)/include \
	    -L$(CUDA_HOME)/lib64 \
	    -libverbs -lpthread \
	    -std=c++17
	@echo "Built: $@"

# ── Build ARM programs on violet1-bf3-1 (aarch64, inside apptainer)
arm: $(BUILD)/arm_pipeline $(BUILD)/arm_receiver

$(BUILD)/arm_pipeline: arm_pipeline.cpp common.h | $(BUILD)
	g++ -std=c++17 $< -o $@ \
	    -I$(VORTEX_HOME)/runtime/include \
	    -I$(VORTEX_HOME)/build/hw \
	    -I$(VORTEX_HOME)/sim/common \
	    -L$(VORTEX_RT) -lvortex \
	    -libverbs -lpthread \
	    -Wl,-rpath,$(VORTEX_RT)
	@echo "Built: $@"

$(BUILD)/arm_receiver: arm_receiver.cpp common.h | $(BUILD)
	g++ -std=c++17 $< -o $@ \
	    -libverbs -lpthread
	@echo "Built: $@"

# ── Build Vortex kernel (RISC-V, run on violet1-bf3-1 inside apptainer)
RISCV_SYSROOT   = $(RISCV_TOOLCHAIN)/riscv32-none-elf
LIBCRT_VORTEX   ?= /home/tools/libcrt32
LIBC_VORTEX     ?= /home/tools/libc32

VX_CFLAGS  = --sysroot=$(RISCV_SYSROOT) --gcc-toolchain=$(RISCV_TOOLCHAIN)
VX_CFLAGS += -Xclang -target-feature -Xclang +vortex
VX_CFLAGS += -Xclang -target-feature -Xclang +zicond
VX_CFLAGS += -mllvm -disable-loop-idiom-all
VX_CFLAGS += --target=riscv32-none-elf
VX_CFLAGS += -march=rv32imaf -mabi=ilp32f -O3 -mcmodel=medany
VX_CFLAGS += -fno-rtti -fno-exceptions -nostartfiles -nostdlib
VX_CFLAGS += -fdata-sections -ffunction-sections
VX_CFLAGS += -I$(VORTEX_HOME)/kernel/include
VX_CFLAGS += -I$(VORTEX_HOME)/build/hw
VX_CFLAGS += -DXLEN_32 -DNDEBUG

VX_LDFLAGS  = -Wl,-z,norelro -fuse-ld=lld
VX_LDFLAGS += -Wl,-Bstatic,--gc-sections,-T,$(VORTEX_HOME)/kernel/scripts/link32.ld,--defsym=STARTUP_ADDR=0x80000000
VX_LDFLAGS += $(VORTEX_HOME)/build/kernel/libvortex.a
VX_LDFLAGS += -L$(LIBC_VORTEX)/lib -lm -lc
VX_LDFLAGS += $(LIBCRT_VORTEX)/lib/baremetal/libclang_rt.builtins-riscv32.a

kernel: $(BUILD)/kernel.vxbin

$(BUILD)/kernel.elf: kernel.cpp common.h | $(BUILD)
	$(LLVM_VORTEX)/bin/clang++ $(VX_CFLAGS) $< \
	    -Wl,$(VX_LDFLAGS) -o $@
	@echo "Built: $@"

$(BUILD)/kernel.vxbin: $(BUILD)/kernel.elf
	OBJCOPY=$(LLVM_VORTEX)/bin/llvm-objcopy \
	    $(VORTEX_HOME)/kernel/scripts/vxbin.py $< $@
	@echo "Built: $@"

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

# ── How to run ───────────────────────────────────────────────────
help:
	@echo ""
	@echo "=== In-Network Compute Prototype ==="
	@echo ""
	@echo "Build all:"
	@echo "  On violet1 (x86):        make host"
	@echo "  On violet1-bf3-1 (ARM):  make arm kernel   (inside apptainer)"
	@echo ""
	@echo "Run (4 terminals, in this order):"
	@echo ""
	@echo "  Terminal 1 — violet2 host:"
	@echo "    ./build/gpu_receiver"
	@echo ""
	@echo "  Terminal 2 — violet2-bf3-1 ARM:"
	@echo "    ./build/arm_receiver 143.215.138.111"
	@echo ""
	@echo "  Terminal 3 — violet1-bf3-1 ARM (inside apptainer):"
	@echo "    VORTEX_DRIVER=simx LD_LIBRARY_PATH=... ./build/arm_pipeline \\"
	@echo "        10.10.10.49 10.10.10.70 ./build/kernel.vxbin"
	@echo ""
	@echo "  Terminal 4 — violet1 host:"
	@echo "    sudo modprobe nvidia_peermem"
	@echo "    ./build/gpu_host 10.10.10.69 262144"
	@echo ""
	@echo "Expected result:"
	@echo "  H100 VRAM filled with 2.0"
	@echo "  Vortex scales by 0.5  →  result 1.0"
	@echo "  A100 VRAM contains 1.0  →  PASSED"
