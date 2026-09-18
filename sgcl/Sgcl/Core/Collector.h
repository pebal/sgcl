//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Collector: the collector as the program sees it, a cycle on demand, the
// counts and the statistics, the memory ceiling, what holds an object,
// the collector one gate at a time for the tests of the engine.
#pragma once

#include "../../core/collector.h"

#include <utility>

namespace Sgcl {
    struct Collector {
        using InnerType = sgcl::collector;

        // The collector paused while the guard lives: what the live
        // objects and the referrers come with. Movable; Reset() ends the
        // pause early
        class PauseGuard {
        public:
            using InnerType = sgcl::collector::pause_guard;

            PauseGuard() = default;

            PauseGuard(InnerType g) noexcept
            : _g(std::move(g)) {
            }

            PauseGuard(PauseGuard&&) noexcept = default;
            PauseGuard& operator=(PauseGuard&&) noexcept = default;

            void Reset() noexcept {
                _g.reset();
            }

            explicit operator bool() const noexcept {
                return (bool)_g;
            }

            InnerType& Inner() noexcept {
                return _g;
            }

        private:
            InnerType _g;
        };

        // The counters of the collector's work, read without waiting
        struct Statistics {
            size_t Cycles;              // completed since the start
            size_t FullCycles;          // of which full (all of them unless generational)
            size_t LiveObjects;         // objects marked by the last cycle
            size_t LiveBytes;           // managed memory in use by the allocators now
            size_t CommittedBytes;      // managed memory committed now
            double LastCycleMs;         // wall time of the last cycle
            unsigned HelperThreads;     // helper threads started so far
            unsigned LastHelpersUsed;   // of which the last cycle used
            bool HelpersEnabled;        // the helpers are on for the next cycle
            double PhasesMs[8];         // the last cycle's phases: registration, states, roots, marking, updated states, sweep, page release, trim

            static Statistics From(const sgcl::collector::statistics& s) noexcept {
                Statistics r{s.cycles, s.full_cycles, s.live_objects, s.live_bytes, s.committed_bytes, s.last_cycle_ms, s.helper_threads, s.last_helpers_used, s.helpers_enabled, {}};
                for (int i = 0; i < 8; ++i) {
                    r.PhasesMs[i] = s.phases_ms[i];
                }
                return r;
            }
        };

        // The names of the eight phases of Statistics::PhasesMs
        static constexpr const char* const* PhaseNames = sgcl::collector::phase_names;

        // The live objects of one type after a cycle
        struct TypeStatistics {
            const std::type_info* Type;   // the object's type, or the array type of a buffer (typeid(T[]))
            bool Buffers;                 // true for the buffers of the containers
            size_t ObjectSize;            // bytes of one object, or of one element of a buffer
            size_t LiveObjects;           // objects (or buffers) of this type after the cycle
            size_t LiveBytes;             // their bytes: the slots they occupy
            size_t Pages;                 // pages of this type's pools; 0 for buffers
        };

        // A word that points at an object: where it is
        struct Referrer {
            enum class Kind { Object, Buffer, Stack, Cell, Unique, Weak };
            Kind From;
            const void* Holder;           // the object, buffer or block that holds the word; the word itself on a stack
            const std::type_info* Type;   // the holder's type; a buffer's element type (typeid(T[])); null for a stack word
            size_t Offset;                // the word's byte offset in the holder
            std::thread::id Thread;       // a stack word: the thread whose stack it is on
        };

        // What dies with an object
        struct Retained {
            size_t Objects;
            size_t Bytes;
        };

        // A cycle now, waited for when `wait`; false when the collector is
        // terminating. Always inline, as the functions below that count:
        // the unused stack is zeroed below the caller's frame, with no
        // frame of the wrapper's between
        SGCL_ALWAYS_INLINE static bool Collect(bool wait = false) noexcept {
            return sgcl::collector::force_collect(wait);
        }

        SGCL_ALWAYS_INLINE static size_t LiveObjectCount() {
            return sgcl::collector::get_live_object_count();
        }

        // Every live object, the collector paused while the guard lives
        SGCL_ALWAYS_INLINE static std::tuple<PauseGuard, std::vector<void*>> LiveObjects() {
            auto [guard, objects] = sgcl::collector::get_live_objects();
            return {PauseGuard(std::move(guard)), std::move(objects)};
        }

        // The unused stack below this frame zeroed: stale pointers gone
        SGCL_ALWAYS_INLINE static void ClearStack(size_t bytes = sgcl::config::StackClearSize) noexcept {
            sgcl::collector::clear_stack(bytes);
        }

        // The collector's threads gone, for the end of the program
        static void Terminate() noexcept {
            sgcl::collector::terminate();
        }

        static Statistics GetStatistics() noexcept {
            return Statistics::From(sgcl::collector::get_statistics());
        }

        SGCL_ALWAYS_INLINE static std::vector<TypeStatistics> GetTypeStatistics() {
            auto inner = sgcl::collector::get_type_statistics();
            std::vector<TypeStatistics> r;
            r.reserve(inner.size());
            for (auto& t : inner) {
                r.push_back({t.type, t.buffers, t.object_size, t.live_objects, t.live_bytes, t.pages});
            }
            return r;
        }

        static size_t CommittedMemory() noexcept {
            return sgcl::collector::get_committed_memory();
        }

        static size_t MemoryLimit() noexcept {
            return sgcl::collector::get_memory_limit();
        }

        static void SetMemoryLimit(size_t bytes) noexcept {
            sgcl::collector::set_memory_limit(bytes);
        }

        // What holds the object, the path from a root, what it keeps alive
        static std::tuple<PauseGuard, std::vector<Referrer>> GetReferrers(const void* p) {
            auto [guard, found] = sgcl::collector::get_referrers(p);
            return {PauseGuard(std::move(guard)), _referrers(found)};
        }

        static std::tuple<PauseGuard, std::vector<Referrer>> GetPathToRoot(const void* p) {
            auto [guard, found] = sgcl::collector::get_path_to_root(p);
            return {PauseGuard(std::move(guard)), _referrers(found)};
        }

        static Retained GetRetained(const void* p) {
            auto r = sgcl::collector::get_retained(p);
            return {r.objects, r.bytes};
        }

        static void Explain(const void* p, std::ostream& out) {
            sgcl::collector::explain(p, out);
        }

        // The collector one gate at a time, for the tests of the engine:
        // no cycle runs by itself while a Stepper exists; the collector
        // stands at a gate until Step() lets it through
        class Stepper {
        public:
            using InnerType = sgcl::collector::stepper;

            enum class Phase { Start, Flipped, Registered, Roots, Marked, Swept, Released };

            explicit Stepper(bool full = true)
            : _s(full) {
            }

            Stepper(const Stepper&) = delete;
            Stepper& operator=(const Stepper&) = delete;

            // One gate: the phase the collector stands at
            Phase Step() noexcept {
                return Phase(int(_s.step()));
            }

            // Gates until the collector stands at the next `p`
            Phase AdvanceTo(Phase p) noexcept {
                return Phase(int(_s.advance_to(InnerType::phase(int(p)))));
            }

            // To Released
            void FinishCycle() noexcept {
                _s.finish_cycle();
            }

            // The kind of the cycles from the next one on
            void Full(bool full) noexcept {
                _s.full(full);
            }

            // This many helper threads for every pass, whatever the work; 0 the policy
            void Helpers(unsigned n) noexcept {
                _s.helpers(n);
            }

            Phase Current() const noexcept {
                return Phase(int(_s.current()));
            }

            InnerType& Inner() noexcept {
                return _s;
            }

        private:
            InnerType _s;
        };

    private:
        static std::vector<Referrer> _referrers(const std::vector<sgcl::collector::referrer>& found) {
            std::vector<Referrer> r;
            r.reserve(found.size());
            for (auto& f : found) {
                r.push_back({Referrer::Kind(int(f.from)), f.holder, f.type, f.offset, f.thread});
            }
            return r;
        }
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

