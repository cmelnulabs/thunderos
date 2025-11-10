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
    gcc-riscv64-linux-gnu \
    qemu-system-misc \
    && rm -rf /var/lib/apt/lists/*

# Verify installations
RUN riscv64-linux-gnu-gcc --version && \
    qemu-system-riscv64 --version

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
