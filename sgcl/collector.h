//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/collector.h"

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
    };
}
