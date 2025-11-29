# ThunderOS Userland Programs

User-space programs compiled as RISC-V ELF64 executables.

## Directory Structure

```
userland/
├── bin/          # Shell and main executables
│   └── ush.c     # User shell
├── core/         # Core file/directory utilities
│   ├── cat.c     # Display file contents
│   ├── clear.c   # Clear screen
│   ├── ls.c      # List directory
│   ├── mkdir.c   # Create directory
│   ├── pwd.c     # Print working directory
│   ├── rm.c      # Remove file
│   ├── rmdir.c   # Remove directory
│   ├── sleep.c   # Sleep for seconds
│   └── touch.c   # Create empty file
├── system/       # System utilities
│   ├── kill.c    # Send signal to process
│   ├── ps.c      # List processes
│   ├── tty.c     # Print terminal name
│   ├── uname.c   # System information
│   ├── uptime.c  # System uptime
│   └── whoami.c  # Print current user
├── tests/        # Test programs
│   ├── hello.c   # Hello world
│   ├── clock.c   # Elapsed time display
│   ├── exec_test.c
│   ├── exit_test.c
│   ├── fork_test.c
│   ├── fork_simple_test.c
│   ├── pipe_test.c
│   ├── pipe_simple_test.c
│   ├── signal_test.c
│   ├── syscall_test.c
│   └── minimal_test.S
├── lib/          # Shared code
│   ├── syscall.S # System call wrappers
│   └── user.ld   # Linker script
└── build/        # Compiled binaries (generated)
```

## Building

```bash
./build_userland.sh
```

Or via the main Makefile:

```bash
make userland
```

## Adding New Programs

1. Create source file in the appropriate directory
2. Add build command to `build_userland.sh`
3. Add to filesystem in `Makefile` (fs target)
