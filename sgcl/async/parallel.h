//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A loop over indices run on the scheduler's workers: parallel_for(n, f),
// parallel_for(begin, end, f) and parallel_for(begin, end, step, f), with
// f(i) or f(i, lane); parallel_reduce(begin, end, init, map, combine); and
// parallel_for_each(range, f) over the elements of a random-access range.
// Each runs the loop inside the call, on a thread or in a task alike, and
// returns when it is done: the caller computes as lane 0 and waits at the
// end only for the chunks other lanes have under way.
#pragma once

#include "../core/aliases.h"
#include "../core/config.h"
#include "../core/detail/backoff.h"
#include "../core/detail/ticks.h"
#include "../core/dynamic_array.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "coroutine.h"
#include "scheduler.h"

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <ranges>
#include <system_error>
#include <type_traits>
#include <utility>

namespace sgcl::async {
    // How a parallel loop is spread: the number of lanes (the caller and
    // the tasks it starts on the workers; 0: scheduler::workers()) and the
    // number of indices a lane claims at a time (0: about eight claims per
    // lane, at least one index)
    struct parallel_options {
        unsigned lanes = 0;
        size_t grain = 0;
    };

    namespace detail { using namespace sgcl::detail; }
    namespace detail {
        // An index of a parallel loop: an integer of up to 64 bits, not bool
        template<class T>
        concept ParallelIndex = std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool> && sizeof(T) <= sizeof(uint64_t);

        // f(x, lane) when f takes the lane, else f(x); called through a
        // const reference, as every lane calls the same object at once
        template<class F, class A>
        concept TakesLane = std::invocable<const F&, A, unsigned>;

        template<class F, class A>
        concept ParallelBody = std::invocable<const F&, A> || std::invocable<const F&, A, unsigned>;

        template<class F, class A>
        decltype(auto) call_lane(const F& f, A&& a, unsigned lane) {
            if constexpr (TakesLane<F, A>) {
                return std::invoke(f, std::forward<A>(a), lane);
            } else {
                return std::invoke(f, std::forward<A>(a));
            }
        }

        // The claims a lane makes by default (grain 0): enough for a lane
        // that is slow (a core taken by another program, a chunk dearer
        // than the rest) to be made up for by the others, few enough that
        // the counter they share is touched a few dozen times a call
        inline constexpr uint64_t ParallelClaimsPerLane = 8;

        // At most 2^62 chunks, so that the claims past the last one (one
        // per lane, at most 2^32 lanes) never wrap the counter; a range of
        // more indices takes a grain large enough. Never reached by a
        // range that could be walked
        inline constexpr uint64_t ParallelMaxChunks = uint64_t(1) << 62;

        // The indices of a call cut into chunks, and the lanes that run them
        struct ParallelSpace {
            uint64_t count = 0;    // indices
            uint64_t grain = 1;    // indices per chunk
            uint64_t chunks = 0;   // ceil(count / grain)
            unsigned lanes = 1;    // lanes that run: at most the chunks
        };

        // The lanes asked for, or the workers; a scheduler that cannot
        // start its workers (std::system_error) leaves the caller alone
        inline unsigned parallel_lanes(const parallel_options& o) noexcept {
            if (o.lanes) {
                return o.lanes;
            }
            try {
                return scheduler_instance().workers();
            } catch (const std::system_error&) {
                return 1;
            }
        }

        inline ParallelSpace parallel_space(uint64_t count, const parallel_options& o) noexcept {
            ParallelSpace s;
            s.count = count;
            if (count <= 1) {
                s.chunks = count;
                return s;
            }
            const unsigned lanes = parallel_lanes(o);
            uint64_t g = o.grain;
            if (g == 0) {
                g = std::max<uint64_t>(1, count / (uint64_t(lanes) * ParallelClaimsPerLane));
            }
            g = std::max(g, (count - 1) / ParallelMaxChunks + 1);
            s.grain = g;
            s.chunks = (count - 1) / g + 1;
            s.lanes = (unsigned)std::min<uint64_t>(lanes, s.chunks);
            return s;
        }

        // The number of indices from begin towards end by step (step not
        // zero): up from begin below end for a positive step, down from
        // begin above end for a negative one, none when begin is past end.
        // Counted on 64 unsigned bits, where the distance between any two
        // values of T is exact
        template<class T>
        uint64_t parallel_count(T begin, T end, T step) noexcept {
            if (step > 0) {
                if (!(begin < end)) {
                    return 0;
                }
                return (uint64_t(end) - uint64_t(begin) - 1) / uint64_t(step) + 1;
            }
            if (!(end < begin)) {
                return 0;
            }
            return (uint64_t(begin) - uint64_t(end) - 1) / (uint64_t(0) - uint64_t(step)) + 1;
        }

        // What a call shares with the tasks it starts, a managed object:
        // the counter the lanes claim chunks from, the chunks left to end,
        // and the first exception. The job (the loop's body and its
        // results) stays the caller's, on its stack, and is reached through
        // a plain pointer: a lane calls it only for a chunk it claimed, and
        // the caller does not return before every claimed chunk has ended,
        // so a task that starts after the call has returned claims nothing
        // and touches only this object, which its frame holds.
        //
        // `pending` starts at the chunks plus one, the caller's: a lane
        // takes off the chunks it ran (or gave up, after a throw) once it
        // claims no more, the caller its own and the one; whoever takes it
        // to zero wakes the caller if it sleeps on the word. Every chunk's
        // writes are before its lane's decrement (release), and the caller
        // reads them after it sees zero (acquire)
        class ParallelState {
        public:
            using Run = void (*)(void* job, uint64_t chunk, uint64_t first, uint64_t last, unsigned lane);

            ParallelState(void* job, Run run, const ParallelSpace& space) noexcept
            : _job(job)
            , _run(run)
            , _count(space.count)
            , _grain(space.grain)
            , _chunks(space.chunks)
            , _pending(space.chunks + 1) {
            }

            ParallelState(const ParallelState&) = delete;
            ParallelState& operator=(const ParallelState&) = delete;

            // A lane started on the workers: its chunks, then its count off
            void lane(unsigned lane) noexcept {
                if (auto done = claim(lane)) {
                    _finish(done);
                }
            }

            // Chunks claimed and run until none is left; the chunks this
            // lane ran, and those it gave up for everyone after a throw
            uint64_t claim(unsigned lane) noexcept {
                uint64_t done = 0;
                for (;;) {
                    if (_stopped.load(std::memory_order_relaxed)) [[unlikely]] {
                        // a lane threw: the chunks nobody claimed yet are
                        // given up, by the one lane whose exchange gets them
                        auto left = _next.exchange(_chunks, std::memory_order_relaxed);
                        if (left < _chunks) {
                            done += _chunks - left;
                        }
                        return done;
                    }
                    auto c = _next.fetch_add(1, std::memory_order_relaxed);
                    if (c >= _chunks) {
                        return done;
                    }
                    const uint64_t first = c * _grain;
                    const uint64_t last = first + std::min(_grain, _count - first);
                    try {
                        _run(_job, c, first, last, lane);
                    } catch (...) {
                        _fail();
                    }
                    ++done;
                }
            }

            // The caller, its chunks run and none left to claim: its count
            // and its one off, then a wait for the chunks other lanes still
            // have under way. Such a chunk runs on another thread (a lane is
            // a task on a worker, and this thread is busy here) and waits
            // for nothing of this call, so it ends: the wait spins on the
            // word for the workers' spin time, the last chunks being mostly
            // near their end, then sleeps on it. On a worker the sleep holds
            // the worker only until those chunks end, as the computation
            // before it held it
            void wait(uint64_t done) noexcept {
                const uint64_t mine = done + 1;
                if (_pending.fetch_sub(mine, std::memory_order_seq_cst) == mine) {
                    return;
                }
                const uint64_t until = spin_until();
                detail::Backoff<32> backoff;
                do {
                    if (_pending.load(std::memory_order_acquire) == 0) {
                        return;
                    }
                    backoff();
                } while (cpu_ticks() < until);
                _sleeping.store(true, std::memory_order_seq_cst);
                for (auto v = _pending.load(std::memory_order_seq_cst); v != 0; v = _pending.load(std::memory_order_seq_cst)) {
                    _pending.wait(v, std::memory_order_seq_cst);
                }
            }

            // What a lane threw first, rethrown in the caller once all ended
            void rethrow() {
                if (_error) {
                    std::rethrow_exception(std::exchange(_error, nullptr));
                }
            }

        private:
            // The first exception kept, and every lane told to claim no more
            void _fail() noexcept {
                if (!_stopped.exchange(true, std::memory_order_acq_rel)) {
                    _error = std::current_exception();
                }
            }

            // A lane's chunks counted off; the last one wakes the caller
            // if it sleeps (this object is the lane's frame's, so it is
            // still there after the caller has returned)
            void _finish(uint64_t done) noexcept {
                if (_pending.fetch_sub(done, std::memory_order_seq_cst) == done && _sleeping.load(std::memory_order_seq_cst)) {
                    _pending.notify_all();
                }
            }

            // the claims, read by every lane at every chunk
            std::atomic<uint64_t> _next = {0};
            std::atomic<bool> _stopped = {false};
            void* const _job;
            const Run _run;
            const uint64_t _count;
            const uint64_t _grain;
            const uint64_t _chunks;
            // the count down, a line apart (written once per lane)
            [[maybe_unused]] unsigned char _pad[config::cache_line_size] = {};
            std::atomic<uint64_t> _pending;
            std::atomic<bool> _sleeping = {false};
            std::exception_ptr _error;
        };

        // A lane on the workers: a task of its own, let go of (go)
        inline task<> parallel_lane(tracked_ptr<ParallelState> state, unsigned lane) {
            state->lane(lane);
            co_return;
        }

        // The lanes past the caller's started, lane 1 to lanes - 1; a
        // scheduler that cannot start its workers ends the starting (what
        // was queued runs at its next start, and finds nothing to claim)
        inline void parallel_start(const tracked_ptr<ParallelState>& state, unsigned lanes) noexcept {
            for (unsigned lane = 1; lane < lanes; ++lane) {
                try {
                    go(parallel_lane(state, lane));
                } catch (const std::system_error&) {
                    return;
                }
            }
        }

        template<class Job>
        void parallel_run_chunk(void* job, uint64_t chunk, uint64_t first, uint64_t last, unsigned lane) {
            static_cast<Job*>(job)->run(chunk, first, last, lane);
        }

        // A call carried out where it is made, on a thread or on the worker
        // of a task alike: alone when one lane runs it, else the lanes
        // started, the caller's chunks run as lane 0, the chunks the
        // others have under way waited for, the first exception rethrown,
        // the result made
        template<class Job>
        decltype(auto) parallel_run(Job& job, const parallel_options& o) {
            const auto space = parallel_space(job.count, o);
            if (space.lanes <= 1) {
                return job.run_alone();
            }
            job.prepare(space.chunks);
            tracked_ptr<ParallelState> state = make_tracked<ParallelState>(&job, &parallel_run_chunk<Job>, space);
            parallel_start(state, space.lanes);
            state->wait(state->claim(0));
            state->rethrow();
            return job.finish();
        }

        // The index of position k: begin + k * step, on 64 unsigned bits
        // (exact modulo 2^64, and so exact for every index of the range)
        template<class T, bool Unit>
        struct ParallelIndices {
            uint64_t base = 0;
            uint64_t step = 1;

            T operator[](uint64_t k) const noexcept {
                if constexpr (Unit) {
                    return static_cast<T>(base + k);
                } else {
                    return static_cast<T>(base + k * step);
                }
            }
        };

        // parallel_for: f over the indices of a chunk
        template<class T, bool Unit, class F>
        struct ForJob {
            const F& f;
            ParallelIndices<T, Unit> at;
            uint64_t count = 0;

            void run(uint64_t, uint64_t first, uint64_t last, unsigned lane) const {
                for (uint64_t k = first; k < last; ++k) {
                    call_lane(f, at[k], lane);
                }
            }

            void run_alone() const {
                run(0, 0, count, 0);
            }

            void prepare(uint64_t) noexcept {
            }

            void finish() noexcept {
            }
        };

        // parallel_for_each: f over the elements of a chunk
        template<class R, class F>
        struct ForEachJob {
            R& range;   // the caller's argument, an lvalue here whatever it was given as
            const F& f;
            uint64_t count = 0;

            // Every lane reads begin() at once, as any number of readers may
            void run(uint64_t, uint64_t first, uint64_t last, unsigned lane) const {
                auto it = std::ranges::begin(range);
                using D = std::iter_difference_t<decltype(it)>;
                for (uint64_t k = first; k < last; ++k) {
                    call_lane(f, it[D(k)], lane);
                }
            }

            void run_alone() const {
                run(0, 0, count, 0);
            }

            void prepare(uint64_t) noexcept {
            }

            void finish() noexcept {
            }
        };

        // parallel_reduce: a chunk folded into its own partial result,
        // left to right from its first index; the partials folded into
        // init in the order of the chunks at the end, so combine meets
        // neighbours only, left before right
        template<class T, class R, class Map, class Combine>
        struct ReduceJob {
            const Map& map;
            const Combine& combine;
            R& init;
            ParallelIndices<T, true> at;
            uint64_t count = 0;
            dynamic_array<optional<R>> partials;
            optional<R>* slots = nullptr;

            R fold(R acc, uint64_t first, uint64_t last, unsigned lane) const {
                for (uint64_t k = first; k < last; ++k) {
                    R m = call_lane(map, at[k], lane);
                    acc = std::invoke(combine, std::move(acc), std::move(m));
                }
                return acc;
            }

            void run(uint64_t chunk, uint64_t first, uint64_t last, unsigned lane) const {
                R acc = call_lane(map, at[first], lane);
                slots[chunk].emplace(fold(std::move(acc), first + 1, last, lane));
            }

            R run_alone() const {
                return fold(std::move(init), 0, count, 0);
            }

            void prepare(uint64_t chunks) {
                partials = dynamic_array<optional<R>>(chunks);
                slots = partials.data();
            }

            R finish() {
                R acc = std::move(init);
                for (auto& p : partials) {
                    acc = std::invoke(combine, std::move(acc), std::move(*p));
                }
                return acc;
            }
        };

        // A body whose call cannot throw, in the form the loop calls it
        template<class F, class A>
        concept NothrowParallelBody = (TakesLane<F, A> && std::is_nothrow_invocable_v<const F&, A, unsigned>)
                                   || (!TakesLane<F, A> && std::is_nothrow_invocable_v<const F&, A>);

        // A range whose size, begin and subscript cannot throw, and a body
        // over its elements that cannot either
        template<class R, class F>
        concept NothrowParallelRange = NothrowParallelBody<F, std::ranges::range_reference_t<R>>
                                    && noexcept(std::ranges::size(std::declval<R&>()))
                                    && noexcept(std::ranges::begin(std::declval<R&>()))
                                    && noexcept(std::declval<std::ranges::iterator_t<R&>&>()[std::ranges::range_difference_t<R>()]);
    }

    // f(i), or f(i, lane), for every i of 0 .. count - 1 (none for a count
    // of zero or less), on the lanes of the options; returns when done
    template<class T, class F>
        requires detail::ParallelIndex<T> && detail::ParallelBody<F, T>
    void parallel_for(T count, F f, const parallel_options& options = {}) noexcept(detail::NothrowParallelBody<F, T>) {
        const uint64_t n = count > 0 ? uint64_t(count) : 0;
        detail::ForJob<T, true, F> job{f, {0, 1}, n};
        detail::parallel_run(job, options);
    }

    // f(i), or f(i, lane), for every i of begin .. end - 1 (none when
    // begin is not below end)
    template<class T, class F>
        requires detail::ParallelIndex<T> && detail::ParallelBody<F, T>
    void parallel_for(T begin, T end, F f, const parallel_options& options = {}) noexcept(detail::NothrowParallelBody<F, T>) {
        const uint64_t n = detail::parallel_count(begin, end, T(1));
        detail::ForJob<T, true, F> job{f, {uint64_t(begin), 1}, n};
        detail::parallel_run(job, options);
    }

    // f(i), or f(i, lane), for i = begin, begin + step, ... while i is
    // below end (step > 0) or above it (step < 0); a step of zero is
    // invalid_argument, before anything runs
    template<class T, class F>
        requires detail::ParallelIndex<T> && detail::ParallelBody<F, T>
    void parallel_for(T begin, T end, T step, F f, const parallel_options& options = {}) {
        if (step == 0) {
            throw invalid_argument("sgcl::async::parallel_for: a step of zero");
        }
        const uint64_t n = detail::parallel_count(begin, end, step);
        detail::ForJob<T, false, F> job{f, {uint64_t(begin), uint64_t(step)}, n};
        detail::parallel_run(job, options);
    }

    // f(x), or f(x, lane), for every element x of a random-access range,
    // an lvalue or an rvalue one alike: the call refers to it, and it
    // lives until the call returns
    template<class R, class F>
        requires std::ranges::random_access_range<R> && std::ranges::sized_range<R>
              && detail::ParallelBody<F, std::ranges::range_reference_t<R>>
    void parallel_for_each(R&& range, F f, const parallel_options& options = {}) noexcept(detail::NothrowParallelRange<R, F>) {
        const uint64_t n = uint64_t(std::ranges::size(range));
        detail::ForEachJob<std::remove_reference_t<R>, F> job{range, f, n};
        detail::parallel_run(job, options);
    }

    // init combined with map(i) (or map(i, lane)) of every i of begin ..
    // end - 1, in the order of the indices: the result of the left fold
    // combine(...combine(combine(init, map(begin)), map(begin + 1))...,
    // map(end - 1)), regrouped, so combine must be associative and need
    // not be commutative. map's results convert to R; init, a result of
    // map and one of combine are each an R
    template<class T, class R, class Map, class Combine>
        requires detail::ParallelIndex<T> && detail::ParallelBody<Map, T> && std::invocable<const Combine&, R, R>
    R parallel_reduce(T begin, T end, R init, Map map, Combine combine, const parallel_options& options = {}) {
        const uint64_t n = detail::parallel_count(begin, end, T(1));
        detail::ReduceJob<T, R, Map, Combine> job{map, combine, init, {uint64_t(begin), 1}, n, {}, nullptr};
        return detail::parallel_run(job, options);
    }

    // The same with combine `a + b`
    template<class T, class R, class Map>
        requires detail::ParallelIndex<T> && detail::ParallelBody<Map, T>
    R parallel_reduce(T begin, T end, R init, Map map, const parallel_options& options = {}) {
        return parallel_reduce(begin, end, std::move(init), std::move(map), std::plus<>(), options);
    }
}
