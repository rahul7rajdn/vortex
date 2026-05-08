# Case 10 — GPUDirect RDMA (H100 ↔ A100 via ConnectX-7)

## What this case does

Measures RDMA bandwidth where the source and destination buffers are GPU VRAM
(not host RAM). Using `nvidia_peermem`, the ConnectX-7 NIC can DMA directly
from/to GPU memory over PCIe, completely bypassing the CPU and host RAM.
This is the fastest possible GPU-to-GPU transfer path.

## Hardware

| Node | GPU | PCIe addr | IP (RDMA) | Device |
|---|---|---|---|---|
| violet1 | H100 PCIe | D8:00 | 10.10.10.49 | mlx5_0 |
| violet2 | A100 PCIe | A8:00 | 10.10.10.50 | mlx5_0 |

## Data path

```
H100 GPU VRAM ──PCIe DMA──→ ConnectX-7 (mlx5_0)
                               ↓ RoCEv2 · 100GbE · Nexus switch
                             ConnectX-7 (mlx5_0) ──PCIe DMA──→ A100 GPU VRAM
```

Host RAM is completely bypassed. CPU is not in the data path.

## Prerequisites

`nvidia_peermem` kernel module must be loaded on both hosts:
```bash
sudo modprobe nvidia_peermem
lsmod | grep nvidia_peermem
```

This module bridges the NVIDIA GPU memory allocator and the RDMA stack,
allowing `ibv_reg_mr()` to register GPU VRAM as an RDMA memory region.

## Steps

### Step 1 — Load nvidia_peermem on both nodes
```bash
# On violet1:
sudo modprobe nvidia_peermem
# On violet2:
sudo modprobe nvidia_peermem
```

### Step 2 — Start server on violet2
```bash
[rn84@violet2 ~]$ ib_write_bw -d mlx5_0 -s 1048576 --report_gbits --use_cuda=0
```

Output during setup:
```
initializing CUDA
CUDA device 0: PCIe address is A8:00
allocated GPU buffer at 0x557f359d17d0 for type CUDA_MEM_DEVICE
* Waiting for client to connect... *
```

### Step 3 — Run client on violet1
```bash
[rn84@violet1 ~]$ ib_write_bw -d mlx5_0 -s 1048576 --report_gbits --use_cuda=0 10.10.10.50
```

Output during setup:
```
initializing CUDA
CUDA device 0: PCIe address is D8:00
allocated GPU buffer at 0x55ac4eeb57f0 for type CUDA_MEM_DEVICE
```

## Example output

**violet2 (server):**
```
 local address: LID 0000 QPN 0x031d PSN 0x1331c RKey 0x203e5d VAddr 0x007f8f83300000
 GID: 00:00:00:00:00:00:00:00:00:00:255:255:10:10:10:50
 remote address: LID 0000 QPN 0x03cb PSN 0x7c11c8 RKey 0x203eac VAddr 0x007f96c6700000
 GID: 00:00:00:00:00:00:00:00:00:00:255:255:10:10:10:49

 #bytes     #iterations    BW peak[Gb/sec]    BW average[Gb/sec]
 1048576    5000             92.58              92.58              0.011036
```

## Key results

| Metric | Value |
|---|---|
| Peak bandwidth | 92.58 Gb/s |
| Average bandwidth | 92.58 Gb/s |
| Buffer type | CUDA_MEM_DEVICE (GPU VRAM) |
| Transport | RoCEv2 RC · mlx5_0 · GID 3 |
| Host RAM involvement | None |
| CPU involvement | None (in data path) |

## Critical finding

GPUDirect RDMA (92.58 Gb/s) **= Host RDMA (Case 6: 92.58 Gb/s)**

This means the PCIe path from GPU to NIC is not a bottleneck.
The full 100GbE bandwidth is accessible directly from GPU VRAM.

## Troubleshooting

- `Couldn't allocate MR with error=14 (EFAULT)`: nvidia_peermem not loaded
- `Couldn't connect to host:18515`: server not running, firewall blocking port 18515
- Stop firewalld: `sudo systemctl stop firewalld`
