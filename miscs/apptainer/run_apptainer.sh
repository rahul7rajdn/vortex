#!/bin/bash

ARCH=$(uname -m)

if [ "$ARCH" = "x86_64" ]; then
    export GIT_REPO_PATH=${GIT_REPO_PATH:-~/USERSCRATCH/x86_code/vortex}
    export VORTEX_TOOLCHAIN_PATH=${VORTEX_TOOLCHAIN_PATH:-~/USERSCRATCH/tools}
    export CONTAINER_IMAGE=${CONTAINER_IMAGE:-vortex_x86.sif}
elif [ "$ARCH" = "aarch64" ]; then
    export GIT_REPO_PATH=${GIT_REPO_PATH:-~/USERSCRATCH/may_code/vortex}
    export VORTEX_TOOLCHAIN_PATH=${VORTEX_TOOLCHAIN_PATH:-~/USERSCRATCH/aarch_vortex_tools}
    export CONTAINER_IMAGE=${CONTAINER_IMAGE:-vortex_aarch64.sif}
else
    echo "Unsupported architecture: $ARCH"
    exit 1
fi

echo "Launching $CONTAINER_IMAGE on $ARCH..."

# Helper: only add --bind if the source path exists on the host
bind_if_exists() {
    local src="$1"
    local dst="${2:-$1}"
    if [ -e "$src" ]; then
        echo "--bind ${src}:${dst}"
    else
        echo "WARNING: skipping bind for $src (not found on host)" >&2
    fi
}

apptainer shell --fakeroot --cleanenv --writable-tmpfs \
    $(bind_if_exists /dev/bus/usb) \
    $(bind_if_exists /sys/bus/pci) \
    $(bind_if_exists /projects) \
    $(bind_if_exists /lib/firmware) \
    $(bind_if_exists /opt/xilinx /opt/xilinx/) \
    $(bind_if_exists /tools) \
    $(bind_if_exists /netscratch) \
    --bind "$VORTEX_TOOLCHAIN_PATH":/home/tools \
    --bind "$GIT_REPO_PATH":/home/vortex \
    "$CONTAINER_IMAGE"
