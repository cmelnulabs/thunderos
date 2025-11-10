# ThunderOS Build Environment
# Matches local development environment for consistent builds
FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install base dependencies and RISC-V toolchain
RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    curl \
    git \
    python3 \
    python3-pip \
    xz-utils \
    qemu-system-misc \
    && rm -rf /var/lib/apt/lists/*

# Download and install RISC-V GNU toolchain (bare-metal)
RUN cd /tmp && \
    wget -q https://github.com/riscv-collab/riscv-gnu-toolchain/releases/download/2024.04.12/riscv64-elf-ubuntu-22.04-gcc-nightly-2024.04.12-nightly.tar.gz && \
    tar xzf riscv64-elf-ubuntu-22.04-gcc-nightly-2024.04.12-nightly.tar.gz -C /opt && \
    rm riscv64-elf-ubuntu-22.04-gcc-nightly-2024.04.12-nightly.tar.gz

# Add toolchain to PATH
ENV PATH="/opt/riscv/bin:${PATH}"

# Verify installations
RUN riscv64-unknown-elf-gcc --version && \
    qemu-system-riscv64 --version

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
