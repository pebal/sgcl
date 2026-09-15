//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/collector.h"

#include <ostream>
#include <tuple>
#include <vector>

namespace sgcl {
    class collector {
    public:
        using pause_guard = detail::Collector::PauseGuard;

        // The stack is scanned conservatively, so these three run their work
        // through a trampoline that hides the caller's registers and zeroes
        // the stack below: pointers that the caller's earlier callees left in
        // dead frames, or that its optimized code keeps in a register, would
        // otherwise count as roots. They are inlined into the caller so that
        // no frame of their own lies between the caller and the zeroed area.
        // What stays visible is the caller's own frame: a raw pointer or an
        // iterator kept there does retain its target (README, "Stack roots").
        SGCL_ALWAYS_INLINE static size_t get_live_object_count() {
            size_t count = 0;
            detail::os::hidden_call([](void* out) {
                *(size_t*)out = std::get<1>(detail::collector_instance().get_live_objects()).size();
            }, &count, detail::stack_clear_limit(config::StackClearSize));
            return count;
        }

        SGCL_ALWAYS_INLINE static std::tuple<pause_guard, std::vector<void*>> get_live_objects() {
            std::tuple<pause_guard, std::vector<void*>> result;
            detail::os::hidden_call([](void* out) {
                *(std::tuple<pause_guard, std::vector<void*>>*)out = detail::collector_instance().get_live_objects();
            }, &result, detail::stack_clear_limit(config::StackClearSize));
            return result;
        }

        SGCL_ALWAYS_INLINE static bool force_collect(bool wait = false) noexcept {
            bool args[2] = {wait, false};
            detail::os::hidden_call([](void* p) {
                auto args = (bool*)p;
                args[1] = detail::collector_instance().force_collect(args[0]);
            }, args, detail::stack_clear_limit(config::StackClearSize));
            return args[1];
        }

        // Zeroes `bytes` of stack below the caller's frame (the whole unused
        // stack for SIZE_MAX). Objects referenced only by words left behind
        // in dead frames become collectable.
        SGCL_ALWAYS_INLINE static void clear_stack(size_t bytes = config::StackClearSize) noexcept {
            detail::os::hidden_call([](void*) {}, nullptr, detail::stack_clear_limit(bytes));
        }

        // Stops the collector: the current cycle finishes, then cycles run
        // until nothing dies any more (the objects still reachable are not
        // destroyed), the helper threads and the collector thread exit, and
        // the call returns. Optional: a program may simply end. After it no
        // cycle runs: objects are still allocated and destroyed through
        // unique_ptr, tracked garbage stays until the process exits.
        inline static void terminate() noexcept {
            detail::Collector::terminate();
        }

        // Counters of the collector's work, read without stopping it: the
        // values are of the last cycle that completed, except committed_bytes
        // and live_bytes, which are read now (live_bytes counts the pages in
        // use by the allocators, garbage not yet swept included).
        struct statistics {
            size_t cycles;              // completed since the start
            size_t full_cycles;         // of which full (all of them unless generational)
            size_t live_objects;        // objects marked by the last cycle
            size_t live_bytes;          // managed memory in use by the allocators now
            size_t committed_bytes;     // managed memory committed now
            double last_cycle_ms;       // wall time of the last cycle
            unsigned helper_threads;    // helper threads started so far
            unsigned last_helpers_used; // of which the last cycle used
            bool helpers_enabled;       // the helpers are on for the next cycle
            double phases_ms[8];        // the last cycle's phases: registration, states, roots, marking, updated states, sweep, page release, trim
        };

        static constexpr const char* phase_names[8] = {"registration", "states", "roots", "marking", "updated", "sweep", "release", "trim"};

        // The collector's counters as they are: a few atomic reads, never a
        // wait (docs/collector.md: statistics)
        inline static statistics get_statistics() noexcept {
            auto& c = detail::collector_instance();
            auto s = c.statistics<statistics>();
            for (int i = 0; i < 8; ++i) {
                s.phases_ms[i] = c.phase_ms(i);
            }
            return s;
        }

        // The live objects by type, after a full cycle: what a heap that
        // grows is made of. Objects by their type; the buffers of the
        // containers (vector, array, the maps of deque, the buckets of the
        // hash tables) by their array type (typeid(T[]) for elements T),
        // with the slot they occupy as their bytes and no pages (the pages
        // of buffers belong to size classes). Sorted by bytes. Like
        // get_live_objects(): a full cycle runs first, the caller's dead
        // frames are zeroed.
        struct type_statistics {
            const std::type_info* type;
            bool buffers;
            size_t object_size;
            size_t live_objects;
            size_t live_bytes;
            size_t pages;
        };

        SGCL_ALWAYS_INLINE static std::vector<type_statistics> get_type_statistics() {
            std::vector<type_statistics> result;
            detail::os::hidden_call([](void* out) {
                auto& collector = detail::collector_instance();
                auto [guard, objects] = collector.get_live_objects();
                auto& stats = collector.type_statistics();
                auto& result = *(std::vector<type_statistics>*)out;
                result.reserve(stats.size());
                for (auto& s : stats) {
                    result.push_back(type_statistics{s.type, s.buffers, s.object_size, s.live_objects, s.live_bytes, s.pages});
                }
            }, &result, detail::stack_clear_limit(config::StackClearSize));
            return result;
        }

        // Bytes of committed managed memory right now.
        inline static size_t get_committed_memory() noexcept {
            return detail::Heap::instance().committed_bytes();
        }

        // Ceiling on committed managed memory: by default 90% of the cgroup
        // limit (Linux) or of the physical memory. Near it the collector runs
        // more often and returns free chunks at once; at it an allocation
        // forces a collection and throws std::bad_alloc if that is not enough.
        // 0 disables the ceiling.
        inline static size_t get_memory_limit() noexcept {
            return detail::Heap::instance().memory_limit();
        }

        inline static void set_memory_limit(size_t bytes) noexcept {
            detail::Heap::instance().set_memory_limit(bytes);
        }

        // What holds an object: the words that point at it, wherever they
        // are, and a chain from it up to a root. Both run a full cycle first
        // and keep the collector paused while the pause_guard lives (as
        // get_live_objects), so that the live objects are exactly the
        // marked ones and no page moves under the walk; the mutators run
        // on, so a word is read as the scan reads it. `p` may point into
        // the object. A chain is [0] what holds the object, [1] what holds
        // that holder, ..., the last a root: a word on a stack (`holder` is
        // the word's address, the thread unnamed), an object a unique_ptr
        // owns (`unique`: `holder` is the object itself), a released block
        // of cells of gc::tracked_ptrs in unmanaged memory (`cell`: the
        // block, `offset` the cell; the gc::tracked_ptr that owns the cell
        // is not known to the collector; the block itself follows as a
        // `unique` link, a root by its state). The calling thread's own
        // frames are searched last, and only when nothing else reaches the
        // object: the caller holds the pointer it asks about and asks what
        // else does, so a chain that ends on its own stack says that
        // nothing else does (a local, or a word a dead frame left: garbage
        // once the frame is gone). Empty: `p` is not into a live managed
        // object, or only the diagnostic's own frames hold it. get_referrers
        // lists every word, the calling thread's frames above the call
        // included, and the `weak` cells, which hold nothing and are never
        // in a chain.
        struct referrer {
            enum class kind : int { object, buffer, stack, cell, unique, weak };
            kind from;
            const void* holder;
            const std::type_info* type;   // the holder's type, a buffer's element type (typeid(T[])); null for a stack word
            size_t offset;                // the word's byte offset in the holder
            std::thread::id thread;       // a stack word: the thread whose stack it is on
        };

        SGCL_NOINLINE static std::tuple<pause_guard, std::vector<referrer>> get_referrers(const void* p) {
            auto boundary = (uintptr_t)__builtin_frame_address(0);
            auto [guard, objects] = get_live_objects();
            return {std::move(guard), _referrers(detail::collector_instance().referrers(p, boundary))};
        }

        SGCL_NOINLINE static std::tuple<pause_guard, std::vector<referrer>> get_path_to_root(const void* p) {
            auto boundary = (uintptr_t)__builtin_frame_address(0);
            auto [guard, objects] = get_live_objects();
            return {std::move(guard), _referrers(detail::collector_instance().path_to_root(p, boundary))};
        }

        // What dies with the object: the objects reachable from it and
        // from nowhere else, itself included, and their bytes (the slots
        // they occupy). A full cycle first, the collector paused for the
        // walk, as get_referrers; {0, 0} for a pointer that is not into a
        // live managed object.
        struct retained {
            size_t objects;
            size_t bytes;
        };

        SGCL_NOINLINE static retained get_retained(const void* p) {
            auto boundary = (uintptr_t)__builtin_frame_address(0);
            auto [guard, objects] = get_live_objects();
            auto r = detail::collector_instance().retained(p, boundary);
            return {r.objects, r.bytes};
        }

        // The chain of get_path_to_root as text, a line per link, and what
        // the object retains; or why there is no chain
        SGCL_NOINLINE static void explain(const void* p, std::ostream& out) {
            auto boundary = (uintptr_t)__builtin_frame_address(0);
            auto [guard, objects] = get_live_objects();
            auto& c = detail::collector_instance();
            auto path = _referrers(c.path_to_root(p, boundary));
            if (path.empty()) {
                out << p << (c.object_of(p).first ? ": held by nothing but the frames of this call: garbage once they are gone\n" : ": not a live managed object\n");
                return;
            }
            auto r = c.retained(p, boundary);
            out << p << " is held by\n";
            for (auto& r : path) {
                switch (r.from) {
                    case referrer::kind::object: out << "  a " << r.type->name() << " at " << r.holder << ", the word at byte " << r.offset << '\n'; break;
                    case referrer::kind::buffer: out << "  a buffer of " << r.type->name() << " at " << r.holder << ", the word at byte " << r.offset << '\n'; break;
                    case referrer::kind::cell: out << "  a cell of a gc::tracked_ptr in unmanaged memory (block " << r.holder << ", cell " << r.offset / sizeof(void*) << ")\n"; break;
                    case referrer::kind::stack: out << "  a word on the stack of thread " << r.thread << ", at " << r.holder << (_own_stack(r.holder, boundary) ? " (this thread, above the call)\n" : "\n"); break;
                    case referrer::kind::unique:
                        if (*r.type == typeid(detail::CellBlock)) {
                            out << "  the block of cells, a root while a cell of it is in use\n";
                        } else {
                            out << "  a unique_ptr: the " << r.type->name() << " at " << r.holder << " is its object\n";
                        }
                        break;
                    case referrer::kind::weak: out << "  a weak_ptr's cell at " << r.holder << " (holds nothing)\n"; break;
                }
            }
            out << "and keeps alive " << r.objects << (r.objects == 1 ? " object, " : " objects, ") << r.bytes << " bytes, itself included\n";
        }

    private:
        // A word of the calling thread's stack above the boundary frame
        // (the diagnostic's own frames lie below it)
        static bool _own_stack(const void* word, uintptr_t boundary) noexcept {
            return (uintptr_t)word >= boundary && detail::thread_stack.holds(word);
        }

        // The engine's referrers as the public ones
        static std::vector<referrer> _referrers(const std::vector<detail::Collector::Referrer>& found) {
            std::vector<referrer> result;
            result.reserve(found.size());
            for (auto& r : found) {
                result.push_back({referrer::kind(int(r.kind)), r.holder, r.type, r.offset, r.thread});
            }
            return result;
        }

    public:
        // The collector one gate at a time, for the tests of the engine.
        // While a stepper exists no cycle runs on its own: the collector
        // stands at a gate, a boundary between the phases of a cycle, until
        // step() lets it through to the next one, and the test does a
        // mutator's work in between, in the window a race would have to
        // land in. The gates: `start` (a cycle about to begin), `flipped`
        // (the epoch flipped, nothing registered yet), `registered` (the
        // pages, objects and threads of before the flip registered),
        // `roots` (the stacks scanned, the dirty pages traced), `marked`
        // (the marking converged, the weak cells cleared), `swept` (the
        // garbage destroyed and freed), `released` (the empty pages back
        // in the heap: the cycle is over). The cycles are full unless the
        // stepper is made with `full = false`. The stepper's thread is the
        // mutator; between gates it must not wait for the collector
        // (force_collect, get_live_objects and the other queries that
        // wait for a cycle would deadlock). A cycle in flight when the
        // stepper is made runs to its end first; the destructor lets the
        // collector run on by itself.
        class stepper {
        public:
            enum class phase : int { start, flipped, registered, roots, marked, swept, released };

            explicit stepper(bool full = true) {
                detail::collector_instance().step_begin(full);
            }

            ~stepper() {
                detail::collector_instance().step_end();
            }

            stepper(const stepper&) = delete;
            stepper& operator=(const stepper&) = delete;

            // One gate: the phase the collector stands at then
            phase step() noexcept {
                return phase(int(detail::collector_instance().step()));
            }

            // Gates until the collector stands at `p`, the next one of that name
            phase advance_to(phase p) noexcept {
                phase s;
                do {
                    s = step();
                } while (s != p);
                return s;
            }

            // The rest of the current cycle, to `released`
            void finish_cycle() noexcept {
                advance_to(phase::released);
            }

            // The kind of the cycles from the next one on
            void full(bool full) noexcept {
                detail::collector_instance().step_full(full);
            }

            // This many helper threads for every pass of the cycles from
            // here on, whatever the amount of work: the parallel marking,
            // sweep, stack scan and states pass on a heap of any size; 0
            // is the policy again. Set while the collector stands at a gate.
            void helpers(unsigned n) noexcept {
                detail::collector_instance().step_helpers(n);
            }

            phase current() const noexcept {
                return phase(int(detail::collector_instance().step_gate()));
            }
        };
    };
}
