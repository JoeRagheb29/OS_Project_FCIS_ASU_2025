# FOS Project 2025 - Professional README

## Project Overview

**FOS (Flexible Operating System)** is a comprehensive 32-bit x86 educational operating system kernel project designed to teach OS concepts including memory management, process scheduling, file systems, and virtual memory.

---

## Table of Contents

1. Project Structure
2. Prerequisites
3. Building the Project
4. Project Components
5. Key Features
6. Configuration
7. Testing
8. Development Guidelines

---

## Project Structure

```
FOS_PROJECT_2025_TEMPLATE/
├── boot/                      # Bootloader code
│   ├── boot.S                # Assembly boot code
│   ├── main.c                # Boot initialization
│   └── Makefrag              # Build rules
├── kern/                      # Kernel implementation
│   ├── cmd/                  # Command interpreter
│   ├── cpu/                  # CPU scheduling
│   ├── disk/                 # Disk and page file management
│   ├── mem/                  # Memory management (heap, paging)
│   ├── proc/                 # Process/environment management
│   ├── trap/                 # Interrupt and exception handling
│   ├── tests/                # Kernel test suite
│   ├── init.c                # Kernel initialization
│   └── COPYRIGHT             # License information
├── user/                      # User-level programs and libraries
├── lib/                       # Library implementations
├── inc/                       # Header files
│   ├── string.h              # String utilities
│   ├── stdio.h               # I/O functions
│   ├── lib.h                 # System calls
│   ├── mmu.h                 # Memory management unit definitions
│   ├── syscall.h             # System call definitions
│   └── elf.h                 # ELF binary format
├── conf/                      # Configuration files
│   ├── env.mk                # Environment variables
│   └── lab.mk                # Lab configuration
├── .bochsrc                   # Bochs emulator configuration
├── .bochsrc-debug             # Debug configuration
├── GNUmakefile               # Main build system
└── README.md                 # This file
```

---

## Prerequisites

### Required Tools

- **GCC Cross-Compiler**: `i386-elf-gcc` (32-bit x86 ELF target)
- **GNU Make**: Build system
- **Bochs**: x86 emulator for testing
- **GDB**: Debugging (optional)
- **Perl**: Build scripts

### System Requirements

- Linux/Unix environment (or Windows with Cygwin/WSL)
- 2GB+ RAM
- 500MB+ disk space

### Installation

```bash
# Ubuntu/Debian
sudo apt-get install build-essential gcc-multilib gdb bochs bochs-sdl

# macOS (with Homebrew)
brew install i386-elf-gcc bochs
```

---

## Building the Project

### Standard Build

```bash
make clean
make -j8 all
```

### Build Configuration

Edit `conf/lab.mk` to set the lab number and configuration:

```makefile
LAB=3
PACKAGEDATE=Thu Sep 28 16:09:45 EDT 2006
```

### Build Options

- **Clean Build**: `make clean && make -j8 all`
- **Verbose Build**: `V= make all` (see `GNUmakefile`)
- **Debug Build**: Build with `-DTEST=` flags enabled

---

## Project Components

### 1. **Boot Subsystem** (`boot/`)

Handles CPU initialization and transitions from real mode to protected mode.

- `boot.S`: Assembly bootloader
- `main.c`: Boot configuration and memory setup
- Signs kernel binary for Bochs compatibility

### 2. **Kernel Core** (`kern/`)

#### Memory Management (`kern/mem/`)

- **kheap**: Kernel heap allocator with placement strategies
- **Chunk Operations**: Page allocation/deallocation
- **Virtual Memory**: Paging and page tables

#### Process Management (`kern/proc/`)

- **User Environments**: Process abstraction
- **Priority Scheduling**: Process priority management
- **Context Switching**: Environment state management

#### Scheduling (`kern/cpu/`)

- FIFO, Round-Robin, Priority-based scheduling
- Context switching and dispatcher

#### Disk & Virtual Memory (`kern/disk/`)

- **Page File Manager**: Secondary storage management
- Demand paging support

#### Fault Handling (`kern/trap/`)

- Exception and interrupt handlers
- Page fault processing
- System call dispatcher

#### Command Interface (`kern/cmd/`)

- Interactive command prompt
- 50+ kernel commands
- Test harness integration

### 3. **User Space** (`user/`)

User-level programs including:

- `malloc`/`free` implementations
- Shared memory access
- Protected memory tests
- Synchronization primitives

### 4. **Standard Library** (`lib/`, `inc/`)

Core utilities:

- String operations (`inc/string.h`)
- I/O functions (`inc/stdio.h`)
- System calls (`inc/lib.h`)
- Type definitions (`inc/types.h`)

---

## Key Features

### Memory Management

- **Paging**: 4KB page-based virtual memory
- **Page Tables**: Two-level page table hierarchy
- **Heap Allocation**: Multiple placement strategies (First-Fit, Best-Fit, Custom-Fit)
- **Page File**: Disk-based virtual memory extension
- **Working Set**: LRU-based page replacement

### Process Management

- Environment-based process model
- Parent-child process relationships
- Dynamic process creation and termination
- Inter-process communication

### Scheduling

- Multiple scheduling algorithms
- Priority-based scheduling
- Round-robin time slicing
- Preemptive context switching

### Virtual Memory

- Demand paging with lazy allocation
- Copy-on-Write support
- Shared memory regions
- Protected memory isolation

### Testing & Debugging

- 50+ integrated test cases
- Command-line test interface
- Bochs debugger integration
- Comprehensive test suite in `kern/tests/`

---

## Configuration

### Build Configuration (`conf/env.mk`)

```makefile
# Compiler prefix for cross-compilation
GCCPREFIX = i386-elf-

# Email for submissions
HANDIN_EMAIL = your-email@example.com
```

### Emulator Configuration (`.bochsrc`)

```
# CPU and memory settings
cpu: count=1, ips=200000000
memory: guest=512, host=256

# Disk configuration
ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14
```

### Debug Configuration (`.bochsrc-debug`)

Enables GDB debugging and detailed Bochs logging.

---

## Testing

### Run All Tests

```bash
make clean && make -j8 all
bochs -f .bochsrc
# Type 'help' in kernel prompt for available tests
```

### Key Test Commands

```bash
# Kernel heap tests
tst kheap init
tst kheap alloc
tst kheap free

# Dynamic allocator tests
tst dynalloc init
tst dynalloc alloc

# User environment tests
run tst_placement
run tst_invalid_access

# Scheduler tests
schedPRIRR 10 40 1000
tst priorityRR 0
```

### Test Files

Located in `kern/tests/`:

- Kernel heap tests
- Priority scheduling tests
- User program tests

---

## Development Guidelines

### Code Standards

See `coding` file for detailed conventions:

- **Naming**: `function_name()`, `MACRO_NAME`, `variable_name`
- **Indentation**: Tabs (not spaces)
- **Comments**: C++ style (`//`) preferred
- **Pointers**: Space in type declarations: `(uint16_t *)`
- **Functions**: Single function per line definition

### Adding New Commands

Edit `kern/cmd/commands.c`:

```c
int command_my_function(int argc, char **argv)
{
    // Implementation
    return 0;
}

// In commands array:
{"myCmd", "Description", command_my_function, arg_count},
```

### Adding System Calls

1. Add to `inc/syscall.h`:

   ```c
   enum {
       // ... existing calls
       SYS_my_syscall,  // Add here
   };
   ```

2. Implement in `kern/syscall.c`
3. Declare in `inc/lib.h`

### Memory Allocation

Use kernel heap allocation:

```c
#include <kern/mem/kheap.h>

void *ptr = kmalloc(size);
kfree(ptr);
```

---

## Troubleshooting

### Common Issues

| Issue                      | Solution                                         |
| -------------------------- | ------------------------------------------------ |
| `i386-elf-gcc: not found`  | Install cross-compiler toolchain                 |
| `Bochs: symbol not found`  | Rebuild with `make clean && make -j8 all`        |
| Page fault errors          | Check `kern/trap/fault_handler.c` implementation |
| Memory allocation failures | Verify kernel heap size in `kern/mem/kheap.c`    |

### Debugging

```bash
# Use GDB with Bochs
bochs -f .bochsrc-debug
# In Bochs: `continue` then connect GDB
gdb obj/kern/kernel
(gdb) target remote localhost:1234
```

---

## References

- **ELF Format**: See `inc/elf.h`
- **MMU Details**: See `inc/mmu.h`
- **Syscalls**: See `inc/syscall.h`
- **Copyright**: See `inc/COPYRIGHT`

---

## License

See `inc/COPYRIGHT` for detailed license information.

---

## Contributors

- **Text Coloring Feature**: Abd-Alrahman Zedan (Frozen-Bytes Team, FCIS'24-25)
- **Modern GCC Port**: See `Readme_Project_Port_To_ModernGCC.txt`

---

## Getting Help

1. Check existing tests in `kern/tests/`
2. Review kernel commands: Type `help` in kernel prompt
3. Consult header files in `inc/` for API documentation
4. Run test suite with `tst` command

---

**Last Updated**: December 2025  
**Project Version**: LAB 3  
**Target Architecture**: 32-bit x86 (i386)
