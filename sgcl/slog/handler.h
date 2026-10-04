//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "level.h"
#include "record.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "../io/stream.h"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace sgcl::slog {
    class handler;

    namespace req {
        // What takes records (slog.Handler): handle(const record&),
        // called on a const object, and, if it has one, enabled(level),
        // asked before a record is made (none: every level). T is the
        // type as passed, references and const looked through. Neither a
        // lambda (it has no handle) nor a tracked_ptr to a handler is
        // one: the class below takes such a pointer, the concept asks of
        // the type itself.
        template<class T>
        concept handler = requires(const std::remove_cvref_t<T>& h, const record& r) {
            h.handle(r);
        };
    }

    namespace detail {
        struct HandlerTable {
            void (*handle)(void* object, const record& r);
            bool (*enabled)(void* object, slog::level l);
        };

        // The table of an empty handler: a call is a mistake of the
        // program, a logic_error (an empty table rather than a null one:
        // a held handler's call has no test on its way)
        [[noreturn]] inline void no_handler() {
            throw std::logic_error("sgcl::slog::handler: no handler is held");
        }

        inline constexpr HandlerTable EmptyHandlerTable{
            [](void*, const record&) {
                no_handler();
            },
            [](void*, slog::level) -> bool {
                no_handler();
            }};

        template<class T>
        const HandlerTable& handler_table() noexcept {
            static constexpr HandlerTable table{
                [](void* object, const record& r) {
                    static_cast<const T*>(object)->handle(r);
                },
                [](void* object, slog::level l) -> bool {
                    if constexpr (requires(const T& t) { { t.enabled(l) } -> std::convertible_to<bool>; }) {
                        return static_cast<const T*>(object)->enabled(l);
                    } else {
                        (void)object;
                        (void)l;
                        return true;
                    }
                }};
            return table;
        }

        // A handle of the library (slog::memory): copied into the handler,
        // since a copy is the same object
        template<class T>
        inline constexpr bool IsHandlerHandle = false;

        template<class T>
        struct HandlerBox {
            template<class U>
            SGCL_INLINE_HOT explicit HandlerBox(U&& u) noexcept(std::is_nothrow_constructible_v<T, U&&>)
            : value(std::forward<U>(u)) {
            }

            T value;
        };

        template<class T>
        inline constexpr bool IsTracked = false;

        template<class T>
        inline constexpr bool IsTracked<tracked_ptr<T>> = true;

        // Whether handler's constructor cannot throw: only the copy of a
        // temporary or a handle of the library into its box can, the
        // copy being the program's
        template<class H>
        SGCL_INLINE_HOT constexpr bool nothrow_handler() noexcept {
            using S = std::remove_cvref_t<H>;
            if constexpr (IsTracked<S> || (std::is_lvalue_reference_v<H&&> && !IsHandlerHandle<S>)) {
                return true;
            } else {
                return std::is_nothrow_constructible_v<S, H&&>;
            }
        }
    }

    // Any handler as a value, the way io::writer holds any writer: a
    // tracked_ptr of what keeps it, a pointer to it and a table of its two
    // methods, made once per type; nothing virtual. A handler given by
    // tracked_ptr is held by it; one of your own given by reference is
    // referenced (its managed object kept, if it lies in one; one on a
    // stack or a global is yours to keep alive); a temporary or a handle
    // of the library (slog::memory) is copied into a managed object of
    // its own. handle() is called on the thread
    // that logs, from every thread that logs: a handler of your own takes
    // records from many at once.
    class handler {
    public:
        handler() noexcept = default;

        template<class H>
        requires (!std::is_same_v<std::remove_cvref_t<H>, handler>)
              && (req::handler<H> || (detail::IsTracked<std::remove_cvref_t<H>> && req::handler<typename std::remove_cvref_t<H>::element_type>))
        handler(H&& h) noexcept(detail::nothrow_handler<H>()) {
            using S = std::remove_cvref_t<H>;
            if constexpr (detail::IsTracked<S>) {
                using T = std::remove_cv_t<typename S::element_type>;
                _owner = tracked_ptr<const void>(h);
                _object = const_cast<void*>(static_cast<const void*>(h.get()));
                _table = &detail::handler_table<T>();
            } else if constexpr (std::is_lvalue_reference_v<H&&> && !detail::IsHandlerHandle<S>) {
                void* p = const_cast<void*>(static_cast<const void*>(std::addressof(h)));
                _owner = io::detail::owner_of(p);
                _object = p;
                _table = &detail::handler_table<S>();
            } else {
                tracked_ptr<detail::HandlerBox<S>> box = make_tracked<detail::HandlerBox<S>>(std::forward<H>(h));
                _object = std::addressof(box->value);
                _owner = tracked_ptr<const void>(box);
                _table = &detail::handler_table<S>();
            }
        }

        SGCL_INLINE_HOT void handle(const record& r) const {
            _table->handle(_object, r);
        }

        SGCL_INLINE_HOT bool enabled(slog::level l) const {
            return _table->enabled(_object, l);
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return _table != &detail::EmptyHandlerTable;
        }

    private:
        tracked_ptr<const void> _owner;
        void* _object = nullptr;
        const detail::HandlerTable* _table = &detail::EmptyHandlerTable;
    };
}
