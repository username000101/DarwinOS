/*
 * Copyright (c) 2026 antonov. All rights reserved.
 *
 * Minimal C++ runtime support for dyld
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" void* malloc(size_t);
extern "C" void  free(void*);

// Fortified libc stubs
extern "C" {
void *__memcpy_chk(void *dst, const void *src, size_t len, size_t dstlen) {
    (void)dstlen;
    return memcpy(dst, src, len);
}
void *__memmove_chk(void *dst, const void *src, size_t len, size_t dstlen) {
    (void)dstlen;
    return memmove(dst, src, len);
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
}

// Memory allocation operators
void* operator new(size_t size) { return malloc(size); }
void* operator new[](size_t size) { return malloc(size); }
void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete(void* p, size_t) noexcept { free(p); }
void operator delete[](void* p, size_t) noexcept { free(p); }

// Exception handling
static void* s_current_exception = nullptr;

extern "C" {
void __cxa_pure_virtual() {
    while (1) {}
}

void* __cxa_allocate_exception(size_t thrown_size) noexcept {
    return malloc(thrown_size);
}

void __cxa_free_exception(void* thrown_exception) noexcept {
    free(thrown_exception);
}

extern "C" void _simple_dprintf(int fd, const char *fmt, ...);

void __cxa_throw(void* thrown_exception, void* tinfo, void (*dest)(void*)) {
    (void)tinfo;
    (void)dest;
    s_current_exception = thrown_exception;
    const char *msg = thrown_exception ? *(const char**)thrown_exception : "(null)";
    _simple_dprintf(2, "FATAL: __cxa_throw: %s (raw=%p)\n", msg, thrown_exception);
    abort();
}

void __cxa_rethrow() {
    while (1) {}
}

void* __cxa_begin_catch(void* exception_object) noexcept {
    return exception_object;
}

void __cxa_end_catch() {
    s_current_exception = nullptr;
}

int __gxx_personality_v0(int version, int actions, uint64_t exception_class,
                         void* exception_object, void* context) {
    (void)version; (void)actions; (void)exception_class;
    (void)exception_object; (void)context;
    return 8; // _URC_CONTINUE_UNWIND
}

// ABI Destructors and Constructors
void _ZNSt9type_infoD1Ev() {}
void _ZNSt9type_infoD2Ev() {}

void _ZNSt9exceptionD1Ev() {}
void _ZNSt9exceptionD2Ev() {}

void _ZNSt20bad_array_new_lengthC1Ev() {}
void _ZNSt20bad_array_new_lengthC2Ev() {}
void _ZNSt20bad_array_new_lengthD1Ev() {}
void _ZNSt20bad_array_new_lengthD2Ev() {}

void _ZNSt11logic_errorC1EPKc(void* this_ptr, const char* msg) { (void)this_ptr; (void)msg; }
void _ZNSt11logic_errorC2EPKc(void* this_ptr, const char* msg) { (void)this_ptr; (void)msg; }
void _ZNSt11logic_errorD1Ev() {}
void _ZNSt11logic_errorD2Ev() {}

void _ZNSt12out_of_rangeC1EPKc(void* this_ptr, const char* msg) { (void)this_ptr; (void)msg; }
void _ZNSt12out_of_rangeC2EPKc(void* this_ptr, const char* msg) { (void)this_ptr; (void)msg; }
void _ZNSt12out_of_rangeD1Ev() {}
void _ZNSt12out_of_rangeD2Ev() {}

void _ZNSt12length_errorC1EPKc(void* this_ptr, const char* msg) { (void)this_ptr; (void)msg; }
void _ZNSt12length_errorC2EPKc(void* this_ptr, const char* msg) { (void)this_ptr; (void)msg; }
void _ZNSt12length_errorD1Ev() {}
void _ZNSt12length_errorD2Ev() {}

// Export typeinfos and vtables
__attribute__((used)) const void* _ZTIPKc[4] = { nullptr };
__attribute__((used)) const void* _ZTIPc[4] = { nullptr };
__attribute__((used)) const void* _ZTISt9exception[4] = { nullptr };
__attribute__((used)) const void* _ZTISt11logic_error[4] = { nullptr };
__attribute__((used)) const void* _ZTISt12out_of_range[4] = { nullptr };
__attribute__((used)) const void* _ZTISt12length_error[4] = { nullptr };
__attribute__((used)) const void* _ZTISt20bad_array_new_length[4] = { nullptr };
__attribute__((used)) const void* _ZTISt9type_info[4] = { nullptr };
__attribute__((used)) const void* _ZTIN10__cxxabiv117__class_type_infoE[4] = { nullptr };
__attribute__((used)) const void* _ZTIN10__cxxabiv120__si_class_type_infoE[4] = { nullptr };

__attribute__((used)) const void* _ZTVSt12out_of_range[4] = { nullptr };
__attribute__((used)) const void* _ZTVSt12length_error[4] = { nullptr };
__attribute__((used)) const void* _ZTVN10__cxxabiv117__class_type_infoE[4] = { nullptr };
__attribute__((used)) const void* _ZTVN10__cxxabiv120__si_class_type_infoE[4] = { nullptr };
}
