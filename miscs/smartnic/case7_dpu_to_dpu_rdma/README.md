# Case 7 — DPU ARM to DPU ARM RDMA (violet1-bf3-1 ↔ violet2-bf3-1)

## What this case does

Measures raw RDMA bandwidth between the ARM processors of two BF3 SmartNICs
using the Scalable Function (SF) interface mlx5_2 and RoCEv2.
This proves the ARM cores inside the BF3 can communicate at full 100GbE bandwidth,
which is a prerequisite for the in-network compute pipeline.

## Hardware

| Node | Role | IP (RDMA) | Device | GID index |
|---|---|---|---|---|
| violet1-bf3-1 | BF3 ARM — client | 10.10.10.69 | mlx5_2 (enp3s0f0s0) | 1 |
| violet2-bf3-1 | BF3 ARM — server | 10.10.10.70 | mlx5_2 (enp3s0f0s0) | 1 |

## Network path (ARM-side)

```
violet1-bf3-1 ARM
    ↓ enp3s0f0s0 (Scalable Function)
    ↓ OVS bridge (internal BF3 switching)
    ↓ p0 physical port (10.10.10.69)
    ↓ QSFP → Nexus 9336C Eth1/10
              Nexus 9336C Eth1/14 → QSFP
    ↓ p0 physical port (10.10.10.70)
    ↓ OVS bridge
    ↓ enp3s0f0s0
violet2-bf3-1 ARM
```

Note: GID index 1 (not 3) is used on ARM because the SF interface populates a different
GID table than the host-side mlx5_0.

## Steps

### Step 1 — Start server on violet2-bf3-1
```bash
(base) rn84@violet2-bf3-1:~$ ib_write_bw -d mlx5_2 -s 1048576 --report_gbits
```

Wait for:
```
************************************
* Waiting for client to connect... *
************************************
```

### Step 2 — Run client on violet1-bf3-1
```bash
(base) rn84@violet1-bf3-1:~$ ib_write_bw -d mlx5_2 -s 1048576 --report_gbits 10.10.10.70
```

## Example output

**violet2-bf3-1 (server):**
```
RDMA_Write BW Test
 Device         : mlx5_2
 Link type       : Ethernet
 GID index       : 1

 local address: LID 0000 QPN 0x0e3d PSN 0x264ade RKey 0x053e00 VAddr 0x00ee75e9060000
 GID: 00:00:00:00:00:00:00:00:00:00:255:255:10:10:10:70
 remote address: LID 0000 QPN 0x0e42 PSN 0xe4c7de RKey 0x053e00 VAddr 0x00eed30bae0000
 GID: 00:00:00:00:00:00:00:00:00:00:255:255:10:10:10:69

 #bytes     #iterations    BW peak[Gb/sec]    BW average[Gb/sec]
 1048576    5000             92.62              92.62              0.011041
```

**violet1-bf3-1 (client):**
```
 local address: GID ...10:10:10:69
 remote address: GID ...10:10:10:70

 #bytes     #iterations    BW peak[Gb/sec]    BW average[Gb/sec]
 1048576    5000             92.62              92.62              0.011041
```

## Key results

| Metric | Value |
|---|---|
| Peak bandwidth | 92.62 Gb/s |
| Average bandwidth | 92.62 Gb/s |
| Message rate | 0.011041 Mpps |
| Transport | RoCEv2 RC via SF → OVS bridge → p0 |
| Buffer type | ARM host RAM |
| GID index | 1 (mlx5_2 SF port) |

## Comparison with host RDMA (Case 6)

| Case | Device | BW | Notes |
|---|---|---|---|
| Case 6 Host RDMA | mlx5_0 (host NIC) | 92.58 Gb/s | Host RAM buffer |
| Case 7 DPU ARM RDMA | mlx5_2 (ARM SF) | 92.62 Gb/s | ARM RAM buffer |

**Result: ARM achieves the same bandwidth as the host** — the SF/OVS path introduces
no measurable overhead. The BF3 ARM can move data at full line rate.
