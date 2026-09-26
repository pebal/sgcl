//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/collector.h"
#include "detail/frame_word.h"
#include "detail/maker.h"
#include "root_ptr.h"
#include "tracked_ptr.h"
#include "unique_ptr.h"

#include <coroutine>
#include <type_traits>
#include <utility>

namespace sgcl::detail {
    // The words in front of every managed frame's buffer (managed_frame:
    // operator new; the coroutine's own frame starts past them): a header
    // that belongs to whoever drives the coroutine. The core gives it no
    // layout, only its length; the async module lays its header out there
    // (async/coroutine.h: FrameHeader, the executor the frame runs on, its
    // task-locals, the link of an executor's queue) and asserts that it
    // fits. A coroutine type that needs no header (the generator) leaves
    // the words zero, which is what null tracked words are. Four words, a
    // multiple of sixteen bytes, so that the coroutine's frame past them
    // stays as aligned as the buffer (array_base.h: sixteen), which is
    // what operator new promises a frame.
    inline constexpr size_t FrameHeaderWords = 4;
    static_assert(FrameHeaderWords * sizeof(FrameWord) % 16 == 0);

    // A frame is named by its buffer's address (the promise's `self`,
    // every queue's and waiter's word): the header is there, the
    // coroutine's frame, what the handle addresses, four words past it.
    // A pointer into the middle of a buffer keeps nothing (README, rule
    // 4), which is why the frame's pointers are the buffer's and the
    // handle is computed, not the other way round
    inline std::coroutine_handle<> handle_of(void* frame) noexcept {
        return std::coroutine_handle<>::from_address((FrameWord*)frame + FrameHeaderWords);
    }

    inline FrameWord* frame_of_handle(void* address) noexcept {
        return (FrameWord*)address - FrameHeaderWords;
    }
}

namespace sgcl {
    // The frame of a coroutine is allocated with operator new and is no
    // place for a tracked_ptr (README, "The rules"): a promise type that
    // derives from managed_frame gets its frames from the managed heap
    // instead, as buffers of words traced conservatively (detail/frame_word.h),
    // so the tracked_ptr parameters, locals and promise members of the
    // coroutine are roots for as long as the frame is held. A frame is
    // held through a frame_ptr (below), made from the coroutine handle in
    // get_return_object; it leaves operator new in the state of an object
    // a unique_ptr owns (a root), which the frame_ptr takes over. operator
    // delete does nothing for a frame taken over: destroying the coroutine
    // (frame_ptr::destroy, the handle's destroy) runs the destructors of
    // its locals and promise, and the memory is the collector's once
    // nothing holds it; a frame nothing took over (an exception before
    // get_return_object) is freed at once. The promise keeps the frame's
    // own tracked_ptr (`self`, set by the frame_ptr that takes it over):
    // a cycle of one, which holds nothing alive, and the word an awaiter
    // copies to hold the frame while the coroutine waits on a channel or
    // sits on the scheduler's queue (a raw handle would not do: the frame
    // is a managed array, which the checks of the raw constructor of
    // tracked_ptr do not accept).
    // The buffer is four words longer than the frame, and the frame starts
    // past them: the first four words are the header (detail::FrameHeaderWords;
    // async's FrameHeader: the executor the frame runs on, its task-locals,
    // the link of an executor's queue), zero at the allocation like the
    // rest of the buffer, which is what null tracked words are. `self`,
    // the entries of the queues and the words of the waiters address the
    // buffer, as a container's pointer does; the handle's address is four
    // words further (detail::handle_of).
    struct managed_frame {
        static void* operator new(size_t size) {
            auto words = (size + sizeof(detail::FrameWord) - 1) / sizeof(detail::FrameWord);
            return detail::Maker<detail::FrameWord[]>::make_tracked_data(words + detail::FrameHeaderWords).release() + detail::FrameHeaderWords;
        }

        static void operator delete(void* p, size_t) noexcept {
            auto frame = detail::frame_of_handle(p);
            if (detail::Page::is_unique(frame)) {
                detail::Collector::delete_unique(frame);
            }
        }

        tracked_ptr<detail::FrameWord> self;
    };

    namespace detail {
        // The frame of a coroutine from its typed handle: the promise
        // must derive from managed_frame (the rule of every wait: the
        // tracked pointers of the frame are roots only there)
        template<class P>
        tracked_ptr<FrameWord> frame_of(std::coroutine_handle<P> h) noexcept {
            static_assert(std::is_base_of_v<managed_frame, P>, "a coroutine that waits (on a channel, on a task, on the scheduler) must have a managed frame: derive its promise from sgcl::managed_frame, or use sgcl::async::task");
            return h.promise().self;
        }
    }

    // The owner of a coroutine whose promise derives from managed_frame:
    // a root_ptr to the frame and the coroutine handle. Move-only;
    // destroys the coroutine when destroyed, which runs the destructors of
    // its locals and promise. A root_ptr, so that the handle lives
    // anywhere: a task in a std::vector of tasks, in an object on the
    // unmanaged heap, in a managed object or in another frame; a cell
    // per handle (root_ptr.h), which a handle, one per coroutine, can
    // afford.
    template<class Promise>
    class frame_ptr {
    public:
        using promise_type = Promise;
        using handle_type = std::coroutine_handle<Promise>;

        frame_ptr() noexcept = default;

        explicit frame_ptr(handle_type h)
        : _frame(_take(detail::frame_of_handle(h.address())))
        , _handle(h) {
            h.promise().self = _frame.ptr();
        }

        frame_ptr(frame_ptr&& o) noexcept
        : _frame(std::move(o._frame))
        , _handle(std::exchange(o._handle, {})) {
        }

        frame_ptr& operator=(frame_ptr&& o) noexcept {
            if (this != &o) {
                destroy();
                _frame = std::move(o._frame);
                _handle = std::exchange(o._handle, {});
            }
            return *this;
        }

        frame_ptr(const frame_ptr&) = delete;
        frame_ptr& operator=(const frame_ptr&) = delete;

        ~frame_ptr() {
            destroy();
        }

        // The handle's interface: whether there is a coroutine, its handle
        // and promise, resume() and done()
        explicit operator bool() const noexcept {
            return (bool)_handle;
        }

        handle_type handle() const noexcept {
            return _handle;
        }

        Promise& promise() const {
            return _handle.promise();
        }

        void resume() {
            _handle.resume();
        }

        bool done() const noexcept {
            return !_handle || _handle.done();
        }

        // Runs the destructors of the coroutine's locals and promise and
        // lets go of the frame
        void destroy() noexcept {
            if (_handle) {
                _handle.destroy();
                _handle = {};
            }
            _frame = nullptr;
        }

        // Lets go of the frame without destroying the coroutine: it goes
        // on wherever it is (a task detached); the handle given back, as
        // unique_ptr::release gives the pointer, no longer keeps the frame
        [[nodiscard]] handle_type release() noexcept {
            _frame = nullptr;
            return std::exchange(_handle, {});
        }

    private:
        // The frame, from the state operator new left it in (owned by a
        // unique_ptr) to a tracked one: the same path a container's buffer
        // takes (vector.h: _allocate)
        static tracked_ptr<detail::FrameWord> _take(detail::FrameWord* frame) {
            return unique_ptr<detail::FrameWord>(detail::UniquePtr<detail::FrameWord>(frame));
        }

        root_ptr<detail::FrameWord> _frame;
        handle_type _handle;
    };
}
