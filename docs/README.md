# ThunderOS Documentation

This directory contains comprehensive documentation for ThunderOS, a RISC-V operating system specialized for AI workloads.

## Documentation Structure

The documentation is built using [Sphinx](https://www.sphinx-doc.org/) and organized into the following sections:

### Main Sections

- **Introduction** (`source/introduction.rst`) - Project overview, goals, and getting started
- **Architecture** (`source/architecture.rst`) - High-level system architecture and component overview
- **Internals** (`source/internals/`) - Detailed implementation documentation
- **RISC-V Reference** (`source/riscv/`) - RISC-V architecture reference guide
- **Development** (`source/development.rst`) - Development workflow, tools, and practices
- **API Reference** (`source/api.rst`) - Function and data structure reference

### Internals Documentation

The `internals/` directory contains detailed technical documentation for each implemented component:

1. **Bootloader** (`bootloader.rst`) - Assembly bootloader implementation
   - Entry point and initialization
   - Stack setup
   - BSS clearing
   - Control transfer to kernel

2. **UART Driver** (`uart_driver.rst`) - Serial console I/O
   - NS16550A hardware interface
   - Character and string output
   - Input handling
   - Polling mode operation

3. **Trap Handler** (`trap_handler.rst`) - Exception and interrupt handling
   - Trap vector assembly code
   - Context save/restore (trap frame)
   - Exception handling
   - Interrupt routing
   - CSR register management

4. **Timer/CLINT** (`timer_clint.rst`) - Timer interrupts
   - SBI-based timer programming
   - CLINT (Core Local Interruptor) interface
   - Timer interrupt handling
   - Tick counting and timekeeping
   - rdtime instruction usage

5. **Testing Framework** (`testing_framework.rst`) - Automated kernel testing
   - KUnit-inspired testing API
   - Assertion macros
   - Test organization
   - Running tests in QEMU
   - Test output format

- **Linker Script** - Memory layout definition
- **Memory Layout** - Address space organization
- **Registers** - RISC-V register reference

### RISC-V Reference Guide

The `riscv/` directory contains practical reference material for RISC-V architecture:

- **Instruction Set** - Complete instruction reference with examples
- **Privilege Levels** - U/S/M modes and transitions
- **CSR Registers** - Control and Status Registers
- **Memory Model** - Weak ordering and virtual memory
- **Interrupts/Exceptions** - Trap handling mechanism
- **Assembly Guide** - Calling conventions and inline assembly

## Building Documentation

### Prerequisites

Install Sphinx and the RTD theme:

```bash
pip install sphinx sphinx_rtd_theme
```

### Build HTML Documentation

```bash
cd docs
make html
```

The generated HTML will be in `build/html/`. Open `build/html/index.html` in a browser.

### Other Formats

```bash
make latexpdf  # PDF via LaTeX (requires LaTeX installation)
make epub      # EPUB format
make text      # Plain text
```

### Clean Build

```bash
make clean
```

## Documentation Coverage

### Implemented Components (✓)

The following components are fully documented with comprehensive internals guides:

- ✓ Bootloader (boot.S)
- ✓ UART Driver (uart.c)
- ✓ Trap Handler (trap_entry.S, trap.c)
- ✓ Timer/CLINT Driver (clint.c)
- ✓ Testing Framework (kunit.h, kunit.c)
- ✓ Linker Script (kernel.ld)
- ✓ Memory Layout
- ✓ RISC-V Registers

### Pending Documentation (TODO)

Future components that will be documented as they are implemented:

- Memory Management (physical allocator, paging)
- Process Management (scheduler, context switching)
- System Calls (ecall interface)
- PLIC Driver (external interrupts)
- AI Accelerator Support

## Documentation Style

The documentation follows these conventions:

- **ReStructuredText** format (`.rst` files)
- **Code examples** with syntax highlighting
- **Diagrams** using ASCII art
- **Tables** for structured information
- **Cross-references** between sections
- **Clear section hierarchy** (title, sections, subsections)

## Key Features

1. **Comprehensive Coverage**: Every major component has detailed documentation
2. **Code Examples**: Real code snippets from the implementation
3. **Visual Aids**: ASCII diagrams showing flow and architecture
4. **Cross-Referenced**: Links between related components
5. **Searchable**: Full-text search in HTML output
6. **Version Controlled**: Documentation evolves with code

## Reading the Documentation

### For New Users

Start with:
1. Introduction - Understand project goals
2. Architecture - See the big picture
3. Development - Set up development environment

### For Kernel Developers

Focus on:
1. Internals documentation for each component
2. API reference for function signatures
3. Testing framework for writing tests

### For RISC-V Learners

Study:
1. **RISC-V Reference** - Start here for architecture basics
2. Instruction Set - Complete instruction reference
3. Privilege Levels - Understanding U/S/M modes
4. Trap Handler - Exception/interrupt mechanism in practice
5. Assembly Guide - Practical assembly programming

## Contributing to Documentation

When implementing new features:

1. Create a new `.rst` file in `source/internals/`
2. Follow the existing documentation structure
3. Include:
   - Overview and purpose
   - Architecture/flow diagrams
   - Code implementation details
   - Usage examples
   - Testing information
   - Future enhancements
4. Add to `source/internals/index.rst`
5. Update `source/architecture.rst` status
6. Build and verify: `make html`

## Documentation Standards

### Section Structure

Each component documentation should include:

```
Component Name
==============

Overview
--------
Brief description

Architecture
------------
Components, flow diagrams

Implementation
--------------
Detailed code walkthrough

API/Interface
-------------
Functions and usage

Testing
-------
Test coverage and examples

Debugging
---------
Troubleshooting tips

Future Enhancements
-------------------
Planned improvements

References
----------
Specifications, links

See Also
--------
Related components
```

### Code Formatting

- Use `.. code-block:: c` for C code
- Use `.. code-block:: asm` for assembly
- Use `.. code-block:: bash` for shell commands
- Use `.. code-block:: text` for output/diagrams

### Linking

- Cross-reference with `:doc:` directive
- Use backticks for code: \`variable_name\`
- Use `**bold**` for emphasis
- Use `*italic*` for terms

## Viewing Documentation Locally

### Start HTTP Server

```bash
cd docs/build/html
python3 -m http.server 8000
```

Then open http://localhost:8000 in your browser.

### Or Open Directly

```bash
xdg-open docs/build/html/index.html  # Linux
open docs/build/html/index.html      # macOS
```

## Documentation Metrics

Current documentation size:
- **20 RST files** (14 internals + 6 RISC-V reference)
- **~12,000+ lines** of documentation
- **~80+ code examples**
- **~50+ diagrams/tables**
- **100% coverage** of implemented components
- **Complete RISC-V reference** for OS development

## License

Documentation is licensed under the same terms as ThunderOS (see LICENSE file).

## Contact

For documentation issues or suggestions, please open an issue in the GitHub repository.
