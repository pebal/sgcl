//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "child_pointers.h"
#include "states.h"
#include "thread.h"
#include "types.h"

#include <algorithm>
#include <condition_variable>
#include <functional>

#if SGCL_LOG_PRINT_LEVEL > 0
#include <iomanip>
#include <iostream>
#include <unordered_map>
#endif

namespace sgcl::detail {
    class Collector {
    public:
        using PauseGuard = std::unique_ptr<std::mutex, std::function<void(std::mutex*)>>;

        Collector() {
            if (!created()) {
                _created = true;
                // The heap's range is read by the scan on the collector
                // thread: create it before that thread exists.
                Heap::instance();
                // The frames that set the heap up leave its base address
                // and values near it in the dead part of this stack; scanned
                // later, they would retain whatever lands on the first pages.
                _clear_frames_below();
                std::thread([this]{_main_loop();}).detach();
            }
        }

        ~Collector() {
            _terminate();
        }

        void waking_up() {
            sleep_flag.store(false, std::memory_order_release);
            sleep_cv.notify_one();
        }

        bool force_collect(bool wait) noexcept {
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] force collect " << (wait ? "and wait " : "") << "from id: " << std::this_thread::get_id() << std::endl;
#endif
            if (wait) {
                std::unique_lock<std::mutex> lock(_mutex);
                if (_terminating) {
                    return false;
                }
                auto generation = _report_generation;
                _force_collect();
                _cv_data_ready.wait(lock, [this, generation] {
                    return _report_generation != generation || _terminating;
                });
                return _report_generation != generation;
            }
            _force_collect();
            return true;
        }

        // Two young cycles (the current one may be half done), for the
        // tests of the generational mode. Memory pressure still forces
        // full cycles.
        bool collect_young(bool wait) noexcept {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_terminating) {
                return false;
            }
            auto generation = _report_generation;
            auto count = _young_collect_count.load(std::memory_order_relaxed);
            while (count < 2 && !_young_collect_count.compare_exchange_weak(count, 2, std::memory_order_release, std::memory_order_relaxed)) {
            }
            waking_up();
            if (wait) {
                _cv_data_ready.wait(lock, [this, generation] {
                    return _report_generation != generation || _terminating;
                });
                return _report_generation != generation;
            }
            return true;
        }

        std::tuple<PauseGuard, std::vector<void*>> get_live_objects() noexcept {
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] get live objects from id: " << std::this_thread::get_id() << std::endl;
            std::flush(std::cout);
#endif
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_terminating) {
                auto generation = _report_generation;
                _live_objects_request.store(true, std::memory_order_release);
                _force_collect();
                _cv_data_ready.wait(lock, [this, generation] {
                    return _report_generation != generation || _terminating;
                });
            }
            lock.release();
            return {
                PauseGuard(&_mutex, [this](std::mutex* mutex) {
                    _dataProcessed = true;
                    _cv_data_processed.notify_one();
                    mutex->unlock();

                })
              , std::move(_live_objects)
            };
        }

        void force_short_sleep() {
            _short_sleep = true;
        }

        void static terminate() noexcept {
            if (created()) {
                collector_instance()._terminate();
            }
        }

        inline static bool terminated() noexcept {
            return _terminating.load();
        }

        inline static bool created() {
            return _created.load(std::memory_order_acquire);
        }

        // Stack scans that ran on the helpers so far (tests, diagnostics).
        size_t parallel_stack_scans() const noexcept {
            return _parallel_stack_scans.load(std::memory_order_relaxed);
        }

        // Marking passes that ran on the helpers so far (tests, diagnostics).
        size_t parallel_mark_runs() const noexcept {
            return _parallel_mark_runs.load(std::memory_order_relaxed);
        }

        // The live objects and bytes by type after the cycle that served a
        // get_live_objects() request: valid while that request's pause
        // guard is held (sgcl::collector::get_type_statistics).
        struct TypeStatistics {
            const std::type_info* type;
            bool buffers;
            size_t object_size;
            size_t live_objects;
            size_t live_bytes;
            size_t pages;
        };

        const std::vector<TypeStatistics>& type_statistics() const noexcept {
            return _type_statistics;
        }

        // After the marking, before the sweep: the marked registered slots
        // of every page, by the page's type; a buffer by the element type
        // in its header, with the page's slot size as its bytes (the pages
        // of buffers belong to size classes, so they are not attributed).
        void _collect_type_statistics() {
            std::unordered_map<const std::type_info*, TypeStatistics> objects;
            std::unordered_map<const std::type_info*, TypeStatistics> buffers;
            for (auto page : _pages) {
                if (!page->is_used) {
                    continue;
                }
                auto metadata = page->metadata;
                auto flags = page->flags();
                auto count = page->flags_count();
                if (metadata->is_array) {
                    for (unsigned i = 0; i < count; ++i) {
                        auto live = flags[i].registered & flags[i].marked;
                        while (live) {
                            auto index = i * Page::FlagBitCount + std::countr_zero(live);
                            live &= live - 1;
                            auto array = (ArrayBase*)page->pointer_of(index);
                            auto element = array->metadata;
                            auto& entry = buffers[&element->type_info];
                            entry.type = &element->type_info;
                            entry.buffers = true;
                            entry.object_size = element->object_size;
                            ++entry.live_objects;
                            entry.live_bytes += metadata->pool_allocated ? metadata->object_size : page->page_count * config::PageSize;
                        }
                    }
                } else {
                    auto& entry = objects[&metadata->type_info];
                    entry.type = &metadata->type_info;
                    entry.buffers = false;
                    entry.object_size = metadata->object_size;
                    entry.pages += page->page_count;
                    for (unsigned i = 0; i < count; ++i) {
                        entry.live_objects += std::popcount(flags[i].registered & flags[i].marked);
                    }
                    entry.live_bytes = entry.live_objects * metadata->object_size;
                }
            }
            _type_statistics.clear();
            for (auto& [type, entry] : objects) {
                _type_statistics.push_back(entry);
            }
            for (auto& [type, entry] : buffers) {
                _type_statistics.push_back(entry);
            }
            std::sort(_type_statistics.begin(), _type_statistics.end(), [](const TypeStatistics& a, const TypeStatistics& b) {
                return a.live_bytes != b.live_bytes ? a.live_bytes > b.live_bytes : a.live_objects > b.live_objects;
            });
        }

        static constexpr int PhaseCount = 8;

        double phase_ms(int i) const noexcept {
            return _stats_phase_ms[i].load(std::memory_order_relaxed);
        }

        // sgcl::collector::statistics; every field a relaxed load of a
        // counter the collector thread stores at the end of a cycle.
        template<class Statistics>
        Statistics statistics() const noexcept {
            return Statistics {
                .cycles = _stats_cycles.load(std::memory_order_relaxed),
                .full_cycles = _stats_full_cycles.load(std::memory_order_relaxed),
                .live_objects = _last_live_object_count.load(std::memory_order_relaxed),
                .live_bytes = MemoryCounters::live_bytes(),
                .committed_bytes = Heap::instance().committed_bytes(),
                .last_cycle_ms = _stats_last_cycle_ms.load(std::memory_order_relaxed),
                .helper_threads = _stats_helper_threads.load(std::memory_order_relaxed),
                .last_helpers_used = _stats_last_helpers_used.load(std::memory_order_relaxed),
                .helpers_enabled = _stats_helpers_enabled.load(std::memory_order_relaxed)
            };
        }

        inline static void delete_unique(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            auto index = page->index_of(p);
            _destroy(page, page->pointer_of(index));
            page->states()[index].store(State::Destroyed, std::memory_order_release);
        }

    private:
        // Registration protocol: mutators only push (a CAS loop on the head),
        // the collector takes the whole list with one exchange. Nothing is
        // ever compared against a remembered node, so a freed and reused
        // address cannot be mistaken for an old one (no ABA guard needed).
        void _register_threads() {
            auto thread = Thread::threads_data.exchange(nullptr, std::memory_order_acq_rel);
            while(thread) {
                auto next = thread->next;
                thread->next_registered = _registered_threads;
                _registered_threads = thread;
                thread = next;
            }
        }

        void _register_pages() {
            Thread::Data* prev = nullptr;
            auto thread = _registered_threads;
            while(thread) {
                auto next = thread->next_registered;
                // is_deleted is the thread's last store (release), so after
                // seeing it every page it ever published is on its list
                bool thread_deleted = thread->is_deleted.load(std::memory_order_acquire);
                _register_pages(thread->pages);
                if (thread_deleted) {
                    if (!prev) {
                        _registered_threads = next;
                    } else {
                        prev->next_registered = next;
                    }
                    delete thread;
                } else {
                    prev = thread;
                }
                thread = next;
            }
        }

        // The registered pages are an array (_pages): the passes over them
        // index it, split it by arithmetic and read the headers in order,
        // where a list cost a dependent load per header and a walk to
        // count. Appended here, compacted in _release_unused_pages.
        void _register_pages(std::atomic<Page*>& pages) {
            auto page = pages.exchange(nullptr, std::memory_order_acq_rel);
            while(page) {
                auto next = page->next;
                _pages.push_back(page);
                if (page->metadata->is_weak_cell) {
                    _weak_pages.push_back(page);
                }
                page = next;
            }
        }

        // Registers the objects created on the page before the cycle began:
        // the unregistered slots whose state says created, except the ones
        // Fresh with the current parity, which were handed to their first
        // tracked_ptr after the flip of the epoch. Those stay unregistered
        // this cycle (not swept, not traced): their stores read the new
        // epoch, so what they point to is reachable by state; registering
        // them would make them roots for nothing, a million a cycle with
        // four threads allocating (DESIGN, young cycles). The page keeps
        // its object_created flag for them. The page's state_updated flag
        // is lowered here (the demotion pass used to): the pass over all
        // the states at the start of the marking sees every state set
        // before this point, the later ones raise the flag again. The
        // slots past the last object are masked out of the last word; the
        // states are read relaxed, the run this is part of fenced on entry.
        static size_t _register_page(Page* page) noexcept {
            size_t objects_created = 0;
            if (page->is_used) {
                if (page->state_updated.load(std::memory_order_relaxed)) {
                    page->state_updated.exchange(false, std::memory_order_acq_rel);
                    page->retire = true;   // states of the other parity to retire (_update_page_marks<true>)
                }
                if (page->object_created.load(std::memory_order_relaxed) && page->object_created.exchange(false, std::memory_order_acq_rel)) {
                    auto states = page->states();
                    auto flags = page->flags();
                    auto count = page->flags_count();
                    auto object_count = page->metadata->object_count;
                    auto tail = object_count % Page::FlagBitCount;
                    auto last_valid = tail ? (Page::Flag(1) << tail) - 1 : ~Page::Flag(0);
                    auto after_flip_state = State(Page::reachable_state() | State::Fresh);
                    bool updated = false;
                    bool skipped = false;
                    for (unsigned i = 0; i < count; ++i) {
                        auto& flag = flags[i];
                        auto unregistered = ~flag.registered;
                        if (i == count - 1) {
                            unregistered &= last_valid;
                        }
                        auto offset = i * Page::FlagBitCount;
                        auto registered = flag.registered;
                        // eight states per step (states.h), the groups
                        // without an unregistered slot skipped
                        for (unsigned g = 0; g < Page::FlagBitCount; g += 8) {
                            auto group = (unregistered >> g) & 0xFF;
                            if (!group) {
                                continue;
                            }
                            auto w = States8::load(states + offset + g);
                            auto created = States8::created(w) & group;
                            auto after_flip = States8::equal(w, after_flip_state) & created;
                            auto fresh = created & ~after_flip;
                            skipped |= after_flip != 0;
                            registered |= Page::Flag(fresh) << g;
                            updated |= (States8::equal(w, State::UniqueLock) & fresh) != 0;   // a root by state: the pass over the states must look here
                            objects_created += std::popcount(fresh);
                        }
                        flag.registered = registered;
                    }
                    if (updated) {
                        page->state_updated.store(true, std::memory_order_relaxed);
                    }
                    if (skipped) {
                        page->object_created.store(true, std::memory_order_release);
                    }
                }
            }
            return objects_created;
        }

        // Registration of the objects created before the cycle, and the
        // clearing of the flags of the previous one, in one pass over the
        // pages. No demotion of states: the flip of the epoch did that.
        size_t _register_objects() noexcept {
            auto full = _full;
            auto counts = _parallel_array<size_t>(_pages.size(), [this, full](size_t begin, size_t end) {
                std::atomic_thread_fence(std::memory_order_acquire);
                size_t created = 0;
                for (auto i = begin; i < end; ++i) {
                    auto page = _pages[i];
                    created += _register_page(page);
                    page->clear_flags(full);
                }
                return created;
            });
            size_t created = 0;
            for (auto c : counts) {
                created += c;
            }
            return created;
        }

        void _update_hazard_pointers() {
            _hazard_pointers.clear();
            auto thread = _registered_threads;
            while(thread) {
                auto pointer = thread->hazard_pointer.load(std::memory_order_acquire);
                if (pointer) {
                    _hazard_pointers.emplace_back((uintptr_t)pointer);
                }
                thread = thread->next_registered;
            }
        }

        // The hazard pointers (a handful) as roots, after a pass over the
        // pages: resolved one by one instead of searched for on every page.
        void _mark_hazard_pointers() noexcept {
            for (auto p : _hazard_pointers) {
#ifdef SGCL_TRACE_STACK
                if (_mark_pointer((const void*)p)) {
                    std::fprintf(stderr, "[hazard] %p\n", (void*)p);
                }
#else
                _mark_pointer((const void*)p);
#endif
            }
        }

        // One pointer as a root: the registered, unmarked object it
        // addresses is made reachable, its page queued for the next pass
        // (same rule as _mark_slot(): unregistered slots are invisible).
        // Returns whether it was.
        bool _mark_pointer(const void* p) noexcept {
            auto page = Heap::page_of_checked(p);
            if (!page) {
                return false;
            }
            auto index = page->index_of(p);
            if (index >= page->metadata->object_count) {
                return false;
            }
            auto& flag = page->flags()[Page::flag_index_of(index)];
            auto mask = Page::flag_mask_of(index);
            if ((flag.registered & mask) && !(flag.marked & mask)) {
                flag.reachable |= mask;
                if (!page->reachable) {
                    page->reachable = true;
                    page->next_reachable = _reachable_pages;
                    _reachable_pages = page;
                }
                return true;
            }
            return false;
        }

        // The object at `p` is registered and the cycle has not marked it:
        // garbage once the marking has converged (the rule of
        // _mark_hazard_pointers and Page::dying). False for anything else:
        // a word outside the heap, an array, a slot created during the
        // cycle (unregistered slots are never swept).
        static bool _unmarked_object(const void* p) noexcept {
            auto page = Heap::page_of_checked(p);
            if (!page) {
                return false;
            }
            auto index = page->index_of(p);
            if (index >= page->metadata->object_count) {
                return false;
            }
            auto& flag = page->flags()[Page::flag_index_of(index)];
            auto mask = Page::flag_mask_of(index);
            return (flag.registered & mask) && !(flag.marked & mask);
        }

        // The weak phase (weak_cell.h; weak_ptr.h, expiry_queue.h), run
        // every time the marking has converged, over every allocated cell
        // on the pages of the cell type. Every allocated cell counts,
        // registered or not: a cell created during the cycle may hold a
        // target that dies in it (the mutator made it from a strong pointer
        // it has dropped since). The cells the cycle found unreachable are
        // skipped: they are garbage themselves, and keep nothing. Two steps:
        // - the keeping (_keep_watched_targets): the target of a cell that
        //   an expiry_queue watches, found unreachable, is made reachable
        //   instead of cleared, once with the Expired flag set for the
        //   queue and then every cycle until the queue has drained the
        //   entry (Drained). The marking goes on with what it made
        //   reachable and comes back here when it has converged again; the
        //   clearing waits for a round that kept nothing.
        // - the clearing (_clear_weak_cells): every other cell whose target
        //   the cycle found unreachable has its word cleared, so that no
        //   weak_ptr can hand out the object the sweep destroys or the slot
        //   it will be reused for.
        // Returns whether anything was cleared: then the hazard pointers
        // are read again, after the fence, and the states are passed over
        // once more, for a lock() that raced with the clearing. It published
        // its hazard before re-reading the cell (weak_ptr::lock: a seq_cst
        // store, then a seq_cst load) and the clearing is a seq_cst
        // exchange followed by a seq_cst fence before the hazards are read,
        // so either the lock saw the null or its hazard is seen here and
        // the target is marked by the pass that follows; a lock that got
        // its pointer and dropped the hazard already has set the state of
        // the target with the tracked_ptr it built, which the same pass
        // finds. The cell stays clear then: the target was reachable only
        // through weak pointers when the cycle looked, and the one lock
        // that won the race holds it strongly now.
        // A cell was written before its slot was published
        // (make_tracked_before_publish), ordered before the reads here by
        // the release store of the state and the acquire fence this thread
        // passed since; the thread sanitizer does not follow that fence
        // and would report the constructor's store against the reads, so
        // they are hidden from it, as in _mark_array_childs.
        enum class WeakPhase { Nothing, Kept, Cleared };

        WeakPhase _weak_phase() noexcept {
            if (_keep_watched_targets()) {
                return WeakPhase::Kept;
            }
            return _clear_weak_cells() ? WeakPhase::Cleared : WeakPhase::Nothing;
        }

        // The allocated cells that are not garbage in this cycle
        template<class F>
        void _for_each_weak_cell(F&& f) noexcept {
            for (auto page : _weak_pages) {
                if (!page->is_used) {
                    continue;
                }
                auto states = page->states();
                auto flags = page->flags();
                auto count = page->metadata->object_count;
                for (unsigned i = 0; i < count; ++i) {
                    if (states[i].load(std::memory_order_relaxed) & State::FreeMask) {   // a free slot
                        continue;
                    }
                    auto& flag = flags[Page::flag_index_of(i)];
                    auto mask = Page::flag_mask_of(i);
                    if ((flag.registered & mask) && !(flag.marked & mask)) {   // a dead cell
                        continue;
                    }
                    f((WeakCell*)page->pointer_of(i));
                }
            }
        }

        SGCL_NO_SANITIZE bool _keep_watched_targets() noexcept {
            bool kept = false;
            _for_each_weak_cell([&](WeakCell* cell) {
                auto flags = cell->flags.load(std::memory_order_acquire);
                if (!(flags & WeakCell::Watched) || (flags & WeakCell::Drained)) {
                    return;
                }
                auto target = cell->target.load(std::memory_order_acquire);
                if (target && _mark_pointer(target)) {
                    kept = true;
                    if (!(flags & WeakCell::Expired)) {
                        cell->flags.fetch_or(WeakCell::Expired, std::memory_order_acq_rel);
                    }
                }
            });
            return kept;
        }

        SGCL_NO_SANITIZE bool _clear_weak_cells() noexcept {
            bool cleared = false;
            _for_each_weak_cell([&](WeakCell* cell) {
                auto target = cell->target.load(std::memory_order_acquire);
                if (target && _unmarked_object(target)) {
                    cleared |= cell->target.compare_exchange_strong(target, nullptr, std::memory_order_seq_cst);
                }
            });
            if (cleared) {
                std::atomic_thread_fence(std::memory_order_seq_cst);
            }
            return cleared;
        }

        // An array is referenced only through a pointer to its first element
        // (what a container holds): a word pointing elsewhere into it, at an
        // element or into the header, is not a root. Pointers into other
        // objects count wherever they point (base subobjects, members).
        static bool _is_array_start(Page* page, unsigned index, const void* ptr) noexcept {
            return (uintptr_t)ptr == (uintptr_t)page->pointer_of(index) + sizeof(ArrayBase);
        }

        // The state of one marking thread. Sequential marking (this thread
        // alone) uses _markers[0]; the parallel pass gives every helper one.
        // `work` is the stack of objects found but not traced yet, in the
        // parallel pass only: the sequential pass queues pages instead
        // (reachable bits, _mark_reachable).
        struct MarkItem {
            Page* page;
            void* ptr;
        };

        // A cache line of its own: the workers push, pop and count on their
        // markers at every object.
        struct alignas(config::CacheLineSize) Marker {
            std::vector<MarkItem> work;
            std::vector<void*> objects;   // the live objects, when requested
            size_t live = 0;              // objects marked for the first time
            bool unregistered_hit = false;
        };

        void _found(Marker& m, void* ptr) noexcept {
            ++m.live;
            if (_share_live_objects) {
                m.objects.push_back(ptr);
            }
        }

        // -DSGCL_TRACE_STACK prints every marking decision to stderr: which
        // stack word retained what, which states were still Reachable.
        // Sequential: the slot gets its reachable bit and the page goes on
        // the list for _mark_reachable to trace. Parallel: the marked bit is
        // the only word the threads share; a relaxed load first, so that an
        // object marked already costs no atomic operation, then fetch_or,
        // whose old value says which thread got the object and traces it.
        template<bool Parallel>
        void _mark_slot(Page* page, unsigned index, Marker& m) noexcept {
            auto flag_index = Page::flag_index_of(index);
            auto mask = Page::flag_mask_of(index);
            auto& flag = page->flags()[flag_index];
#ifdef SGCL_TRACE_STACK
            std::fprintf(stderr, "[mark] %p type %s registered %d marked %d reachable %d state %d\n", page->pointer_of(index), page->metadata->type_info.name(), !!(flag.registered & mask), !!(flag.marked & mask), !!(flag.reachable & mask), (int)page->states()[index].load());
#endif
            if (!(flag.registered & mask)) {
                m.unregistered_hit = true;
                return;
            }
            if constexpr(Parallel) {
                std::atomic_ref<Page::Flag> marked(flag.marked);
                if (marked.load(std::memory_order_relaxed) & mask) {
                    return;
                }
                if (marked.fetch_or(mask, std::memory_order_relaxed) & mask) {
                    return;
                }
                auto ptr = page->pointer_of(index);
                _found(m, ptr);
                m.work.push_back({page, ptr});
            } else {
                if ((~flag.marked & ~flag.reachable & mask)) {
                    flag.reachable |= mask;
                    if (!page->reachable) {
                        page->reachable = true;
                        page->next_reachable = _reachable_pages;
                        _reachable_pages = page;
                    }
                }
            }
        }

        // Conservative roots: every used page of every live thread's stack,
        // word by word. A word is a root when it points into a registered
        // slot of the managed heap; anything else is ignored. The thread
        // keeps running meanwhile: a pointer it copies during the scan is
        // covered by the write barrier (state Reachable on the target), the
        // same argument as for pointers stored into objects.
        struct StackSegment {
            uintptr_t begin;
            uintptr_t end;
        };

        // Conservative roots: every used page of every live thread's stack,
        // word by word. A word is a root when it points into a registered
        // slot of the managed heap; anything else is ignored. The thread
        // keeps running meanwhile: a pointer it copies during the scan is
        // covered by the write barrier (state Reachable on the target), the
        // same argument as for pointers stored into objects.
        //
        // First the used pages of every stack are found (one mincore or
        // pagemap query per thread) and cut into segments. Small total: this
        // thread scans and marks them as before. Large total: the helpers
        // read the segments and collect the words that point into the heap,
        // this thread marks the collected words afterwards. The helpers only
        // read (stacks, heap tables), so nothing is shared while they run.
        void _mark_stack_roots() noexcept {
            _stack_segments.clear();
            _scanned_threads.clear();
            size_t bytes = 0;
            for (auto thread = _registered_threads; thread; thread = thread->next_registered) {
                if (thread->is_deleted.load(std::memory_order_acquire)) {
                    continue;
                }
                thread->stack_scan.store(true, std::memory_order_seq_cst);
                if (thread->exiting.load(std::memory_order_seq_cst)) {
                    thread->stack_scan.store(false, std::memory_order_release);
                    continue;
                }
                _scanned_threads.push_back(thread);
                bytes += _stack_segments_of(thread->stack_begin, thread->stack_end);
            }
            auto workers = _pool.workers(bytes, config::StackScanThreshold, true);
            if (!workers) {
                for (auto& segment : _stack_segments) {
                    _scan_words(segment.begin, segment.end);
                }
            } else {
                std::vector<std::vector<const void*>> found(workers + 1);
                auto run = _stack_segments.size() / (workers + 1);
                std::function<void(unsigned)> fn = [&](unsigned w) {
                    _collect_words(w * run, (w + 1) * run, found[w]);
                };
                _parallel_stack_scans.fetch_add(1, std::memory_order_relaxed);
                _pool.start(workers, &fn);
                _collect_words(workers * run, _stack_segments.size(), found[workers]);
                _pool.wait();
                for (auto& words : found) {
                    for (auto word : words) {
                        _mark_conservative<false>(word, _markers[0]);
                    }
                }
            }
            for (auto thread : _scanned_threads) {
                thread->stack_scan.store(false, std::memory_order_release);
            }
        }

        // Appends the used part of one stack to _stack_segments, in pieces
        // of at most config::StackScanSegment bytes; returns the bytes added.
        size_t _stack_segments_of(uintptr_t begin, uintptr_t end) noexcept {
            begin = (begin + sizeof(uintptr_t) - 1) & ~(uintptr_t)(sizeof(uintptr_t) - 1);
            end &= ~(uintptr_t)(sizeof(uintptr_t) - 1);
            if (begin >= end) {
                return 0;
            }
            size_t bytes = 0;
            auto add = [&](uintptr_t from, uintptr_t to) {
                bytes += to - from;
                for (; from < to; from += config::StackScanSegment) {
                    _stack_segments.push_back({from, std::min(to, from + config::StackScanSegment)});
                }
            };
            auto page = os::touched_pages((void*)begin, end - begin, _touched_pages);
#ifdef SGCL_TRACE_STACK
            size_t touched = 0;
            for (auto t : _touched_pages) touched += t != 0;
            std::fprintf(stderr, "[scan] stack %p..%p page %zu touched %zu of %zu\n", (void*)begin, (void*)end, page, touched, _touched_pages.size());
#endif
            if (!page) {
                add(begin, end);
                return bytes;
            }
            auto first = begin & ~(page - 1);
            size_t i = 0;
            while (i < _touched_pages.size()) {
                if (!_touched_pages[i]) {
                    ++i;
                    continue;
                }
                auto j = i;
                while (j < _touched_pages.size() && _touched_pages[j]) {
                    ++j;
                }
                add(std::max(begin, first + i * page), std::min(end, first + j * page));
                i = j;
            }
            return bytes;
        }

        // Helper side of the scan: the words of segments [first, last) that
        // point at a page of the heap, without marking anything.
        void _collect_words(size_t first, size_t last, std::vector<const void*>& found) noexcept {
            for (auto i = first; i < last; ++i) {
                os::scan_heap_words(_stack_segments[i].begin, _stack_segments[i].end, Heap::base(), Heap::size(), [&](uintptr_t word) {
                    if (Heap::page_of_checked((const void*)word)) {
                        found.push_back((const void*)word);
                    }
                });
            }
        }

        void _scan_words(uintptr_t begin, uintptr_t end) noexcept {
            os::scan_heap_words(begin, end, Heap::base(), Heap::size(), [&](uintptr_t word) {
#ifdef SGCL_TRACE_STACK
                if (Heap::page_of_checked((const void*)word)) std::fprintf(stderr, "[stack] -> %p\n", (void*)word);
#endif
                _mark_conservative<false>((const void*)word, _markers[0]);
            });
        }

        // A word that may or may not point at an object: checks the page and
        // the slot before marking. Pages are freed and headers released only
        // by the collector thread, outside marking, so a header found here
        // stays valid.
        template<bool Parallel>
        void _mark_conservative(const void* ptr, Marker& m) noexcept {
            auto page = Heap::page_of_checked(ptr);
            if (!page) {
                return;
            }
            auto index = page->index_of(ptr);
            if (index >= page->metadata->object_count) {
                return;
            }
            if (page->metadata->is_array && !_is_array_start(page, index, ptr)) {
                return;
            }
            _mark_slot<Parallel>(page, index, m);
        }

        // Follows the candidate pointer words of one object (page_info.h:
        // ChildPointers). Zero: nothing. A managed address: marked with the
        // same checks as a stack word (page, slot, registration). Anything
        // else: the offset is data, removed from the map for good, unless
        // the type is conservative (a coroutine frame: the same offset is
        // a pointer in another frame).
        template<bool Parallel>
        void _mark_childs(ChildPointers& childs, void* ptr, Marker& m) noexcept {
            for (size_t w = 0; w < childs.map.size(); ++w) {
                auto bits = childs.word(w);
                while (bits) {
                    auto offset = w * 64 + std::countr_zero(bits);
                    bits &= bits - 1;
                    auto word = os::load_word((RawPointer*)ptr + offset);
                    if (!word) {
                        continue;
                    }
                    if (Heap::contains((const void*)word)) {
                        _mark_conservative<Parallel>((const void*)word, m);
                    } else if (!childs.conservative) {
                        childs.remove(offset);
                    }
                }
            }
#if !defined(NDEBUG)
            _check_removed_offsets(childs, ptr);
#endif
        }

#if !defined(NDEBUG)
        // Rule 2 diagnostic: a word at an offset already classified as data
        // holds a pointer to a live object. Either a tracked_ptr shares its
        // storage with data (a union, std::variant, an inline buffer), which
        // the collector cannot follow, or an integer happens to hold an
        // address. Reported once per type.
        void _check_removed_offsets(ChildPointers& childs, void* ptr) noexcept {
            if (childs.warned.load(std::memory_order_relaxed)) {
                return;
            }
            for (size_t w = 0; w < childs.removed.size(); ++w) {
                auto bits = childs.removed[w].load(std::memory_order_relaxed);
                while (bits) {
                    auto offset = w * 64 + std::countr_zero(bits);
                    bits &= bits - 1;
                    auto word = (const void*)os::load_word((RawPointer*)ptr + offset);
                    auto page = Heap::page_of_checked(word);
                    if (page && _is_registered(page, word)) {
                        childs.warned.store(true, std::memory_order_relaxed);
                        std::fprintf(stderr, "[sgcl] type %s: the word at byte offset %zu was classified as data but holds a pointer to a managed object; a tracked_ptr sharing storage with data is not supported\n", childs.type.name(), offset * sizeof(RawPointer));
                        return;
                    }
                }
            }
        }
#endif

        // Slot state of the object a word points at.
        static bool _is_registered(Page* page, const void* p) noexcept {
            auto index = page->index_of(p);
            if (index >= page->metadata->object_count) {
                return false;
            }
            return page->flags()[Page::flag_index_of(index)].registered & Page::flag_mask_of(index);
        }

        // The header of the buffer was written before its slot was published
        // and is ordered before the reads here by the release store of the
        // slot's state and the acquire fence this run started with
        // (object_pool_allocator_base.h, alloc). The thread sanitizer does
        // not follow that fence and would report the plain reads of the
        // header as a race: hidden from it, not synchronized for it.
        template<bool Parallel>
        SGCL_NO_SANITIZE void _mark_array_childs(void* ptr, Marker& m) noexcept {
            auto data = (uintptr_t)ptr;
            auto array = (ArrayBase*)data;
            auto metadata = array->metadata;
            if (!metadata) {
                return;
            }
            auto& pointers = metadata->child_pointers;
            if (!pointers.any.load(std::memory_order_relaxed)) {
                return;
            }
            // every slot up to the capacity: the buffer was zeroed, so an
            // unconstructed element holds null pointers
            auto object_size = metadata->object_size;
            data += sizeof(ArrayBase);
            for (size_t i = 0; i < array->capacity; ++i, data += object_size) {
                _mark_childs<Parallel>(pointers, (void*)data, m);
            }
        }

        // Follows the children of one object. A child that is not registered
        // yet (created during this cycle, or a store the registration has not
        // seen) will be registered and demoted in the next cycle with this
        // object possibly its only referrer: the page is carded so that the
        // next young cycle traces this object again.
        template<bool Parallel>
        void _trace(Page* page, void* ptr, bool is_array, Marker& m) noexcept {
            m.unregistered_hit = false;
            if (is_array) {
                _mark_array_childs<Parallel>(ptr, m);
            } else {
                _mark_childs<Parallel>(page->metadata->child_pointers, ptr, m);
            }
            if (m.unregistered_hit) {
                Heap::stamp_card(ptr, _epoch);
            }
        }

        // Young cycle: the marked objects are not traced from the roots, so
        // the pointers stored into them since the previous cycle are found
        // here, by tracing every marked object on a carded page (page.h:
        // mark_card). Their old children are marked already and cost one
        // flag test each; the young ones get queued like any root.
        // The dirty pages are collected here; traced on this thread when
        // the cycle marks alone, dealt out to the workers by _mark_parallel
        // otherwise (the parallel pass takes them before the reachable
        // pages, each object traced at once and its finds drained).
        // Returns the objects to trace, for the decision on the workers.
        size_t _collect_dirty_pages() noexcept {
            _dirty_pages.clear();
            size_t objects = 0;
            for (auto page : _pages) {
                if (page->is_used && page->dirty_since(_epoch)) {
#ifdef SGCL_TRACE_STACK
                    std::fprintf(stderr, "[dirty] page %p epoch %u\n", (void*)page->data, _epoch);
#endif
                    _dirty_pages.push_back(page);
                    auto flags = page->flags();
                    auto count = page->flags_count();
                    for (unsigned i = 0; i < count; ++i) {
                        objects += std::popcount(flags[i].registered & flags[i].marked);
                    }
                }
            }
            return objects;
        }

        template<bool Parallel>
        void _trace_dirty_page(Page* page, Marker& m) noexcept {
            auto flags = page->flags();
            auto is_array = page->metadata->is_array;
            auto count = page->flags_count();
            for (unsigned i = 0; i < count; ++i) {
                // the marks are being set by the other helpers meanwhile
                // (an object marked twice over is traced twice, no worse)
                auto old = flags[i].registered & std::atomic_ref<Page::Flag>(flags[i].marked).load(std::memory_order_relaxed);
                auto offset = i * Page::FlagBitCount;
                while (old) {
                    auto index = offset + std::countr_zero(old);
                    old &= old - 1;
                    _trace<Parallel>(page, page->pointer_of(index), is_array, m);
                    if constexpr(Parallel) {
                        _drain(m);
                    }
                }
            }
        }

        void _trace_dirty_pages() noexcept {
            for (auto page : _dirty_pages) {
                _trace_dirty_page<false>(page, _markers[0]);
            }
            _dirty_pages.clear();
        }

        // Traces the objects whose reachable bit is set, on the pages listed
        // in _reachable_pages, and everything they lead to.
        void _mark_reachable() noexcept {
#ifdef SGCL_MARK_STATS
            if (_mark_workers || _mark_force) {
#else
            if (_mark_workers) {
#endif
                _mark_parallel();
                return;
            }
            auto& m = _markers[0];
#ifdef SGCL_MARK_STATS
            auto t0 = std::chrono::steady_clock::now();
            auto live0 = m.live;
            struct Report {
                std::chrono::steady_clock::time_point t0; size_t live0; Marker& m;
                ~Report() { std::fprintf(stderr, "[mark] sequential time %.2fms live %zu\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), m.live - live0); }
            } report{t0, live0, m};
#endif
            auto page = _reachable_pages;
            _reachable_pages = nullptr;
            while(page) {
                auto metadata = page->metadata;
                const bool is_array = metadata->is_array;
                auto flags = page->flags();
                auto count = page->flags_count();
                bool marked;
                do {
                    marked = false;
                    for (unsigned i = 0; i < count; ++i) {
                        auto& flag = flags[i];
                        while (flag.reachable) {
                            auto reachable = flag.reachable;
                            auto offset = i * Page::FlagBitCount;
                            while(reachable) {
                                auto countr_zero = std::countr_zero(reachable);
                                auto mask = Page::Flag(1) << countr_zero;
                                // a large array is re-queued when more of its
                                // pages become live; count it once
                                bool first = !(flag.marked & mask);
                                flag.marked |= mask;
                                auto index = offset + countr_zero;
                                auto ptr = page->pointer_of(index);
                                _trace<false>(page, ptr, is_array, m);
                                if (first) {
                                    _found(m, ptr);
                                }
                                reachable &= reachable - 1;
                            }
                            flag.reachable &= ~flag.marked;
                            marked = true;
                        }
                    }
                } while(marked);
                page->reachable = false;
                page = page->next_reachable;
                if (!page) {
                    page = _reachable_pages;
                    _reachable_pages = nullptr;
                }
            }
        }

        // The same on the pool. The listed pages are dealt out one at a time;
        // an object taken from one (its marked bit set here, atomically) is
        // traced at once and what it leads to goes on the thread's own stack
        // of work, traced depth first. A thread whose stack and the shared
        // pile are both empty parks; a thread that has two or more items and
        // sees a parked one moves half of its stack, the older half, to the
        // pile. The pass ends when every thread is parked.
        struct MarkQueue {
            std::mutex mutex;
            std::condition_variable cv;
            std::vector<MarkItem> shared;
            std::atomic<size_t> next_page = {0};
            std::atomic<size_t> next_dirty = {0};
            unsigned total = 0;
            bool done = false;
            // read by every worker at every object, away from the mutex
            alignas(config::CacheLineSize) std::atomic<unsigned> idle = {0};
#ifdef SGCL_MARK_STATS
            std::atomic<size_t> spills = {0};
            std::atomic<size_t> takes = {0};
            std::atomic<size_t> parks = {0};
#endif
        };

        void _mark_parallel() noexcept {
            auto workers = _mark_workers;
            _parallel_mark_runs.fetch_add(1, std::memory_order_relaxed);
            _mark_pages.clear();
            for (auto page = _reachable_pages; page; page = page->next_reachable) {
                _mark_pages.push_back(page);
            }
            _reachable_pages = nullptr;
            if (_markers.size() < workers + 1) {
                _markers.resize(workers + 1);
            }
            auto& q = _mark_queue;
            q.shared.clear();
            q.next_page.store(0, std::memory_order_relaxed);
            q.next_dirty.store(0, std::memory_order_relaxed);
            q.idle.store(0, std::memory_order_relaxed);
            q.total = workers + 1;
            q.done = false;
            std::function<void(unsigned)> fn = [this](unsigned w) {
                _mark_work(_markers[w + 1]);
            };
#ifdef SGCL_MARK_STATS
            q.spills.store(0); q.takes.store(0); q.parks.store(0);
            auto t0 = std::chrono::steady_clock::now();
#endif
            _pool.start(workers, &fn);
            _mark_work(_markers[0]);
            _pool.wait();
            _dirty_pages.clear();
#ifdef SGCL_MARK_STATS
            auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::fprintf(stderr, "[mark] workers %u pages %zu spills %zu takes %zu parks %zu time %.2fms live:", workers, _mark_pages.size(), q.spills.load(), q.takes.load(), q.parks.load(), ms);
            for (auto& m : _markers) std::fprintf(stderr, " %zu", m.live);
            std::fprintf(stderr, "\n");
#endif
        }

        void _mark_work(Marker& m) noexcept {
            auto& q = _mark_queue;
            for (;;) {
                auto d = q.next_dirty.fetch_add(1, std::memory_order_relaxed);
                if (d < _dirty_pages.size()) {
                    _trace_dirty_page<true>(_dirty_pages[d], m);
                    continue;
                }
                auto i = q.next_page.fetch_add(1, std::memory_order_relaxed);
                if (i < _mark_pages.size()) {
                    _mark_page(m, _mark_pages[i]);
                    continue;
                }
                if (!_take_shared(m)) {
                    return;
                }
                _drain(m);
            }
        }

        // One of the listed pages: its reachable bits are read and cleared
        // by this thread alone (the parallel pass sets none), the marked
        // bits it shares with the others.
        void _mark_page(Marker& m, Page* page) noexcept {
            auto flags = page->flags();
            auto count = page->flags_count();
            const bool is_array = page->metadata->is_array;
            for (unsigned i = 0; i < count; ++i) {
                auto bits = flags[i].reachable;
                if (!bits) {
                    continue;
                }
                flags[i].reachable = 0;
                std::atomic_ref<Page::Flag> marked(flags[i].marked);
                auto offset = i * Page::FlagBitCount;
                while (bits) {
                    auto countr_zero = std::countr_zero(bits);
                    bits &= bits - 1;
                    auto mask = Page::Flag(1) << countr_zero;
                    if (marked.load(std::memory_order_relaxed) & mask) {
                        continue;
                    }
                    if (marked.fetch_or(mask, std::memory_order_relaxed) & mask) {
                        continue;
                    }
                    auto ptr = page->pointer_of(offset + countr_zero);
                    _found(m, ptr);
                    _trace<true>(page, ptr, is_array, m);
                    _drain(m);
                }
            }
            page->reachable = false;
        }

        void _drain(Marker& m) noexcept {
            auto& q = _mark_queue;
            while (!m.work.empty()) {
                auto item = m.work.back();
                m.work.pop_back();
                _trace<true>(item.page, item.ptr, item.page->metadata->is_array, m);
                if (m.work.size() >= 2 && q.idle.load(std::memory_order_relaxed)) {
                    _spill(m);
                }
            }
        }

        void _spill(Marker& m) noexcept {
            auto& q = _mark_queue;
            auto half = m.work.begin() + m.work.size() / 2;
            {
                std::lock_guard<std::mutex> lock(q.mutex);
                q.shared.insert(q.shared.end(), m.work.begin(), half);
            }
            m.work.erase(m.work.begin(), half);
            q.cv.notify_all();
#ifdef SGCL_MARK_STATS
            q.spills.fetch_add(1, std::memory_order_relaxed);
#endif
        }

        // Takes a share of the pile, or parks until there is one. False: the
        // pass is over (every thread parked with the pile empty).
        bool _take_shared(Marker& m) noexcept {
            auto& q = _mark_queue;
            std::unique_lock<std::mutex> lock(q.mutex);
            for (;;) {
                if (!q.shared.empty()) {
                    auto n = std::max<size_t>(1, q.shared.size() / q.total);
                    m.work.insert(m.work.end(), q.shared.end() - n, q.shared.end());
                    q.shared.resize(q.shared.size() - n);
#ifdef SGCL_MARK_STATS
                    q.takes.fetch_add(1, std::memory_order_relaxed);
#endif
                    return true;
                }
                if (q.idle.fetch_add(1, std::memory_order_relaxed) + 1 == q.total) {
                    q.done = true;
                    lock.unlock();
                    q.cv.notify_all();
                    return false;
                }
#ifdef SGCL_MARK_STATS
                q.parks.fetch_add(1, std::memory_order_relaxed);
#endif
                q.cv.wait(lock, [&q] { return !q.shared.empty() || q.done; });
                if (q.done) {
                    return false;
                }
                q.idle.fetch_sub(1, std::memory_order_relaxed);
            }
        }

        // Pages found reachable / unreachable by one run of a pass; spliced
        // into the collector's lists after the run.
        struct PageLists {
            Page* reachable = nullptr;
            Page* reachable_tail = nullptr;
            Page* unreachable = nullptr;
            Page* unreachable_tail = nullptr;

            void push_reachable(Page* page) noexcept {
                page->reachable = true;
                page->next_reachable = reachable;
                reachable = page;
                if (!reachable_tail) {
                    reachable_tail = page;
                }
            }

            void push_unreachable(Page* page) noexcept {
                page->unreachable = true;
                page->next_unreachable = unreachable;
                unreachable = page;
                if (!unreachable_tail) {
                    unreachable_tail = page;
                }
            }
        };

        // The pass over all the pages (All) also retires the states of the
        // other parity on registered slots, to Used: the parity has two
        // values, so a state set two cycles ago would read as current
        // again. Safe, since such a state was set before the flip, by a
        // store into an object that existed before it, which this cycle
        // registers and traces if it is reachable; the states set after
        // the flip have the current parity and are left alone (the
        // check-then-act race of the old demotion, a barrier reading a
        // state the collector then demoted, cannot happen: the barrier
        // reads and stores the current parity only). A compare-exchange,
        // since the barrier may promote the state meanwhile.
        template<bool All>
        void _update_page_marks(Page* page, PageLists& lists) noexcept {
            bool reachable_page = false;
            [[maybe_unused]] bool unreachable_page = false;
            if (All || page->state_updated.load(std::memory_order_acquire)) {
                auto current = Page::reachable_state();
                [[maybe_unused]] auto stale = State(current ^ State::Parity);   // Reachable of the other parity
                [[maybe_unused]] bool retire = false;
                if constexpr(All) {
                    retire = page->retire;
                    page->retire = false;
                }
                auto states = page->states();
                auto flags = page->flags();
                auto count = page->flags_count();
                for (unsigned i = 0; i < count; ++i) {
                    auto& flag = flags[i];
                    auto unreachable = flag.registered & ~flag.marked;
                    auto offset = i * Page::FlagBitCount;
                    // eight states per step (states.h): the unmarked slots
                    // whose state the barrier set, as bits
                    for (unsigned g = 0; g < Page::FlagBitCount; g += 8) {
                        auto group = (unreachable >> g) & 0xFF;
                        [[maybe_unused]] auto registered = retire ? (flag.registered >> g) & 0xFF : 0;
                        if (!group && !registered) {
                            continue;
                        }
                        auto w = States8::load(states + offset + g);
                        if constexpr(All) {
                            // the stale bytes of the word to Used in one
                            // compare-exchange of the word (eight slots): a
                            // barrier storing into any of the eight meanwhile
                            // fails it, and the word is read again
                            std::atomic_ref<uint64_t> word(*reinterpret_cast<uint64_t*>(states + offset + g));
                            for (;;) {
                                auto old = (States8::equal(w, stale) | States8::equal(w, State(stale | State::Fresh))) & registered;
                                if (!old) {
                                    break;
                                }
                                uint64_t desired = w;
                                for (auto bits = old; bits; bits &= bits - 1) {
                                    desired &= ~(uint64_t(0xFF) << (8 * std::countr_zero(bits)));   // Used
                                }
                                if (word.compare_exchange_weak(w, desired, std::memory_order_relaxed, std::memory_order_relaxed)) {
                                    w = desired;
                                    break;
                                }
                            }
                            if (!group) {
                                continue;
                            }
                        }
                        auto found = States8::reachable(w, current) & group;
#ifdef SGCL_TRACE_STACK
                        for (auto f = found; f; f &= f - 1) {
                            std::fprintf(stderr, "[updated%s] %p type %s\n", All ? " all" : "", page->pointer_of(offset + g + std::countr_zero(f)), page->metadata->type_info.name());
                        }
#endif
                        flag.reachable |= Page::Flag(found) << g;
                        reachable_page |= found != 0;
                        if constexpr(All) {
                            unreachable_page |= (group & ~found) != 0;
                        }
                    }
                }
            }
            if (reachable_page && !page->reachable) {
                lists.push_reachable(page);
            }
            if constexpr(All) {
                if (unreachable_page && !page->unreachable) {
                    lists.push_unreachable(page);
                }
            }
        }

        // Objects whose state the barrier set since the demotion: over every
        // registered page (All) or over the pages that still hold unmarked
        // objects, when their flag says something was stored.
        template<bool All>
        void _mark_updated() noexcept {
            std::atomic_thread_fence(std::memory_order_acquire);
            std::vector<PageLists> results;
            if constexpr(All) {
                results = _parallel_array<PageLists>(_pages.size(), [this](size_t begin, size_t end) {
                    std::atomic_thread_fence(std::memory_order_acquire);
                    PageLists lists;
                    for (auto i = begin; i < end; ++i) {
                        _update_page_marks<All>(_pages[i], lists);
                    }
                    return lists;
                });
            } else {
                results = _parallel_pages<&Page::next_unreachable, PageLists>(_unreachable_pages, [this](Page* page, size_t count) {
                    std::atomic_thread_fence(std::memory_order_acquire);
                    PageLists lists;
                    for (; count; --count, page = page->next_unreachable) {
                        _update_page_marks<All>(page, lists);
                    }
                    return lists;
                });
            }
            for (auto& lists : results) {
                if (lists.reachable) {
                    lists.reachable_tail->next_reachable = _reachable_pages;
                    _reachable_pages = lists.reachable;
                }
                if (lists.unreachable) {
                    lists.unreachable_tail->next_unreachable = _unreachable_pages;
                    _unreachable_pages = lists.unreachable;
                }
            }
            if constexpr(All) {
                _mark_hazard_pointers();
            }
        }

        // Runs the destructor of a dying object. Its tracked_ptr members are
        // left as they are: the objects they point to may be dying in the
        // same sweep, destroyed already or on another thread, so a
        // destructor must not read them (README, "Pointer maps"). Nothing
        // is nulled first: the map of pointer words is approximate (a word
        // is in it until seen holding a non-heap value), and a store through
        // it could hit data. An owning unique_ptr is different: the owned
        // object is never garbage while owned, its owner destroys it.
        // Buffers have no destroy function (array_base.h): a container
        // destroys its own elements, the collector only frees the pages.
        inline static void _destroy(Page* page, void* ptr) noexcept {
            auto destroy = page->metadata->destroy;
            if (destroy) {
                destroy(ptr);
            }
        }

        // Destroys the garbage of one run of unreachable pages and frees the
        // slots; the page's registered bits are folded by _remove_garbage
        // once every run is done.
        static size_t _sweep(Page* page, size_t count) noexcept {
            std::atomic_thread_fence(std::memory_order_acquire);
            sweeping = true;
            size_t removed = 0;
            for (; count; --count, page = page->next_unreachable) {
                auto states = page->states();
                auto flags = page->flags();
                auto words = page->flags_count();
                auto destroy = page->metadata->destroy;
                for (unsigned i = 0; i < words; ++i) {
                    auto& flag = flags[i];
                    auto unreachable = flag.registered & ~flag.marked;
                    auto offset = i * Page::FlagBitCount;
                    if (unreachable && !destroy) {
                        // nothing to run per object: the states set eight
                        // at once where a whole group dies (states.h)
                        page->unused_occur.store(true, std::memory_order_relaxed);
                        removed += std::popcount(unreachable);
                        page->unused_counter_gc += (uint16_t)std::popcount(unreachable);
                        for (unsigned g = 0; g < Page::FlagBitCount; g += 8) {
                            auto group = (unreachable >> g) & 0xFF;
                            if (group == 0xFF) {
                                States8::store(states + offset + g, State::Unused);
                            } else {
                                while (group) {
                                    states[offset + g + std::countr_zero(group)].store(State::Unused, std::memory_order_relaxed);
                                    group &= group - 1;
                                }
                            }
                        }
                    } else if (unreachable) {
                        page->unused_occur.store(true, std::memory_order_relaxed);
                        do {
                            auto countr_zero = std::countr_zero(unreachable);
                            auto index = offset + countr_zero;
                            auto state = states[index].load(std::memory_order_relaxed);
                            assert(state != Page::reachable_state() && state != State(Page::reachable_state() | State::Fresh) && state != State::UniqueLock);
#ifdef SGCL_TRACE_STACK
                            std::fprintf(stderr, "[sweep] %p type %s state %d\n", page->pointer_of(index), page->metadata->type_info.name(), (int)state);
#endif
                            if (!(state & (State::Destroyed | State::BadAlloc))) {   // Used, or Reachable of an old parity
                                _destroy(page, page->pointer_of(index));
                            }
                            ++removed;
                            states[index].store(State::Unused, std::memory_order_relaxed);
                            ++page->unused_counter_gc;
                            unreachable &= unreachable - 1;
                        } while(unreachable);
                    }
                }
            }
            sweeping = false;
            std::atomic_thread_fence(std::memory_order_release);
            return removed;
        }

        // The sweep runs here while the garbage of a cycle is small, and on
        // the pool (config::SweepPageThreshold, SweepThreadsMax) when it is
        // not: the unreachable pages are cut into equal runs, one per
        // thread, this thread taking the last one.
        size_t _remove_garbage() noexcept {
            auto counts = _parallel_pages<&Page::next_unreachable, size_t>(_unreachable_pages, [](Page* first, size_t count) {
                return _sweep(first, count);
            });
            size_t removed = 0;
            for (auto c : counts) {
                removed += c;
            }
            // the fold, once every run of the sweep is done, as a pass of its own
            _parallel_pages<&Page::next_unreachable, int>(_unreachable_pages, [](Page* page, size_t count) {
                for (; count; --count, page = page->next_unreachable) {
                    auto flags = page->flags();
                    auto words = page->flags_count();
                    for (unsigned i = 0; i < words; ++i) {
                        flags[i].registered &= flags[i].marked;
                    }
                    page->unreachable = false;
                }
                return 0;
            });
            _unreachable_pages = nullptr;
            return removed;
        }

        // Helper threads for the passes over pages. Created on first use,
        // parked on a condition variable between uses, told to leave when the
        // collector terminates. A worker that runs a destructor creating a
        // tracked_ptr registers itself like any thread; its parked stack is
        // harmless.
        class WorkerPool {
        public:
            // 0 while a pass has little to do; a pass over `pages` pages is
            // worth sharing from config::SweepPageThreshold on
            unsigned workers_for(size_t pages) {
                return workers(pages, config::SweepPageThreshold);
            }

            // 0 below `threshold` units of work; otherwise one helper per
            // quarter of the threshold, up to the maximum
            // `regardless`: the work does not come from allocation (stack
            // scanning), so the growth policy does not apply to it.
            unsigned workers(size_t units, size_t threshold, bool regardless = false) {
                if (units < threshold || (!_enabled && !regardless)) {
                    return 0;
                }
                auto max = config::SweepThreadsMax ? config::SweepThreadsMax : std::min<size_t>(8, std::max<size_t>(1, std::thread::hardware_concurrency() / 2));
                auto wanted = (unsigned)std::min<size_t>(max, units / (threshold / 4));
                _last = std::max(_last, wanted);
                if (wanted > _threads.size()) {
                    while (_threads.size() < wanted) {
                        auto index = (unsigned)_threads.size();
                        _threads.emplace_back([this, index] { _run(index); });
                    }
                }
                return wanted;
            }

            unsigned last_workers() const noexcept {
                return _last;
            }

            unsigned size() const noexcept {
                return (unsigned)_threads.size();
            }

            // Growth policy, once per cycle: `live` is the live memory now,
            // `allocated` what the mutators allocated since the previous
            // decision. The growth is measured from the lowest live memory
            // seen since the helpers were last switched off, so that a
            // steady growth of a few MB per cycle adds up to the threshold
            // like one spike would. The level kept for the hysteresis is a
            // rate (bytes per second), not an amount per cycle: with the
            // helpers on the cycles get shorter and the amount per cycle
            // drops although the mutators allocate exactly as before.
            void decide(int64_t live, int64_t allocated) noexcept {
                if constexpr(config::HelpersGrowthThreshold == 0) {   // always on (measurements, tests)
                    _enabled = true;
                    return;
                }
                auto now = std::chrono::steady_clock::now();
                auto seconds = std::chrono::duration<double>(now - _decided).count();
                _decided = now;
                auto rate = seconds > 0 ? allocated / seconds : 0.0;
                if (!_enabled) {
                    _live_floor = std::min(_live_floor, live);
                    if (live - _live_floor >= (int64_t)config::HelpersGrowthThreshold) {
                        _enabled = true;
                        _level = rate;
                    }
                } else if (rate < _level / 2) {
                    _enabled = false;
                    _live_floor = live;
                }
            }

            bool enabled() const noexcept {
                return _enabled;
            }

            void reset_last() noexcept {
                _last = 0;
            }

            // Runs fn(0) .. fn(workers - 1) on the helpers; the caller runs
            // its own share meanwhile and then wait()s.
            void start(unsigned workers, std::function<void(unsigned)>* fn) {
                std::lock_guard<std::mutex> lock(_mutex);
                _fn = fn;
                _active = workers;
                _pending = workers;
                ++_generation;
                _wake.notify_all();
            }

            void wait() {
                std::unique_lock<std::mutex> lock(_mutex);
                _done.wait(lock, [this] { return _pending == 0; });
                _active = 0;
                _fn = nullptr;
            }

            void stop() {
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    _stopping = true;
                    _wake.notify_all();
                }
                for (auto& t : _threads) {
                    t.join();
                }
                _threads.clear();
            }

        private:
            void _run(unsigned index) {
                uint64_t seen = 0;
                for (;;) {
                    std::function<void(unsigned)>* fn;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        _wake.wait(lock, [&] { return _stopping || (_generation != seen && index < _active); });
                        if (_stopping) {
                            return;
                        }
                        seen = _generation;
                        fn = _fn;
                    }
                    (*fn)(index);
                    _clear_own_stack();
                    std::lock_guard<std::mutex> lock(_mutex);
                    if (--_pending == 0) {
                        _done.notify_one();
                    }
                }
            }

            std::vector<std::thread> _threads;
            std::mutex _mutex;
            std::condition_variable _wake;
            std::condition_variable _done;
            std::function<void(unsigned)>* _fn = nullptr;
            uint64_t _generation = 0;
            unsigned _last = 0;
            bool _enabled = false;
            int64_t _live_floor = 0;
            double _level = 0;
            std::chrono::steady_clock::time_point _decided = std::chrono::steady_clock::now();
            unsigned _active = 0;
            unsigned _pending = 0;
            bool _stopping = false;
        };

        // Zeroes the dead frames below the caller's, on any thread (no
        // registration needed): config::StackClearSize, within the stack.
        SGCL_NOINLINE static void _clear_frames_below() noexcept {
            uintptr_t here = (uintptr_t)&here;
            uintptr_t begin = 0, end = 0;
            auto limit = here > config::StackClearSize ? here - config::StackClearSize : 0;
            if (os::thread_stack(begin, end)) {
                limit = std::max(limit, begin + config::StackGuardMargin);
            }
            limit = std::max(limit, os::lowest_touched(limit, here));
            os::hidden_call([](void*) {}, nullptr, limit);
        }

        // A collector thread that ran destructors creating tracked_ptrs is a
        // registered thread: its stack is scanned, and while it is parked the
        // dead frames of those destructors would keep whatever they pointed
        // to alive. Zeroed after every task and every cycle.
        static void _clear_own_stack() noexcept {
            if (!thread_registered) {
                return;
            }
            os::hidden_call([](void*) {}, nullptr, stack_clear_limit(config::StackClearSize));
        }

        // A pass over a page list, cut into equal runs shared with the pool
        // (one run per helper, the last one here); per_run(first, count)
        // returns this run's result, in list order.
        template<Page* Page::*Next, class R, class F>
        std::vector<R> _parallel_pages(Page* head, F per_run) {
            size_t pages = 0;
            for (auto page = head; page; page = page->*Next) {
                ++pages;
            }
            auto workers = _pool.workers_for(pages);
            std::vector<R> results(workers + 1);
            if (!workers) {
                results[0] = per_run(head, pages);
                return results;
            }
            auto run = pages / (workers + 1);
            std::vector<Page*> firsts(workers + 1);
            auto page = head;
            for (unsigned w = 0; w <= workers; ++w) {
                firsts[w] = page;
                for (size_t i = 0; i < run && w < workers; ++i) {
                    page = page->*Next;
                }
            }
            std::function<void(unsigned)> fn = [&](unsigned w) {
                results[w] = per_run(firsts[w], run);
            };
            _pool.start(workers, &fn);
            results[workers] = per_run(firsts[workers], pages - run * workers);
            _pool.wait();
            return results;
        }

        // The same over an index range [0, count), per_range(begin, end)
        // returning its result.
        template<class R, class F>
        std::vector<R> _parallel_array(size_t count, F per_range) {
            auto workers = _pool.workers_for(count);
            std::vector<R> results(workers + 1);
            if (!workers) {
                results[0] = per_range(size_t(0), count);
                return results;
            }
            auto run = count / (workers + 1);
            std::function<void(unsigned)> fn = [&](unsigned w) {
                results[w] = per_range(w * run, (w + 1) * run);
            };
            _pool.start(workers, &fn);
            results[workers] = per_range(workers * run, count);
            _pool.wait();
            return results;
        }

        // The same over an index range [0, count), without results.
        template<class F>
        void _parallel_range(size_t count, F per_range) {
            auto workers = _pool.workers_for(count);
            if (!workers) {
                per_range(size_t(0), count);
                return;
            }
            auto run = count / (workers + 1);
            std::function<void(unsigned)> fn = [&](unsigned w) {
                per_range(w * run, (w + 1) * run);
            };
            _pool.start(workers, &fn);
            per_range(workers * run, count);
            _pool.wait();
        }

        void _release_unused_pages() {
            std::atomic_thread_fence(std::memory_order_acquire);
            Metadata* metadata = nullptr;
            for (auto page : _pages) {
                if (page->unused_occur.load(std::memory_order_acquire)) {
                    // on_empty_list before owned: the mutator stores them the
                    // other way round, so a page it just took from the buffer
                    // is never seen as loose
                    if (!page->on_empty_list.load(std::memory_order_acquire) && !page->owned.load(std::memory_order_acquire)) {
                        auto count = page->metadata->object_count;
                        auto unused = page->unused_counter_gc;
                        if (unused > count / 2) {
                            page->unused_occur.store(false, std::memory_order_relaxed);
                            if (!page->metadata->used) {
                                page->metadata->used = true;
                                page->metadata->next = metadata;
                                metadata = page->metadata;
                            }
                            page->on_empty_list.store(true, std::memory_order_relaxed);
                            page->next_empty = page->metadata->empty_page;
                            page->metadata->empty_page = page;
                        }
                    }
                }
            }

            // the free bitmaps of the candidates are rebuilt from their
            // states here, on the pool when there are many; the allocators
            // then only sort them into their buffers
            _candidates.clear();
            for (auto m = metadata; m; m = m->next) {
                if (m->pool_allocated) {
                    for (auto p = m->empty_page; p; p = p->next_empty) {
                        _candidates.push_back(p);
                    }
                }
            }
            _parallel_range(_candidates.size(), [this](size_t begin, size_t end) {
                for (auto i = begin; i < end; ++i) {
                    ObjectPoolAllocatorBase::rebuild_free_bitmap(_candidates[i]);
                }
            });
            while(metadata) {
                metadata->free(metadata->empty_page);
                metadata->used = false;
                metadata->empty_page = nullptr;
                metadata = metadata->next;
            }
            // The pages went back to the heap above; the headers are dropped
            // from the array and deleted here, in the GC thread (they are
            // small, and the heap decommits data pages without touching them).
            if (!_weak_pages.empty()) {
                std::erase_if(_weak_pages, [](Page* page) { return !page->is_used; });
            }
            std::erase_if(_pages, [](Page* page) {
                if (!page->is_used) {
                    Page::release(page);
                    return true;
                }
                return false;
            });
        }

#if SGCL_LOG_PRINT_LEVEL >= 2
        double total_time = 0;
#endif
        void _main_loop() noexcept {
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] start collector id: " << std::this_thread::get_id() << std::endl;
#endif
            using namespace std::chrono_literals;
            int finalization_counter = 5;
            do {
                auto start = std::chrono::steady_clock::now();
                _pool.reset_last();
                auto live_size = MemoryCounters::live_bytes();
                _pool.decide(live_size, MemoryCounters::last_alloc() * config::PageSize);
                _full = _choose_full_cycle(live_size);
                // the workers by what a cycle of this kind marked last time: a
                // young cycle marks the young survivors, a full one the heap
                _mark_workers = _pool.workers(_full ? _last_marked_full : _last_marked_young, config::MarkObjectThreshold);
#ifdef SGCL_MARK_STATS
                // SGCL_MARK_WORKERS: -1 sequential, 0 the parallel code on this thread alone, k helpers
                if (auto e = std::getenv("SGCL_MARK_WORKERS")) {
                    auto w = std::atoi(e);
                    _mark_workers = w < 0 ? 0 : std::min<unsigned>(w, _pool.workers(1u << 30, 4));
                    _mark_force = w >= 0;
                }
#endif
#ifdef SGCL_TRACE_STACK
                std::fprintf(stderr, "[cycle] %s epoch %u\n", _full ? "full" : "young", _epoch + 1);
#endif
                Page::flip_epoch(++_epoch);
                // the phases timed always (eight clock reads a cycle): statistics()
                auto phase_t = std::chrono::steady_clock::now();
                double phase_ms[PhaseCount] = {};
                auto phase = [&](int i) { auto now = std::chrono::steady_clock::now(); phase_ms[i] += std::chrono::duration<double, std::milli>(now - phase_t).count(); phase_t = now; };
                // One round of registration, after the flip of the epoch:
                // every object created before the flip belongs to a thread
                // registered before it (a thread registers itself before its
                // first allocation) and to a page that thread published
                // before allocating from it, so the round sees them all.
                // Everything created after the flip stays unregistered this
                // cycle: not swept, and whatever it points to has the state
                // its stores set with the new epoch. The threads are looked
                // at once more before the stacks are scanned, for the ones
                // that registered during the round.
                _register_threads();
                _register_pages();
                [[maybe_unused]] size_t last_objects_created = _register_objects();
                phase(0);
                phase(1);
                _register_threads();
                _mark_stack_roots();
                if (!_full) {
                    // the dirty pages count like the marked objects of the
                    // previous cycle: enough of them and the pass goes to the pool
                    auto dirty = _collect_dirty_pages();
                    _mark_workers = std::max(_mark_workers, _pool.workers(dirty, config::MarkObjectThreshold));
                    if (!_mark_workers) {
                        _trace_dirty_pages();
                    }
                }
                phase(2);
                for (auto& m : _markers) {
                    m.live = 0;
                    m.objects.clear();
                }
                // A live-objects request seen here is served by this cycle,
                // which is a full cycle from the requester's point of view
                // because _force_collect() guarantees at least one more.
                if (_live_objects_request.exchange(false, std::memory_order_acq_rel)) {
                    _share_live_objects = true;
                }
                // Marking runs until a pass over the registered pages finds
                // no unmarked object with a state set by the barrier. Objects
                // created during the cycle are not registered here: they are
                // never garbage in this cycle (unregistered slots are not
                // swept) and the barrier protects the old objects they point
                // to, so registering them would only feed the loop faster
                // than it converges (a mutator allocating at full speed kept
                // one cycle open for the whole run). Threads are picked up so
                // that their hazard pointers are honoured.
                do {
                    _mark_reachable();
                    phase(3);
                    if (!_unreachable_pages) {
                        _register_threads();
                        _update_hazard_pointers();
                        _mark_updated<true>();
                    } else {
                        _mark_updated<false>();
                    }
                    phase(4);
                    // The marking has converged: the weak phase, and when it
                    // cleared anything, a pass for the states and hazards of
                    // the locks that raced with it (_clear_weak_cells). The
                    // pages of the cleared targets are on the unreachable
                    // list (they hold unmarked registered objects), so the
                    // pass over that list sees the states; the hazards are
                    // resolved one by one.
                    if (!_reachable_pages && !_weak_pages.empty() && _weak_phase() == WeakPhase::Cleared) {
                        _register_threads();
                        _update_hazard_pointers();
                        _mark_updated<false>();
                        _mark_hazard_pointers();
                    }
                } while(_reachable_pages);
                // a young cycle counts the objects marked for the first time;
                // the marked ones from before are live until a full cycle says otherwise
                size_t marked = 0;
                for (auto& m : _markers) {
                    marked += m.live;
                }
                (_full ? _last_marked_full : _last_marked_young) = marked;
                if (_share_live_objects) {
                    _live_objects.clear();
                    for (auto& m : _markers) {
                        _live_objects.insert(_live_objects.end(), m.objects.begin(), m.objects.end());
                    }
                    _collect_type_statistics();
                }
                _live_total = _full ? marked : _live_total + marked;
                _last_live_object_count.store(_live_total, std::memory_order_relaxed);
                size_t last_objects_removed = _remove_garbage();
                phase(5);
                _release_unused_pages();
                phase(6);
                Heap::instance().trim();
                MemoryCounters::end_cycle();
                _clear_own_stack();
                phase(7);
#ifdef SGCL_MARK_STATS
                std::fprintf(stderr, "[cycle] %s register %.1f states %.1f roots %.1f mark %.1f updated %.1f sweep %.1f release %.1f trim %.1f ms, removed %zu\n", _full ? "full" : "young", phase_ms[0], phase_ms[1], phase_ms[2], phase_ms[3], phase_ms[4], phase_ms[5], phase_ms[6], phase_ms[7], last_objects_removed);
#endif
                if (_full) {
                    _young_cycles = 0;
                    _live_after_full = MemoryCounters::live_bytes();
                } else {
                    ++_young_cycles;
                }
                double duration = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                _stats_cycles.fetch_add(1, std::memory_order_relaxed);
                if (_full) {
                    _stats_full_cycles.fetch_add(1, std::memory_order_relaxed);
                }
                _stats_last_cycle_ms.store(duration, std::memory_order_relaxed);
                _stats_helper_threads.store(_pool.size(), std::memory_order_relaxed);
                _stats_last_helpers_used.store(_pool.last_workers(), std::memory_order_relaxed);
                _stats_helpers_enabled.store(_pool.enabled(), std::memory_order_relaxed);
                for (int i = 0; i < PhaseCount; ++i) {
                    _stats_phase_ms[i].store(phase_ms[i], std::memory_order_relaxed);
                }
#if SGCL_LOG_PRINT_LEVEL >= 2
                total_time += duration;
                std::cout << "[sgcl] mem allocs:" << std::setw(7) << MemoryCounters::alloc_since_cycle()
                          << ",    mem removed:" << std::setw(7) << MemoryCounters::free_since_cycle()
                          << ",    total mem:" << std::setw(7) << MemoryCounters::live_pages()
                          << ",    objects created:" << std::setw(9) << last_objects_created
                          << ",    objects removed:" << std::setw(9) << last_objects_removed
                          << ",    live objects:" << std::setw(9) << _live_total
                          << ",    cycle:" << (_full ? "full " : "young")
                          << ",    helpers:" << (_pool.enabled() ? "on " : "off") << " used:" << std::setw(2) << _pool.last_workers()
                          << ",    time:" << std::setw(8) << std::fixed << std::setprecision(3) << duration << "ms"
                          << ",    total time:" << std::setw(10) << std::fixed << std::setprecision(3) << total_time << "ms"
                          << std::endl;
#endif
                bool can_sleep = true;
                if (_young_collect_count.load(std::memory_order_acquire)) {
                    if (_young_collect_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        std::unique_lock<std::mutex> lock(_mutex);
                        ++_report_generation;
                        _cv_data_ready.notify_all();
                    } else {
                        can_sleep = false;
                    }
                }
                if (_forced_collect_count.load(std::memory_order_acquire)) {
                    // A forced collection reports only after a cycle that found
                    // nothing to remove. Destructors run while sweeping may store
                    // tracked pointers, and the write barrier then keeps their
                    // targets alive for one more cycle each, so a fixed number
                    // of cycles is not a full collection. Bounded, because a
                    // concurrently allocating mutator produces garbage forever.
                    if (_forced_collect_count.load(std::memory_order_relaxed) == 1
                            && last_objects_removed
                            && ++_quiescence_rounds < MaxQuiescenceRounds) {
                        can_sleep = false;
                    } else if (_forced_collect_count.fetch_sub(1, std::memory_order_relaxed) == 1) {
                        _quiescence_rounds = 0;
                        {
                            std::unique_lock<std::mutex> lock(_mutex);
                            ++_report_generation;
                            _cv_data_ready.notify_all();
                        }
                        if (_share_live_objects) {
                            std::unique_lock<std::mutex> lock(_mutex);
#if SGCL_LOG_PRINT_LEVEL > 2
                            std::cout << "[sgcl] suspended collector id: " << std::this_thread::get_id() << std::endl;
#endif
                            _cv_data_processed.wait(lock, [this] {
                                return _dataProcessed;
                            });
                            _dataProcessed = false;
                            _share_live_objects = false;
#if SGCL_LOG_PRINT_LEVEL > 2
                            std::cout << "[sgcl] resumed collector id: " << std::this_thread::get_id() << std::endl;
#endif
                        }
                    }
                    else {
                        can_sleep = false;
                    }
                }
                if (!_terminating && can_sleep) {
                    sleep_flag.store(true, std::memory_order_relaxed);
                    std::unique_lock<std::mutex> lock(_mutex);
                    std::chrono::nanoseconds sleep_time = _short_sleep ? config::ShortSleepTime : config::LongSleepTime;
                    if (Heap::instance().under_pressure()) {
                        sleep_time = config::PressureSleepTime;
                    }
                    _short_sleep = false;
                    sleep_cv.wait_for(lock, sleep_time, [this]{
                        return !sleep_flag.load(std::memory_order_acquire)
                            || _forced_collect_count.load(std::memory_order_relaxed)
                            || _young_collect_count.load(std::memory_order_relaxed)
                            || _terminating.load(std::memory_order_relaxed);
                    });
                }
                MemoryCounters::begin_cycle();
                if (!last_objects_removed && _terminating) {
                    if (_live_total) {
                        --finalization_counter;
                    } else {
                        finalization_counter = 0;
                    }
                }
            } while(finalization_counter);
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] stop collector id: " << std::this_thread::get_id() << std::endl;
#endif
            _pool.stop();
            if (_terminating) {
                _terminated = true;
                _terminated.notify_all();
            }
        }

        // config.h: YoungCyclesMax, FullCycleGrowthPercent. A forced
        // collection is always full: the caller waits for a complete one.
        bool _choose_full_cycle(size_t live_size) const noexcept {
            if constexpr(!config::Generational) {
                return true;
            }
            if (_forced_collect_count.load(std::memory_order_acquire) || _terminating || Heap::instance().under_pressure()) {
                return true;
            }
            if (_young_collect_count.load(std::memory_order_acquire)) {
                return false;
            }
            if (_young_cycles >= config::YoungCyclesMax) {
                return true;
            }
            auto base = std::max<size_t>(_live_after_full, config::ChunkSize);
            return live_size >= base + base * config::FullCycleGrowthPercent / 100;
        }

        void _force_collect() noexcept {
            // "At least one full cycle after this call": the current cycle may
            // be half done, so two must remain. Never lower the count, so a
            // request arriving during another request's sequence extends it
            // instead of restarting it.
            auto count = _forced_collect_count.load(std::memory_order_relaxed);
            while (count < 2 && !_forced_collect_count.compare_exchange_weak(count, 2, std::memory_order_release, std::memory_order_relaxed)) {
            }
            waking_up();
        }

        void _terminate() noexcept {
            if (!_terminating) {
#if SGCL_LOG_PRINT_LEVEL > 0
                std::cout << "[sgcl] terminate collector from id: " << std::this_thread::get_id() << std::endl;
#endif
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _terminating.store(true);
                    waking_up();
                    _cv_data_ready.notify_all();
                }
                _terminated.wait(false);
            }
        }

        Thread::Data* _registered_threads = {nullptr};
        Page* _reachable_pages = {nullptr};
        Page* _unreachable_pages = {nullptr};
        std::vector<Page*> _pages;   // the registered pages
        std::vector<Page*> _weak_pages;   // of them, the pages of weak cells (weak_cell.h)
        std::atomic<int> _forced_collect_count = {0};
        std::atomic<int> _young_collect_count = {0};
        std::mutex _mutex;
        std::vector<void*> _live_objects;
        std::vector<TypeStatistics> _type_statistics;
        std::atomic<double> _stats_phase_ms[PhaseCount] = {};
        std::vector<Marker> _markers = std::vector<Marker>(1);
        std::vector<Page*> _mark_pages;
        std::vector<Page*> _dirty_pages;   // the young cycle's, traced by the marking pass
        MarkQueue _mark_queue;
        unsigned _mark_workers = 0;
#ifdef SGCL_MARK_STATS
        bool _mark_force = false;
#endif
        size_t _last_marked_full = 0;    // objects marked by the last full cycle
        size_t _last_marked_young = 0;   // by the last young one
        std::atomic<size_t> _parallel_mark_runs = {0};
        size_t _live_total = 0;
        std::atomic<size_t> _last_live_object_count = {0};
        std::atomic<size_t> _stats_cycles = {0};
        std::atomic<size_t> _stats_full_cycles = {0};
        std::atomic<double> _stats_last_cycle_ms = {0};
        std::atomic<unsigned> _stats_helper_threads = {0};
        std::atomic<unsigned> _stats_last_helpers_used = {0};
        std::atomic<bool> _stats_helpers_enabled = {false};
        bool _full = true;
        uint32_t _epoch = 1;
        unsigned _young_cycles = 0;
        size_t _live_after_full = 0;
        std::atomic<bool> _live_objects_request = {false};
        std::atomic<bool> _terminated = {false};
        bool _share_live_objects = {false};
        inline static std::atomic<bool> _created = {false};
        inline static std::atomic<bool> _terminating = {false};
        std::condition_variable sleep_cv;
        std::atomic<bool> sleep_flag = {true};
        std::vector<uintptr_t> _hazard_pointers;
        std::vector<unsigned char> _touched_pages;
        std::vector<StackSegment> _stack_segments;
        std::vector<Thread::Data*> _scanned_threads;
        std::atomic<size_t> _parallel_stack_scans = {0};
        WorkerPool _pool;
        std::vector<Page*> _candidates;
        std::condition_variable _cv_data_ready;
        std::condition_variable _cv_data_processed;
        uint64_t _report_generation = 0;
        bool _dataProcessed = false;
        bool _short_sleep = false;
        static constexpr unsigned MaxQuiescenceRounds = 8;
        unsigned _quiescence_rounds = 0;

        friend inline void delete_unique(const void*) noexcept;
    };

    inline Collector& collector_instance() {
        static Collector collector_instance;
        return collector_instance;
    }

    inline void collector_init() {
        collector_instance();
    }

    inline void terminate_collector() noexcept {
        Collector::terminate();
    }

    inline void waking_up_collector() noexcept {
        collector_instance().waking_up();
    }

    inline void force_short_sleep() noexcept {
        collector_instance().force_short_sleep();
    }

    inline void collect_before_bad_alloc() {
        collector_instance().force_collect(true);
    }
}
