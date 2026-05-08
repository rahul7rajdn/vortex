# Case 5 — Vortex MPI: 2 BF3 ARM Nodes over RDMA

## What this case does

Runs Vortex SIMX MPI with 2 ranks distributed across two BF3 ARM nodes using
RDMA (RoCEv2 via UCX dc_mlx5) as the MPI transport. This demonstrates that
Vortex SIMX computation is transport-transparent — identical IPC on TCP and RDMA —
while the underlying network delivers 100GbE bandwidth instead of 1GbE.

## Hardware

| Node | IP (RDMA) | IP (OOB) | RDMA device | GID index |
|---|---|---|---|---|
| violet1-bf3-1 | 10.10.10.69 | 143.215.138.93 | mlx5_2 (SF) | 1 |
| violet2-bf3-1 | 10.10.10.70 | 143.215.138.124 | mlx5_2 (SF) | 1 |

The BF3 ARM accesses the NIC via a Scalable Function (SF) virtual function
exposed as `enp3s0f0s0` → routed through OVS bridge → physical port → Nexus 100GbE switch.

## Hostfiles

**hosts.txt** (OOB IPs — for TCP fallback):
```
143.215.138.124 slots=1
143.215.138.93  slots=1
```

**rdma_hosts.txt** (RDMA IPs — for UCX RDMA transport):
```
10.10.10.70 slots=1
10.10.10.69 slots=1
```

## Steps

### Step 1 — Set environment on violet1-bf3-1
```bash
export VORTEX_DRIVER=simx
export LD_LIBRARY_PATH=/net/netscratch/rn84/may_code/vortex/build/runtime:\
/net/netscratch/rn84/may_code/vortex/third_party/ramulator:$LD_LIBRARY_PATH
```

### Step 2 — Verify RDMA transport is used
```bash
mpirun -np 2 --hostfile rdma_hosts.txt \
    --mca pml ucx \
    --mca oob_tcp_if_include enp3s0f0s0 \
    -x UCX_NET_DEVICES=mlx5_2:1 \
    -x UCX_IB_GID_INDEX=1 \
    -x UCX_TLS=dc_mlx5,sm,self \
    -x UCX_LOG_LEVEL=info \
    -x LD_LIBRARY_PATH -x VORTEX_DRIVER \
    --wdir /net/netscratch/rn84/may_code/vortex/build/tests/regression/mpi_vecadd \
    ./mpi_vecadd -n5000 2>&1 | grep -i "dc_mlx5\|transport\|inter-node"
```

Expected confirmation:
```
UCX INFO ucp_context_0 inter-node cfg#1 tag(dc_mlx5/mlx5_2:1)
```

### Step 3 — Run Vortex MPI vecadd over RDMA
```bash
time mpirun -np 2 --hostfile rdma_hosts.txt \
    --mca pml ucx \
    --mca oob_tcp_if_include enp3s0f0s0 \
    -x UCX_NET_DEVICES=mlx5_2:1 \
    -x UCX_IB_GID_INDEX=1 \
    -x UCX_TLS=dc_mlx5,sm,self \
    -x LD_LIBRARY_PATH -x VORTEX_DRIVER \
    --wdir /net/netscratch/rn84/may_code/vortex/build/tests/regression/mpi_vecadd \
    ./mpi_vecadd -n500000
```

## Example output

```
rank = 0, world_size = 2, host = violet2-bf3-1
rank = 1, world_size = 2, host = violet1-bf3-1
Rank: 0- Upload kernel binary
Rank: 1- Upload kernel binary
PERF: instrs=7503628, cycles=9278327, IPC=0.808726
PASSED!
PERF: instrs=7503628, cycles=9278327, IPC=0.808726

real    0m55.617s
user    0m46.970s
sys     0m1.046s
```

## Key observations

- UCX transport confirmed: `dc_mlx5/mlx5_2:1` (hardware-offloaded RDMA)
- IPC = 0.808726 — identical to TCP case (Case 4), proving transport transparency
- Wall time: 55.617s vs 56.004s TCP — nearly identical (compute dominated)
- GID index 1 = RoCEv2 GID for 10.10.10.69/10.10.10.70
- OOB TCP (`enp3s0f0s0`) used for MPI control messages; RDMA for data

## What transport transparency means

The fact that IPC is identical (0.808726) across TCP and RDMA transports means:
- Vortex SIMX computation is completely independent of how MPI data moves
- The RDMA advantage (100GbE vs 1GbE, 7.4µs vs 66µs latency) becomes visible
  at larger data sizes where communication is not dominated by compute time
