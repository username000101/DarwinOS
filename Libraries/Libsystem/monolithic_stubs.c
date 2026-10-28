/*
 * Copyright (c) 2026 antonov. All rights reserved.
 *
 * Monolithic libSystem support stubs for darwinOS
 */

#undef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 0

#include <sys/types.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <xlocale.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <locale.h>
#include <nl_types.h>
#include <signal.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <wctype.h>
#include <runetype.h>
#include <_ctype.h>

struct hostent;
long strtol(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);
double pow(double x, double y);
double fmod(double x, double y);

#define ARENA_CHUNK (2 * 1024 * 1024)
#define NUM_BUCKETS 18

typedef struct Header {
    size_t size;
    int is_free;
    int pad;
    struct Header *next;
} __attribute__((aligned(16))) Header;

static Header *buckets[NUM_BUCKETS] = {0};
static void *arena_curr = NULL;
static void *arena_end = NULL;
static volatile int malloc_lock = 0;

static inline void _malloc_lock(void) {
    while (__sync_lock_test_and_set(&malloc_lock, 1)) {
    }
}

static inline void _malloc_unlock(void) {
    __sync_lock_release(&malloc_lock);
}

static inline int get_bucket_index(size_t size) {
    if (size <= 16) return 0;
    if (size > 2097152) return -1;
    int bits = 64 - __builtin_clzll((unsigned long long)(size - 1));
    return bits - 4;
}

void *malloc(size_t size) {
    if (size == 0) size = 16;
    int idx = get_bucket_index(size);
    if (idx < 0) {
        size_t total = sizeof(Header) + ((size + 15) & ~15);
        void *p = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (p == MAP_FAILED) return NULL;
        Header *h = (Header *)p;
        h->size = size;
        h->is_free = 0;
        h->next = NULL;
        return (void *)(h + 1);
    }

    _malloc_lock();
    if (buckets[idx]) {
        Header *h = buckets[idx];
        buckets[idx] = h->next;
        h->is_free = 0;
        h->next = NULL;
        _malloc_unlock();
        return (void *)(h + 1);
    }

    size_t alloc_size = (size_t)1 << (idx + 4);
    size_t total = sizeof(Header) + alloc_size;
    if (!arena_curr || ((char *)arena_curr + total > (char *)arena_end)) {
        size_t chunk = ARENA_CHUNK;
        if (total > chunk) chunk = total;
        void *p = mmap(NULL, chunk, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (p == MAP_FAILED) {
            _malloc_unlock();
            return NULL;
        }
        arena_curr = p;
        arena_end = (char *)p + chunk;
    }

    Header *h = (Header *)arena_curr;
    h->size = alloc_size;
    h->is_free = 0;
    h->next = NULL;
    arena_curr = (char *)arena_curr + total;
    _malloc_unlock();
    return (void *)(h + 1);
}

void free(void *ptr) {
    if (!ptr) return;
    Header *h = ((Header *)ptr) - 1;
    if (h->is_free) return;
    h->is_free = 1;
    int idx = get_bucket_index(h->size);
    if (idx < 0) {
        munmap(h, sizeof(Header) + ((h->size + 15) & ~15));
        return;
    }
    _malloc_lock();
    h->next = buckets[idx];
    buckets[idx] = h;
    _malloc_unlock();
}

void *calloc(size_t count, size_t size) {
    size_t total = count * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }
    Header *h = ((Header *)ptr) - 1;
    if (h->size >= size) return ptr;
    void *new_p = malloc(size);
    if (new_p) {
        memcpy(new_p, ptr, h->size);
        free(ptr);
    }
    return new_p;
}

void abort(void) {
    _exit(134);
}

void _Exit(int status) {
    _exit(status);
}

// Fortified libc stubs
void *__memcpy_chk(void *dst, const void *src, size_t len, size_t dstlen) {
    (void)dstlen;
    return memcpy(dst, src, len);
}
void *__memmove_chk(void *dst, const void *src, size_t len, size_t dstlen) {
    (void)dstlen;
    return memmove(dst, src, len);
}
void *__memset_chk(void *dst, int c, size_t len, size_t dstlen) {
    (void)dstlen;
    return memset(dst, c, len);
}
char *__strcpy_chk(char *dst, const char *src, size_t dstlen) {
    (void)dstlen;
    return strcpy(dst, src);
}
size_t __strlcpy_chk(char *dst, const char *src, size_t maxlen, size_t dstlen) {
    (void)dstlen;
    return strlcpy(dst, src, maxlen);
}
size_t __strlcat_chk(char *dst, const char *src, size_t maxlen, size_t dstlen) {
    (void)dstlen;
    return strlcat(dst, src, maxlen);
}
char *strncat(char *dest, const char *src, size_t n) {
    char *p = dest;
    while (*p) p++;
    while (n-- && *src) *p++ = *src++;
    *p = '\0';
    return dest;
}

static void _putc(char **str, size_t *size, char c) {
    if (*size > 1) {
        **str = c;
        (*str)++;
        (*size)--;
    }
}

static void _puts(char **str, size_t *size, const char *s) {
    if (!s) s = "(null)";
    while (*s) {
        _putc(str, size, *s++);
    }
}

static void _putu(char **str, size_t *size, unsigned long long val, int base, int width, char pad) {
    char buf[32];
    int i = 0;
    const char digits[] = "0123456789abcdef";
    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val > 0) {
            buf[i++] = digits[val % base];
            val /= base;
        }
    }
    while (i < width) {
        _putc(str, size, pad);
        width--;
    }
    while (i > 0) {
        _putc(str, size, buf[--i]);
    }
}

static void _putd(char **str, size_t *size, long long val, int width, char pad) {
    if (val < 0) {
        _putc(str, size, '-');
        val = -val;
        if (width > 0) width--;
    }
    _putu(str, size, (unsigned long long)val, 10, width, pad);
}

#undef vsnprintf
int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    if (!str || size == 0) return 0;
    char *orig = str;
    const char *p = format;
    while (*p) {
        if (*p != '%') {
            _putc(&str, &size, *p++);
            continue;
        }
        p++;
        char pad = ' ';
        if (*p == '0') {
            pad = '0';
            p++;
        }
        int width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }
        int is_long = 0;
        while (*p == 'l' || *p == 'z') {
            is_long++;
            p++;
        }
        if (*p == 's') {
            _puts(&str, &size, va_arg(ap, const char *));
        } else if (*p == 'c') {
            _putc(&str, &size, (char)va_arg(ap, int));
        } else if (*p == 'd' || *p == 'i') {
            long long v = (is_long >= 2) ? va_arg(ap, long long) : (is_long == 1 ? va_arg(ap, long) : va_arg(ap, int));
            _putd(&str, &size, v, width, pad);
        } else if (*p == 'u') {
            unsigned long long v = (is_long >= 2) ? va_arg(ap, unsigned long long) : (is_long == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int));
            _putu(&str, &size, v, 10, width, pad);
        } else if (*p == 'x' || *p == 'X') {
            unsigned long long v = (is_long >= 2) ? va_arg(ap, unsigned long long) : (is_long == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int));
            _putu(&str, &size, v, 16, width, pad);
        } else if (*p == 'p') {
            _puts(&str, &size, "0x");
            _putu(&str, &size, (unsigned long long)(uintptr_t)va_arg(ap, void *), 16, 0, ' ');
        } else if (*p == '%') {
            _putc(&str, &size, '%');
        } else {
            _putc(&str, &size, *p);
        }
        if (*p) p++;
    }
    *str = '\0';
    return (int)(str - orig);
}

#undef vsprintf
int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, 0x7fffffff, format, ap);
}

char *__strcat_chk(char *dst, const char *src, size_t dstlen) {
    (void)dstlen;
    return strcat(dst, src);
}
char *__strncat_chk(char *dst, const char *src, size_t len, size_t dstlen) {
    (void)dstlen;
    return strncat(dst, src, len);
}
int __sprintf_chk(char *str, int flag, size_t strlen, const char *format, ...) {
    (void)flag; (void)strlen;
    va_list ap;
    va_start(ap, format);
    int ret = vsprintf(str, format, ap);
    va_end(ap);
    return ret;
}

int __snprintf_chk(char *str, size_t maxlen, int flag, size_t strlen, const char *format, ...) {
    (void)flag; (void)strlen;
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, maxlen, format, ap);
    va_end(ap);
    return ret;
}

int __vsnprintf_chk(char *str, size_t maxlen, int flag, size_t strlen, const char *format, va_list ap) {
    (void)flag; (void)strlen;
    return vsnprintf(str, maxlen, format, ap);
}

int __vsprintf_chk(char *str, int flag, size_t strlen, const char *format, va_list ap) {
    (void)flag; (void)strlen;
    return vsprintf(str, format, ap);
}

// Stdio stubs
static FILE _dummy_stderr = {0};
static FILE _dummy_stdin  = {0};
FILE *__stderrp = &_dummy_stderr;
FILE *__stdinp  = &_dummy_stdin;
extern FILE *__stdoutp;

int fileno(FILE *f) {
    if (f && f == __stdoutp) return 1;
    if (f && f == __stdinp) return 0;
    return 2;
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    char buf[2048];
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    if (n > 0) {
        int fd = fileno(stream);
        write(fd, buf, (size_t)n);
    }
    return n;
}

int fprintf(FILE *f, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int fputs(const char *s, FILE *stream) {
    if (!s) return 0;
    int fd = fileno(stream);
    size_t len = strlen(s);
    return (int)write(fd, s, len);
}

int puts(const char *s) {
    if (!s) return 0;
    fputs(s, __stdoutp);
    fputs("\n", __stdoutp);
    return 0;
}

int fputc(int c, FILE *stream) {
    char ch = (char)c;
    int fd = fileno(stream);
    write(fd, &ch, 1);
    return (unsigned char)c;
}
int putc(int c, FILE *stream) { return fputc(c, stream); }
int putchar(int c) { return fputc(c, __stdoutp); }

int printf(const char *format, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    if (n > 0) write(2, buf, n);
    return n;
}

#undef sprintf
int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsprintf(str, format, ap);
    va_end(ap);
    return ret;
}

#undef snprintf
int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int asprintf(char **ret, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    if (n < 0) return -1;
    *ret = (char *)malloc(n + 1);
    if (!*ret) return -1;
    memcpy(*ret, buf, n + 1);
    return n;
}

int vasprintf(char **ret, const char *format, va_list ap) {
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    if (n < 0) return -1;
    *ret = (char *)malloc(n + 1);
    if (!*ret) return -1;
    memcpy(*ret, buf, n + 1);
    return n;
}



void __assert_rtn(const char *func, const char *file, int line, const char *expr) {
    printf("ASSERTION FAILED: %s (%s:%s:%d)\n", expr, file, func, line);
    abort();
}

// Rune Locale support
_RuneLocale _DefaultRuneLocale = {
    _RUNE_MAGIC_A,
    "NONE",
    NULL,
    NULL,
    0xFFFD,
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, _CTYPE_S|_CTYPE_B, _CTYPE_S, _CTYPE_S, _CTYPE_S, _CTYPE_S, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        _CTYPE_S|_CTYPE_B|_CTYPE_R,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|0, _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|1,
        _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|2, _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|3,
        _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|4, _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|5,
        _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|6, _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|7,
        _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|8, _CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|9,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|10, _CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|11,
        _CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|12, _CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|13,
        _CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|14, _CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|15,
        _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G,
        _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G,
        _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G,
        _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G,
        _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G,
        _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G,
        _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G, _CTYPE_U|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|10, _CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|11,
        _CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|12, _CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|13,
        _CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|14, _CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|15,
        _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G,
        _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G,
        _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G,
        _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G,
        _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G,
        _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G,
        _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G, _CTYPE_L|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G, _CTYPE_P|_CTYPE_R|_CTYPE_G,
        _CTYPE_P|_CTYPE_R|_CTYPE_G, 0
    }
};
_RuneLocale *_CurrentRuneLocale = &_DefaultRuneLocale;

// Blocks Runtime stubs
void *_NSConcreteStackBlock[32] = {0};
void *_NSConcreteGlobalBlock[32] = {0};
void *_Block_copy(const void *aBlock) { return (void *)aBlock; }
void _Block_release(const void *aBlock) { (void)aBlock; }
int _Block_has_signature(void *aBlock) { (void)aBlock; return 0; }
int _Block_use_stret(void *aBlock) { (void)aBlock; return 0; }
void _Block_object_assign(void *dest, const void *object, const int flags) { (void)dest; (void)object; (void)flags; }
void _Block_object_dispose(const void *object, const int flags) { (void)object; (void)flags; }

// Stack protector
uint64_t __stack_chk_guard = 0x595e9fbd94fda766;
void __stack_chk_fail(void) { abort(); }

// Dynamic linking stubs (dyld / dlopen / dlsym)
void dyld_stub_binder(void) {}
void *dlopen(const char *path, int mode) { (void)path; (void)mode; return NULL; }
void *dlsym(void *handle, const char *symbol) { (void)handle; (void)symbol; return NULL; }
const char *dlerror(void) { return NULL; }
int dladdr(const void *addr, void *info) { (void)addr; (void)info; return 0; }

int _dyld_func_lookup(const char* name, void** address);

uint32_t dyld_get_program_sdk_version(void) { return 0x000a0f00; }
bool dyld_shared_cache_some_image_overridden(void) { return false; }
const void* dyld_image_header_containing_address(const void* addr) {
    static const void* (*p)(const void*) = NULL;
    if (p == NULL) {
        _dyld_func_lookup("__dyld_get_image_header_containing_address", (void**)&p);
    }
    if (p) return p(addr);
    return NULL;
}

void _dyld_objc_notify_register(void *mapped, void *init, void *unmapped) {
    write(2, "[monolithic_stubs] _dyld_objc_notify_register called\n", 53);
    static void (*p)(void *, void *, void *) = NULL;
    if (p == NULL) {
        int r = _dyld_func_lookup("__dyld_objc_notify_register", (void **)&p);
        if (r == 0 || p == NULL) {
            write(2, "[monolithic_stubs] _dyld_func_lookup for __dyld_objc_notify_register FAILED\n", 76);
        }
    }
    if (p) {
        write(2, "[monolithic_stubs] forwarding to dyld registerObjCNotifiers\n", 60);
        p(mapped, init, unmapped);
    }
}
int _dyld_is_memory_immutable(const void *addr, size_t length) {
    (void)addr; (void)length;
    return 0;
}
const char *_dyld_get_objc_selector(const char *selName) {
    (void)selName;
    return NULL;
}
const void *_dyld_get_shared_cache_range(size_t *length) {
    if (length) *length = 0;
    return NULL;
}
int _dyld_shared_cache_some_image_overridden(void) { return 0; }
uint32_t _dyld_get_program_sdk_version(void) { return 0x000a0f00; }
const void *_dyld_image_header_containing_address(const void *addr) { (void)addr; return NULL; }
const char *_dyld_image_path_containing_address(const void *addr) { (void)addr; return NULL; }
void _dyld_for_each_objc_class(void *handler) { (void)handler; }
void _dyld_for_each_objc_protocol(void *handler) { (void)handler; }

#include <pwd.h>
#include <grp.h>

static struct passwd _root_passwd = {
    .pw_name = "root",
    .pw_passwd = "*",
    .pw_uid = 0,
    .pw_gid = 0,
    .pw_change = 0,
    .pw_class = "",
    .pw_gecos = "System Administrator",
    .pw_dir = "/var/root",
    .pw_shell = "/bin/sh",
    .pw_expire = 0,
};

struct passwd *getpwuid(uid_t uid) {
    (void)uid;
    return &_root_passwd;
}

struct passwd *getpwnam(const char *name) {
    (void)name;
    return &_root_passwd;
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t bufsize, struct passwd **result) {
    (void)uid; (void)buffer; (void)bufsize;
    if (!pwd || !result) return -1;
    *pwd = _root_passwd;
    *result = pwd;
    return 0;
}

int getpwnam_r(const char *name, struct passwd *pwd, char *buffer, size_t bufsize, struct passwd **result) {
    (void)name; (void)buffer; (void)bufsize;
    if (!pwd || !result) return -1;
    *pwd = _root_passwd;
    *result = pwd;
    return 0;
}

struct passwd *getpwent(void) { return &_root_passwd; }
void setpwent(void) {}
void endpwent(void) {}

struct passwd *getpwuuid(uuid_t uuid) { (void)uuid; return &_root_passwd; }
int getpwuuid_r(uuid_t uuid, struct passwd *pwd, char *buffer, size_t bufsize, struct passwd **result) {
    (void)uuid; (void)buffer; (void)bufsize;
    if (!pwd || !result) return -1;
    *pwd = _root_passwd;
    *result = pwd;
    return 0;
}

static char *_root_members[] = { "root", NULL };
static struct group _wheel_group = {
    .gr_name = "wheel",
    .gr_passwd = "*",
    .gr_gid = 0,
    .gr_mem = _root_members,
};

struct group *getgrgid(gid_t gid) { (void)gid; return &_wheel_group; }
struct group *getgrnam(const char *name) { (void)name; return &_wheel_group; }
int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen, struct group **result) {
    (void)gid; (void)buf; (void)buflen;
    if (!grp || !result) return -1;
    *grp = _wheel_group;
    *result = grp;
    return 0;
}
int getgrnam_r(const char *name, struct group *grp, char *buf, size_t buflen, struct group **result) {
    (void)name; (void)buf; (void)buflen;
    if (!grp || !result) return -1;
    *grp = _wheel_group;
    *result = grp;
    return 0;
}
struct group *getgrent(void) { return &_wheel_group; }
void setgrent(void) {}
void endgrent(void) {}

// Environment and program variables
char *__progname = "WindowServer";
#define MAX_ENV_VARS 256
static char *_env_storage[MAX_ENV_VARS] = {
    "PATH=/bin:/usr/bin:/sbin:/usr/sbin",
    "HOME=/",
    "USER=root",
    NULL
};
static char **_env_p = _env_storage;
static int _dummy_argc = 1;
static char *_dummy_argv[] = { "WindowServer", NULL };
static char **_argv_p = _dummy_argv;

int *_NSGetArgc(void) { return &_dummy_argc; }
char ***_NSGetArgv(void) {
    static char ***argv_pp = &_argv_p;
    return argv_pp;
}
char ***_NSGetEnviron(void) {
    static char ***env_pp = &_env_p;
    return env_pp;
}
int _NSGetExecutablePath(char *buf, uint32_t *bufsize) {
    const char *p = "/System/Library/CoreServices/WindowServer.app/Contents/darwinOS/WindowServer";
    size_t len = strlen(p);
    if (*bufsize <= len) { *bufsize = len + 1; return -1; }
    strcpy(buf, p);
    return 0;
}
char *getenv(const char *name) {
    if (!name || !*name) return NULL;
    size_t nlen = strlen(name);
    for (char **ep = _env_p; *ep != NULL; ++ep) {
        if (strncmp(*ep, name, nlen) == 0 && (*ep)[nlen] == '=') {
            return *ep + nlen + 1;
        }
    }
    return NULL;
}
int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !*name || strchr(name, '=') != NULL) return -1;
    if (!value) value = "";
    size_t nlen = strlen(name);
    for (char **ep = _env_p; *ep != NULL; ++ep) {
        if (strncmp(*ep, name, nlen) == 0 && (*ep)[nlen] == '=') {
            if (!overwrite) return 0;
            size_t vlen = strlen(value);
            char *entry = (char *)malloc(nlen + 1 + vlen + 1);
            if (!entry) return -1;
            memcpy(entry, name, nlen);
            entry[nlen] = '=';
            memcpy(entry + nlen + 1, value, vlen + 1);
            *ep = entry;
            return 0;
        }
    }
    int count = 0;
    while (_env_p[count] != NULL && count < MAX_ENV_VARS - 2) count++;
    if (count >= MAX_ENV_VARS - 2) return -1;
    size_t vlen = strlen(value);
    char *entry = (char *)malloc(nlen + 1 + vlen + 1);
    if (!entry) return -1;
    memcpy(entry, name, nlen);
    entry[nlen] = '=';
    memcpy(entry + nlen + 1, value, vlen + 1);
    _env_p[count] = entry;
    _env_p[count + 1] = NULL;
    return 0;
}
int unsetenv(const char *name) {
    if (!name || !*name || strchr(name, '=') != NULL) return -1;
    size_t nlen = strlen(name);
    for (char **ep = _env_p; *ep != NULL; ++ep) {
        if (strncmp(*ep, name, nlen) == 0 && (*ep)[nlen] == '=') {
            char **next = ep;
            while (*next != NULL) {
                *next = *(next + 1);
                next++;
            }
            return 0;
        }
    }
    return 0;
}
int putenv(char *string) {
    if (!string) return -1;
    char *eq = strchr(string, '=');
    if (!eq) return unsetenv(string);
    *eq = '\0';
    int ret = setenv(string, eq + 1, 1);
    *eq = '=';
    return ret;
}

typedef unsigned __int128 uint128_t;
uint128_t __udivti3(uint128_t n, uint128_t d) {
    if (d == 0) return 0;
    uint128_t q = 0, r = 0;
    for (int i = 127; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) {
            r -= d;
            q |= ((uint128_t)1 << i);
        }
    }
    return q;
}

int fpclassify(double d) { (void)d; return 0; }
int signbit(double d) { return (d < 0.0); }
const void *kCFStreamPropertySocketNativeHandle = "kCFStreamPropertySocketNativeHandle";

// Math and string helpers
int atoi(const char *str) { return (int)strtol(str, NULL, 10); }
double acos(double x) { (void)x; return 0.0; }
double atan2(double y, double x) { (void)y; (void)x; return 0.0; }
double atof(const char *nptr) { return strtod(nptr, NULL); }
double exp(double x) { (void)x; return 1.0; }
float fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }
double frexp(double value, int *exp) { if (exp) *exp = 0; return value; }
double log(double x) { (void)x; return 0.0; }
int rand(void) { static unsigned long next = 1; next = next * 1103515245 + 12345; return (int)((next / 65536) % 32768); }

FILE *fopen(const char *path, const char *mode) { (void)path; (void)mode; return NULL; }
int fclose(FILE *stream) { (void)stream; return 0; }
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) { (void)ptr; (void)size; (void)nmemb; (void)stream; return 0; }
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || size == 0 || nmemb == 0) return 0;
    int fd = fileno(stream);
    write(fd, ptr, size * nmemb);
    return nmemb;
}
int ferror(FILE *stream) { (void)stream; return 0; }
struct tm *gmtime(const time_t *timep) { static struct tm t; return localtime_r(timep, &t); }

double sin(double x) { return x; }
double cos(double x) { return 1.0 - (x * x) / 2.0; }
float powf(float base, float exp) { return (float)pow((double)base, (double)exp); }
double log10(double x) { (void)x; return 1.0; }
int __maskrune(wint_t c, unsigned long f) {
    (void)f;
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f');
}

int iswspace(wint_t wc);
char *setlocale(int category, const char *locale) { (void)category; (void)locale; return "C"; }
struct lconv *localeconv(void) {
    static struct lconv lc = {
        .decimal_point     = ".",
        .thousands_sep     = ",",
        .grouping          = "",
        .int_curr_symbol   = "USD",
        .currency_symbol   = "$",
        .mon_decimal_point = ".",
        .mon_thousands_sep = ",",
        .mon_grouping      = "",
        .positive_sign     = "",
        .negative_sign     = "-",
        .int_frac_digits   = 2,
        .frac_digits       = 2,
        .p_cs_precedes     = 1,
        .p_sep_by_space    = 0,
        .n_cs_precedes     = 1,
        .n_sep_by_space    = 0,
        .p_sign_posn       = 1,
        .n_sign_posn       = 1,
    };
    return &lc;
}
char *nl_langinfo(nl_item item) { (void)item; return ""; }
void tzset(void) {}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact) {
    (void)sig; (void)act; (void)oact; return 0;
}
void (*signal(int sig, void (*func)(int)))(int) { (void)sig; return func; }

pid_t wait3(int *status, int options, struct rusage *rusage) {
    (void)status; (void)options; (void)rusage; return -1;
}
pid_t waitpid(pid_t pid, int *status, int options) {
    (void)pid; (void)status; (void)options; return -1;
}
pid_t fork(void) { return -1; }
int remove(const char *path) { return unlink(path); }


int sscanf(const char *str, const char *format, ...) {
    (void)str; (void)format; return 0;
}

in_addr_t inet_addr(const char *cp) { (void)cp; return 0; }
char *inet_ntoa(struct in_addr in) { (void)in; return "127.0.0.1"; }
struct hostent *gethostbyname(const char *name) { (void)name; return NULL; }
struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type) {
    (void)addr; (void)len; (void)type; return NULL;
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    (void)destructor;
    static int next_key = 256;
    if (key) *key = next_key++;
    return 0;
}

long long llabs(long long j) { return (j < 0) ? -j : j; }
double modf(double x, double *iptr) {
    long long i = (long long)x;
    if (iptr) *iptr = (double)i;
    return x - (double)i;
}
double scalbn(double x, int n) {
    while (n > 0) { x *= 2.0; n--; }
    while (n < 0) { x /= 2.0; n++; }
    return x;
}
char *__strncpy_chk(char *dst, const char *src, size_t len, size_t dstlen) {
    (void)dstlen;
    return strncpy(dst, src, len);
}
int snprintf_l(char *str, size_t size, locale_t loc, const char *format, ...) {
    (void)loc;
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}
int fprintf_l(FILE *stream, locale_t loc, const char *format, ...) {
    (void)stream; (void)loc; (void)format;
    return 0;
}
int gethostname(char *name, size_t len) {
    const char *h = "darwinOS";
    if (!name || len == 0) return -1;
    strncpy(name, h, len);
    name[len - 1] = '\0';
    return 0;
}
struct tm *localtime_r(const time_t *timep, struct tm *result) {
    if (!result) return NULL;
    memset(result, 0, sizeof(*result));
    time_t t = timep ? *timep : 0;
    result->tm_sec = t % 60;
    result->tm_min = (t / 60) % 60;
    result->tm_hour = (t / 3600) % 24;
    result->tm_mday = 1;
    result->tm_year = 126;
    return result;
}
int mkstemp(char *template) {
    (void)template;
    return open("/tmp/tmpfile", O_RDWR | O_CREAT, 0600);
}
ssize_t send(int s, const void *msg, size_t len, int flags) {
    (void)flags;
    return write(s, msg, len);
}
ssize_t recv(int s, void *buf, size_t len, int flags) {
    (void)flags;
    return read(s, buf, len);
}
int dlclose(void *handle) { (void)handle; return 0; }
bool dlopen_preflight(const char *path) { (void)path; return true; }

size_t malloc_good_size(size_t size) { return size; }
void malloc_zone_free(void *zone, void *ptr) { (void)zone; free(ptr); }
void *malloc_zone_realloc(void *zone, void *ptr, size_t size) { (void)zone; return realloc(ptr, size); }

typedef intptr_t dispatch_once_t;
typedef void (^dispatch_block_t)(void);
typedef void *dispatch_source_t;
typedef void (*dispatch_function_t)(void *);

void dispatch_once(dispatch_once_t *predicate, dispatch_block_t block) {
    if (predicate && *predicate == 0) {
        *predicate = ~0l;
        if (block) block();
    }
}
void dispatch_once_f(dispatch_once_t *predicate, void *context, dispatch_function_t function) {
    if (predicate && *predicate == 0) {
        *predicate = ~0l;
        if (function) function(context);
    }
}
void dispatch_source_set_event_handler_f(dispatch_source_t source, dispatch_function_t handler) {
    (void)source; (void)handler;
}
void dispatch_source_set_cancel_handler_f(dispatch_source_t source, dispatch_function_t handler) {
    (void)source; (void)handler;
}
mach_port_t _dispatch_get_main_queue_port_4CF(void) { return (mach_port_t)0; }
void _dispatch_main_queue_callback_4CF(void *msg) { (void)msg; }
void *_dispatch_runloop_root_queue_create_4CF(const char *label, unsigned long flags) { (void)label; (void)flags; return NULL; }
mach_port_t _dispatch_runloop_root_queue_get_port_4CF(void *queue) { (void)queue; return (mach_port_t)0; }
void _dispatch_runloop_root_queue_perform_4CF(void *queue) { (void)queue; }
void _dispatch_source_set_runloop_timer_4CF(void *source, uint64_t fire, uint64_t interval, uint64_t leeway) {
    (void)source; (void)fire; (void)interval; (void)leeway;
}

void *asl_new(uint32_t type) { (void)type; return (void *)1; }
void *asl_open(const char *ident, const char *facility, uint32_t opts) { (void)ident; (void)facility; (void)opts; return (void *)1; }
void asl_close(void *asl) { (void)asl; }
void asl_free(void *asl) { (void)asl; }
int asl_send(void *asl, void *msg) { (void)asl; (void)msg; return 0; }
int asl_set(void *msg, const char *key, const char *val) { (void)msg; (void)key; (void)val; return 0; }

kern_return_t bootstrap_check_in(mach_port_t bp, const char *service_name, mach_port_t *sp) {
    (void)bp; (void)service_name; if (sp) *sp = 1; return 0;
}
kern_return_t bootstrap_look_up2(mach_port_t bp, const char *service_name, mach_port_t *sp, unsigned int flags) {
    (void)bp; (void)service_name; (void)flags; if (sp) *sp = 1; return 0;
}
kern_return_t bootstrap_register2(mach_port_t bp, const char *service_name, mach_port_t sp, unsigned int flags) {
    (void)bp; (void)service_name; (void)sp; (void)flags; return 0;
}
const char *bootstrap_strerror(kern_return_t r) { (void)r; return "success"; }

void _vproc_transaction_begin(void) {}
size_t _vproc_transaction_count(void) { return 0; }
void _vproc_transaction_end(void) {}
bool _vproc_transaction_try_exit(int status) { (void)status; return false; }
void _vproc_transactions_enable(void) {}

int sysdir_start_search_path_enumeration(int dir, int domainMask) { (void)dir; (void)domainMask; return 0; }
int sysdir_get_next_search_path_enumeration(int state, char *pathBuffer) {
    (void)state; (void)pathBuffer; return 0;
}
void *getsectbynamefromheader_64(const void *mhp, const char *segname, const char *sectname) {
    (void)mhp; (void)segname; (void)sectname; return NULL;
}
void *getsegbyname(const char *segname) { (void)segname; return NULL; }
const void *NXFindBestFatArch(void *arch, void *fat_archs, uint32_t nfat_archs) {
    (void)arch; (void)fat_archs; (void)nfat_archs; return NULL;
}
const void *NXGetLocalArchInfo(void) { return NULL; }

// Malloc zone stubs
size_t malloc_size(const void *ptr) {
    if (!ptr) return 0;
    Header *h = ((Header *)ptr) - 1;
    return h->size;
}
void *malloc_default_zone(void) { return (void *)1; }
void *malloc_zone_malloc(void *zone, size_t size) { (void)zone; return malloc(size); }
void *malloc_zone_calloc(void *zone, size_t count, size_t size) { (void)zone; return calloc(count, size); }
void *malloc_zone_memalign(void *zone, size_t align, size_t size) { (void)zone; (void)align; return malloc(size); }
void *malloc_zone_from_ptr(const void *ptr) { (void)ptr; return (void *)1; }
unsigned malloc_zone_batch_malloc(void *zone, size_t size, void **results, unsigned count) {
    (void)zone;
    for (unsigned i = 0; i < count; i++) {
        results[i] = malloc(size);
        if (!results[i]) return i;
    }
    return count;
}
int posix_memalign(void **memptr, size_t alignment, size_t size) {
    (void)alignment;
    void *p = malloc(size);
    if (!p) return 12;
    *memptr = p;
    return 0;
}

// Mach-O section and segment data lookup
uint8_t *getsectiondata(void *mhp, const char *segname, const char *sectname, unsigned long *size) {
    if (!mhp || !segname || !sectname) {
        if (size) *size = 0;
        return NULL;
    }
    const struct mach_header_64 *mh = (const struct mach_header_64 *)mhp;
    if (mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) {
        const struct mach_header *mh32 = (const struct mach_header *)mhp;
        if (mh32->magic == MH_MAGIC || mh32->magic == MH_CIGAM) {
            uintptr_t slide = 0;
            const uint8_t *cmds = (const uint8_t *)(mh32 + 1);
            const uint8_t *p = cmds;
            for (uint32_t i = 0; i < mh32->ncmds; i++) {
                const struct segment_command *seg = (const struct segment_command *)p;
                if (seg->cmd == LC_SEGMENT && seg->fileoff == 0 && seg->filesize != 0) {
                    slide = (uintptr_t)mhp - seg->vmaddr;
                    break;
                }
                p += seg->cmdsize;
            }
            p = cmds;
            for (uint32_t i = 0; i < mh32->ncmds; i++) {
                const struct segment_command *seg = (const struct segment_command *)p;
                if (seg->cmd == LC_SEGMENT && strncmp(seg->segname, segname, sizeof(seg->segname)) == 0) {
                    const struct section *sec = (const struct section *)(seg + 1);
                    for (uint32_t j = 0; j < seg->nsects; j++) {
                        if (strncmp(sec[j].sectname, sectname, sizeof(sec[j].sectname)) == 0) {
                            if (size) *size = sec[j].size;
                            return (uint8_t *)(sec[j].addr + slide);
                        }
                    }
                }
                p += seg->cmdsize;
            }
        }
        if (size) *size = 0;
        return NULL;
    }

    uintptr_t slide = 0;
    const uint8_t *cmds = (const uint8_t *)(mh + 1);
    const uint8_t *p = cmds;
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        const struct segment_command_64 *seg = (const struct segment_command_64 *)p;
        if (seg->cmd == LC_SEGMENT_64 && seg->fileoff == 0 && seg->filesize != 0) {
            slide = (uintptr_t)mhp - seg->vmaddr;
            break;
        }
        p += seg->cmdsize;
    }

    p = cmds;
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        const struct segment_command_64 *seg = (const struct segment_command_64 *)p;
        if (seg->cmd == LC_SEGMENT_64 && strncmp(seg->segname, segname, sizeof(seg->segname)) == 0) {
            const struct section_64 *sec = (const struct section_64 *)(seg + 1);
            for (uint32_t j = 0; j < seg->nsects; j++) {
                if (strncmp(sec[j].sectname, sectname, sizeof(sec[j].sectname)) == 0) {
                    if (size) *size = sec[j].size;
                    return (uint8_t *)(sec[j].addr + slide);
                }
            }
        }
        p += seg->cmdsize;
    }

    if (size) *size = 0;
    return NULL;
}

uint8_t *getsegmentdata(void *mhp, const char *segname, unsigned long *size) {
    if (!mhp || !segname) {
        if (size) *size = 0;
        return NULL;
    }
    const struct mach_header_64 *mh = (const struct mach_header_64 *)mhp;
    if (mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) {
        const struct mach_header *mh32 = (const struct mach_header *)mhp;
        if (mh32->magic == MH_MAGIC || mh32->magic == MH_CIGAM) {
            uintptr_t slide = 0;
            const uint8_t *cmds = (const uint8_t *)(mh32 + 1);
            const uint8_t *p = cmds;
            for (uint32_t i = 0; i < mh32->ncmds; i++) {
                const struct segment_command *seg = (const struct segment_command *)p;
                if (seg->cmd == LC_SEGMENT && seg->fileoff == 0 && seg->filesize != 0) {
                    slide = (uintptr_t)mhp - seg->vmaddr;
                    break;
                }
                p += seg->cmdsize;
            }
            p = cmds;
            for (uint32_t i = 0; i < mh32->ncmds; i++) {
                const struct segment_command *seg = (const struct segment_command *)p;
                if (seg->cmd == LC_SEGMENT && strncmp(seg->segname, segname, sizeof(seg->segname)) == 0) {
                    if (size) *size = seg->vmsize;
                    return (uint8_t *)(seg->vmaddr + slide);
                }
                p += seg->cmdsize;
            }
        }
        if (size) *size = 0;
        return NULL;
    }

    uintptr_t slide = 0;
    const uint8_t *cmds = (const uint8_t *)(mh + 1);
    const uint8_t *p = cmds;
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        const struct segment_command_64 *seg = (const struct segment_command_64 *)p;
        if (seg->cmd == LC_SEGMENT_64 && seg->fileoff == 0 && seg->filesize != 0) {
            slide = (uintptr_t)mhp - seg->vmaddr;
            break;
        }
        p += seg->cmdsize;
    }

    p = cmds;
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        const struct segment_command_64 *seg = (const struct segment_command_64 *)p;
        if (seg->cmd == LC_SEGMENT_64 && strncmp(seg->segname, segname, sizeof(seg->segname)) == 0) {
            if (size) *size = seg->vmsize;
            return (uint8_t *)(seg->vmaddr + slide);
        }
        p += seg->cmdsize;
    }

    if (size) *size = 0;
    return NULL;
}

// Unwind stubs
int unw_getcontext(void *uc) { (void)uc; return 0; }
int unw_init_local(void *c, void *uc) { (void)c; (void)uc; return 0; }
int unw_step(void *c) { (void)c; return 0; }
int unw_get_reg(void *c, int reg, void *val) { (void)c; (void)reg; (void)val; return 0; }
int unw_get_proc_info(void *c, void *pi) { (void)c; (void)pi; return 0; }
uintptr_t _Unwind_GetCFA(void *ctx) { (void)ctx; return 0; }
uintptr_t _Unwind_GetIP(void *ctx) { (void)ctx; return 0; }
void _Unwind_Resume(void *exc) { (void)exc; abort(); }

// C++ exceptions & typeinfo
void *__cxa_allocate_exception(size_t size) { return malloc(size); }
void __cxa_free_exception(void *p) { free(p); }
void __cxa_throw(void *obj, void *tinfo, void (*dest)(void *)) { (void)tinfo; (void)dest; abort(); }
void *__cxa_begin_catch(void *obj) { return obj; }
void __cxa_end_catch(void) {}
void __cxa_rethrow(void) { abort(); }
void __cxa_pure_virtual(void) { abort(); }
void __cxa_call_unexpected(void *p) { (void)p; abort(); }
void *__cxa_current_exception_type(void) { return NULL; }
int __gxx_personality_v0(int v, int a, uint64_t c, void *obj, void *ctx) { (void)v; (void)a; (void)c; (void)obj; (void)ctx; return 8; }
void _ZSt9terminatev(void) { abort(); }
void (*_ZSt13set_terminatePFvvE(void (*f)(void)))(void) { return f; }
const char _ZSt7nothrow[] = {0};
const void *_ZTISt9exception[2] = {0};
const void *_ZTISt9bad_alloc[2] = {0};
const void *_ZTISt20bad_array_new_length[2] = {0};
const void *_ZTVSt9exception[4] = {0};
const void *_ZTVN10__cxxabiv117__class_type_infoE[4] = {0};
const void *_ZTVN10__cxxabiv120__si_class_type_infoE[4] = {0};
void _ZNSt9exceptionD2Ev(void) {}
const char *_ZNKSt9exception4whatEv(void *this) { (void)this; return "exception"; }
void _ZNSt20bad_array_new_lengthC1Ev(void) {}
void _ZNSt20bad_array_new_lengthD1Ev(void) {}

// Miscellaneous helpers
const char *CRGetCrashLogMessage(void) __asm__("__Z20CRGetCrashLogMessagev");
const char *CRGetCrashLogMessage(void) { return ""; }
void CRSetCrashLogMessage(const char *msg) __asm__("__Z20CRSetCrashLogMessagePKc");
void CRSetCrashLogMessage(const char *msg) { (void)msg; }
const char *dispatch_queue_get_label(void *q) { (void)q; return "main"; }
uint64_t clock_gettime_nsec_np(clockid_t id) { (void)id; return 0; }
// Thread-specific data and pthread_self support
extern int _thread_set_tsd_base(void *tsd_base);
extern uintptr_t _pthread_ptr_munge_token;

static struct {
    long sig;
    void *cleanup_stack;
    char pad[256];
    uint64_t thread_id;
    void *tsd[768];
} _static_main_thread = { 0 };

static int _tsd_inited = 0;

static void _init_static_main_thread(void) {
    if (_tsd_inited) return;
    _tsd_inited = 1;
    _static_main_thread.sig = (long)((uintptr_t)&_static_main_thread ^ _pthread_ptr_munge_token);
    _static_main_thread.thread_id = 1;
    _static_main_thread.tsd[0] = &_static_main_thread; // TSD slot 0 = PTHREAD_SELF
    _thread_set_tsd_base(_static_main_thread.tsd);
}

void *pthread_self(void) {
    _init_static_main_thread();
    return (void*)&_static_main_thread;
}

void *pthread_getspecific(unsigned long key) {
    _init_static_main_thread();
    if (key < 768) {
        return _static_main_thread.tsd[key];
    }
    return NULL;
}

int pthread_setspecific(unsigned long key, const void *value) {
    _init_static_main_thread();
    if (key < 768) {
        _static_main_thread.tsd[key] = (void*)value;
        return 0;
    }
    return 0;
}

int __pthread_setspecific_static(unsigned long key, void *value) {
    _init_static_main_thread();
    if (key < 768) {
        _static_main_thread.tsd[key] = value;
        return 0;
    }
    return 0;
}

void _pthread_tsd_cleanup(void *thread) __asm__("__pthread_tsd_cleanup");
void _pthread_tsd_cleanup(void *thread) {
    (void)thread;
}

int pthread_key_init_np(int k, void (*d)(void *)) { (void)k; (void)d; return 0; }
int backtrace(void **b, int s) { (void)b; (void)s; return 0; }
char **backtrace_symbols(void *const *b, int s) { (void)b; (void)s; return NULL; }
void backtrace_symbols_fd(void *const *b, int s, int fd) { (void)b; (void)s; (void)fd; }

size_t strcspn(const char *s, const char *reject) {
    const char *p, *r;
    for (p = s; *p != '\0'; ++p) {
        for (r = reject; *r != '\0'; ++r) {
            if (*p == *r) return (p - s);
        }
    }
    return (p - s);
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    (void)base;
    unsigned long val = 0;
    while (*nptr >= '0' && *nptr <= '9') {
        val = val * 10 + (*nptr - '0');
        nptr++;
    }
    if (endptr) *endptr = (char *)nptr;
    return val;
}

// Stubs for Libc initialization
void __atexit_init(void) {}
void __chk_init(void) {}
void __confstr_init(void) {}
void __cxa_finalize(void *d) { (void)d; }
void __guard_setup(void) {}
void __xlocale_init(void) {}
void _libc_fork_init(void) {}
void _program_vars_init(void) {}
void _tlv_exit(void) {}

// Time / locale stubs
struct tm *localtime(const time_t *t) {
    (void)t;
    static struct tm dummy_tm;
    memset(&dummy_tm, 0, sizeof(dummy_tm));
    return &dummy_tm;
}

int strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

// Cache flush stubs
void sys_dcache_flush(void *p, size_t s) { (void)p; (void)s; }
void sys_icache_invalidate(void *p, size_t s) { (void)p; (void)s; }

// Workqueue / thread stubs
void start_wqthread(void) {}
void thread_start(void) {}

voucher_mach_msg_state_t voucher_mach_msg_adopt(mach_msg_header_t *m) { (void)m; return (voucher_mach_msg_state_t)0; }
void voucher_mach_msg_revert(voucher_mach_msg_state_t m) { (void)m; }

// Early initialization constructor
extern void mach_init(void);

__attribute__((constructor))
static void _libsystem_monolithic_init(void) {
    mach_init();
    _init_static_main_thread();
}

// WindowServer / input stubs
void *CGColorSpaceCreateDeviceRGB(void) { return (void *)1; }
void CGColorSpaceRelease(void *cs) { (void)cs; }
void *CGDisplayModeRetain(void *dm) { return dm; }
void CGDisplayModeRelease(void *dm) { (void)dm; }

kern_return_t bootstrap_register(mach_port_t bp, const char *service_name, mach_port_t sp) {
    (void)bp; (void)service_name; (void)sp; return 0;
}
char *crypt(const char *key, const char *salt) { (void)key; (void)salt; return ""; }
int execl(const char *path, const char *arg0, ...) { (void)path; (void)arg0; return -1; }
int execle(const char *path, const char *arg0, ...) { (void)path; (void)arg0; return -1; }
int getopt(int argc, char * const argv[], const char *optstring) { (void)argc; (void)argv; (void)optstring; return -1; }
int initgroups(const char *name, int basegid) { (void)name; (void)basegid; return 0; }
int setlogin(const char *name) { (void)name; return 0; }
void perror(const char *s) { (void)s; }
long random(void) { return (long)rand(); }
void srandomdev(void) {}
unsigned int sleep(unsigned int seconds) { (void)seconds; return 0; }
int system(const char *command) { (void)command; return 0; }
pid_t wait(int *status) { (void)status; return -1; }

void *kvm_open(const char *execfile, const char *corefile, const char *swapfile, int flags, const char *errstr) {
    (void)execfile; (void)corefile; (void)swapfile; (void)flags; (void)errstr; return (void *)1;
}
int kvm_close(void *kd) { (void)kd; return 0; }

// libinput & udev stubs
void *udev_new(void) { return (void *)1; }
void *udev_unref(void *udev) { (void)udev; return NULL; }
void *libinput_udev_create_context(const void *interface, void *user_data, void *udev) {
    (void)interface; (void)user_data; (void)udev; return (void *)1;
}
int libinput_udev_assign_seat(void *li, const char *seat_id) { (void)li; (void)seat_id; return 0; }
void libinput_log_set_priority(void *li, int priority) { (void)li; (void)priority; }
int libinput_get_fd(void *li) { (void)li; return -1; }
int libinput_dispatch(void *li) { (void)li; return 0; }
void *libinput_get_event(void *li) { (void)li; return NULL; }
void libinput_event_destroy(void *event) { (void)event; }
void *libinput_unref(void *li) { (void)li; return NULL; }

const char *libinput_device_get_name(void *device) { (void)device; return "PS/2 Controller"; }
void *libinput_event_get_device(void *event) { (void)event; return (void *)1; }
int libinput_event_get_type(void *event) { (void)event; return 0; }
void *libinput_event_get_keyboard_event(void *event) { (void)event; return (void *)1; }
void *libinput_event_get_pointer_event(void *event) { (void)event; return (void *)1; }
uint32_t libinput_event_keyboard_get_key(void *event) { (void)event; return 0; }
int libinput_event_keyboard_get_key_state(void *event) { (void)event; return 0; }
double libinput_event_pointer_get_absolute_x_transformed(void *event, uint32_t width) { (void)event; (void)width; return 0.0; }
double libinput_event_pointer_get_absolute_y_transformed(void *event, uint32_t height) { (void)event; (void)height; return 0.0; }
uint32_t libinput_event_pointer_get_button(void *event) { (void)event; return 0; }
int libinput_event_pointer_get_button_state(void *event) { (void)event; return 0; }
double libinput_event_pointer_get_dx_unaccelerated(void *event) { (void)event; return 0.0; }
double libinput_event_pointer_get_dy_unaccelerated(void *event) { (void)event; return 0.0; }

// xkbcommon stubs
void *xkb_context_new(int flags) { (void)flags; return (void *)1; }
void xkb_context_unref(void *context) { (void)context; }
void *xkb_keymap_new_from_names(void *context, const void *names, int flags) {
    (void)context; (void)names; (void)flags; return (void *)1;
}
void xkb_keymap_unref(void *keymap) { (void)keymap; }
void *xkb_state_new(void *keymap) { (void)keymap; return (void *)1; }
void xkb_state_unref(void *state) { (void)state; }
uint32_t xkb_state_key_get_one_sym(void *state, uint32_t key) { (void)state; (void)key; return 0; }
int xkb_state_key_get_utf8(void *state, uint32_t key, char *buf, size_t size) {
    (void)state; (void)key; (void)buf; (void)size; return 0;
}
int xkb_state_mod_name_is_active(void *state, const char *name, int type) {
    (void)state; (void)name; (void)type; return 0;
}
int xkb_state_update_key(void *state, uint32_t key, int direction) {
    (void)state; (void)key; (void)direction; return 0;
}

// Stub binder helper for lazy symbol binding
struct __DATA__dyld { 
	void*			dyldLazyBinder;
	int				(*lookup)(const char*, void**);
	const void*		mh;
	int*			NXArgcPtr;
	const char***	NXArgvPtr;
	const char***	environPtr;
	const char**	__prognamePtr;
};

__attribute__ ((section ("__DATA,__dyld"))) 
volatile struct __DATA__dyld myDyldSection = { 
    NULL, 
    NULL, 
    NULL, 
    &_dummy_argc, 
    (const char***)&_argv_p, 
    (const char***)&_env_p, 
    (const char**)&__progname 
};

int _dyld_func_lookup(const char* name, void** address)
{
	if (myDyldSection.lookup != NULL) {
		return (*myDyldSection.lookup)(name, address);
	}
	*address = NULL;
	return 0;
}

void* _dyld_fast_stub_entry(void* loadercache, long lazyinfo) __asm__("__Z21_dyld_fast_stub_entryPvl");
void* _dyld_fast_stub_entry(void* loadercache, long lazyinfo) {
    static void* (*p)(void*, long) = NULL;
    if (p == NULL) {
        if (!_dyld_func_lookup("__dyld_fast_stub_entry", (void**)&p) || p == NULL) {
            write(2, "FATAL: failed to lookup __dyld_fast_stub_entry!\n", 48);
            abort();
        }
    }
    return p(loadercache, lazyinfo);
}
