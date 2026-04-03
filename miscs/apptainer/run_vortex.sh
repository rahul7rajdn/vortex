#!/bin/bash

export CCACHE_DISABLE=1

pwd
echo " ===================== Entering Vortex ============================"
cd /home/vortex
git status
pwd
mkdir -p  32_build
cd 32_build
../configure --xlen=32 --tooldir=$HOME/tools
source ./ci/toolchain_env.sh

verilator --version

pwd
echo " ===================== Starting make ============================"
make -j$(nproc)

pwd
echo " ===================== Running Tests  ============================"
./ci/blackbox.sh --cores=2 --app=demo --driver=simx
./ci/blackbox.sh --cores=2 --app=demo --driver=rtlsim
./ci/blackbox.sh --cores=2 --app=demo --driver=xrt

echo " ===================== Done  ============================"
