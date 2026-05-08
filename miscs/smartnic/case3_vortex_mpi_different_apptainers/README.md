# Case 3 — Vortex MPI: 2 Ranks Without Apptainer (Same BF3 Host)

## What this case does

Runs Vortex SIMX MPI with 2 ranks natively on the BF3 ARM — no apptainer.
Each rank runs as a separate native process with shared environment variables.
This is the simplest multi-rank configuration and confirms that Vortex SIMX
runs correctly without container overhead on the BF3.

## Hardware

| Node | Role | Arch |
|---|---|---|
| violet1-bf3-1 | BF3 DPU ARM (single host) | aarch64 |

## Steps

### Step 1 — Set environment on violet1-bf3-1
```bash
export VORTEX_DRIVER=simx
export LD_LIBRARY_PATH=/net/netscratch/rn84/may_code/vortex/build/runtime:\
/net/netscratch/rn84/may_code/vortex/third_party/ramulator:$LD_LIBRARY_PATH

echo $LD_LIBRARY_PATH
# /net/netscratch/rn84/may_code/vortex/build/runtime:...
echo $VORTEX_DRIVER
# simx
```

### Step 2 — Run mpirun (native, no apptainer)
```bash
mpirun --wdir /net/netscratch/rn84/may_code/vortex/build/tests/regression/mpi_vecadd \
    --allow-run-as-root --oversubscribe -np 2 \
    ./mpi_vecadd -n5000
```

## Example output

```
rank = 1, world_size = 2, host = violet1-bf3-1
rank = 0, world_size = 2, host = violet1-bf3-1
Rank: 1- Upload kernel binary
Rank: 0- Upload kernel binary
PASSED!
PERF: instrs=78632, cycles=94631, IPC=0.830933
PERF: instrs=78632, cycles=94631, IPC=0.830933
```

## Key observations

- IPC = 0.830933 (higher than 4-rank case because less oversubscription on 8 physical cores)
- Both ranks on same host: `violet1-bf3-1`
- Transport: shared memory (intra-node MPI)
- No container overhead
- Binary path on shared filesystem `/net/netscratch/rn84/...` accessible from BF3

## Comparison with Case 1 and 2

| Case | Ranks | Container | IPC | Host |
|---|---|---|---|---|
| Case 1 | 4 | Single apptainer (inside) | 0.748032 | violet1-bf3-1 |
| Case 2 | 4 | Per-rank apptainer exec | 0.748032 | violet1-bf3-1 |
| Case 3 | 2 | Native (no apptainer) | 0.830933 | violet1-bf3-1 |

Higher IPC with fewer ranks because 2 Vortex SIMX instances compete less for 8 physical ARM cores.
