//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/root_ptr.h"
#include "channel.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <initializer_list>
#include <map>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#define SGCL_SIGNALS_POSIX 1
#else
#define SGCL_SIGNALS_POSIX 0
#endif

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // The signals of the process as a channel, the way Go's os/signal has
    // them: `signals({SIGINT, SIGTERM})` is a channel that gets the number
    // of every signal of those delivered to the process from then on, so
    // that a task `co_await`s it, a thread receives on it, a select takes
    // it as a case: the shutdown of a server is `co_await
    // select(sigint->on_receive([&] { running = false; }), ...)`.
    // The delivery: the handler installed for the number does nothing but
    // write the number to a pipe (the one thing a handler may safely do),
    // and one thread of the module reads the pipe and sends the number on
    // every channel registered for it, without waiting: a channel that is
    // full drops the signal, so a burst is coalesced to the capacity of
    // the channel (one, by default, as Go recommends; the kernel coalesces
    // a burst of the same signal as well, a pending signal being one bit).
    // `reset_signals({...})` gives the numbers back their disposition
    // from before the first registration (Go's signal.Reset),
    // `ignore_signals({...})` sets them to be ignored (signal.Ignore);
    // both forget the channels registered for the numbers, which are not
    // closed, since a channel may serve other numbers still. The channel
    // is held by the registration until then: it lives while the process
    // listens. POSIX only for now (Windows, with the platform matrix).
    namespace detail {
        // One registration: the channel, through the object that holds it
        struct SignalWait {
            tracked_ptr<void> keep;
            ChannelState<int>* ch = nullptr;
        };

        class Signals {
        public:
            SGCL_INLINE_HOT ~Signals() {
                stop();
            }

            // The channel registered for the numbers: their handler
            // installed (the disposition saved once, for reset), the
            // thread and the pipe started on the first registration. A
            // pipe or a thread that cannot be made is a std::system_error,
            // thrown before any handler is installed. Not on Windows yet:
            // a std::system_error at the call (an assertion at compile
            // time would be one in every program that includes async.h)
            void notify(std::initializer_list<int> numbers, const tracked_ptr<void>& keep, ChannelState<int>* ch) {
#if SGCL_SIGNALS_POSIX
                std::lock_guard lock(_m);
                _start();
                root_ptr<SignalWait> w = make_tracked<SignalWait>();
                w->keep = keep;
                w->ch = ch;
                for (int n : numbers) {
                    _save(n);
                    struct sigaction sa = {};
                    sa.sa_handler = _handler;
                    sigemptyset(&sa.sa_mask);
                    sa.sa_flags = SA_RESTART;   // a system call of the program interrupted by the signal is restarted, as Go does it
                    ::sigaction(n, &sa, nullptr);
                    _waits[n].push_back(w);
                }
#else
                (void)numbers; (void)keep; (void)ch;
                throw std::system_error(std::make_error_code(std::errc::function_not_supported), "signals: POSIX only for now (Windows is to come with the platform matrix)");
#endif
            }

            // The disposition from before the first registration back
            // (every number registered, for an empty list), the channels
            // registered for the numbers forgotten
            // noexcept, as ignore: the std::mutex fails only when misused
            // (a lock taken twice), which this never does; a disposition
            // saved is memory, never thrown (DESIGN 356)
            void reset(std::initializer_list<int> numbers) noexcept {
#if SGCL_SIGNALS_POSIX
                std::lock_guard lock(_m);
                if (numbers.size() == 0) {
                    for (auto& [n, saved] : _saved) {
                        ::sigaction(n, &saved, nullptr);
                    }
                    _saved.clear();
                    _waits.clear();
                    return;
                }
                for (int n : numbers) {
                    if (auto it = _saved.find(n); it != _saved.end()) {
                        ::sigaction(n, &it->second, nullptr);
                        _saved.erase(it);
                    }
                    _waits.erase(n);
                }
#else
                (void)numbers;
#endif
            }

            // One channel's registration dropped, for every number it was
            // registered for (Go's signal.Stop): a number no channel is
            // registered for any more gets back the disposition from
            // before the first registration; the channels of the others
            // stay as they are
            void forget(const ChannelState<int>* ch) {
#if SGCL_SIGNALS_POSIX
                std::lock_guard lock(_m);
                for (auto it = _waits.begin(); it != _waits.end();) {
                    auto& ws = it->second;
                    std::erase_if(ws, [ch](const root_ptr<SignalWait>& w) { return w->ch == ch; });
                    if (!ws.empty()) {
                        ++it;
                        continue;
                    }
                    if (auto s = _saved.find(it->first); s != _saved.end()) {
                        ::sigaction(it->first, &s->second, nullptr);
                        _saved.erase(s);
                    }
                    it = _waits.erase(it);
                }
#else
                (void)ch;
#endif
            }

            // The numbers ignored (the disposition before the first
            // registration still saved, for reset), their channels forgotten
            void ignore(std::initializer_list<int> numbers) noexcept {
#if SGCL_SIGNALS_POSIX
                std::lock_guard lock(_m);
                for (int n : numbers) {
                    _save(n);
                    struct sigaction sa = {};
                    sa.sa_handler = SIG_IGN;
                    sigemptyset(&sa.sa_mask);
                    ::sigaction(n, &sa, nullptr);
                    _waits.erase(n);
                }
#else
                (void)numbers;
#endif
            }

            // The thread joined and the pipe closed, the dispositions
            // given back: the end of the program
            void stop() {
#if SGCL_SIGNALS_POSIX
                std::unique_lock lock(_m);
                if (!_thread.joinable()) {
                    return;
                }
                for (auto& [n, saved] : _saved) {
                    ::sigaction(n, &saved, nullptr);
                }
                _saved.clear();
                _waits.clear();
                _pipe.store(-1, std::memory_order_release);   // before the pipe is closed: a handler still running (a signal delivered before the dispositions went back, on another thread) writes nowhere rather than to a closed descriptor, or to whatever reused its number
                char zero = 0;                      // the thread's stop: a number no signal has
                while (::write(_fd[1], &zero, 1) < 0 && (errno == EINTR || errno == EAGAIN)) {   // EAGAIN: the pipe full of a burst the thread has not read yet; it drains
                    if (errno == EAGAIN) {
                        std::this_thread::yield();
                    }
                }
                lock.unlock();
                _thread.join();
                ::close(_fd[0]);
                ::close(_fd[1]);
                _fd[0] = _fd[1] = -1;
#endif
            }

        private:
#if SGCL_SIGNALS_POSIX
            // The pipe and the thread, unless they are there; either that
            // cannot be made is a std::system_error, with nothing left
            // behind (the next registration tries again)
            void _start() {
                if (_thread.joinable()) {
                    return;
                }
                if (::pipe(_fd) < 0) {
                    _fd[0] = _fd[1] = -1;
                    throw std::system_error(errno, std::generic_category(), "signals: pipe");
                }
                ::fcntl(_fd[0], F_SETFD, FD_CLOEXEC);
                ::fcntl(_fd[1], F_SETFD, FD_CLOEXEC);
                ::fcntl(_fd[1], F_SETFL, ::fcntl(_fd[1], F_GETFL) | O_NONBLOCK);   // the handler never blocks: a full pipe drops the signal
                try {
                    _thread = std::thread([this] { _run(); });
                } catch (...) {
                    ::close(_fd[0]);
                    ::close(_fd[1]);
                    _fd[0] = _fd[1] = -1;
                    throw;
                }
                _pipe.store(_fd[1], std::memory_order_release);   // after the thread: no handler is installed before this returns
            }

            SGCL_INLINE_HOT void _save(int n) noexcept {
                if (!_saved.contains(n)) {
                    struct sigaction old = {};
                    ::sigaction(n, nullptr, &old);
                    _saved.emplace(n, old);
                }
            }

            // The handler: async-signal-safe, the number as one byte on
            // the pipe, errno left as it was
            SGCL_INLINE_HOT static void _handler(int n) noexcept {
                int saved = errno;
                int fd = _pipe.load(std::memory_order_relaxed);
                if (fd >= 0) {
                    char b = (char)n;
                    (void)!::write(fd, &b, 1);
                }
                errno = saved;
            }

            void _run() {
                char buf[256];
                for (;;) {
                    auto n = ::read(_fd[0], buf, sizeof buf);
                    if (n < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        return;
                    }
                    if (n == 0) {
                        return;
                    }
                    for (auto i = 0; i < n; ++i) {
                        if (buf[i] == 0) {
                            return;
                        }
                        _deliver((unsigned char)buf[i]);
                    }
                }
            }

            // The number sent on every channel registered for it, without
            // waiting; the registrations read under the lock, the sends
            // done without it (a send may wake a task)
            void _deliver(int n) {
                std::vector<root_ptr<SignalWait>> waits;
                {
                    std::lock_guard lock(_m);
                    if (auto it = _waits.find(n); it != _waits.end()) {
                        waits = it->second;
                    }
                }
                for (auto& w : waits) {
                    w->ch->try_send(n);
                }
            }

            inline static std::atomic<int> _pipe = {-1};             // the write end, for the handler
            int _fd[2] = {-1, -1};
            std::map<int, std::vector<root_ptr<SignalWait>>> _waits;   // guarded by _m
            std::map<int, struct sigaction> _saved;                  // the dispositions before the first registration
#endif
            std::mutex _m;
            std::thread _thread;
        };

        inline Signals& signals_instance() {
            static Signals signals;
            return signals;
        }
    }

    // A channel that gets the number of every signal of `numbers`
    // delivered to the process from now on; `capacity` elements held
    // for a receiver that is not there yet, the rest dropped
    SGCL_INLINE_HOT channel<int> signals(std::initializer_list<int> numbers, size_t capacity = 1) {
        tracked_ptr<detail::ChannelState<int>> ch = detail::make_linked_state<int>(capacity);
        detail::signals_instance().notify(numbers, ch, ch.get());
        return detail::ChannelAccess::make(std::move(ch));
    }

    // The disposition the numbers had before the first `signals` back,
    // the channels registered for them forgotten; every number, for an
    // empty list
    SGCL_INLINE_HOT void reset_signals(std::initializer_list<int> numbers = {}) noexcept {
        detail::signals_instance().reset(numbers);
    }

    // The numbers ignored by the process, the channels registered for
    // them forgotten; `reset_signals` undoes it
    SGCL_INLINE_HOT void ignore_signals(std::initializer_list<int> numbers) noexcept {
        detail::signals_instance().ignore(numbers);
    }
}
