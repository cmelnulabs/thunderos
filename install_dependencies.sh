#!/bin/bash

# ThunderOS - Dependency Installation Script
# This script installs all required dependencies for building and running ThunderOS

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=====================================${NC}"
echo -e "${BLUE}ThunderOS Dependency Installation${NC}"
echo -e "${BLUE}=====================================${NC}\n"

# Check if running as root for some installations
if [[ $EUID -eq 0 ]]; then
   echo -e "${RED}Please run this script as a normal user (not root).${NC}"
   echo -e "${YELLOW}The script will ask for sudo password when needed.${NC}\n"
   exit 1
fi

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Update package lists
echo -e "${YELLOW}[1/5] Updating package lists...${NC}"
sudo apt update

# Install RISC-V toolchain
echo -e "\n${YELLOW}[2/5] Installing RISC-V toolchain...${NC}"
if command_exists riscv64-unknown-elf-gcc; then
    echo -e "${GREEN}✓ RISC-V toolchain already installed${NC}"
else
    echo -e "${BLUE}Installing RISC-V GNU toolchain...${NC}"
    sudo apt install -y gcc-riscv64-unknown-elf
    
    if ! command_exists riscv64-unknown-elf-gcc; then
        echo -e "${YELLOW}Standard package not available, installing from source...${NC}"
        sudo apt install -y autoconf automake autotools-dev curl python3 \
            libmpc-dev libmpfr-dev libgmp-dev gawk build-essential \
            bison flex texinfo gperf libtool patchutils bc \
            zlib1g-dev libexpat-dev
        
        echo -e "${YELLOW}Note: You may need to manually install the RISC-V toolchain.${NC}"
        echo -e "${YELLOW}Visit: https://github.com/riscv-collab/riscv-gnu-toolchain${NC}"
    fi
fi

# Install QEMU for RISC-V
echo -e "\n${YELLOW}[3/5] Installing QEMU for RISC-V emulation...${NC}"
if command_exists qemu-system-riscv64; then
    echo -e "${GREEN}✓ QEMU RISC-V already installed${NC}"
else
    echo -e "${BLUE}Installing QEMU...${NC}"
    sudo apt install -y qemu-system-misc
fi

# Install build essentials and make
echo -e "\n${YELLOW}[4/5] Installing build tools...${NC}"
sudo apt install -y \
    build-essential \
    make \
    git

echo -e "${GREEN}✓ Build tools installed${NC}"

# Install Sphinx for documentation
echo -e "\n${YELLOW}[5/5] Installing Sphinx for documentation...${NC}"
if command_exists sphinx-build; then
    echo -e "${GREEN}✓ Sphinx already installed${NC}"
else
    echo -e "${BLUE}Installing Sphinx and dependencies...${NC}"
    sudo apt install -y python3-pip
    pip3 install --user sphinx sphinx-rtd-theme
fi

# Install LaTeX for PDF generation
echo -e "\n${YELLOW}[BONUS] Installing LaTeX for PDF generation...${NC}"
if command_exists xelatex; then
    echo -e "${GREEN}✓ LaTeX already installed${NC}"
else
    echo -e "${BLUE}Installing TeX Live and required packages...${NC}"
    sudo apt install -y \
        texlive-xetex \
        texlive-latex-extra \
        texlive-fonts-recommended \
        texlive-fonts-extra \
        latexmk
    echo -e "${GREEN}✓ LaTeX installed${NC}"
fi

# Verify installations
echo -e "\n${BLUE}=====================================${NC}"
echo -e "${BLUE}Verifying installations...${NC}"
echo -e "${BLUE}=====================================${NC}\n"

error_count=0

# Check RISC-V toolchain
if command_exists riscv64-unknown-elf-gcc; then
    version=$(riscv64-unknown-elf-gcc --version | head -n1)
    echo -e "${GREEN}✓ RISC-V GCC:${NC} $version"
else
    echo -e "${RED}✗ RISC-V GCC not found${NC}"
    ((error_count++))
fi

# Check QEMU
if command_exists qemu-system-riscv64; then
    version=$(qemu-system-riscv64 --version | head -n1)
    echo -e "${GREEN}✓ QEMU RISC-V:${NC} $version"
else
    echo -e "${RED}✗ QEMU RISC-V not found${NC}"
    ((error_count++))
fi

# Check make
if command_exists make; then
    version=$(make --version | head -n1)
    echo -e "${GREEN}✓ Make:${NC} $version"
else
    echo -e "${RED}✗ Make not found${NC}"
    ((error_count++))
fi

# Check Sphinx
if command_exists sphinx-build; then
    version=$(sphinx-build --version 2>&1)
    echo -e "${GREEN}✓ Sphinx:${NC} $version"
else
    echo -e "${RED}✗ Sphinx not found${NC}"
    echo -e "${YELLOW}  Try: pip3 install --user sphinx sphinx-rtd-theme${NC}"
    echo -e "${YELLOW}  Then add ~/.local/bin to your PATH${NC}"
    ((error_count++))
fi

# Check LaTeX (optional for PDF generation)
if command_exists xelatex; then
    version=$(xelatex --version | head -n1)
    echo -e "${GREEN}✓ XeLaTeX:${NC} $version"
else
    echo -e "${YELLOW}⚠ XeLaTeX not found (optional - needed for PDF generation)${NC}"
    echo -e "${YELLOW}  Install with: sudo apt install texlive-xetex texlive-latex-extra${NC}"
fi

# Summary
echo -e "\n${BLUE}=====================================${NC}"
if [ $error_count -eq 0 ]; then
    echo -e "${GREEN}✓ All dependencies installed successfully!${NC}"
    echo -e "\n${BLUE}You can now build ThunderOS:${NC}"
    echo -e "  ${CYAN}make all${NC}         - Build the kernel"
    echo -e "  ${CYAN}make qemu${NC}        - Run in QEMU emulator"
    echo -e "  ${CYAN}cd docs && make html${NC} - Build HTML documentation"
    echo -e "  ${CYAN}cd docs && make pdf${NC}  - Build PDF documentation (requires LaTeX)"
else
    echo -e "${RED}✗ Installation completed with $error_count error(s)${NC}"
    echo -e "${YELLOW}Please review the errors above and install missing dependencies manually.${NC}"
fi
echo -e "${BLUE}=====================================${NC}\n"

# Add PATH suggestion for Sphinx if needed
if ! command_exists sphinx-build && [ -f "$HOME/.local/bin/sphinx-build" ]; then
    echo -e "${YELLOW}Note: Sphinx was installed to ~/.local/bin${NC}"
    echo -e "${YELLOW}Add this to your ~/.bashrc:${NC}"
    echo -e "${CYAN}  export PATH=\"\$HOME/.local/bin:\$PATH\"${NC}\n"
fi
