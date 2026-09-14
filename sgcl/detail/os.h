//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#elif defined(__x86_64__) && defined(__SSE4_2__)
#include <nmmintrin.h>
#endif

#include "../config.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#if defined(__linux__)
#include <fcntl.h>
#endif

// The collector reads other threads' stacks word by word while they run:
// racy by design, and the words may be in a frame's red zone. Functions
// doing that are excluded from the sanitizers.
#if defined(__clang__)
#define SGCL_NO_SANITIZE __attribute__((no_sanitize("address", "thread", "undefined")))
#define SGCL_NOINLINE __attribute__((noinline))
#define SGCL_ALWAYS_INLINE __attribute__((always_inline)) inline
#elif defined(__GNUC__)
#define SGCL_NO_SANITIZE __attribute__((no_sanitize_address, no_sanitize_thread, no_sanitize_undefined))
#define SGCL_NOINLINE __attribute__((noinline))
#define SGCL_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define SGCL_NO_SANITIZE
#define SGCL_NOINLINE __declspec(noinline)
#define SGCL_ALWAYS_INLINE __forceinline
#endif

// Hot paths of the containers: inlined into the caller in optimized
// builds; in debug builds a plain call, so that the locals of the fast
// path (a pointer to a buffer that a growth then replaces) do not linger
// in the caller's frame and retain the old buffer in the counting tests.
#ifdef NDEBUG
#define SGCL_INLINE_HOT SGCL_ALWAYS_INLINE
#else
#define SGCL_INLINE_HOT inline
#endif

// Thin platform layer for the managed heap: reserve a virtual range once,
// commit and decommit it in chunks. Everything else in the library is
// platform independent.
namespace sgcl::detail::os {
    // The child of a fork. The collector's thread and the other mutators
    // do not exist in it, and the mutexes are copies, held or not as they
    // were in the parent at the fork: the child may read the managed heap
    // (its pages a copy-on-write snapshot; the headers live outside the
    // pages, so the parent's marking does not dirty them) and exec or
    // exit, and that is what it does in practice, but a managed
    // allocation that needs a page or a call into the collector would
    // hang on a copied lock or wait for a thread that is not there. Set
    // by the atfork handler the collector registers when it starts
    // (collector.h); the slow paths that would hang check it and fail
    // with a message instead (fail_after_fork).
    inline std::atomic<bool> forked_child = {false};

    inline void register_fork_handler() noexcept {
#if !defined(_WIN32)
        ::pthread_atfork(nullptr, nullptr, [] { forked_child.store(true, std::memory_order_relaxed); });
#endif
    }

    [[noreturn]] inline void fail_after_fork(const char* what) noexcept {
        std::fprintf(stderr, "[sgcl] %s in the child of a fork: the collector does not run there; the child may read the managed heap and exec or exit, not allocate managed objects or collect\n", what);
        std::terminate();
    }

    struct Reservation {
        void* base = nullptr;
        size_t size = 0;
        // true: pages must be committed (and can be decommitted) explicitly;
        // false: the range is readable and writable as reserved, the kernel
        // backs pages lazily on first touch
        bool needs_commit = false;
    };

    // The stacks are scanned conservatively, so a word that merely looks
    // like an address inside the range retains whatever lives there. Five
    // printable ASCII bytes with zero padding (a short string such as the
    // program's name, kept by the runtime for the life of the process) read
    // as the addresses 0x2020202020..0x7E7E7E7E7E, and that is where the
    // default placement of large mappings lands on macOS and Linux. The
    // range is therefore asked for at 1 TB, above every such word (six
    // bytes start at 35 TB); the hint is only a hint, whatever the system
    // returns is used.
    inline constexpr uintptr_t ReserveHint = uintptr_t(1) << 40;

    // A writable range backed by the operating system lazily, zero until
    // touched: for tables indexed by address (heap.h: the cards), of which
    // a program touches a few pages. Null when the system refuses.
    inline void* map_lazy(size_t size) noexcept {
#if defined(_WIN32)
        return ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(MAP_NORESERVE)
        flags |= MAP_NORESERVE;
#endif
        auto p = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, -1, 0);
        return p == MAP_FAILED ? nullptr : p;
#endif
    }

    inline Reservation reserve(size_t size) noexcept {
#if defined(_WIN32)
        auto p = ::VirtualAlloc((void*)ReserveHint, size, MEM_RESERVE, PAGE_NOACCESS);
        if (!p) {
            p = ::VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
        }
        return {p, p ? size : 0, true};
#else
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(MAP_NORESERVE)
        flags |= MAP_NORESERVE;
#endif
        // Under Linux overcommit modes 0 and 1 a writable MAP_NORESERVE range
        // costs nothing; under the strict mode 2 it counts toward the commit
        // charge, so fall back to PROT_NONE and commit per chunk with mprotect.
        auto p = ::mmap((void*)ReserveHint, size, PROT_READ | PROT_WRITE, flags, -1, 0);
        if (p != MAP_FAILED) {
            return {p, size, false};
        }
        p = ::mmap((void*)ReserveHint, size, PROT_NONE, flags, -1, 0);
        if (p != MAP_FAILED) {
            return {p, size, true};
        }
        return {};
#endif
    }

    inline void release(const Reservation& r) noexcept {
        if (r.base) {
#if defined(_WIN32)
            ::VirtualFree(r.base, 0, MEM_RELEASE);
#else
            ::munmap(r.base, r.size);
#endif
        }
    }

    inline bool commit(void* p, size_t size) noexcept {
#if defined(_WIN32)
        return ::VirtualAlloc(p, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
#else
        return ::mprotect(p, size, PROT_READ | PROT_WRITE) == 0;
#endif
    }

    inline void decommit(void* p, size_t size, bool needs_commit) noexcept {
#if defined(_WIN32)
        (void)needs_commit;
        ::VirtualFree(p, size, MEM_DECOMMIT);
#else
        if (needs_commit) {
            // Drops the pages and the access rights in one step; commit()
            // re-enables the range.
            ::mmap(p, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED
#if defined(MAP_NORESERVE)
                | MAP_NORESERVE
#endif
                , -1, 0);
        } else {
            ::madvise(p, size, MADV_DONTNEED);
        }
#endif
    }

    inline void advise_huge_pages([[maybe_unused]] void* p, [[maybe_unused]] size_t size) noexcept {
#if defined(MADV_HUGEPAGE)
        ::madvise(p, size, MADV_HUGEPAGE);
#endif
    }

    // [lo, hi) of the calling thread's stack: the whole range the thread may
    // ever use, not just the part in use now.
    inline bool thread_stack(uintptr_t& lo, uintptr_t& hi) noexcept {
#if defined(_WIN32)
        ULONG_PTR l, h;
        ::GetCurrentThreadStackLimits(&l, &h);
        lo = l;
        hi = h;
        return true;
#elif defined(__APPLE__)
        auto self = ::pthread_self();
        auto top = (uintptr_t)::pthread_get_stackaddr_np(self);
        auto size = ::pthread_get_stacksize_np(self);
        lo = top - size;
        hi = top;
        return size != 0;
#else
        pthread_attr_t attr;
        if (::pthread_getattr_np(::pthread_self(), &attr)) {
            return false;
        }
        void* addr;
        size_t size;
        auto rc = ::pthread_attr_getstack(&attr, &addr, &size);
        ::pthread_attr_destroy(&attr);
        if (rc) {
            return false;
        }
        lo = (uintptr_t)addr;
        hi = lo + size;
        return true;
#endif
    }

    inline size_t page_size() noexcept {
#if defined(_WIN32)
        SYSTEM_INFO info;
        ::GetSystemInfo(&info);
        return info.dwPageSize;
#else
        return (size_t)::sysconf(_SC_PAGESIZE);
#endif
    }

#if defined(__linux__)
    // /proc/self/pagemap: bit 63 present, bit 62 swapped. Holes in the
    // address space (the part of the main stack not grown yet) read as 0.
    inline bool _touched_pages_pagemap(uintptr_t begin, size_t pages, size_t page, unsigned char* out) noexcept {
        int fd = ::open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return false;
        }
        uint64_t buf[512];
        size_t done = 0;
        bool ok = true;
        while (done < pages) {
            auto n = std::min(pages - done, std::size(buf));
            auto offset = (off_t)((begin / page + done) * sizeof(uint64_t));
            auto r = ::pread(fd, buf, n * sizeof(uint64_t), offset);
            if (r != (ssize_t)(n * sizeof(uint64_t))) {
                ok = false;
                break;
            }
            for (size_t i = 0; i < n; ++i) {
                out[done + i] = (buf[i] >> 62) ? 1 : 0;
            }
            done += n;
        }
        ::close(fd);
        return ok;
    }
#endif

#if !defined(_WIN32)
    // mincore over the whole range, page by page when part of it is not
    // mapped (ENOMEM). A page has been used when it is resident, referenced,
    // modified, copied or paged out; macOS also flags every page of an
    // anonymous mapping (MINCORE_ANONYMOUS, 0x80), touched or not, and that
    // bit is ignored.
    inline bool _touched_pages_mincore(uintptr_t begin, size_t pages, size_t page, unsigned char* out) noexcept {
#if defined(__APPLE__)
        using Vec = char;
        constexpr unsigned char Used = 0x7F;
#else
        using Vec = unsigned char;
        constexpr unsigned char Used = 0xFF;
#endif
        if (::mincore((void*)begin, pages * page, (Vec*)out) != 0) {
            for (size_t i = 0; i < pages; ++i) {
                Vec v = 0;
                out[i] = ::mincore((void*)(begin + i * page), page, &v) == 0 ? (unsigned char)v : 0;
            }
        }
        for (size_t i = 0; i < pages; ++i) {
            out[i] &= Used;
        }
        return true;
    }
#endif

    // One byte per page of [begin, begin + size), nonzero when the thread has
    // ever used the page (resident or swapped out). Untouched pages of a
    // stack are never read: the read would materialize them. Returns the
    // page size, or 0 when the answer is unknown and every page has to be
    // treated as touched.
    inline size_t touched_pages(const void* begin, size_t size, std::vector<unsigned char>& out) noexcept {
        auto page = page_size();
        auto first = (uintptr_t)begin & ~(page - 1);
        auto last = ((uintptr_t)begin + size + page - 1) & ~(page - 1);
        auto pages = (last - first) / page;
        out.assign(pages, 0);
#if defined(_WIN32)
        // committed and readable pages; the guard page marks the growth edge
        MEMORY_BASIC_INFORMATION info;
        for (auto p = first; p < last; p += info.RegionSize) {
            if (!::VirtualQuery((void*)p, &info, sizeof(info))) {
                return 0;
            }
            auto region = (uintptr_t)info.BaseAddress;
            bool touched = info.State == MEM_COMMIT && !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS));
            auto from = std::max(region, first);
            auto to = std::min(region + info.RegionSize, last);
            if (touched) {
                std::memset(out.data() + (from - first) / page, 1, (to - from) / page);
            }
            info.RegionSize = to - p;
        }
        return page;
#else
#if defined(__linux__)
        if (_touched_pages_pagemap(first, pages, page, out.data())) {
            return page;
        }
#endif
        return _touched_pages_mincore(first, pages, page, out.data()) ? page : 0;
#endif
    }

    // Calls fn(arg) with the callee-saved registers zeroed for the duration,
    // their values kept inverted in this frame, after zeroing the stack
    // between `limit` and the frame (nothing when limit is not below it).
    // Below the caller's own frame the stack then holds nothing that looks
    // like a pointer: not the registers that the caller's optimized code may
    // keep raw pointers in (a callee would push them), not the frames of the
    // caller's earlier callees, and no frame lies between this one and the
    // zeroed area. Where no such trampoline exists a fixed buffer is zeroed
    // in a frame of its own and the scan may see a little more.
#if defined(__aarch64__) && (defined(__clang__) || defined(__GNUC__)) && !defined(_WIN32)
    __attribute__((naked, noinline)) static void hidden_call(void (*)(void*), void*, uintptr_t) noexcept {
        asm volatile(
            "stp x29, x30, [sp, #-112]!\n"
            "mov x29, sp\n"
            "mvn x9, x19\n" "mvn x10, x20\n" "stp x9, x10, [sp, #16]\n"
            "mvn x9, x21\n" "mvn x10, x22\n" "stp x9, x10, [sp, #32]\n"
            "mvn x9, x23\n" "mvn x10, x24\n" "stp x9, x10, [sp, #48]\n"
            "mvn x9, x25\n" "mvn x10, x26\n" "stp x9, x10, [sp, #64]\n"
            "mvn x9, x27\n" "mvn x10, x28\n" "stp x9, x10, [sp, #80]\n"
            "stp x0, x1, [sp, #96]\n"
            "mov x19, #0\n" "mov x20, #0\n" "mov x21, #0\n" "mov x22, #0\n" "mov x23, #0\n"
            "mov x24, #0\n" "mov x25, #0\n" "mov x26, #0\n" "mov x27, #0\n" "mov x28, #0\n"
            "mov x3, #0\n" "mov x4, #0\n" "mov x5, #0\n" "mov x6, #0\n" "mov x7, #0\n" "mov x8, #0\n"
            "mov x11, #0\n" "mov x12, #0\n" "mov x13, #0\n" "mov x14, #0\n" "mov x15, #0\n"
            "mov x9, sp\n"
            "and x10, x2, #-16\n"
            "1:\n"
            "cmp x10, x9\n"
            "b.hs 2f\n"
            "stp xzr, xzr, [x10], #16\n"
            "b 1b\n"
            "2:\n"
            "ldp x9, x0, [sp, #96]\n"
            "blr x9\n"
            "ldp x9, x10, [sp, #16]\n" "mvn x19, x9\n" "mvn x20, x10\n"
            "ldp x9, x10, [sp, #32]\n" "mvn x21, x9\n" "mvn x22, x10\n"
            "ldp x9, x10, [sp, #48]\n" "mvn x23, x9\n" "mvn x24, x10\n"
            "ldp x9, x10, [sp, #64]\n" "mvn x25, x9\n" "mvn x26, x10\n"
            "ldp x9, x10, [sp, #80]\n" "mvn x27, x9\n" "mvn x28, x10\n"
            "stp xzr, xzr, [sp, #96]\n"
            "ldp x29, x30, [sp], #112\n"
            "ret\n");
    }
#elif defined(__x86_64__) && (defined(__clang__) || defined(__GNUC__)) && !defined(_WIN32)
    __attribute__((naked, noinline)) static void hidden_call(void (*)(void*), void*, uintptr_t) noexcept {
        asm volatile(
            "pushq %rbp\n"
            "movq %rsp, %rbp\n"
            "subq $64, %rsp\n"
            "movq %rbx, %rax\n" "notq %rax\n" "movq %rax, -8(%rbp)\n"
            "movq %r12, %rax\n" "notq %rax\n" "movq %rax, -16(%rbp)\n"
            "movq %r13, %rax\n" "notq %rax\n" "movq %rax, -24(%rbp)\n"
            "movq %r14, %rax\n" "notq %rax\n" "movq %rax, -32(%rbp)\n"
            "movq %r15, %rax\n" "notq %rax\n" "movq %rax, -40(%rbp)\n"
            "movq %rdi, -48(%rbp)\n"
            "movq %rsi, -56(%rbp)\n"
            "movq $0, -64(%rbp)\n"
            "xorl %ebx, %ebx\n" "xorl %r12d, %r12d\n" "xorl %r13d, %r13d\n" "xorl %r14d, %r14d\n" "xorl %r15d, %r15d\n"
            "xorl %r8d, %r8d\n" "xorl %r9d, %r9d\n" "xorl %r10d, %r10d\n" "xorl %r11d, %r11d\n"
            "movq %rsp, %rcx\n"
            "andq $-8, %rdx\n"
            "1:\n"
            "cmpq %rcx, %rdx\n"
            "jae 2f\n"
            "movq $0, (%rdx)\n"
            "addq $8, %rdx\n"
            "jmp 1b\n"
            "2:\n"
            "movq -48(%rbp), %rax\n"
            "movq -56(%rbp), %rdi\n"
            "call *%rax\n"
            "movq -8(%rbp), %rax\n" "notq %rax\n" "movq %rax, %rbx\n"
            "movq -16(%rbp), %rax\n" "notq %rax\n" "movq %rax, %r12\n"
            "movq -24(%rbp), %rax\n" "notq %rax\n" "movq %rax, %r13\n"
            "movq -32(%rbp), %rax\n" "notq %rax\n" "movq %rax, %r14\n"
            "movq -40(%rbp), %rax\n" "notq %rax\n" "movq %rax, %r15\n"
            "movq %rbp, %rsp\n"
            "popq %rbp\n"
            "ret\n");
    }
#else
    SGCL_NOINLINE inline void _clear_stack_buffer() noexcept {
        volatile uintptr_t buffer[config::StackClearSize / sizeof(uintptr_t)];
        for (auto& w : buffer) {
            w = 0;
        }
    }

    inline void hidden_call(void (*fn)(void*), void* arg, uintptr_t) noexcept {
        _clear_stack_buffer();
        fn(arg);
    }
#endif

    // Makes the address of an object known to "someone": from here on the
    // compiler must keep the object in memory and perform its atomic
    // operations there. Without it a local tracked_ptr whose address never
    // escapes can live in a register only, invisible to the stack scan
    // (clang does that to non-escaping atomics at -O2).
#if defined(__GNUC__) || defined(__clang__)
    SGCL_ALWAYS_INLINE void escape(const void* p) noexcept {
        asm volatile("" : : "r"(p));
    }
#else
    SGCL_NOINLINE inline void escape(const void*) noexcept {
    }
#endif

    // Lowest address of a touched page in [begin, end), or `end` when none:
    // how far down the calling thread's stack has ever reached, so that
    // clearing it does not touch pages that were never used (the scan would
    // then read them every cycle).
    inline uintptr_t lowest_touched(uintptr_t begin, uintptr_t end) noexcept {
        if (begin >= end) {
            return end;
        }
        std::vector<unsigned char> touched;
        auto page = touched_pages((const void*)begin, end - begin, touched);
        if (!page) {
            return begin;
        }
        auto first = begin & ~(page - 1);
        for (size_t i = 0; i < touched.size(); ++i) {
            if (touched[i]) {
                return std::max(begin, first + i * page);
            }
        }
        return end;
    }

    // A word of another thread's stack, read while that thread may be
    // writing it: a plain volatile load (single-copy atomic for an aligned
    // word on every supported target), not an atomic builtin, which the
    // sanitizers intercept regardless of the attribute.
    SGCL_NO_SANITIZE inline uintptr_t load_word(const void* p) noexcept {
        return *(const volatile uintptr_t*)p;
    }

    // The words of [begin, end) that lie in [base, base + size), each
    // handed to f: the stack scan. Eight words a step with the range test
    // on vectors (NEON on arm64, SSE4.2 on x86-64), the words read one by
    // one only in a step that holds a hit; a stack is mostly data and
    // pointers elsewhere. Hidden from the sanitizers like load_word: the
    // words are another thread's stack, read while it runs.
    template<class F>
    SGCL_NO_SANITIZE inline void scan_heap_words(uintptr_t begin, uintptr_t end, uintptr_t base, size_t size, F&& f) noexcept {
#if defined(__aarch64__) && defined(__ARM_NEON)
        const uint64x2_t vbase = vdupq_n_u64(base);
        const uint64x2_t vsize = vdupq_n_u64(size);
        for (; begin + 8 * sizeof(uintptr_t) <= end; begin += 8 * sizeof(uintptr_t)) {
            auto p = (const uint64_t*)begin;
            auto m0 = vcltq_u64(vsubq_u64(vld1q_u64(p), vbase), vsize);
            auto m1 = vcltq_u64(vsubq_u64(vld1q_u64(p + 2), vbase), vsize);
            auto m2 = vcltq_u64(vsubq_u64(vld1q_u64(p + 4), vbase), vsize);
            auto m3 = vcltq_u64(vsubq_u64(vld1q_u64(p + 6), vbase), vsize);
            auto any = vorrq_u64(vorrq_u64(m0, m1), vorrq_u64(m2, m3));
            if (vmaxvq_u32(vreinterpretq_u32_u64(any))) [[unlikely]] {
                for (int i = 0; i < 8; ++i) {
                    auto w = (uintptr_t)p[i];
                    if (w - base < size) {
                        f(w);
                    }
                }
            }
        }
#elif defined(__x86_64__) && defined(__SSE4_2__)
        // unsigned (w - base) < size as a signed compare of the values with the sign bit flipped
        const __m128i vbase = _mm_set1_epi64x((long long)base);
        const __m128i flip = _mm_set1_epi64x((long long)0x8000000000000000ull);
        const __m128i vsize = _mm_xor_si128(_mm_set1_epi64x((long long)size), flip);
        for (; begin + 8 * sizeof(uintptr_t) <= end; begin += 8 * sizeof(uintptr_t)) {
            auto p = (const __m128i*)begin;
            __m128i any = _mm_setzero_si128();
            for (int i = 0; i < 4; ++i) {
                auto d = _mm_xor_si128(_mm_sub_epi64(_mm_loadu_si128(p + i), vbase), flip);
                any = _mm_or_si128(any, _mm_cmpgt_epi64(vsize, d));
            }
            if (!_mm_testz_si128(any, any)) [[unlikely]] {
                auto q = (const uintptr_t*)begin;
                for (int i = 0; i < 8; ++i) {
                    auto w = q[i];
                    if (w - base < size) {
                        f(w);
                    }
                }
            }
        }
#endif
        for (; begin < end; begin += sizeof(uintptr_t)) {
            auto w = load_word((const void*)begin);
            if (w - base < size) {
                f(w);
            }
        }
    }

    inline size_t physical_memory() noexcept {
#if defined(_WIN32)
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        if (::GlobalMemoryStatusEx(&status)) {
            return (size_t)status.ullTotalPhys;
        }
        return 0;
#else
        auto pages = ::sysconf(_SC_PHYS_PAGES);
        auto page_size = ::sysconf(_SC_PAGESIZE);
        if (pages > 0 && page_size > 0) {
            return (size_t)pages * (size_t)page_size;
        }
        return 0;
#endif
    }

#if defined(__linux__)
    // Reads a byte count from a cgroup file; 0 when missing or unlimited.
    inline size_t _cgroup_limit(const char* path) noexcept {
        auto f = std::fopen(path, "r");
        if (!f) {
            return 0;
        }
        char buf[64] = {};
        auto n = std::fread(buf, 1, sizeof(buf) - 1, f);
        std::fclose(f);
        if (!n || !std::strncmp(buf, "max", 3)) {
            return 0;
        }
        auto value = std::strtoull(buf, nullptr, 10);
        // cgroup v1 reports "unlimited" as a huge number
        return value < (unsigned long long)1 << 60 ? (size_t)value : 0;
    }
#endif

    // The memory the process may actually use: the cgroup limit when the
    // process runs under one (exceeding it means the OOM killer, not swap),
    // otherwise the physical memory. 0 when unknown.
    inline size_t memory_limit() noexcept {
        auto physical = physical_memory();
        size_t limit = 0;
#if defined(__linux__)
        // cgroup v2: this process's own cgroup, then the root of the namespace
        if (auto f = std::fopen("/proc/self/cgroup", "r")) {
            char line[512];
            while (std::fgets(line, sizeof(line), f)) {
                if (!std::strncmp(line, "0::", 3)) {
                    line[std::strcspn(line, "\n")] = 0;
                    char path[600];
                    std::snprintf(path, sizeof(path), "/sys/fs/cgroup%s/memory.max", line + 3);
                    limit = _cgroup_limit(path);
                    break;
                }
            }
            std::fclose(f);
        }
        if (!limit) {
            limit = _cgroup_limit("/sys/fs/cgroup/memory.max");
        }
        if (!limit) {
            limit = _cgroup_limit("/sys/fs/cgroup/memory/memory.limit_in_bytes");   // v1
        }
#endif
        if (limit && (!physical || limit < physical)) {
            return limit;
        }
        return physical;
    }

}
