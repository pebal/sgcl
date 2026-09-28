//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "chacha_core.h"
#include "../secure_zero.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <type_traits>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt")
#else
#include <cerrno>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/random.h>
#endif
#endif

// The generator behind crypto::random: ChaCha20 in user space, a state per
// thread, seeded from the operating system's generator (getentropy,
// BCryptGenRandom) and reseeded from it.
//
// Fast key erasure (D. J. Bernstein, "Fast-key-erasure random-number
// generators", 2017; the shape of BoringSSL's and of Go's runtime
// generators): a refill runs ChaCha20 under the thread's key for nine
// blocks (576 bytes, one step of the NEON path), the first 32 bytes become
// the next key at once and the other 544 are given out, each byte zeroed
// in the buffer as it is given. The key that made them is gone before the
// first byte leaves, so a state read later (a core dump, a thread's memory
// after an exploit) tells nothing of what was given before. A request over
// 1 KiB takes a one-time key of 32 bytes from the buffer and fills its
// output with ChaCha20 under that key directly, the key zeroed after it.
//
// The nonce is 0 and the block counter starts at 0 for every key. That is
// safe ONLY because no key is ever used twice: the thread's key is
// replaced at every refill (the erasure above) and a large request's key
// is used for that request alone. Whoever "saves" the erasure, or keeps a
// key across refills to spare a block, makes the generator repeat its
// output: the erasure is the security of this file, not a cost of it.
//
// Reseeded from the system after 1 MiB given or 60 s since the last seed,
// both checked at a refill (every 544 bytes, never on a call's fast path):
// the fresh 32 bytes are XORed into the key, so that a weak source cannot
// spoil a state that is already good. After a fork the child's copy of the
// state is thrown away and seeded afresh, not reseeded: a counter of forks
// raised by a pthread_atfork handler, compared with the thread's on every
// call (one relaxed load), and the process id compared at every refill,
// for a fork that runs no atfork handlers. The clock counts time asleep
// (CLOCK_MONOTONIC on macOS, CLOCK_BOOTTIME on Linux), so a machine woken,
// or a virtual machine restored, after a minute reseeds at its next refill.
//
// The state lives in the thread's own storage, never in managed memory,
// and is zeroed when the thread ends; a call after that (from the
// destructor of another thread_local) takes its bytes from the system
// directly. Not async-signal-safe: a signal handler that asks for random
// bytes while its thread is inside fill() would take them from the same
// position.
namespace sgcl::crypto::detail {
    [[noreturn]] inline void no_entropy() noexcept {
        std::fputs("sgcl::crypto::random: the operating system gave no random bytes\n", stderr);
        std::terminate();
    }

    // n bytes from the operating system's generator
    inline void system_random(unsigned char* p, size_t n) noexcept {
#if defined(_WIN32)
        while (n != 0) {
            ULONG take = ULONG(std::min<size_t>(n, 1u << 30));
            if (!BCRYPT_SUCCESS(::BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(p), take, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
                no_entropy();
            }
            p += take;
            n -= take;
        }
#else
        // getentropy gives at most 256 bytes a call
        while (n != 0) {
            size_t take = std::min<size_t>(n, 256);
            if (::getentropy(p, take) != 0) {
                if (errno == EINTR) {
                    continue;
                }
                no_entropy();
            }
            p += take;
            n -= take;
        }
#endif
    }

    inline constexpr size_t drbg_buffer_size = 576;            // nine ChaCha20 blocks
    inline constexpr size_t drbg_key_size = 32;
    inline constexpr size_t drbg_large = 1024;                 // a request over this: a key of its own
    inline constexpr uint64_t drbg_reseed_bytes = uint64_t(1) << 20;
    inline constexpr uint64_t drbg_reseed_ns = uint64_t(60) * 1000000000u;

    // Nanoseconds on a clock that counts time asleep
    inline uint64_t drbg_system_clock() noexcept {
#if defined(_WIN32)
        return uint64_t(::GetTickCount64()) * 1000000u;
#else
        timespec t{};
#if defined(__linux__)
        ::clock_gettime(CLOCK_BOOTTIME, &t);
#else
        ::clock_gettime(CLOCK_MONOTONIC, &t);
#endif
        return uint64_t(t.tv_sec) * 1000000000u + uint64_t(t.tv_nsec);
#endif
    }

    inline int drbg_pid() noexcept {
#if defined(_WIN32)
        return 0;
#else
        return int(::getpid());
#endif
    }

    // For the tests: the clock the reseed by time reads, and a function
    // the end of a thread calls with its state once it is zeroed
    inline uint64_t (*drbg_clock)() noexcept = &drbg_system_clock;
    inline void (*drbg_on_thread_exit)(const void* state, size_t size) noexcept = nullptr;

    // Raised in the child of every fork
    inline std::atomic<uint64_t> drbg_forks{0};

    // A thread's generator. Rule 1: no tracked word lives in thread
    // storage, which the collector does not scan; the state is bytes and
    // counters only, trivially copyable and trivially destructible (a
    // tracked_ptr or a handle in it would fail both assertions below), so
    // that it needs neither a constructor run nor a destructor registered
    // on the fast path, and a call after the thread's wiper ran reads
    // memory that is still there.
    struct DrbgState {
        enum : uint8_t { fresh, seeded, dead };

        alignas(64) unsigned char buffer[drbg_buffer_size] = {};   // [pos, 576): keystream not given; the rest zeros
        unsigned char key[drbg_key_size] = {};
        uint32_t pos = drbg_buffer_size;
        uint8_t status = fresh;
        int pid = 0;
        uint64_t forks = 0;             // drbg_forks at the last seed
        uint64_t given = 0;             // bytes since the last seed or reseed
        uint64_t seeded_at = 0;         // drbg_clock() then
        uint64_t reseeds = 0;           // for the tests: seeds and reseeds so far
    };

    static_assert(std::is_trivially_copyable_v<DrbgState>, "the generator's state must hold no tracked word (Rule 1)");
    static_assert(std::is_trivially_destructible_v<DrbgState>, "the generator's state must outlive its wiper without a destructor of its own");

    inline constinit thread_local DrbgState drbg_state{};

    // Zeroes the thread's state when the thread ends; armed at the thread's
    // first seed (the only access that registers its destructor)
    struct DrbgWiper {
        bool armed = false;

        ~DrbgWiper() {
            DrbgState& s = drbg_state;
            secure_zero(&s, sizeof s);
            s.status = DrbgState::dead;
            if (auto hook = drbg_on_thread_exit) {
                hook(&s, sizeof s);
            }
        }
    };

    inline thread_local DrbgWiper drbg_wiper;

    // 576 bytes of keystream under the key into the buffer (all zeros by
    // then: every byte given was zeroed), the first 32 the next key
    inline void drbg_refill_now(DrbgState& s) noexcept {
        static constexpr unsigned char nonce[12] = {};
        ChachaState c;
        chacha_load(c, s.key, nonce);
        chacha_xor(c, 0, s.buffer, s.buffer, drbg_buffer_size);
        secure_zero(&c, sizeof c);
        std::memcpy(s.key, s.buffer, drbg_key_size);
        secure_zero(s.buffer, drbg_key_size);
        s.pos = drbg_key_size;
    }

    // A new state from the system: the first of a thread, or after a fork
    inline void drbg_seed(DrbgState& s, uint64_t forks) noexcept {
        secure_zero(s.buffer, sizeof s.buffer);
        system_random(s.key, drbg_key_size);
        s.forks = forks;
        s.pid = drbg_pid();
        s.given = 0;
        s.seeded_at = drbg_clock();
        ++s.reseeds;
        s.status = DrbgState::seeded;
        drbg_refill_now(s);
    }

    inline void drbg_after_fork() noexcept {
        drbg_forks.fetch_add(1, std::memory_order_relaxed);
    }

    // The slow path of a call: a thread's first, the first after a fork,
    // or one after the thread's end; false for the last (the caller takes
    // the system's bytes)
    inline bool drbg_prepare(DrbgState& s) noexcept {
        if (s.status == DrbgState::dead) {
            return false;
        }
        if (s.status == DrbgState::fresh) {
#if !defined(_WIN32)
            static const bool registered = [] {
                return ::pthread_atfork(nullptr, nullptr, &drbg_after_fork) == 0;
            }();
            (void)registered;
#endif
            drbg_wiper.armed = true;
        }
        drbg_seed(s, drbg_forks.load(std::memory_order_relaxed));
        return true;
    }

    // An empty buffer refilled, reseeded first when the bytes or the time
    // say so, seeded afresh when the process is not the one that seeded it
    inline void drbg_refill(DrbgState& s) noexcept {
        if (drbg_pid() != s.pid) {
            drbg_seed(s, drbg_forks.load(std::memory_order_relaxed));
            return;
        }
        const uint64_t now = drbg_clock();
        if (s.given >= drbg_reseed_bytes || now - s.seeded_at >= drbg_reseed_ns) {
            unsigned char fresh[drbg_key_size];
            system_random(fresh, drbg_key_size);
            for (size_t i = 0; i < drbg_key_size; ++i) {
                s.key[i] ^= fresh[i];
            }
            secure_zero(fresh, sizeof fresh);
            s.given = 0;
            s.seeded_at = now;
            ++s.reseeds;
        }
        drbg_refill_now(s);
    }

    // n bytes from the buffer, refilled as it empties
    inline void drbg_take(DrbgState& s, unsigned char* out, size_t n) noexcept {
        while (n != 0) {
            if (s.pos == drbg_buffer_size) {
                drbg_refill(s);
            }
            const size_t take = std::min<size_t>(n, drbg_buffer_size - s.pos);
            std::memcpy(out, s.buffer + s.pos, take);
            secure_zero(s.buffer + s.pos, take);
            s.pos += uint32_t(take);
            s.given += take;
            out += take;
            n -= take;
        }
    }

    // A large request: a one-time key from the buffer, ChaCha20 under it
    // into out; a new key every 2^32 blocks (256 GiB), where the counter
    // would wrap
    inline void drbg_large_fill(DrbgState& s, unsigned char* out, size_t n) noexcept {
        static constexpr unsigned char nonce[12] = {};
        static constexpr uint64_t chunk = uint64_t(1) << 38;
        while (n != 0) {
            const size_t m = size_t(std::min<uint64_t>(n, chunk));
            unsigned char k[drbg_key_size];
            drbg_take(s, k, drbg_key_size);
            ChachaState c;
            chacha_load(c, k, nonce);
            secure_zero(k, sizeof k);
            std::memset(out, 0, m);
            chacha_xor(c, 0, out, out, m);
            secure_zero(&c, sizeof c);
            s.given += m;
            out += m;
            n -= m;
        }
    }

    inline void drbg_fill(unsigned char* out, size_t n) noexcept {
        DrbgState& s = drbg_state;
        if (s.status != DrbgState::seeded || s.forks != drbg_forks.load(std::memory_order_relaxed)) [[unlikely]] {
            if (!drbg_prepare(s)) {
                system_random(out, n);
                return;
            }
        }
        if (n > drbg_large) {
            drbg_large_fill(s, out, n);
        } else {
            drbg_take(s, out, n);
        }
    }
}
