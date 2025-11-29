# ThunderOS Build Environment
# Matches local development environment for consistent builds
FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Set locale for Sphinx documentation builds
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# Install base dependencies and build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    curl \
    git \
    python3 \
    python3-pip \
    xz-utils \
    e2fsprogs \
    ninja-build \
    pkg-config \
    libglib2.0-dev \
    libpixman-1-dev \
    libslirp-dev \
    # VNC viewer support for GPU testing
    novnc \
    websockify \
    # GitHub CLI for releases
    gh \
    && rm -rf /var/lib/apt/lists/*

# Install Python dependencies for QEMU build and documentation
RUN pip3 install tomli sphinx sphinx_rtd_theme

# Download and install RISC-V GNU toolchain (bare-metal)
RUN cd /tmp && \
    wget -q https://github.com/riscv-collab/riscv-gnu-toolchain/releases/download/2024.04.12/riscv64-elf-ubuntu-22.04-gcc-nightly-2024.04.12-nightly.tar.gz && \
    tar xzf riscv64-elf-ubuntu-22.04-gcc-nightly-2024.04.12-nightly.tar.gz -C /opt && \
    rm riscv64-elf-ubuntu-22.04-gcc-nightly-2024.04.12-nightly.tar.gz

# Build QEMU 10.1.2 with RISC-V support and SSTC extension
RUN cd /tmp && \
    wget -q https://download.qemu.org/qemu-10.1.2.tar.xz && \
    tar xJf qemu-10.1.2.tar.xz && \
    cd qemu-10.1.2 && \
    ./configure --target-list=riscv64-softmmu --prefix=/opt/qemu && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && \
    rm -rf qemu-10.1.2 qemu-10.1.2.tar.xz

# Add toolchain and QEMU to PATH
ENV PATH="/opt/riscv/bin:/opt/qemu/bin:${PATH}"

# Verify installations
RUN riscv64-unknown-elf-gcc --version && \
    qemu-system-riscv64 --version

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
