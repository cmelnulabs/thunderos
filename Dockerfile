# ThunderOS Build Environment
# Matches local development environment for consistent builds
FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install base dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    curl \
    git \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install RISC-V toolchain (GCC 10.2.0)
RUN apt-get update && apt-get install -y \
    gcc-riscv64-unknown-elf \
    binutils-riscv64-unknown-elf \
    && rm -rf /var/lib/apt/lists/*

# Install QEMU 6.2.0 (matching local version)
RUN apt-get update && apt-get install -y \
    qemu-system-misc \
    && rm -rf /var/lib/apt/lists/*

# Verify installations
RUN riscv64-unknown-elf-gcc --version && \
    qemu-system-riscv64 --version

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
