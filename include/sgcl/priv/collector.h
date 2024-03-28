//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include "../configuration.h"
#include "array.h"
#include "counter.h"
#include "timer.h"
#include "thread.h"
#include "unique_ptr.h"
#include "maker.h"

#include <condition_variable>
#include <thread>

#include <iostream>
#if SGCL_LOG_PRINT_LEVEL
#endif

namespace sgcl {
    namespace Priv {
        inline Collector& Collector_instance();

        struct Collector {
            Collector() {
                if (!created()) {
                    _created = true;
                    std::thread([this]{_main_loop();}).detach();
                }
            }

            ~Collector() {
                _terminate();
            }

            void force_collect(bool wait) noexcept {
#if SGCL_LOG_PRINT_LEVEL
                std::cout << "[sgcl] force collect " << (wait ? "and wait " : "") << "from id: " << std::this_thread::get_id() << std::endl;
#endif
                if (wait) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    if (!_terminating.load(std::memory_order_relaxed)) {
                        _forced_collect_count.store(3, std::memory_order_release);
                        _forced_collect_cv.wait(lock, [this]{
                            return _forced_collect_count.load(std::memory_order_relaxed) == 0;
                        });
                    }
                } else {
                    _forced_collect_count.store(3, std::memory_order_release);
                }
            }

            int64_t live_object_count() const noexcept {
                return _live_object_count.load(std::memory_order_acquire);
            }

            void live_objects(Unique_ptr<Tracked_ptr[]>& array) noexcept {
                std::unique_lock<std::mutex> lock(_mutex);
                if (!_terminating.load(std::memory_order_relaxed)) {
                    _live_objects_ref.store(&array, std::memory_order_relaxed);
                    _forced_collect_count.store(3, std::memory_order_release);
                    _forced_collect_cv.wait(lock, [this]{
                        return _forced_collect_count.load(std::memory_order_relaxed) == 0;
                    });
                    _live_objects_ref.store(nullptr, std::memory_order_release);
                }
            }

            void static terminate() noexcept {
                if (created()) {
                    Collector_instance()._terminate();
                }
            }

            inline static bool terminated() noexcept {
                return _terminating.load(std::memory_order_acquire);
            }

            inline static bool created() {
                return _created.load(std::memory_order_acquire);
            }

        private:
            Counter _alloc_counter() const {
                Counter allocated;
                auto data = Thread::threads_data.load(std::memory_order_acquire);
                while(data) {
                    allocated.count += data->alloc_count.load(std::memory_order_relaxed);
                    allocated.size += data->alloc_size.load(std::memory_order_relaxed);
                    data = data->next;
                }
                return allocated + _allocated_rest;
            }

            void _update_child_offsets(Child_pointers& pointers) {
                if (!pointers.offsets && pointers.final.load(std::memory_order_acquire)) {
                    pointers.offsets = new Child_pointers::Vector;
                    for (unsigned index = 0; index < pointers.map.size(); ++index) {
                        auto flags = pointers.map[index].load(std::memory_order_relaxed);
                        if (flags) {
                            for (unsigned i = 0; i < 8; ++ i) {
                                auto mask = uint8_t(1) << i;
                                if (flags & mask) {
                                    auto offset = (index * 8 + i) * sizeof(Pointer);
                                    pointers.offsets->emplace_back(offset);
                                }
                            }
                        }
                    }
                }
            }

            void _check_threads() noexcept {
                Thread::Data* prev = nullptr;
                auto thread = Thread::threads_data.load(std::memory_order_acquire);
                while(thread) {
                    auto next = thread->next;
                    if (thread->is_used.load(std::memory_order_relaxed)) {
                        prev = thread;
                    } else {
                        if (!prev) {
                            auto rdata = thread;
                            if (!Thread::threads_data.compare_exchange_strong(rdata, next, std::memory_order_relaxed, std::memory_order_acquire)) {
                                while(rdata->next != thread) {
                                    rdata = rdata->next;
                                }
                                prev = rdata;
                            }
                        }
                        if (prev) {
                            prev->next = next; // NOLINT(clang-analyzer-cplusplus.NewDelete)
                        }
                        _allocated_rest.count += thread->alloc_count.load(std::memory_order_relaxed);
                        _allocated_rest.size += thread->alloc_size.load(std::memory_order_relaxed);
                        delete thread;
                    }
                    thread = next;
                }
            }

            void _update_states() {
                static Timer timer;
                bool atomic = _terminating;
                if (timer.duration() >= DeletionDelayMsec / (State::ReachableAtomic - 2)) {
                    atomic = true;
                    timer.reset();
                }
                std::atomic_thread_fence(std::memory_order_acquire);
                auto page = _registered_pages;
                while(page) {
                    auto states = page->states();
                    auto flags = page->flags();
                    auto count = page->metadata->object_count;
                    for (unsigned i = 0; i < count; ++i) {
                        auto state = states[i].load(std::memory_order_relaxed);
                        if (state >= State::Reachable && state <= State::ReachableAtomic) {
                            auto index = Page::flag_index_of(i);
                            auto mask = Page::flag_mask_of(i);
                            auto& flag = flags[index];
                            if (flag.registered & mask) {
                                if (state > State::Reachable) {
                                    if (atomic) {
                                        states[i].store((State)(state - 1), std::memory_order_relaxed);
                                    }
                                } else {
                                    states[i].store(State::Used, std::memory_order_relaxed);
                                }
                            }
                        }
                    }
                    page = page->next_registered;
                }
                std::atomic_thread_fence(std::memory_order_release);
            }

            void _register_objects() noexcept {
                Page* prev = nullptr;
                auto page = Object_allocator::pages.load(std::memory_order_acquire);
                while(page) {
                    auto next = page->next;
                    if (page->is_used) {
                        page->clear_flags();
                        auto states = page->states();
                        auto flags = page->flags();
                        auto count = page->metadata->object_count;
                        for (unsigned i = 0; i < count; ++i) {
                            auto state = states[i].load(std::memory_order_relaxed);
                            if (state >= State::Reachable && state <= State::BadAlloc) {
                                auto index = Page::flag_index_of(i);
                                auto mask = Page::flag_mask_of(i);
                                auto& flag = flags[index];
                                if (!(flag.registered & mask)) {
                                    flag.registered |= mask;
                                    if (!page->registered) {
                                        page->registered = true;
                                        page->next_registered = _registered_pages;
                                        _registered_pages = page;
                                    }
                                }
                            }
                        }
                        prev = page;
                    } else {
                        if (!prev) {
                            auto rpage = page;
                            if (!Object_allocator::pages.compare_exchange_strong(rpage, next, std::memory_order_relaxed, std::memory_order_acquire)) {
                                while(rpage->next != page) {
                                    rpage = rpage->next;
                                }
                                prev = rpage;
                            }
                        }
                        if (prev) {
                            prev->next = next; // NOLINT(clang-analyzer-cplusplus.NewDelete)
                        }
                        delete page;
                    }
                    page = next;
                }
            }

            void _mark(const void* p) noexcept {
                if (p) {
                    auto page = Page::page_of(p);
                    auto index = page->index_of(p);
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
                auto data = Thread::threads_data.load(std::memory_order_acquire);
                while(data) {
                    for (auto& p: data->stack_roots_allocator->pages) {
                        auto page = p.load(std::memory_order_acquire);
                        if (page) {
                            for (auto& p: *page) {
                                _mark(p.load(std::memory_order_acquire));
                            }
                        }
                    }
                    data = data->next;
                }
            }

            void _mark_heap_roots() noexcept {
                auto node = Heap_roots_allocator::pages.load(std::memory_order_acquire);
                while(node) {
                    for (auto& p: node->page) {
                        _mark(p.load(std::memory_order_acquire));
                    }
                    node = node->next;
                }
            }

            void _mark_childs(uintptr_t data, const Child_pointers::Vector& offsets) noexcept {
                for (auto offset : offsets) {
                    auto ap = (Pointer*)(data + offset);
                    auto p = ap->load(std::memory_order_acquire);
                    if ((size_t)p != std::numeric_limits<size_t>::max()) {
                        _mark(p);
                    }
                }
            }

            void _mark_childs(uintptr_t data, const Child_pointers::Map& map) noexcept {
                for (unsigned index = 0; index < map.size(); ++index) {
                    auto flags = map[index].load(std::memory_order_acquire);
                    if (flags) {
                        for (unsigned i = 0; i < 8; ++ i) {
                            unsigned mask = 1 << i;
                            if (flags & mask) {
                                auto offset = (index * 8 + i) * sizeof(Pointer);
                                auto ap = (Pointer*)(data + offset);
                                auto p = ap->load(std::memory_order_acquire);
                                if ((size_t)p != std::numeric_limits<size_t>::max()) {
                                    _mark(p);
                                }
                            }
                        }
                    }
                }
            }

            void _mark_childs(Child_pointers& pointers, uintptr_t data) noexcept {
                if (pointers.offsets) {
                    _mark_childs(data, *pointers.offsets);
                } else {
                    _mark_childs(data, pointers.map);
                }
            }

            void _mark_array_childs(uintptr_t data) noexcept {
                auto array = (Array_base*)data;
                auto metadata = array->metadata.load(std::memory_order_acquire);
                if (metadata) {
                    auto& pointers = metadata->child_pointers;
                    _update_child_offsets(pointers);
                    data += sizeof(Array_base);
                    auto object_size = metadata->object_size;
                    if (pointers.offsets) {
                        if (pointers.offsets->size()) {
                            for (size_t c = 0; c < array->count; ++c, data += object_size) {
                                _mark_childs(data, *pointers.offsets);
                            }
                        }
                    } else {
                        for (size_t c = 0; c < array->count; ++c, data += object_size) {
                            _mark_childs(data, pointers.map);
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
                                        _mark_childs(page->metadata->child_pointers, page->data_of(index));
                                        if (page->metadata->is_array) {
                                            _mark_array_childs(page->data_of(index));
                                        }
                                        marked = true;
                                        if (_live_objects_request) {
                                            auto p = (void*)page->data_of(index);
                                            _live_objects.emplace_back(p);
                                        }
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

            void _mark_updated() noexcept {
                auto page = _registered_pages;
                while(page) {
                    bool reachable = false;
                    auto states = page->states();
                    auto flags = page->flags();
                    auto count = page->metadata->object_count;
                    for (unsigned i = 0; i < count; ++i) {
                        auto state = states[i].load(std::memory_order_relaxed);
                        if (state >= State::Reachable && state <= State::UniqueLock) {
                            auto index = Page::flag_index_of(i);
                            auto mask = Page::flag_mask_of(i);
                            auto& flag = flags[index];
                            if (flag.registered & ~flag.marked & mask) {
                                flag.reachable |= mask;
                                reachable = true;
                            }
                        }
                    }
                    if (reachable && !page->reachable) {
                        page->reachable = true;
                        page->next_reachable = _reachable_pages;
                        _reachable_pages = page;
                    }
                    page = page->next_registered;
                }
            }

            void _clear_childs(uintptr_t data, const Child_pointers::Vector& offsets) noexcept {
                for (auto offset : offsets) {
                    auto p = (Pointer*)(data + offset);
                    p->store(nullptr, std::memory_order_relaxed);
                }
            }

            void _clear_childs(uintptr_t data, const Child_pointers::Map& map) noexcept {
                for (unsigned index = 0; index < map.size(); ++index) {
                    auto flags = map[index].load(std::memory_order_acquire);
                    if (flags) {
                        for (unsigned i = 0; i < 8; ++ i) {
                            unsigned mask = 1 << i;
                            if (flags & mask) {
                                auto offset = (index * 8 + i) * sizeof(Pointer);
                                auto p = (Pointer*)(data + offset);
                                p->store(nullptr, std::memory_order_relaxed);
                            }
                        }
                    }
                }
            }

            void _clear_childs(Child_pointers& pointers, uintptr_t data) noexcept {
                if (pointers.offsets) {
                    _clear_childs(data, *pointers.offsets);
                } else {
                    _clear_childs(data, pointers.map);
                }
            }

            void _clear_array_childs(uintptr_t data) noexcept {
                auto array = (Array_base*)data;
                auto metadata = array->metadata.load(std::memory_order_acquire);
                if (metadata) {
                    auto& pointers = metadata->child_pointers;
                    _update_child_offsets(pointers);
                    data += sizeof(Array_base);
                    auto object_size = metadata->object_size;
                    if (pointers.offsets) {
                        if (pointers.offsets->size()) {
                            for (size_t c = 0; c < array->count; ++c, data += object_size) {
                                _clear_childs(data, *pointers.offsets);
                            }
                        }
                    } else {
                        for (size_t c = 0; c < array->count; ++c, data += object_size) {
                            _clear_childs(data, pointers.map);
                        }
                    }
                }
            }

            Counter _remove_garbage() noexcept {
                Counter released;
                auto page = _registered_pages;
                while(page) {
                    _update_child_offsets(page->metadata->child_pointers);
                    auto destroy = page->metadata->destroy;
                    auto states = page->states();
                    auto data = page->data;
                    auto object_size = page->metadata->object_size;
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
                                        if (destroy) {
                                            auto data = page->data_of(index);
                                            if (!page->metadata->is_array) {
                                                _clear_childs(page->metadata->child_pointers, data);
                                                if (state != State::Destroyed) {
                                                    destroy((void*)data);
                                                }
                                            } else {
                                                auto array = (Array_base*)data;
                                                auto metadata = array->metadata.load(std::memory_order_acquire);
                                                if (metadata && !metadata->tracked_ptrs_only) {
                                                    _mark_array_childs(data);
                                                    if (state != State::Destroyed) {
                                                        destroy((void*)data);
                                                    }
                                                }
                                            }
                                        }
                                        released.count++;
                                        if (!page->metadata->is_array || object_size != sizeof(Array<PageDataSize>)) {
                                            released.size += object_size;
                                        } else {
                                            auto array = (Array_base*)data;
                                            auto metadata = array->metadata.load(std::memory_order_acquire);
                                            released.size += sizeof(Array_base) + metadata->object_size * array->count;
                                        }
                                    }
                                    states[index].store(State::Unused, std::memory_order_release);
                                }
                            }
                            flag.registered &= flag.marked;
                        }
                    }
                    page = page->next_registered;
                }
                return released;
            }

            void _release_unused_pages() {
                Metadata* metadata = nullptr;
                auto page = _registered_pages;
                while(page) {
                    if (!page->on_empty_list.load(std::memory_order_acquire)) {
                        auto states = page->states();
                        auto count = page->metadata->object_count;
                        for (unsigned i = 0; i < count; ++i) {
                            auto state = states[i].load(std::memory_order_relaxed);
                            if (state == State::Unused) {
                                page->on_empty_list.store(true, std::memory_order_relaxed);
                                if (!page->metadata->empty_page) {
                                    page->metadata->next = metadata;
                                    metadata = page->metadata;
                                }
                                page->next_empty = page->metadata->empty_page;
                                page->metadata->empty_page = page;
                                break;
                            }
                        }
                    }
                    page = page->next_registered;
                }
                while(metadata) {
                    metadata->free(metadata->empty_page);
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
                    } else {
                        prev = page;
                    }
                    page = next;
                }
            }

            void _main_loop() noexcept {
                static constexpr int64_t MinLiveSize = PageSize;
                static constexpr int64_t MinLiveCount = MinLiveSize / sizeof(uintptr_t) * 2;
#if SGCL_LOG_PRINT_LEVEL
                std::cout << "[sgcl] start collector id: " << std::this_thread::get_id() << std::endl;
#endif
                using namespace std::chrono_literals;
                int finalization_counter = 5;
                Counter allocated;
                Counter removed;
                Counter last3_removed[3];
                do {
#if SGCL_LOG_PRINT_LEVEL >= 2
                    auto start = std::chrono::high_resolution_clock::now();
#endif
                    _check_threads();
                    _update_states();
                    _register_objects();
                    _mark_stack_roots();
                    _mark_heap_roots();
                    do {
                        _mark_reachable();
                        _mark_updated();
                    } while(_reachable_pages);
                    Counter last_removed = _remove_garbage();
                    last3_removed[0] = last3_removed[1];
                    last3_removed[1] = last3_removed[2];
                    last3_removed[2] = last_removed;
                    Counter max_removed = max(last3_removed[0], max(last3_removed[1], last3_removed[2]));
                    _release_unused_pages();
                    Counter last_allocated = _alloc_counter() - allocated;
                    Counter live = allocated + last_allocated - (removed + last_removed);
                    _live_object_count.store(live.count, std::memory_order_release);
                    assert(live.count >= 0 && live.size >= 0);
#if SGCL_LOG_PRINT_LEVEL >= 2
                    auto end = std::chrono::high_resolution_clock::now();
                    std::cout << "[sgcl] live objects: " << live.count << ", allocated: " << last_allocated.count << ", destroyed: " << last_removed.count << ", time: "
                              << std::chrono::duration<double, std::milli>(end - start).count() << "ms"
                              << std::endl;
#endif
                    Timer timer;
                    do {
                        if (_terminating) {
                            break;
                        }
                        auto forceed_count = _forced_collect_count.load(std::memory_order_relaxed);
                        if (forceed_count) {
                            forceed_count--;
                            _forced_collect_count.store(forceed_count, std::memory_order_relaxed);
                            if (!forceed_count) {
                                std::lock_guard<std::mutex> lock(_mutex);
                                if (_live_objects_request) {
                                    if (_live_objects.size()) {
                                        auto array = Maker<Tracked_ptr[]>::make_tracked(_live_objects.size());
                                        for (size_t i = 0; i < _live_objects.size(); ++i) {
                                            array[i].store(_live_objects[i]);
                                        }
                                        auto ref = _live_objects_ref.load();
                                        *ref = std::move(array);
                                    }
                                    std::vector<void*>().swap(_live_objects);
                                    _live_objects_request = false;
                                }
                                _forced_collect_cv.notify_all();
                            }
                            else {
                                if (forceed_count == 1 && _live_objects_ref) {
                                    std::vector<void*>().swap(_live_objects);
                                    _live_objects_request = true;
                                }
                                break;
                            }
                        }
                        if ((std::max(last_allocated.count, last_removed.count) * 100 / SGCL_TRIGER_PERCENTAGE >= live.count + MinLiveCount)
                            || (std::max(last_allocated.size, last_removed.size) * 100 / SGCL_TRIGER_PERCENTAGE >= live.size + MinLiveSize)) {
                            break;
                        }
                        if (max_removed.count > last_allocated.count * 2 && max_removed.count > MinLiveCount
                            && timer.duration() >= DeletionDelayMsec / (State::ReachableAtomic - 1)) {
                            break;
                        }
                        std::this_thread::sleep_for(1ms);
                        last_allocated = _alloc_counter() - allocated;
                        live = allocated + last_allocated - (removed + last_removed);
                    } while(!live.count || (timer.duration() < SGCL_MAX_SLEEP_TIME_SEC * 1000));
                    allocated += last_allocated;
                    removed += last_removed;
                    if (!last_removed.count && _terminating) {
                        if (live.count) {
                            --finalization_counter;
                        } else {
                            finalization_counter = 0;
                        }
                    }
                } while(finalization_counter);
#if SGCL_LOG_PRINT_LEVEL
                std::cout << "[sgcl] stop collector id: " << std::this_thread::get_id() << std::endl;
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
#if SGCL_LOG_PRINT_LEVEL
                    std::cout << "[sgcl] terminate collector from id: " << std::this_thread::get_id() << std::endl;
#endif
                    _terminating.store(true, std::memory_order_release);
                    _terminate_cv.wait(lock, [this]{
                        return _terminated;
                    });
                }
            }

            Page* _reachable_pages = {nullptr};
            Page* _registered_pages = {nullptr};
            Counter _allocated_rest;
            std::atomic<int> _forced_collect_count = {0};
            std::condition_variable _forced_collect_cv;
            std::condition_variable _terminate_cv;
            bool _terminated = {false};
            std::mutex _mutex;
            std::atomic<int64_t> _live_object_count = {0};
            inline static std::atomic<bool> _terminating = {false};
            inline static std::atomic<bool> _created = {false};
            std::vector<void*> _live_objects;
            std::atomic<Unique_ptr<Tracked_ptr[]>*> _live_objects_ref = {nullptr};
            bool _live_objects_request = {false};
        };

        inline Collector& Collector_instance() {
            static Collector collector_instance;
            return collector_instance;
        }

        inline void Collector_init() {
            Collector_instance();
        }

        inline void Terminate_collector() {
            Collector::terminate();
        }

        inline void Delete_unique(const void* p) {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            auto data = Page::base_address_of(p);
            auto destroy = page->metadata->destroy;
            if (destroy) {
                destroy(data);
            }
            Page::set_state(p, State::Destroyed);
        }
    }
}
