# Case 6 — Host-to-Host RDMA (violet1 ↔ violet2, x86)

## What this case does

Measures raw RDMA bandwidth between violet1 and violet2 x86 host servers using
the ConnectX-7 NIC (mlx5_0) and RoCEv2 over the 100GbE Nexus switch.
This establishes the baseline network capability of the physical infrastructure.

## Hardware

| Node | Role | IP (RDMA) | RDMA device | GID index |
|---|---|---|---|---|
| violet1 | H100 server — client | 10.10.10.49 | mlx5_0 (ens255f0np0) | 3 |
| violet2 | A100 server — server | 10.10.10.50 | mlx5_0 (enp184s0f0np0) | 3 |

## Network path

```
violet1 (mlx5_0) → QSFP cable → Nexus 9336C Eth1/10
                                 Nexus 9336C Eth1/14 ← QSFP cable ← violet2 (mlx5_0)
```
VLAN 738 · 100GbE · RoCEv2

## Steps

### Step 1 — Start server on violet2
```bash
[rn84@violet2 ~]$ ib_write_bw -d mlx5_0 -s 1048576 --report_gbits
```

Wait until:
```
************************************
* Waiting for client to connect... *
************************************
```

### Step 2 — Run client on violet1 (immediately after server is ready)
```bash
[rn84@violet1 ~]$ ib_write_bw -d mlx5_0 -s 1048576 --report_gbits 10.10.10.50
```

## Example output

**violet2 (server):**
```
RDMA_Write BW Test
 Device         : mlx5_0
 Link type       : Ethernet
 GID index       : 3

 local address: LID 0000 QPN 0x02c8 PSN 0xae0926 RKey 0x203e00 VAddr 0x007fda80aff000
 GID: 00:00:00:00:00:00:00:00:00:00:255:255:10:10:10:50
 remote address: LID 0000 QPN 0x036c PSN 0x86beca RKey 0x203e00 VAddr 0x007f0d03eff000
 GID: 00:00:00:00:00:00:00:00:00:00:255:255:10:10:10:49

 #bytes     #iterations    BW peak[Gb/sec]    BW average[Gb/sec]
 1048576    5000             92.58              92.58
```

**violet1 (client):**
```
 local address: LID 0000 QPN 0x036c PSN 0x86beca
 GID: ...10:10:10:49
 remote address: LID 0000 QPN 0x02c8 PSN 0xae0926
 GID: ...10:10:10:50

 #bytes     #iterations    BW peak[Gb/sec]    BW average[Gb/sec]
 1048576    5000             92.58              92.58              0.011037
```

## Key results

| Metric | Value |
|---|---|
| Peak bandwidth | 92.58 Gb/s |
| Average bandwidth | 92.58 Gb/s |
| Message rate | 0.011037 Mpps |
| Link speed | 100GbE |
| Transport | RoCEv2 RC (Reliable Connected) |
| Buffer type | Host RAM |

## Notes

- GID index 3 = RoCEv2 GID (IPv4-mapped IPv6 format over Ethernet)
- `ib_write_bw` uses `IBV_WR_RDMA_WRITE` operation — one-sided RDMA write
- 5000 iterations with 1MB messages to measure sustained bandwidth
- Result at ~92% of theoretical 100GbE line rate (accounting for protocol overhead)
- `firewalld` must be stopped or port opened before running tests
