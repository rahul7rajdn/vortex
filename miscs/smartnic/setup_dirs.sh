#!/bin/bash
# Run this on violet1-bf3-1 to create the smartnic directory structure
# cd ~/USERSCRATCH/may_code/vortex/miscs && bash setup_dirs.sh

BASE="USERSCRATCH/may_code/vortex/miscs/smartnic"
mkdir -p $BASE

mkdir -p $BASE/case1_vortex_single_apptainer_bf3
mkdir -p $BASE/case2_vortex_mpi_same_host_apptainer
mkdir -p $BASE/case3_vortex_mpi_different_apptainers
mkdir -p $BASE/case4_vortex_mpi_2_bf3_hosts_tcp
mkdir -p $BASE/case5_vortex_mpi_2_bf3_hosts_rdma
mkdir -p $BASE/case6_host_to_host_rdma
mkdir -p $BASE/case7_dpu_to_dpu_rdma
mkdir -p $BASE/case8_gpu_to_gpu_tcp
mkdir -p $BASE/case9_gpu_to_gpu_rdma
mkdir -p $BASE/case10_gpudirect_rdma
mkdir -p $BASE/case11_mpi_halo_exchange
mkdir -p $BASE/case12_mpi_cross_architecture
mkdir -p $BASE/case13_mpi_gpu_simx_heterogeneous
mkdir -p $BASE/case14_end_to_end_pipeline

echo "All 14 case directories created under $BASE"
ls $BASE/
