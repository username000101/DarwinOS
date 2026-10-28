/*
 * Copyright (c) 2026 antonov. All rights reserved.
 *
 * darwinOS system launchd / init (PID 1)
 */

typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int uint32_t;
typedef int int32_t;

#define SYS_exit        1
#define SYS_fork        2
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_wait4       7
#define SYS_unlink     10
#define SYS_chdir      12
#define SYS_mknod      14
#define SYS_chmod      15
#define SYS_chown      16
#define SYS_getpid     20
#define SYS_dup2       90
#define SYS_mkdir     136
#define SYS_mount     167
#define SYS_nanosleep 227
#define SYS_execve     59

#define O_RDONLY        0
#define O_WRONLY        1
#define O_RDWR          2

#define WNOHANG         1

struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

static inline int64_t sys_call4(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    int64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall\n\t"
        "jnc 1f\n\t"
        "neg %%rax\n\t"
        "1:"
        : "=a" (ret)
        : "a" (0x2000000 | num), "D" (arg1), "S" (arg2), "d" (arg3), "r" (r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t sys_call3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    int64_t ret;
    __asm__ volatile (
        "syscall\n\t"
        "jnc 1f\n\t"
        "neg %%rax\n\t"
        "1:"
        : "=a" (ret)
        : "a" (0x2000000 | num), "D" (arg1), "S" (arg2), "d" (arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t sys_call2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    int64_t ret;
    __asm__ volatile (
        "syscall\n\t"
        "jnc 1f\n\t"
        "neg %%rax\n\t"
        "1:"
        : "=a" (ret)
        : "a" (0x2000000 | num), "D" (arg1), "S" (arg2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t sys_call1(uint64_t num, uint64_t arg1) {
    int64_t ret;
    __asm__ volatile (
        "syscall\n\t"
        "jnc 1f\n\t"
        "neg %%rax\n\t"
        "1:"
        : "=a" (ret)
        : "a" (0x2000000 | num), "D" (arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t sys_call0(uint64_t num) {
    int64_t ret;
    __asm__ volatile (
        "syscall\n\t"
        "jnc 1f\n\t"
        "neg %%rax\n\t"
        "1:"
        : "=a" (ret)
        : "a" (0x2000000 | num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t sys_fork(void) {
    int64_t ret;
    __asm__ volatile (
        "movl $0x2000002, %%eax\n\t"
        "syscall\n\t"
        "jnc 1f\n\t"
        "movq $-1, %%rax\n\t"
        "jmp 2f\n\t"
        "1:\n\t"
        "testl %%edx, %%edx\n\t"
        "jz 2f\n\t"
        "xorq %%rax, %%rax\n\t"
        "2:\n\t"
        : "=a" (ret)
        :
        : "rcx", "rdx", "r11", "memory"
    );
    return ret;
}

uint64_t __stack_chk_guard = 0x595e9fbd94fda766;
void __stack_chk_fail(void) { sys_call1(SYS_exit, 127); }
void *memcpy(void *dst, const void *src, uint64_t n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

static int64_t console_fd = 1;

static void print(const char *str) {
    uint64_t len = 0;
    while (str[len]) len++;
    sys_call3(SYS_write, (uint64_t)console_fd, (uint64_t)str, len);
}

static void print_num(int64_t n) {
    char buf[32];
    int i = 0;
    if (n == 0) { print("0"); return; }
    if (n < 0) { print("-"); n = -n; }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    buf[i] = 0;
    print(buf);
}

void _start(void) {
    // Ensure /dev exists and devfs is mounted
    sys_call2(SYS_mkdir, (uint64_t)"/dev", 0755);
    sys_call4(SYS_mount, (uint64_t)"devfs", (uint64_t)"/dev", 0, 0);

    // Open /dev/console
    int64_t cfd = sys_call3(SYS_open, (uint64_t)"/dev/console", O_RDWR, 0);
    if (cfd < 0) {
        cfd = sys_call3(SYS_open, (uint64_t)"/dev/console", O_WRONLY, 0);
    }
    if (cfd >= 0) {
        console_fd = cfd;
        sys_call2(SYS_dup2, (uint64_t)cfd, 0);
        sys_call2(SYS_dup2, (uint64_t)cfd, 1);
        sys_call2(SYS_dup2, (uint64_t)cfd, 2);
    }

    print("\n");
    print("=======================================================\n");
    print("  darwinOS launchd (PID 1) started successfully!\n");
    print("  Kernel: XNU 19.6.0 (x86_64)\n");
    print("  Root filesystem: mounted (HFS+)\n");
    print("=======================================================\n");
    print("[launchd] PID 1 active, devfs mounted on /dev.\n");
    print("[launchd] launching WindowServer compositor...\n");

    int64_t pid = sys_fork();
    if (pid == 0) {
        const char *ws_path = "/System/Library/CoreServices/WindowServer.app/Contents/darwinOS/WindowServer";
        const char *const ws_argv[] = { "WindowServer", 0 };
        const char *const ws_envp[] = {
            "PATH=/bin:/usr/bin:/sbin:/usr/sbin",
            "HOME=/",
            "USER=root",
            "DYLD_PRINT_LIBRARIES=1",
            "DYLD_PRINT_WARNINGS=1",
            "DYLD_PRINT_APIS=1",
            "DYLD_INSERT_LIBRARIES=/System/Library/Frameworks/Onyx2D.framework/Versions/A/Onyx2D",
            0
        };

        print("[launchd] child starting: ");
        print(ws_path);
        print("\n");

        int64_t err = sys_call3(SYS_execve, (uint64_t)ws_path, (uint64_t)ws_argv, (uint64_t)ws_envp);
        print("[launchd] execve failed!\n");
        sys_call1(SYS_exit, 1);
    } else if (pid > 0) {
        print("[launchd] WindowServer process spawned (pid=");
        print_num(pid);
        print(").\n");
    } else {
        print("[launchd] ERROR: fork() failed!\n");
    }

    print("[launchd] entering supervisor loop.\n");

    while (1) {
        int32_t status = 0;
        int64_t p = sys_call4(SYS_wait4, (uint64_t)-1, (uint64_t)&status, 0, 0);
        if (p > 0) {
            print("[launchd] child process pid=");
            print_num(p);
            print(" terminated with status=");
            print_num((int64_t)status);
            print("\n");
        } else {
            print("[launchd] no more children, supervisor idle.\n");
            while (1) {
                __asm__ volatile ("pause");
            }
        }
    }
}
