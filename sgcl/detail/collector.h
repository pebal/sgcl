//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "child_pointers.h"
#include "timer.h"
#include "thread.h"
#include "max_filter.h"
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

        bool force_collect(bool wait) noexcept {
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] force collect " << (wait ? "and wait " : "") << "from id: " << std::this_thread::get_id() << std::endl;
#endif
            if (wait) {
                if (!_paused.load(std::memory_order_acquire)) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    if (!_terminating.load(std::memory_order_relaxed)) {
                        _forced_collect_count.store(3, std::memory_order_release);
                        _forced_collect_cv.wait(lock, [this]{
                            return _forced_collect_count.load(std::memory_order_relaxed) == 0;
                        });
                    }
                } else {
                    return false;
                }
            } else {
                _forced_collect_count.store(3, std::memory_order_release);
            }
            return true;
        }

        size_t last_living_objects_number() const noexcept {
            return _last_living_objects_number.load(std::memory_order_acquire);
        }

        std::tuple<PauseGuard, std::vector<void*>> living_objects() noexcept {
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] get live objects from id: " << std::this_thread::get_id() << std::endl;
#endif
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_terminating.load(std::memory_order_relaxed)) {
                _living_objects_request = true;
                _forced_collect_count.store(3, std::memory_order_release);
                _forced_collect_cv.wait(lock, [this]{
                    return _forced_collect_count.load(std::memory_order_relaxed) == 0;
                });
            }
            return {
                PauseGuard(&_mutex, [&](std::mutex* m) {
                    std::unique_lock<std::mutex> lock(*m);
                    _paused.store(false, std::memory_order_relaxed);
                    _forced_collect_cv.notify_all();
                })
              , std::move(_living_objects)
            };
        }

        void static terminate() noexcept {
            if (created()) {
                collector_instance()._terminate();
            }
        }

        inline static bool terminated() noexcept {
            return _terminating.load(std::memory_order_acquire);
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

        void _update_states() {
            static Timer timer;
            bool atomic = _terminating;
            if (timer.duration() >= config::AtomicDeletionDelayMsec / (State::ReachableAtomic - 2)) {
                atomic = true;
                timer.reset();
            }
            std::atomic_thread_fence(std::memory_order_acquire);
            auto page = _registered_pages;
            while(page) {
                page->clear_flags();
                if (page->state_updated.load(std::memory_order_relaxed)) {
                    page->state_updated.store(false, std::memory_order_relaxed);
                    bool updated = false;
                    auto states = page->states();
                    auto flags = page->flags();
                    auto count = page->flags_count();
                    for (unsigned i = 0; i < count; ++i) {
                        auto& flag = flags[i];
                        for (unsigned j = 0; j < Page::FlagBitCount; ++j) {
                            auto mask = Page::Flag(1) << j;
                            auto index = i * Page::FlagBitCount + j;
                            auto state = states[index].load(std::memory_order_relaxed);
                            if (state >= State::Reachable && state <= State::ReachableAtomic) {
                                if (flag.registered & mask) {
                                    if (state > State::Reachable) {
                                        if (atomic) {
                                            states[index].store((State)(state - 1), std::memory_order_relaxed);
                                        }
                                        updated = true;
                                    } else {
                                        states[index].store(State::Used, std::memory_order_relaxed);
                                    }
                                } else {
                                    updated = true;
                                }
                            }
                        }
                    }
                    if (updated) {
                        page->state_updated.store(true, std::memory_order_relaxed);
                    }
                }
                page = page->next_registered;
            }
        }

        void _register_threads() {
            auto first_thread = Thread::threads_data.load(std::memory_order_acquire);
            auto thread = first_thread;
            while(thread && thread != _last_thread_registered) {
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
            Thread::Data* prev = nullptr;
            auto thread = _registered_threads;
            while(thread) {
                auto next = thread->next_registered;
                bool is_deleted = thread->is_deleted.load(std::memory_order_acquire);
                _register_pages(thread->pages, thread->last_page_registered, is_deleted);
                if (is_deleted) {
                    if (!prev) {
                        _registered_threads = next;
                    } else {
                        prev->next_registered = next;
                    }
                    // To avoid ABA
                    if (!thread->is_last_registered) {
                        delete thread;
                    } else {
                        thread->is_used = false;
                    }
                } else {
                    prev = thread;
                }
                thread = next;
            }
        }

        void _register_pages(std::atomic<Page*>& pages, Page*& last_page_registered, bool thread_deleted) {
            auto first_page = pages.load(std::memory_order_relaxed);
            auto page = first_page;
            while(page && page != last_page_registered) {
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
            Page* prev = nullptr;
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
                            if (unregistered) {
                                auto flagBitCount = Page::FlagBitCount;
                                if (i == count - 1) {
                                    auto object_count = (i + 1) * Page::FlagBitCount;
                                    if (object_count > page->metadata->object_count) {
                                        flagBitCount -= object_count - page->metadata->object_count;
                                    }
                                }
                                for (unsigned j = 0; j < flagBitCount; ++j) {
                                    auto mask = Page::Flag(1) << j;
                                    if (unregistered & mask) {
                                        auto index = i * Page::FlagBitCount + j;
                                        assert(index < page->metadata->object_count);
                                        auto state = states[index].load(std::memory_order_relaxed);
                                        if (state >= State::Reachable && state <= State::BadAlloc) {
                                            flag.registered |= mask;
                                            ++objects_created;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                page = next;
            }
            return objects_created;
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
            auto data = _registered_threads;
            while(data) {
                auto& allocator = *data->stack_roots_allocator;
                for (size_t i = 0; i < std::size(allocator.is_used); ++i) {
                    auto used = allocator.is_used[i].load(std::memory_order_acquire);
                    if (used) {
                        auto first = i * (StackPointerAllocator::PageSize / sizeof(RawPointer));
                        auto last = first + (StackPointerAllocator::PageSize / sizeof(RawPointer));
                        for (size_t index = first; index < last; ++index) {
                            _mark(allocator.data[index].load(std::memory_order_relaxed));
                        }
                    }
                }
                data = data->next_registered;
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
                auto flags = map[index].load(std::memory_order_acquire);
                if (flags) {
                    for (unsigned i = 0; i < 8; ++ i) {
                        unsigned mask = 1 << i;
                        if (flags & mask) {
                            auto offset = (index * 8 + i) * sizeof(RawPointer);
                            auto ap = (RawPointer*)((uintptr_t)ptr + offset);
                            auto p = ap->load(std::memory_order_acquire);
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
            auto metadata = array->metadata.load(std::memory_order_acquire);
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
                            for (unsigned j = 0; j < Page::FlagBitCount; ++j) {
                                auto mask = Page::Flag(1) << j;
                                if (flag.reachable & mask) {
                                    flag.marked |= mask;
                                    auto index = i * Page::FlagBitCount + j;
                                    auto ptr = page->pointer_of(index);
                                    if (page->metadata->is_array) {
                                        _mark_array_childs(ptr);
                                    } else {
                                        _mark_childs(page->metadata->child_pointers, ptr);
                                    }
                                    ++_living_objects_number;
                                    if (_share_living_objects) {
                                        _living_objects.emplace_back(ptr);
                                    }
                                    marked = true;
                                }
                            }
                            flag.reachable &= ~flag.marked;
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
                        if (unreachable) {
                            for (unsigned j = 0; j < Page::FlagBitCount; ++j) {
                                auto mask = Page::Flag(1) << j;
                                if (unreachable & mask) {
                                    auto index = i * Page::FlagBitCount + j;
                                    auto state = states[index].load(std::memory_order_relaxed);
                                    if (state >= State::Reachable && state <= State::UniqueLock) {
                                        flag.reachable |= mask;
                                        reachable_page = true;
                                    } else if constexpr(All) {
                                        unreachable_page = true;
                                    }
                                }
                            }
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
                auto flags = map[index].load(std::memory_order_acquire);
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
            auto metadata = array->metadata.load(std::memory_order_acquire);
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
                    auto metadata = array->metadata.load(std::memory_order_acquire);
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
                    if (unreachable) {
                        for (unsigned j = 0; j < Page::FlagBitCount; ++j) {
                            auto mask = Page::Flag(1) << j;
                            if (unreachable & mask) {
                                auto index = i * Page::FlagBitCount + j;
                                auto state = states[index].load(std::memory_order_relaxed);
                                assert(state < State::Reachable || state > State::UniqueLock);
                                if (state != State::BadAlloc) {
                                    if (state != State::Destroyed) {
                                        _destroy(page, page->pointer_of(index), true);
                                    }
                                }
                                ++removed;
                                states[index].store(State::Unused, std::memory_order_relaxed);
                                page->unused_occur.store(true, std::memory_order_relaxed);
                            }
                        }
                        flag.registered &= flag.marked;
                    }
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
                    auto empty = 0u;
                    auto states = page->states();
                    auto count = page->metadata->object_count;
                    for (unsigned i = 0; i < count; ++i) {
                        auto state = states[i].load(std::memory_order_relaxed);
                        if (state == State::Unused) {
                            ++empty;
                            if (empty > count / 2) {
                                page->unused_occur.store(false, std::memory_order_relaxed);
                                if (!page->metadata->used) {
                                    page->metadata->used = true;
                                    page->metadata->next = metadata;
                                    metadata = page->metadata;
                                }
                                if (!page->on_empty_list.load(std::memory_order_relaxed)) {
                                    page->on_empty_list.store(true, std::memory_order_relaxed);
                                    page->next_empty = page->metadata->empty_page;
                                    page->metadata->empty_page = page;
                                }
                                break;
                            }
                        }
                    }
                }
                page = page->next_registered;
            }
            while(metadata) {
                metadata->free(metadata->empty_page);
                metadata->used = false;
                metadata->empty_page = nullptr;
                metadata = metadata->next;
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

#if SGCL_LOG_PRINT_LEVEL >= 4
        void _check_mem() noexcept {
            size_t used_counter = 0;
            size_t reachable_counter = 0;
            size_t reachable_atomic_counter = 0;
            size_t unique_lock_counter = 0;
            size_t destroyed_counter = 0;
            size_t bad_alloc_counter = 0;
            size_t reserved_counter = 0;
            size_t unused_counter = 0;
            size_t on_empty_list = 0;
            size_t is_used = 0;
            size_t registered = 0;
            size_t counter = 0;
            auto page = _registered_pages;
            while(page) {
                ++counter;
                auto next = page->next_registered;
                if (page->on_empty_list.load(std::memory_order_acquire)) {
                    ++on_empty_list;
                }
                if (page->is_used) {
                    ++is_used;
                }
                auto states = page->states();
                auto flags = page->flags();
                auto count = page->flags_count();
                for (unsigned i = 0; i < count; ++i) {
                    auto& flag = flags[i];
                    auto flagBitCount = Page::FlagBitCount;
                    if (i == count - 1) {
                        auto object_count = (i + 1) * Page::FlagBitCount;
                        if (object_count > page->metadata->object_count) {
                            flagBitCount -= object_count - page->metadata->object_count;
                        }
                    }
                    for (unsigned j = 0; j < flagBitCount; ++j) {
                        auto mask = Page::Flag(1) << j;
                        auto index = i * Page::FlagBitCount + j;
                        assert(index < page->metadata->object_count);
                        auto state = states[index].load(std::memory_order_relaxed);
                        switch(state) {
                        case Used: ++used_counter; break;
                        case Reachable: ++reachable_counter; break;
                        case ReachableAtomic: ++reachable_atomic_counter; break;
                        case UniqueLock: ++unique_lock_counter; break;
                        case Destroyed: ++destroyed_counter; break;
                        case BadAlloc: ++bad_alloc_counter; break;
                        case Reserved: ++reserved_counter; break;
                        case Unused: ++unused_counter; break;
                        default:
                            break;
                        }
                        if (flag.registered & mask) {
                            ++registered;
                        }
                    }
                }
                page = next;
            }
            std::cout << "[sgcl] _check_mem() -------------------" << std::endl;
            std::cout <<  "            used_counter: " << used_counter << std::endl;
            std::cout <<  "       reachable_counter: " << reachable_counter << std::endl;
            std::cout <<  "reachable_atomic_counter: " << reachable_atomic_counter << std::endl;
            std::cout <<  "     unique_lock_counter: " << unique_lock_counter << std::endl;
            std::cout <<  "       destroyed_counter: " << destroyed_counter << std::endl;
            std::cout <<  "       bad_alloc_counter: " << bad_alloc_counter << std::endl;
            std::cout <<  "        reserved_counter: " << reserved_counter << std::endl;
            std::cout <<  "          unused_counter: " << unused_counter << std::endl;
            std::cout <<  "           on_empty_list: " << on_empty_list << std::endl;
            std::cout <<  "                 is_used: " << is_used << std::endl;
            std::cout <<  "              registered: " << registered << std::endl;
            std::cout <<  "                 counter: " << counter << std::endl;
            std::cout << "[sgcl] _check_mem() -------------------" << std::endl;
        }
#endif
#if SGCL_LOG_PRINT_LEVEL >= 2
        double total_time = 0;
#endif
        void _main_loop() noexcept {
            static constexpr int64_t MinMemSize = config::PageSize * Block::PageCount * 4;
            static constexpr int64_t MinMemCount = 4;
            static constexpr int64_t MinObjectsCount = config::PageSize / sizeof(uintptr_t) * 2;
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] start collector id: " << std::this_thread::get_id() << std::endl;
#endif
            using namespace std::chrono_literals;
            int finalization_counter = 5;
            MaxFilter<size_t, 3> max_mem_removed;
            MaxFilter<size_t, 3> max_objects_removed;
            Counter last_mem_allocated;
            do {
#if SGCL_LOG_PRINT_LEVEL >= 2
                auto start = std::chrono::high_resolution_clock::now();
#endif
                _update_states();
                _register_threads();
                _register_pages();
                size_t last_objects_created = _register_objects();
                _mark_stack_roots();
                _living_objects_number = 0;
                do {
                    _mark_reachable();
                    if (_unreachable_pages) {
                        _mark_updated<false>();
                    } else {
                        _mark_updated<true>();
                    }
                } while(_reachable_pages);
                size_t last_living_objects = _last_living_objects_number.load(std::memory_order_relaxed);
                _last_living_objects_number.store(_living_objects_number, std::memory_order_relaxed);
                size_t last_objects_removed = _remove_garbage();
                max_objects_removed.add(last_objects_removed);
                _release_unused_pages();
                Counter last_mem_removed = MemoryCounters::free_counter();
                Counter last_mem_lived = MemoryCounters::live_counter();
                max_mem_removed.add(last_mem_removed.count);
#if SGCL_LOG_PRINT_LEVEL >= 2
                auto end = std::chrono::high_resolution_clock::now();
                double duration = std::chrono::duration<double, std::milli>(end - start).count();
                total_time += duration;
                std::cout << "[sgcl] mem allocs:" << std::setw(6) << MemoryCounters::alloc_counter().count + last_mem_allocated.count
                          << ",    mem removed:" << std::setw(6) << last_mem_removed.count
                          << ",    total mem:" << std::setw(6) << last_mem_lived.count
                          << ",    objects created:" << std::setw(9) << last_objects_created
                          << ",    objects removed:" << std::setw(9) << last_objects_removed
                          << ",    living objects:" << std::setw(9) << _living_objects_number
                          << ",    time:" << std::setw(8) << std::fixed << std::setprecision(3) << duration << "ms"
                          << ",    total time:" << std::setw(10) << std::fixed << std::setprecision(3) << total_time << "ms"
                          << std::endl;
#endif
                Timer timer;
                do {
                    if (_forced_collect_count.load(std::memory_order_acquire)) {
                        if (_forced_collect_count.fetch_sub(1, std::memory_order_relaxed) == 1) {
                            {
                                std::unique_lock<std::mutex> lock(_mutex);
                                _forced_collect_cv.notify_all();
                            }
                            if (_share_living_objects) {
#if SGCL_LOG_PRINT_LEVEL > 0
                                std::cout << "[sgcl] suspended collector id: " << std::this_thread::get_id() << std::endl;
#endif
                                _paused.store(true, std::memory_order_relaxed);
                                std::unique_lock<std::mutex> lock(_mutex);
                                _forced_collect_cv.wait(lock, [this]{
                                    return !_paused.load(std::memory_order_relaxed);
                                });
                                _share_living_objects = false;
#if SGCL_LOG_PRINT_LEVEL > 0
                                std::cout << "[sgcl] resumed collector id: " << std::this_thread::get_id() << std::endl;
#endif
                            }
                        }
                        else {
                            if (_forced_collect_count.load(std::memory_order_relaxed) == 1) {
                                assert(_living_objects.size() == 0);
                                if (_living_objects_request) {
                                    _living_objects_request = false;
                                    _share_living_objects = true;
                                }
                            }
                            break;
                        }
                    }
                    if (_terminating) {
                        break;
                    }
                    last_mem_allocated = MemoryCounters::alloc_counter();
                    if ((std::max(last_mem_allocated.count, last_mem_removed.count) * 100 / SGCL_TRIGER_PERCENTAGE >= last_mem_lived.count + MinMemCount)
                     || (std::max(last_mem_allocated.size, last_mem_removed.size) * 100 / SGCL_TRIGER_PERCENTAGE >= last_mem_lived.size + MinMemSize)
                     || (std::max(last_objects_created, last_objects_removed) * 100 / SGCL_TRIGER_PERCENTAGE >= last_living_objects + MinObjectsCount)) {
                        break;
                    }
                    if (((max_mem_removed.get() > (last_mem_allocated.count * 2 + MinMemCount))
                     || (max_objects_removed.get() > (last_objects_created * 2 + MinObjectsCount)))
                     && timer.duration() >= config::AtomicDeletionDelayMsec / (State::ReachableAtomic - 1)) {
                        break;
                    }
                    std::this_thread::sleep_for(1ms);
                } while(timer.duration() < SGCL_MAX_SLEEP_TIME_SEC * 1000);
                last_mem_allocated = MemoryCounters::alloc_counter();
                MemoryCounters::reset_all();
                if (!last_objects_removed && _terminating) {
                    if (_living_objects_number) {
                        --finalization_counter;
                    } else {
                        finalization_counter = 0;
                    }
                }
            } while(finalization_counter);
#if SGCL_LOG_PRINT_LEVEL > 0
            std::cout << "[sgcl] stop collector id: " << std::this_thread::get_id() << std::endl;
#endif
#if SGCL_LOG_PRINT_LEVEL >= 4
        _check_mem();
#endif

            if (_terminating) {
                std::lock_guard<std::mutex> lock(_mutex);
                _terminated = true;
                _terminate_cv.notify_all();
            }
        }

        void _terminate() noexcept {
            std::unique_lock<std::mutex> lock(_mutex);
            if (!_terminating.load(std::memory_order_relaxed)) {
#if SGCL_LOG_PRINT_LEVEL > 0
                std::cout << "[sgcl] terminate collector from id: " << std::this_thread::get_id() << std::endl;
#endif
                _terminating.store(true, std::memory_order_release);
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
        bool _terminated = {false};
        std::mutex _mutex;
        std::vector<void*> _living_objects;
        size_t _living_objects_number;
        std::atomic<size_t> _last_living_objects_number = {0};
        std::atomic<bool> _living_objects_request = {false};
        std::atomic<bool> _paused = {false};
        bool _share_living_objects = {false};
        inline static std::atomic<bool> _terminating = {false};
        inline static std::atomic<bool> _created = {false};

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
}
