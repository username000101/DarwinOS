# AGENTS.md — DarwinOS Developer & Agent Onboarding Guide

> **Welcome, Agent.** This document serves as the high-context, operational briefing for working on **DarwinOS**. It details the architecture, current working state, critical bugs solved, build procedures, and immediate next steps.

---

## 1. Executive Summary & Architecture

**DarwinOS** is an open-source, modern Mach-O / XNU operating system pairing the Apple XNU 19.6.0 kernel with a Cocoa userland (Foundation, CoreGraphics, Onyx2D, AppKit) and a native compositor (`WindowServer.app`).

### Key Architecture Components:
- **Bootloader**: GRUB2 EFI booting `kernel` (Mach-O 64-bit XNU) with boot arguments `debug=0x14e serial=3 console=3 -v keepsyms=1`.
- **Kernel (`Kernel/xnu-19.6.0`)**: Mach-O XNU kernel with 64-bit SysV ABI syscall table, IOKit driver manager, AHCI storage (`RavynAHCIPort.kext`), and framebuffer GOP syscalls (notably syscall 535: `SYS_fb_present`).
- **Storage & Root**: Initial boot uses memory disk / ramdisk overlays; AHCI driver manages SATA storage.
- **Dynamic Linker (`Libraries/dyld`)**: Pure Mach-O `dyld` loaded at `0x7fff5fc00000`, supporting LC_MAIN and LC_UNIXTHREAD binaries, chained fixup rebasing, initializer ordering, and ObjC runtime registration (`_dyld_objc_notify_register`).
- **C Runtime (`Libraries/Libsystem`)**: Monolithic `libSystem.B.dylib` with custom runtime stubs (`Libraries/Libsystem/monolithic_stubs.c`), an O(1) segregated free-list memory allocator with spinlocks, POSIX locale tables, thread-specific data (TSD), and Mach messaging traps.
- **Objective-C Runtime (`Libraries/objc4`)**: Apple `objc4-787.1` runtime supporting modern class realization, fast method caches, and toll-free bridging.
- **Base Frameworks (`Frameworks/`)**:
  - `CoreFoundation`: Apple CF-Lite implementation.
  - `Foundation`: Cocoa Foundation implementing `NSObject`, `NSString`, `NSDictionary`, `NSThread`, `NSBundle`, `NSUserDefaults`, etc.
  - `CoreGraphics` & `Onyx2D`: 2D rendering primitives, bezier paths, squircle window clipping.
- **PID 1 Supervisor (`SysApps/launchd/launchd.c`)**: Lightweight init process that mounts filesystems, sets up `/dev/console`, populates device nodes, and spawns system daemons and `WindowServer`.
- **Display Server (`CoreServices/WindowServer`)**: Cocoa-based compositor managing windows, surfaces, cursors, and rendering directly to the EFI framebuffer GOP surface via `SYS_fb_present`.

---

## 2. Checkpoint Status (What Was Done)

In this milestone, the execution pipeline from QEMU EFI boot through kernel initialization, launchd PID 1 spawn, dynamic linking, and Foundation object lifecycle has been brought into a working state:

### Major Fixes & Milestones Achieved:
1. **O(1) Segregated Allocator in `monolithic_stubs.c`**:
   - Replaced naive bump allocator with an 18-bucket segregated free-list allocator (classes 16B through 8192B) backed by mmap and protected by atomic spinlocks (`OSSpinLockLock`).
   - Prevents memory leaks, fragmentation, and infinite loops in heavy memory allocation paths (e.g. `malloc`/`free` cycles in CoreFoundation and Foundation).
2. **Objective-C Class Realization Null-Pointer Guard**:
   - In `Libraries/objc4/runtime/objc-runtime-new.mm`, guarded `ro->name && 0 == strcmp(ro->name, "OS_object")` to prevent page faults during early class realization when `ro->name` is null.
3. **POSIX Locale & String Formatting**:
   - Populated standard `struct lconv` in `monolithic_stubs.c` and added fallback guards in `NSLocale_posix.m` to eliminate null dereferences in `-[NSString initWithUTF8String:]`.
4. **Mutual Recursion Elimination in Foundation**:
   - Eliminated infinite recursion cycle between `NSLog`, `[NSBundle mainBundle]`, and `[NSFileManager defaultManager]`.
   - Fixed `-[NSCFString length]` in `Frameworks/Foundation/NSString/NSCFString.m` by removing invalid `& 0xff` truncation.
5. **SysV ABI Register Marshalling for `_NSobjc_msgSendv`**:
   - Rewrote `Frameworks/Foundation/NSObject/msgSendv-Darwin_x86_64.s` from scratch to properly marshal arguments according to the x86_64 SysV ABI (rdi, rsi, rdx, rcx, r8, r9, and aligned stack frames) with `_class_getMethodImplementation` lookup.
6. **Cocoa Forwarding & Selector Handling in `NSObject`**:
   - Fixed `-[NSObject methodSignatureForSelector:]` and `+[NSObject methodSignatureForSelector:]` to return `nil` instead of throwing `NSInvalidArgumentException`, adhering to Cocoa specifications and allowing method forwarding / fallback routines to operate correctly.
7. **Thread Singletons & `NSMutableDictionary_CF` Memory Lifecycle**:
   - Resolved critical object recycling bug where `NSProcessInfo` was deallocated immediately after allocation because `NSMutableDictionary_CF` was using CF callbacks that bypassed Objective-C retain count increments.
   - Updated `NSMutableDictionary_mapTable.m` callbacks to use `[(id)value retain]` and `[(id)value release]` with proper `isEqual:` and `hash` implementations.
   - Updated `_NSThreadSharedInstance` in `NSThread.m` to ensure thread-shared singletons remain retained in memory throughout process lifetime.

---

## 3. Directory Layout & Key Files

```
DarwinOS/
├── Kernel/
│   ├── xnu/                           # XNU 19.6.0 kernel sources
│   ├── Extensions/RavynAHCIPort/      # Native AHCI storage kext
│   └── Makefile                       # Kernel build driver
├── Libraries/
│   ├── Libsystem/
│   │   ├── monolithic_stubs.c         # Allocator, POSIX, Mach traps, syscalls
│   │   └── libsystem_c/               # libc sources & dyld libc
│   ├── objc4/
│   │   ├── runtime/                   # libobjc runtime (objc-runtime-new.mm, etc.)
│   │   └── build_objc.sh              # Cross-compilation script for libobjc
│   └── dyld/
│       ├── dyld/makefile              # Dyld link rules
│       └── src/dyld2.cpp              # Dyld entry, initializers, start glue
├── Frameworks/
│   ├── Foundation/
│   │   ├── build_framework.sh         # Fast rebuild script for Foundation.framework
│   │   ├── NSDictionary/              # NSMutableDictionary_CF and mapTable
│   │   ├── NSObject/                  # NSObject.m, msgSendv-Darwin_x86_64.s
│   │   ├── NSProcessInfo.m            # NSProcessInfo implementation
│   │   └── NSThread/                  # Thread-local storage & shared instances
│   ├── CoreFoundation/                # CF-Lite implementation
│   ├── CoreGraphics/                  # CoreGraphics display & window server primitives
│   └── Onyx2D/                        # 2D graphics rendering engine
├── SysApps/
│   ├── launchd/launchd.c              # Lightweight PID 1 supervisor
│   └── WindowServer/                  # Cocoa desktop display server & compositor
├── ISO/
│   ├── build_linux.sh                 # Full ISO generation script via xorriso
│   ├── rebuild_quick.sh               # Quick build script (launchd + ISO)
│   ├── build_dyld.sh                  # Dyld compilation script
│   └── build_monolithic_libsystem.sh  # libSystem rebuild script
└── dist/
    └── darwinOS.iso                   # Bootable ISO output image
```

---

## 4. Build Environment & Workflows

### Prerequisites
- Host: Windows with WSL2 (Arch Linux / Debian) or native Linux.
- Cross-toolchain: Clang / LLVM Darwin target (`x86_64-apple-darwin19`), `ld64`, `bmake`, `xorriso`, `mtools`.
- Sysroot & build directory: strictly located at `$HOME/build/darwinos` (never hardcode personal user paths).

### Common Build Commands (from WSL2 / Linux host):

#### 1. Rebuilding Foundation Framework
```bash
cd /path/to/DarwinOS
./Frameworks/Foundation/build_framework.sh
```

#### 2. Rebuilding monolithic libSystem
```bash
cd /path/to/DarwinOS
./ISO/build_monolithic_libsystem.sh
```

#### 3. Rebuilding Dyld
```bash
cd /path/to/DarwinOS
./ISO/build_dyld.sh
```

#### 4. Rebuilding launchd and packaging the ISO
```bash
cd /path/to/DarwinOS
./ISO/rebuild_quick.sh
```
> [!IMPORTANT]
> Always terminate any running QEMU process before running `rebuild_quick.sh` or `build_linux.sh`, as Windows/NTFS will lock `dist/darwinOS.iso` while QEMU has it open.

#### 5. Testing with QEMU
```powershell
qemu-system-x86_64 -m 4G -smp 4 `
  -bios "C:\Program Files\qemu\OVMF.fd" `
  -cdrom "dist/darwinOS.iso" `
  -serial file:qemu_trace.log `
  -vga std `
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 `
  -accel tcg
```

---

## 5. Next Steps for Incoming Agents

1. **WindowServer Multi-Process Execution**:
   - `WindowServer` forks a child if `getpid() > 1`.
   - Verify that both the parent wait-loop and child display initialization run past `[NSProcessInfo processInfo]` without any invalid selector or memory corruption exceptions.
2. **Framebuffer Surface Presentation (`SYS_fb_present`)**:
   - Verify that `BSDFramebuffer` detects the UEFI GOP framebuffer parameters (base address, width, height, stride).
   - Test initial background desktop color fill and cursor blitting to the framebuffer.
3. **Input Event Loop (`libinput` / `libevdev`)**:
   - Ensure the event source runs in `NSRunLoop` or dedicated input thread and processes mouse moves / keyboard events into `NSEvent`.
4. **Launching Desktop Applications**:
   - Once `WindowServer` is drawing stable frames, configure `launchd.c` to spawn `Filer.app` (Finder equivalent) and `Dock.app`.

---

## 6. Development Rules & Conventions
- **No Hardcoded Paths**: Never hardcode personal paths like `/home/<username>` or `C:\Users\<username>`. Use `$HOME`, `$ROOT_SOURCE_DIR`, or script-relative paths (`$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)`).
- **Preserve Headers**: Preserve existing `Copyright (c) 2026 antonov. All rights reserved.` headers and open-source license comments.
- **Atomic Commits**: Keep commit messages concise, descriptive, and focused on logical units (e.g. `kernel`, `dyld`, `libsystem`, `foundation`, `windowserver`).
