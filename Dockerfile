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
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

# Install RISC-V GNU toolchain from pre-built binaries
# Using SiFive's pre-built toolchain (GCC 10.2.0)
RUN wget -q https://static.dev.sifive.com/dev-tools/freedom-tools/v2020.12/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14.tar.gz && \
    tar -xzf riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14.tar.gz && \
    mv riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14 /opt/riscv && \
    rm riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14.tar.gz

# Add RISC-V toolchain to PATH
ENV PATH="/opt/riscv/bin:${PATH}"

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
