//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../tracked_ptr.h"

#include <concepts>
#include <type_traits>

namespace sgcl {
    namespace detail {
        // The tag of a handle's constructor from its word (HandleWord::make)
        struct FromWord {
            explicit FromWord() = default;
        };

        // The word of a one-word handle, for the atomics (atomic.h,
        // atomic_ref.h). A handle is a public type made of one tracked
        // word — a string, io::file, net::connection, async::channel, an
        // object's identity inside — whose copies share the object the
        // word addresses. A handle takes part by befriending HandleWord,
        // naming its word to it (`_handle_word()`, the tracked_ptr member
        // by reference, const and not) and taking a constructor from a
        // word, `H(detail::FromWord, const tracked_ptr<State>&)`, private
        // like the rest. HandleWord never makes an object: the word made
        // into a handle is one loaded from, or stored into, an atomic of
        // that handle's type.
        struct HandleWord {
            template<class H>
            static auto word(H& h) noexcept -> decltype(h._handle_word()) {
                return h._handle_word();
            }

            template<class H, class W>
            static auto make(const W& w) noexcept -> decltype(H(FromWord{}, w)) {
                return H(FromWord{}, w);
            }
        };

        template<class W>
        struct IsTrackedWord : std::false_type {};

        template<class T>
        struct IsTrackedWord<tracked_ptr<T>> : std::true_type {};
    }

    namespace req {
        // A handle of one tracked word that names its word to HandleWord
        // and is made from one: the type sgcl::atomic and sgcl::atomic_ref
        // specialize for, over the word (atomic.h). The layout is the
        // word's: standard, one word, so that an atomic of the handle is
        // the word's atomic and nothing beside it.
        template<class H>
        concept handle = std::is_standard_layout_v<H> && sizeof(H) == sizeof(tracked_ptr<void>)
            && requires(H& h, const H& c) {
                { detail::HandleWord::word(h) } -> std::same_as<std::remove_cvref_t<decltype(detail::HandleWord::word(h))>&>;
                requires detail::IsTrackedWord<std::remove_cvref_t<decltype(detail::HandleWord::word(h))>>::value;
                { detail::HandleWord::word(c) } -> std::same_as<const std::remove_cvref_t<decltype(detail::HandleWord::word(h))>&>;
                { detail::HandleWord::make<H>(detail::HandleWord::word(c)) } -> std::same_as<H>;
            };
    }

    namespace detail {
        // The word type of a handle, and the object type it addresses
        // without its const: the atomics operate on a word of the latter
        // (the object is never written through the atomic)
        template<req::handle H>
        using HandleWordOf = std::remove_cvref_t<decltype(HandleWord::word(std::declval<H&>()))>;

        template<req::handle H>
        using HandleStateOf = std::remove_const_t<typename HandleWordOf<H>::element_type>;
    }
}
