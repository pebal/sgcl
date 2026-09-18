//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ExpiryQueue<T>: a callback for an object the
// collector has found unreachable, run by Drain() on the thread that
// calls it, with the object back alive for the call (Java's Cleaner,
// Go's SetFinalizer, the way a pool takes its objects back).
#pragma once

#include "../../containers/expiry_queue.h"
#include "../Core/Ptr.h"

namespace Sgcl {
    template<class T>
    class ExpiryQueue {
    public:
        using ValueType = Ptr<T>;
        using InnerType = sgcl::expiry_queue<T>;
        using SizeType = size_t;

        // The handle of a watch: Cancel() before the object expires, and
        // the callback never runs
        class Entry {
        public:
            Entry() noexcept = default;

            explicit Entry(typename InnerType::entry e) noexcept
            : _e(std::move(e)) {
            }

            bool Cancel() noexcept {
                return _e.cancel();
            }

            bool IsExpired() const noexcept {
                return _e.expired();
            }

            WeakPtr<T> Weak() const noexcept {
                return WeakPtr<T>(_e.weak());
            }

            explicit operator bool() const noexcept {
                return (bool)_e;
            }

            typename InnerType::entry& Inner() noexcept {
                return _e;
            }

        private:
            typename InnerType::entry _e;
        };

        ExpiryQueue() = default;
        ExpiryQueue(ExpiryQueue&&) noexcept = default;
        ExpiryQueue& operator=(ExpiryQueue&&) noexcept = default;
        ExpiryQueue(const ExpiryQueue&) = delete;
        ExpiryQueue& operator=(const ExpiryQueue&) = delete;

        // `onExpire(Ptr<T>)` once the object is found unreachable, from
        // Drain(); the object is alive again for the call
        template<class F>
        Entry Watch(const Ptr<T>& object, F&& onExpire) {
            return Entry(_q.watch(object.Inner(), [f = std::forward<F>(onExpire)](sgcl::tracked_ptr<T> p) mutable { f(Ptr<T>(std::move(p))); }));
        }

        // The callbacks of the expired objects run: how many
        SizeType Drain() {
            return _q.drain();
        }

        SizeType Count() const noexcept {
            return _q.size();
        }

        bool IsEmpty() const noexcept {
            return _q.empty();
        }

        void Clear() noexcept {
            _q.clear();
        }

        InnerType& Inner() noexcept {
            return _q;
        }

        const InnerType& Inner() const noexcept {
            return _q;
        }

    private:
        InnerType _q;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

