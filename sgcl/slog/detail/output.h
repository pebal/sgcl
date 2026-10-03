//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../handler.h"
#include "../level.h"
#include "../../async/scheduler.h"
#include "../../core/detail/os.h"
#include "../../core/root_ptr.h"
#include "../../core/slice.h"
#include "../../core/tracked_ptr.h"
#include "../../io/os.h"
#include "../../io/stream.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Where a logger's lines go: a writer (text, JSON) or a handler of the
// program, and for a buffered logger the batches of lines each worker
// gathers before one write. A record is one write of whole lines, never
// split: unbuffered, the line itself, from the thread that logs, with no
// lock of ours (the writer takes writes from many threads: a file,
// stderr, a connection); buffered, a batch of whole lines, written when
// it is full, at a record of warn and up, when its worker goes to sleep
// (the scheduler's idle hook), at flush() and at exit.
namespace sgcl::slog::detail {
    using async::detail::Scheduler;

    inline constexpr unsigned Slots = Scheduler::MaxWorkers + 1;   // a slot per worker, one for every other thread
    inline constexpr size_t BatchSize = 32 * 1024;

    // A lock for a slot, taken by its worker on every buffered record and
    // by another thread only to write the slot out (flush(), exit): one
    // exchange to take and one to give, when nobody else wants it; a
    // waiter sleeps on the word (Drepper's three-state futex lock)
    class SlotLock {
    public:
        void lock() noexcept {
            uint32_t free = 0;
            if (_s.compare_exchange_strong(free, 1, std::memory_order_acquire, std::memory_order_relaxed)) [[likely]] {
                return;
            }
            _contended();
        }

        void unlock() noexcept {
            if (_s.exchange(0, std::memory_order_release) == 2) [[unlikely]] {
                _s.notify_one();
            }
        }

    private:
        SGCL_NOINLINE void _contended() noexcept {
            while (_s.exchange(2, std::memory_order_acquire) != 0) {
                _s.wait(2, std::memory_order_relaxed);
            }
        }

        std::atomic<uint32_t> _s = {0};
    };

    enum class Format : uint8_t {
        Text,
        Json,
        Custom
    };

    struct Output;

    // The outputs with lines waiting in a slot, per slot: held by a root
    // while they wait, so that a logger let go of with lines in its
    // batches still writes them; the list is taken whole when its worker
    // goes to sleep. Made once and never destroyed: the exit writes them
    // from atexit functions that may run after the statics made later
    // are gone (collector_log's drain)
    struct Waiting {
        std::mutex lock;
        std::vector<root_ptr<Output>> outputs;
        std::atomic<bool> any = {false};
    };

    inline Waiting* waiting() noexcept {
        static Waiting* lists = new Waiting[Slots];
        return lists;
    }

    inline void write_waiting(unsigned slot);
    inline void write_all_waiting();

    // The collector's lines waiting for a thread that may log them
    // (collector_log.h): the flag set by the sink, the drain installed with
    // it. One relaxed load where nothing waits
    inline std::atomic<bool> collector_pending = {false};
    inline std::atomic<void (*)() noexcept> collector_drain = {nullptr};

    inline void drain_collector_if_pending() noexcept {
        if (collector_pending.load(std::memory_order_relaxed)) [[unlikely]] {
            if (auto drain = collector_drain.load(std::memory_order_acquire)) {
                drain();
            }
        }
    }

    // What a worker does on its way to sleep: the collector's lines logged,
    // its batches written
    inline void worker_idle(unsigned slot) {
        drain_collector_if_pending();
        write_waiting(slot);
    }

    // The hook and the exit's writes, set once, when the first buffered
    // output is made
    inline void install_batch_hooks() noexcept {
        static std::once_flag once;
        std::call_once(once, [] {
            (void)waiting();
            async::detail::worker_idle_hook.store(&worker_idle, std::memory_order_relaxed);
            std::atexit([] { write_all_waiting(); });
            std::at_quick_exit([] { write_all_waiting(); });
        });
    }

    struct alignas(64) Slot {
        SlotLock lock;
        bool listed = false;       // in its slot's waiting list
        uint32_t records = 0;      // the lines in data
        size_t n = 0;
        char* data = nullptr;      // BatchSize bytes, made at the slot's first line
    };

    // A slot's lock let go of when its scope ends, by an exception of the
    // program's writer too: a slot left locked would stop every later
    // record of its worker
    struct SlotGuard {
        explicit SlotGuard(Slot& s) noexcept
        : slot(s) {
            slot.lock.lock();
        }

        SlotGuard(const SlotGuard&) = delete;
        SlotGuard& operator=(const SlotGuard&) = delete;

        ~SlotGuard() {
            slot.lock.unlock();
        }

        Slot& slot;
    };

    struct Output {
        Output(Format f, const io::writer& w, bool batch) noexcept
        : format(f), writer(w), buffered(batch) {
            if (buffered) {
                slots = new Slot[Slots];
                install_batch_hooks();
            }
        }

        explicit Output(const slog::handler& h) noexcept
        : format(Format::Custom), custom(h) {
        }

        Output(const Output&) = delete;
        Output& operator=(const Output&) = delete;

        ~Output() {
            if (slots) {
                for (unsigned i = 0; i < Slots; ++i) {
                    std::free(slots[i].data);
                }
                delete[] slots;
            }
        }

        // A line, or a batch of lines, that could not be written: counted,
        // and said once on stderr
        void failed(const io::error& e, uint64_t lines) {
            dropped.fetch_add(lines, std::memory_order_relaxed);
            if (!reported.exchange(true, std::memory_order_relaxed)) {
                std::string text = "sgcl::slog: a write failed: " + io_error_text(e) + "\n";
                (void)io::stderr.write(slice<const byte>(reinterpret_cast<const byte*>(text.data()), text.size()));
            }
        }

        void write_now(const char* p, size_t n, uint64_t lines) {
            auto r = writer.write(slice<const byte>(reinterpret_cast<const byte*>(p), n));
            if (!r) [[unlikely]] {
                failed(r.error(), lines);
            }
        }

        // What a slot holds, written; under its lock. The lines go with
        // the write, also when the writer throws
        void drain(Slot& s) {
            if (s.n) {
                size_t n = s.n;
                uint32_t lines = s.records;
                s.n = 0;
                s.records = 0;
                write_now(s.data, n, lines);
            }
        }

        // The same where nobody takes an exception (a worker on its way to
        // sleep, the exit): the lines of a writer that throws are counted
        // as lost
        void drain_quietly(Slot& s) noexcept {
            const uint32_t lines = s.records;
            try {
                drain(s);
            } catch (...) {
                dropped.fetch_add(lines, std::memory_order_relaxed);
            }
        }

        // A line into the batch of the thread's slot
        static void batch(const tracked_ptr<Output>& self, const char* p, size_t n, slog::level l) {
            Output& o = *self;
            const unsigned w = Scheduler::worker_index();
            Slot& s = o.slots[w];
            SlotGuard guard(s);
            if (s.n + n > BatchSize) {
                o.drain(s);
            }
            if (n > BatchSize) {
                o.write_now(p, n, 1);
            } else {
                if (!s.data) [[unlikely]] {
                    s.data = static_cast<char*>(std::malloc(BatchSize));
                    if (!s.data) [[unlikely]] {
                        sgcl::detail::os::memory_refused("a batch of log lines", BatchSize);
                    }
                }
                sgcl::detail::copy_bytes(s.data + s.n, p, n);
                s.n += n;
                ++s.records;
            }
            if (int(l) >= int(slog::level::warn)) {
                o.drain(s);
            } else if (s.n && !s.listed) {
                s.listed = true;
                Waiting& list = waiting()[w];
                std::lock_guard<std::mutex> g(list.lock);
                list.outputs.emplace_back(self);
                list.any.store(true, std::memory_order_relaxed);
            }
        }

        void write(const tracked_ptr<Output>& self, const char* p, size_t n, slog::level l) {
            if (buffered) {
                batch(self, p, n, l);
            } else {
                write_now(p, n, 1);
            }
        }

        // Every slot written (flush())
        void flush() {
            if (!slots) {
                return;
            }
            for (unsigned i = 0; i < Slots; ++i) {
                SlotGuard guard(slots[i]);
                drain(slots[i]);
            }
        }

        const Format format;
        io::writer writer;
        slog::handler custom;
        const bool buffered = false;
        Slot* slots = nullptr;
        std::atomic<uint64_t> dropped = {0};
        std::atomic<bool> reported = {false};
    };

    // The lines waiting in a slot, written: by its worker on the way to
    // sleep, by the exit for every slot
    inline void write_waiting(unsigned slot) {
        Waiting& list = waiting()[slot];
        if (!list.any.load(std::memory_order_relaxed)) {
            return;
        }
        std::vector<root_ptr<Output>> taken;
        {
            std::lock_guard<std::mutex> g(list.lock);
            taken.swap(list.outputs);
            list.any.store(false, std::memory_order_relaxed);
        }
        for (auto& p : taken) {
            Output& o = *p;
            Slot& s = o.slots[slot];
            SlotGuard guard(s);
            s.listed = false;
            o.drain_quietly(s);
        }
    }

    inline void write_all_waiting() {
        for (unsigned i = 0; i < Slots; ++i) {
            write_waiting(i);
        }
    }
}
