//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "metadata.h"

#include <cassert>
#include <cstring>

namespace sgcl::detail {
    struct Page {
        template<class T>
        using Info = TypeInfo<T>;

        using Flag = uint64_t;
        static constexpr unsigned FlagBitCount = sizeof(Flag) * 8;

        struct Flags {
            Flag registered = {0};
            Flag reachable = {0};
            Flag marked = {0};
        };

        template<class T>
        Page(Block* block, T* data) noexcept
        : metadata(&Info<T>::private_metadata())
        , block(block)
        , data((uintptr_t)data)
        , multiplier((1ull << 32 | 0x10000) / metadata->object_size) {
            assert(metadata != nullptr);
            assert(data != nullptr);
            std::memset(this->states(), State::Reserved, metadata->object_count);
            std::memset(this->flags(), 0, sizeof(Flags) * this->flags_count());
        }

        ~Page() noexcept {
            if constexpr(!std::is_trivially_destructible_v<std::atomic<State>>) {
                auto states = this->states();
                std::destroy(states, states + metadata->object_count);
            }
        }

        std::atomic<State>* states() const noexcept {
            return (std::atomic<State>*)(this + 1);
        }

        Flags* flags() const noexcept {
            auto states_size = (sizeof(std::atomic<State>) * metadata->object_count + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
            return (Flags*)((uintptr_t)states() + states_size);
        }

        unsigned flags_count() const noexcept {
            return (metadata->object_count + FlagBitCount - 1) / FlagBitCount;
        }

        void clear_flags() noexcept {
            auto flags = this->flags();
            auto count = flags_count();
            for (unsigned i = 0; i < count; ++i) {
                flags[i].reachable = 0;
                flags[i].marked = 0;
            }
        }

        static constexpr unsigned flag_index_of(unsigned i) noexcept {
            return i / FlagBitCount;
        }

        static constexpr Flag flag_mask_of(unsigned i) noexcept {
            return Flag(1) << (i % FlagBitCount);
        }

        unsigned index_of(const void* p) noexcept {
            assert(p != nullptr);
            return ((uintptr_t)p - data) * multiplier >> 32;
        }

        void* pointer_of(unsigned index) noexcept {
            return (void*)(data + index * metadata->object_size);
        }

        static Page* page_of(const void* p) noexcept {
            assert(p != nullptr);
            auto page = ((uintptr_t)p & ~(uintptr_t)(config::PageSize - 1));
            return *((Page**)page);
        }

        static Metadata& metadata_of(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            return *page->metadata;
        }

        static void* base_address_of(const void* p) noexcept {
            assert(p != nullptr);
            auto page = page_of(p);
            auto index = page->index_of(p);
            return page->pointer_of(index);
        }

        template<State S>
        static void set_state(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            auto index = page->index_of(p);
            auto &state = page->states()[index];
            if constexpr(S == State::UniqueLock
                      || S == State::BadAlloc) {
                state.store(S, std::memory_order_relaxed);
                page->object_created.store(true, std::memory_order_release);
            } else if constexpr(S == State::Reachable) {
                state.store(S, std::memory_order_relaxed);
                page->state_updated.store(true, std::memory_order_release);
            } else {
                state.store(S, std::memory_order_release);
            }
        }

        static bool is_unique(const void* p) noexcept {
            assert(p != nullptr);
            auto page = Page::page_of(p);
            auto index = page->index_of(p);
            auto &state = page->states()[index];
            return state.load(std::memory_order_acquire) == State::UniqueLock;
        }

        Metadata* const metadata;
        Block* const block;
        const uintptr_t data;
        const uint64_t multiplier;
        size_t alloc_size = 0;
        bool reachable = {false};
        bool unreachable = {false};
        bool is_used = {true};
        bool is_last_registered = {false};
        std::atomic_bool object_created = {false};
        std::atomic_bool state_updated = {false};
        std::atomic_bool on_empty_list = {false};
        std::atomic_bool unused_occur = {true};
        unsigned int unused_counter_mutators = {0};
        unsigned int unused_counter_gc = {0};
        std::atomic_uint unused_atomic = {0};
        Page* next_reachable = {nullptr};
        Page* next_unreachable = {nullptr};
        Page* next_registered = {nullptr};
        Page* next_empty = {nullptr};
        Page* next_unused = {nullptr};
        Page* next_atomic = {nullptr};
        Page* next = {nullptr};
    };
}
