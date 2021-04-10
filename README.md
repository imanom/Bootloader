# Operating Systems & Virtualization Assignments
These assignments are a part of the ECE 6504 Advanced Topics in Operating Systems and Virtualization course at Virginia Tech.

## Developed by
- Monami Dutta Gupta (monami@vt.edu)

## Requirements
- Ubuntu 20.04 LTS Desktop

## Tools
- `sudo apt-get install virtualbox`
- `sudo apt-get install build-essential`
- `sudo apt-get install clang lld` to compile UEFI code.
- `cd /usr/bin` and `sudo ln -s lld lld-link` to create a symbolic link after lld installation.
- `git clone  https://github.com/rusnikola/fwimage`
- `sudo apt-get install mkisofs` to create a bootable ISO image
- The TianoCore/Edk2 headers are also needed.

## Assignment 1

### 1.1 Basic Boot Loader
We have to implement a simple boot loader (BOOTX64.EFI) which loads an OS kernel. 
We use a 32-bit 800*600 video framebuffer to indicate that the OS kernel is loaded.
Once the kernel takes over, we don't return to the boot loader. A simple busy-wait loop (in kernel.c) takes care of that.

### 1.2 Reclaiming Memory
The boot loader is extended to call ExitBootServices() and reclaims boot-time memory. 

### 1.3 Setting up a page table
The code is extended so that the kernel redefines and uses its own page table. This page table provides an identical physical-to-virtual mappings, all pages are assumed to be used for the privileged kernel mode (Ring 0).

## Assignment 2
This assignment builds up on Assignment 1. We need to load a user application (source file - user.c) and extend the boot loader to additionally read this user application into a dedicated page-aligned buffer. We also remove the figure drawn in the framebuffer in the previous assignment and include the framebuffer console to extend console output support to the kernel using a printf-like function.

### Part 1
### 2.1 Basics
- We load the user application into a memory buffer directly from the boot loader. 
- We create a page table for the application so that we can also execute application code from user space. This page table should point to the kernel portion (i.e., the first entry of Level 4’s page table can simply refer to the existing kernel’s Level 3). 
- The kernel portion should only be accessible in
kernel space, whereas the application portion is accessible in both user and kernel space. 
- We assign any virtual addresses from the bottom 1GB for the application. The kernel-space portion can only be accessed in kernel/privileged mode.
- Finally, we jump to the user application code by switching to unprivileged mode.

### 2.2 System Calls
- Extends the application and kernel to support system calls, that is, making user-to-kernel and kernel-to-user transitions.
- Prints a message in the framebuffer console and makes use of the SYSCALL/SYSRET mechanism.

### Part 2
### 2.3 Task State Segment (TSS)
We need to define a TSS segment for the CPU.Each CPU needs a separate TSS segment. Since we are using just one CPU, therefore we will need one TSS segment.

### 2.4 Interrupt Descriptor Table and Exceptions
- We define and load a 256-entry IDT table to handle all 256 interrupts and exceptions. The IDT table is 16-byte aligned. 
- The first 32 entries correspond to CPU exceptions and must be initialized to the default CPU exception handler which prints a message that an exception as occured and halts the system.

### 2.5 Lazy Allocation
- We create a customized handler for page-fault exceptions and call any address in the bottom 1 GB that was not initially mapped.
- The exception handler calls a function that will modify the user page table and reload %cr3.
- The previous address that we were trying to access should now point to an existing page.
- When the exception handler completes, we make another system call to confirm that the program is still alive.

### Part 3
### 2.6 APIC Timer
We extend the code to implement a APIC-based periodic timer. As part of the initialization, we need to enable APIC timer and specify an interrupt vector associated with the APIC timer handle. The corresponding interrupt handler simply prints a message and acknowledges the interrupt to APIC.

### Thread Local Storage
We have to extend the kernel and user program such that they will support TLS for the user thread. We use GCC-specific constructs such as __thread to declare TLS variables.

## Assignment 3

### Launching an HVM Guest
- `sudo apt-get install xtightvncviewer` to see the video output.
- `sudo xl create /etc/xen/code-hvm.cfg` to start the HVM guest.
- `sudo vncviewer localhost:0` to launch the VNV viewer.

### 3.1 Xen Initialization
We extend the kernel to detect Xen hypervisor, initialize hypercalls and map the shared info data structure. We also execute a hypercall which will obtain the Xen version and print the major and minor version on the screen.

### 3.2 PV Clock
We implement a PV clock in the kernel - both monotonic and wall clocks. We print the initial values and then run a busy-wait loop for 1 second using the monotonic time function and print the final values.

### 3.3 Shared Memory
Created two versions of guest domains and a separate config file `/etc/code-other.cfg` which will refer to the other HVM domain. 
- One side initializes the gnttab_table variable by pointing to a buffer. We also initialize another available page (to be shared with the other side) with a null-terminated message.
- The other side uses a hypercall to map the shared page.
- code-hvm writes a message, code-other reads the message and prints it.
- To launch VNV viewer for the first domain: `sudo vncviewer localhost:0`.
For the second domain: `sudo vncviewer localhost:1`.

### 3.4 Adding a Hypercall
Modified the Xen 4.14.1 source code and added a new hypercall that prints a simple message and works for both x86's PV and HVM domains.
The patch is generated by running `diff -urN xen-4.14.1-original xen-4.14.1-modified > xen_hypercall.patch`.
