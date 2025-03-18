//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "child_pointers.h"
#include "thread.h"
#include "types.h"

#include <condition_variable>
#include <functional>

#if SGCL_LOG_PRINT_LEVEL > 0
#include <iomanip>
#include <iostream>
#endif

namespace sgcl::detail {
    class Collector {
    public:
        using PauseGuard = std::unique_ptr<std::mutex, std::function<void(std::mutex*)>>;

        Collector() {
            if (!created()) {
                _created = true;
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
                if (!_paused) {
                    if (!_terminating.load()) {
                        _force_collect();
                        _forced_collect_cv.wait(lock, [this]{
                            return _forced_collect_count.load(std::memory_order_relaxed) == 0;
                        });
                    }
                } else {
                    return false;
                }
            } else {
                _force_collect();
            }
            return true;
        }

        std::tuple<PauseGuard, std::vector<void*>> get_live_objects() noexcept {
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] get live objects from id: " << std::this_thread::get_id() << std::endl;
#endif
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_terminating.load()) {
                _paused = true;
                _live_objects_request = true;
                _force_collect();
                _forced_collect_cv.wait(lock, [this]{
                    return _forced_collect_count.load(std::memory_order_relaxed) == 0;
                });
            }
            return {
                PauseGuard(&_mutex, [&](std::mutex* m) {
                    std::unique_lock<std::mutex> lock(*m);
                    _paused = false;
                    _forced_collect_cv.notify_all();
                })
              , std::move(_live_objects)
            };
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

        inline static void delete_unique(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            auto ptr = Page::base_address_of(p);
            _destroy(page, ptr, false);
            Page::set_state<State::Destroyed>(p);
        }

    private:
        inline static void _update_child_offsets(ChildPointers& childs) {
            if (!childs.offsets && childs.final.load(std::memory_order_acquire)) {
                childs.offsets = new ChildPointers::Vector;
                for (unsigned index = 0; index < childs.map.size(); ++index) {
                    auto flags = childs.map[index].load(std::memory_order_relaxed);
                    if (flags) {
                        for (unsigned i = 0; i < 8; ++ i) {
                            auto mask = uint8_t(1) << i;
                            if (flags & mask) {
                                auto offset = (index * 8 + i) * sizeof(RawPointer);
                                childs.offsets->emplace_back(offset);
                            }
                        }
                    }
                }
            }
        }

        void _register_threads() {
            auto first_thread = Thread::threads_data.load(std::memory_order_acquire);
            auto thread = first_thread;
            while(thread != _last_thread_registered) {
                thread->next_registered = _registered_threads;
                _registered_threads = thread;
                thread = thread->next;
            }
            if (_last_thread_registered != first_thread) {
                // To avoid ABA
                if (_last_thread_registered) {
                    _last_thread_registered->is_last_registered = false;
                    if (!_last_thread_registered->is_used) {
                        delete _last_thread_registered;
                        BlockAllocator::release_empty();
                    }
                }
                _last_thread_registered = first_thread;
                _last_thread_registered->is_last_registered = true;
            }

            if (_terminating) {
                Thread::threads_data.store(nullptr, std::memory_order_relaxed);
                if (_last_thread_registered) {
                    _last_thread_registered->is_last_registered = false;
                }
                _last_thread_registered = nullptr;
            }
        }

        void _register_pages() {
            bool delete_thread = false;
            Thread::Data* prev = nullptr;
            auto thread = _registered_threads;
            while(thread) {
                auto next = thread->next_registered;
                bool thread_deleted = thread->is_deleted.load(std::memory_order_acquire);
                _register_pages(thread->pages, thread->last_page_registered, thread_deleted);
                if (thread_deleted) {
                    if (!prev) {
                        _registered_threads = next;
                    } else {
                        prev->next_registered = next;
                    }
                    // To avoid ABA
                    if (!thread->is_last_registered) {
                        delete thread;
                        delete_thread = true;
                    } else {
                        thread->is_used = false;
                    }
                } else {
                    prev = thread;
                }
                thread = next;
            }
            if (delete_thread) {
                BlockAllocator::release_empty();
            }
        }

        void _register_pages(std::atomic<Page*>& pages, Page*& last_page_registered, bool thread_deleted) {
            auto first_page = pages.load(std::memory_order_relaxed);
            auto page = first_page;
            while(page != last_page_registered) {
                page->next_registered = _registered_pages;
                _registered_pages = page;
                page = page->next;
            }
            if (thread_deleted) {
                if (last_page_registered) {
                    last_page_registered->is_last_registered = false;
                    if (!last_page_registered->is_used) {
                        delete last_page_registered;
                    }
                }
            } else if (last_page_registered != first_page) {
                // To avoid ABA
                if (last_page_registered) {
                    last_page_registered->is_last_registered = false;
                    if (!last_page_registered->is_used) {
                        delete last_page_registered;
                    }
                }
                last_page_registered = first_page;
                last_page_registered->is_last_registered = true;
            }
        }

        size_t _register_objects() noexcept {
            size_t objects_created = 0;
            auto page = _registered_pages;
            while(page) {
                auto next = page->next_registered;
                if (page->is_used) {
                    if (page->object_created.load(std::memory_order_relaxed)) {
                        page->object_created.store(false, std::memory_order_relaxed);
                        auto states = page->states();
                        auto flags = page->flags();
                        auto count = page->flags_count();
                        for (unsigned i = 0; i < count; ++i) {
                            auto& flag = flags[i];
                            auto unregistered = ~flag.registered;
                            auto offset = i * Page::FlagBitCount;
                            auto object_count = page->metadata->object_count;
                            bool last = (i == count - 1);
                            while(unregistered) {
                                auto countr_zero = std::countr_zero(unregistered);
                                auto index = offset + countr_zero;
                                if (!last || index < object_count) {
                                    auto state = states[index].load(std::memory_order_relaxed);
                                    if (state & State::CreatedMask) {
                                        auto mask = Page::Flag(1) << countr_zero;
                                        flag.registered |= mask;
                                        if (state == State::Reachable) {
                                            page->state_updated.store(true, std::memory_order_relaxed);
                                        }
                                        ++objects_created;
                                    }
                                }
                                unregistered &= unregistered - 1;
                            }
                        }
                    }
                }
                page = next;
            }
            return objects_created;
        }

        void _update_states() {
            std::atomic_thread_fence(std::memory_order_acquire);
            auto page = _registered_pages;
            while(page) {
                page->clear_flags();
                if (page->state_updated.load(std::memory_order_relaxed)) {
                    page->state_updated.store(false, std::memory_order_relaxed);
                    auto states = page->states();
                    auto flags = page->flags();
                    auto count = page->flags_count();
                    for (unsigned i = 0; i < count; ++i) {
                        auto registered = flags[i].registered;
                        auto offset = i * Page::FlagBitCount;
                        while(registered) {
                            auto index = offset + std::countr_zero(registered);
                            auto state = states[index].load(std::memory_order_relaxed);
                            if (state == State::Reachable) {
                                states[index].store(State::Used, std::memory_order_relaxed);
                            }
                            registered &= registered - 1;
                        }
                    }
                }
                page = page->next_registered;
            }
        }

        void _update_hazard_pointers() {
            decltype(_hazard_pointers)().swap(_hazard_pointers);
            auto thread = _registered_threads;
            while(thread) {
                auto pointer = thread->hazard_pointer.load(std::memory_order_relaxed);
                if (pointer) {
                    _hazard_pointers.emplace_back((uintptr_t)pointer);
                }
                thread = thread->next_registered;
            }
            std::sort(_hazard_pointers.begin(), _hazard_pointers.end());
        }

        void _mark(const void* ptr) noexcept {
            if (ptr) {
                auto page = Page::page_of(ptr);
                auto index = page->index_of(ptr);
                auto flag_index = Page::flag_index_of(index);
                auto mask = Page::flag_mask_of(index);
                auto& flag = page->flags()[flag_index];
                if ((flag.registered & ~flag.marked & ~flag.reachable & mask)) {
                    flag.reachable |= mask;
                    if (!page->reachable) {
                        page->reachable = true;
                        page->next_reachable = _reachable_pages;
                        _reachable_pages = page;
                    }
                }
            }
        }

        void _mark_stack_roots() noexcept {
            auto thread = _registered_threads;
            while(thread) {
                if (!thread->is_deleted.load(std::memory_order_acquire)) {
                    auto& allocator = *thread->stack_roots_allocator;
                    for (size_t i = 0; i < std::size(allocator.is_used); ++i) {
                        auto used = allocator.is_used[i].load(std::memory_order_relaxed);
                        if (used) {
                            auto first = i * (StackPointerAllocator::PageSize / sizeof(RawPointer));
                            auto last = first + (StackPointerAllocator::PageSize / sizeof(RawPointer));
                            for (size_t index = first; index < last; ++index) {
                                _mark(allocator.data[index].load(std::memory_order_relaxed));
                            }
                        }
                    }
                }
                thread = thread->next_registered;
            }
        }

        void _mark_childs(void* ptr, const ChildPointers::Vector& offsets) noexcept {
            for (auto offset : offsets) {
                auto ap = (RawPointer*)((uintptr_t)ptr + offset);
                auto p = ap->load(std::memory_order_acquire);
                if ((size_t)p != std::numeric_limits<size_t>::max()) {
                    _mark(p);
                }
            }
        }

        void _mark_childs(void* ptr, const ChildPointers::Map& map) noexcept {
            for (unsigned index = 0; index < map.size(); ++index) {
                auto flags = map[index].load(std::memory_order_relaxed);
                if (flags) {
                    for (unsigned i = 0; i < 8; ++ i) {
                        unsigned mask = 1 << i;
                        if (flags & mask) {
                            auto offset = (index * 8 + i) * sizeof(RawPointer);
                            auto ap = (RawPointer*)((uintptr_t)ptr + offset);
                            auto p = ap->load(std::memory_order_relaxed);
                            if ((size_t)p != std::numeric_limits<size_t>::max()) {
                                _mark(p);
                            }
                        }
                    }
                }
            }
        }

        void _mark_childs(ChildPointers& pointers, void* ptr) noexcept {
            if (pointers.offsets) {
                _mark_childs(ptr, *pointers.offsets);
            } else {
                _mark_childs(ptr, pointers.map);
            }
        }

        void _mark_array_childs(void* ptr) noexcept {
            auto data = (uintptr_t)ptr;
            auto array = (ArrayBase*)data;
            auto metadata = array->metadata.load(std::memory_order_relaxed);
            if (metadata) {
                auto& pointers = metadata->child_pointers;
                _update_child_offsets(pointers);
                data += sizeof(ArrayBase);
                auto object_size = metadata->object_size;
                if (pointers.offsets) {
                    if (pointers.offsets->size()) {
                        for (size_t c = 0; c < array->count; ++c, data += object_size) {
                            _mark_childs((void*)data, *pointers.offsets);
                        }
                    }
                } else {
                    for (size_t c = 0; c < array->count; ++c, data += object_size) {
                        _mark_childs((void*)data, pointers.map);
                    }
                }
            }
        }

        void _mark_reachable() noexcept {
            auto page = _reachable_pages;
            _reachable_pages = nullptr;
            while(page) {
                _update_child_offsets(page->metadata->child_pointers);
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
                                flag.marked |= mask;
                                auto index = offset + countr_zero;
                                auto ptr = page->pointer_of(index);
                                if (page->metadata->is_array) {
                                    _mark_array_childs(ptr);
                                } else {
                                    _mark_childs(page->metadata->child_pointers, ptr);
                                }
                                ++_live_object_count;
                                if (_share_live_objects) {
                                    _live_objects.emplace_back(ptr);
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

        template<bool All>
        void _mark_updated() noexcept {
            std::atomic_thread_fence(std::memory_order_acquire);
            auto page = All ? _registered_pages : _unreachable_pages;
            while(page) {
                bool reachable_page = false;
                [[maybe_unused]] bool unreachable_page = false;
                if (All || page->state_updated.load(std::memory_order_relaxed)) {
                    auto states = page->states();
                    auto flags = page->flags();
                    auto count = page->flags_count();
                    for (unsigned i = 0; i < count; ++i) {
                        auto& flag = flags[i];
                        auto unreachable = flag.registered & ~flag.marked;
                        auto offset = i * Page::FlagBitCount;
                        while(unreachable) {
                            auto countr_zero = std::countr_zero(unreachable);
                            auto index = offset + countr_zero;
                            auto state = states[index].load(std::memory_order_relaxed);
                            if (state & State::ReachableMask) {
                                auto mask = Page::Flag(1) << countr_zero;
                                flag.reachable |= mask;
                                reachable_page = true;
                            } else if constexpr(All) {
                                unreachable_page = true;
                            }
                            unreachable &= unreachable - 1;
                        }
                    }
                }
                if constexpr(All) {
                    if (_hazard_pointers.size()) {
                        auto lower = std::lower_bound(_hazard_pointers.begin(), _hazard_pointers.end(), page->data);
                        auto upper = std::lower_bound(lower, _hazard_pointers.end(), page->data + PageDataSize);
                        for (auto i = lower; i != upper; ++i) {
                            auto index = page->index_of((void*)*i);
                            auto flag_index = Page::flag_index_of(index);
                            auto mask = Page::flag_mask_of(index);
                            auto& flag = page->flags()[flag_index];
                            flag.reachable |= mask;
                            reachable_page = true;
                        }
                    }
                }
                if (reachable_page && !page->reachable) {
                    page->reachable = true;
                    page->next_reachable = _reachable_pages;
                    _reachable_pages = page;
                }
                if constexpr(All) {
                    if (unreachable_page && !page->unreachable) {
                        page->unreachable = true;
                        page->next_unreachable = _unreachable_pages;
                        _unreachable_pages = page;
                    }
                    page = page->next_registered;
                } else {
                    page = page->next_unreachable;
                }
            }
        }

        inline static void _clear_childs(void* ptr, const ChildPointers::Vector& offsets) noexcept {
            for (auto offset : offsets) {
                auto p = (RawPointer*)((uintptr_t)ptr + offset);
                p->store(nullptr, std::memory_order_relaxed);
            }
        }

        inline static void _clear_childs(void* ptr, const ChildPointers::Map& map) noexcept {
            for (unsigned index = 0; index < map.size(); ++index) {
                auto flags = map[index].load(std::memory_order_relaxed);
                if (flags) {
                    for (unsigned i = 0; i < 8; ++ i) {
                        unsigned mask = 1 << i;
                        if (flags & mask) {
                            auto offset = (index * 8 + i) * sizeof(RawPointer);
                            auto p = (RawPointer*)((uintptr_t)ptr + offset);
                            p->store(nullptr, std::memory_order_relaxed);
                        }
                    }
                }
            }
        }

        inline static void _clear_childs(ChildPointers& childs, void* ptr) noexcept {
            if (childs.offsets) {
                _clear_childs(ptr, *childs.offsets);
            } else {
                _clear_childs(ptr, childs.map);
            }
        }

        inline static void _clear_array_childs(void* ptr) noexcept {
            auto data = (uintptr_t)ptr;
            auto array = (ArrayBase*)data;
            auto metadata = array->metadata.load(std::memory_order_relaxed);
            if (metadata) {
                auto& pointers = metadata->child_pointers;
                _update_child_offsets(pointers);
                data += sizeof(ArrayBase);
                auto object_size = metadata->object_size;
                if (pointers.offsets) {
                    if (pointers.offsets->size()) {
                        for (size_t c = 0; c < array->count; ++c, data += object_size) {
                            _clear_childs((void*)data, *pointers.offsets);
                        }
                    }
                } else {
                    for (size_t c = 0; c < array->count; ++c, data += object_size) {
                        _clear_childs((void*)data, pointers.map);
                    }
                }
            }
        }

        inline static void _destroy(Page* page, void* ptr, bool clear_childs) noexcept {
            auto destroy = page->metadata->destroy;
            if (destroy) {
                if (!page->metadata->is_array) {
                    if (clear_childs) {
                        _clear_childs(page->metadata->child_pointers, ptr);
                    }
                    destroy(ptr);
                } else {
                    auto array = (ArrayBase*)ptr;
                    auto metadata = array->metadata.load(std::memory_order_relaxed);
                    if (metadata && !metadata->tracked_ptrs_only) {
                        if (clear_childs) {
                            _clear_array_childs(ptr);
                        }
                        destroy(ptr);
                    }
                }
            }
        }

        size_t _remove_garbage() noexcept {
            std::atomic_thread_fence(std::memory_order_acquire);
            size_t removed = 0;
            auto page = _unreachable_pages;
            _unreachable_pages = nullptr;
            while(page) {
                _update_child_offsets(page->metadata->child_pointers);
                auto states = page->states();
                auto flags = page->flags();
                auto count = page->flags_count();
                for (unsigned i = 0; i < count; ++i) {
                    auto& flag = flags[i];
                    auto unreachable = flag.registered & ~flag.marked;
                    auto offset = i * Page::FlagBitCount;
                    if (unreachable) {
                        page->unused_occur.store(true, std::memory_order_relaxed);
                        do {
                            auto countr_zero = std::countr_zero(unreachable);
                            auto index = offset + countr_zero;
                            auto state = states[index].load(std::memory_order_relaxed);
                            assert(state < State::Reachable || state > State::UniqueLock);
                            if (state == State::Unreachable) {
                                _destroy(page, page->pointer_of(index), true);
                            }
                            ++removed;
                            states[index].store(State::Unused, std::memory_order_relaxed);
                            ++page->unused_counter_gc;
                            unreachable &= unreachable - 1;
                        } while(unreachable);
                    }
                    flag.registered &= flag.marked;
                }
                page->unreachable = false;
                page = page->next_unreachable;
            }
            std::atomic_thread_fence(std::memory_order_release);

            return removed;
        }

        void _release_unused_pages() {
            std::atomic_thread_fence(std::memory_order_acquire);
            Metadata* metadata = nullptr;
            auto page = _registered_pages;
            while(page) {
                if (page->unused_occur.load(std::memory_order_relaxed)) {
                    if (!page->on_empty_list.load(std::memory_order_relaxed)) {
                        auto count = page->metadata->object_count;
                        auto unused = page->unused_counter_gc;
                        unused += page->unused_atomic.load(std::memory_order_relaxed);
                        unused -= page->unused_counter_mutators;
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
                page = page->next_registered;
            }
            bool possible_empty_blocks = metadata != nullptr;
            while(metadata) {
                metadata->free(metadata->empty_page);
                metadata->used = false;
                metadata->empty_page = nullptr;
                metadata = metadata->next;
            }
            if (possible_empty_blocks) {
                BlockAllocator::release_empty();
            }
            Page* prev = nullptr;
            page = _registered_pages;
            while(page) {
                auto next = page->next_registered;
                if (!page->is_used) {
                    if (!prev) {
                        _registered_pages = next;
                    } else {
                        prev->next_registered = next;
                    }
                    // To avoid ABA
                    if (!page->is_last_registered) {
                        delete page;
                    }
                } else {
                    prev = page;
                }
                page = next;
            }
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
            Counter last_mem_allocated;
            do {
#if SGCL_LOG_PRINT_LEVEL >= 2
                auto start = std::chrono::high_resolution_clock::now();
#endif
                _register_threads();
                _register_pages();
                [[maybe_unused]] size_t last_objects_created = _register_objects();
                _update_states();
                _mark_stack_roots();
                _live_object_count = 0;
                do {
                    _mark_reachable();
                    if (!_unreachable_pages) {
                        _register_threads();
                        _register_pages();
                        last_objects_created += _register_objects();
                        _update_hazard_pointers();
                        _mark_updated<true>();
                    } else {
                        _mark_updated<false>();
                    }
                } while(_reachable_pages);
                _last_live_object_count.store(_live_object_count, std::memory_order_relaxed);
                size_t last_objects_removed = _remove_garbage();
                _release_unused_pages();
#if SGCL_LOG_PRINT_LEVEL >= 2
                auto end = std::chrono::high_resolution_clock::now();
                double duration = std::chrono::duration<double, std::milli>(end - start).count();
                total_time += duration;
                std::cout << "[sgcl] mem allocs:" << std::setw(7) << MemoryCounters::alloc_counter().count + last_mem_allocated.count
                          << ",    mem removed:" << std::setw(7) << MemoryCounters::free_counter().count
                          << ",    total mem:" << std::setw(7) << MemoryCounters::live_counter().count
                          << ",    objects created:" << std::setw(9) << last_objects_created
                          << ",    objects removed:" << std::setw(9) << last_objects_removed
                          << ",    live objects:" << std::setw(9) << _live_object_count
                          << ",    time:" << std::setw(8) << std::fixed << std::setprecision(3) << duration << "ms"
                          << ",    total time:" << std::setw(10) << std::fixed << std::setprecision(3) << total_time << "ms"
                          << std::endl;
#endif
                bool can_sleep = true;
                if (_forced_collect_count.load(std::memory_order_acquire)) {
                    if (_forced_collect_count.fetch_sub(1, std::memory_order_relaxed) == 1) {
                        _forced_collect_cv.notify_all();
                        if (_share_live_objects) {
#if SGCL_LOG_PRINT_LEVEL > 0
                            std::cout << "[sgcl] suspended collector id: " << std::this_thread::get_id() << std::endl;
#endif
                            std::unique_lock<std::mutex> lock(_mutex);
                            _forced_collect_cv.wait(lock, [this]{
                                return !_paused;
                            });
                            _share_live_objects = false;
#if SGCL_LOG_PRINT_LEVEL > 0
                            std::cout << "[sgcl] resumed collector id: " << std::this_thread::get_id() << std::endl;
#endif
                        }
                    }
                    else {
                        if (_forced_collect_count.load(std::memory_order_relaxed) == 1) {
                            assert(_live_objects.size() == 0);
                            if (_live_objects_request) {
                                _live_objects_request = false;
                                _share_live_objects = true;
                            }
                        }
                        can_sleep = false;
                    }
                }
                if (!_terminating && can_sleep) {
                    sleep_flag.store(true, std::memory_order_relaxed);
                    std::unique_lock<std::mutex> lock(sleep_mutex);
                    sleep_cv.wait_for(lock, config::MaxSleepTime, [this]{
                        return !sleep_flag.load(std::memory_order_acquire)
                            || _forced_collect_count.load(std::memory_order_relaxed)
                            || _terminating.load(std::memory_order_relaxed);
                    });
                }
                last_mem_allocated = MemoryCounters::alloc_counter();
                MemoryCounters::reset_all();
                if (!last_objects_removed && _terminating) {
                    if (_live_object_count) {
                        --finalization_counter;
                    } else {
                        finalization_counter = 0;
                    }
                }
            } while(finalization_counter);
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] stop collector id: " << std::this_thread::get_id() << std::endl;
#endif
            if (_terminating) {
                std::lock_guard<std::mutex> lock(_mutex);
                _terminated = true;
                _terminate_cv.notify_all();
            }
        }

        void _force_collect() noexcept {
            _forced_collect_count.store(2, std::memory_order_release);
            waking_up();
        }

        void _terminate() noexcept {
            if (!_terminating.load()) {
#if SGCL_LOG_PRINT_LEVEL > 0
                std::cout << "[sgcl] terminate collector from id: " << std::this_thread::get_id() << std::endl;
#endif
                _terminating.store(true);
                waking_up();
                std::unique_lock<std::mutex> lock(_mutex);
                _terminate_cv.wait(lock, [this]{
                    return _terminated;
                });
            }
        }

        Thread::Data* _registered_threads = {nullptr};
        Thread::Data* _last_thread_registered = {nullptr};
        Page* _reachable_pages = {nullptr};
        Page* _unreachable_pages = {nullptr};
        Page* _registered_pages = {nullptr};
        std::atomic<int> _forced_collect_count = {0};
        std::condition_variable _forced_collect_cv;
        std::condition_variable _terminate_cv;
        std::mutex _mutex;
        std::vector<void*> _live_objects;
        size_t _live_object_count;
        std::atomic<size_t> _last_live_object_count = {0};
        std::atomic<bool> _live_objects_request = {false};
        bool _paused = {false};
        bool _share_live_objects = {false};
        inline static std::atomic<bool> _created = {false};
        inline static std::atomic<bool> _terminating = {false};
        bool _terminated = {false};
        std::mutex sleep_mutex;
        std::condition_variable sleep_cv;
        std::atomic<bool> sleep_flag = {true};
        std::vector<uintptr_t> _hazard_pointers;

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
}
