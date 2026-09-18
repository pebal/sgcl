//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/root_ptr.h"
#include "channel.h"
#include "scheduler.h"

#include <cerrno>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#define SGCL_REACTOR_KQUEUE 1
#else
#define SGCL_REACTOR_KQUEUE 0
#endif

namespace sgcl {
    // The reactor: the readiness of file descriptors, as channels. `readable(fd)`
    // is a channel that gets one signal when fd can be read without
    // blocking (data, or the end of the stream), then is closed;
    // `writable(fd)` the same for a write. A task writes `co_await
    // readable(fd)->async_receive()` and holds no thread until the data
    // comes; a select bounds it (`readable(fd)->on_receive(f),
    // timeout(1s, g)`), a stop token cancels it. Under them one thread on
    // the kernel's queue (kqueue here; epoll and IOCP on the other
    // platforms, to come), asleep in the kernel until something is ready,
    // which then signals the channel: a push on the scheduler for the task
    // that waits. This is the foundation the io and net modules stand on;
    // a socket or a file in them is a descriptor with a buffer, and a read
    // that would block is `co_await readable(fd)` and a read that will not.
    namespace detail {
        class Reactor;
        inline Reactor& reactor_instance();

        // One wait: the channel to signal, held through the object that
        // holds it
        struct IoWait {
            tracked_ptr<void> keep;
            channel<void>* ch = nullptr;
        };

        // One registration with the kernel: the waits on one descriptor in
        // one direction, signalled together by its first readiness. The
        // kernel keeps one entry per descriptor and filter (a second EV_ADD
        // for the pair replaces the first entry's udata rather than adding
        // one), so two waits on one descriptor share the registration
        // here, or the first would never be signalled. A node in unmanaged
        // memory the kernel hands back (udata).
        struct IoNode {
            int fd;
            short filter;
            std::vector<root_ptr<IoWait>> waits;
        };

        class Reactor {
        public:
            Reactor() {
                scheduler_instance();   // made after the scheduler, so destroyed before it (timer.h: Timers)
            }

            ~Reactor() {
                stop();
            }

            // A one-shot registration: the channel signalled and closed when fd is ready
            void watch(int fd, bool write, tracked_ptr<void> keep, channel<void>* ch) {
#if SGCL_REACTOR_KQUEUE
                _start();
                root_ptr<IoWait> wait = make_tracked<IoWait>();
                wait->keep = std::move(keep);
                wait->ch = ch;
                short filter = write ? EVFILT_WRITE : EVFILT_READ;
                IoNode* node;
                {
                    std::lock_guard lock(_m);
                    auto& slot = _pending[Key(fd, filter)];
                    if (slot) {   // registered already: this wait rides on it
                        slot->waits.push_back(std::move(wait));
                        return;
                    }
                    node = new IoNode{fd, filter, {}};
                    node->waits.push_back(std::move(wait));
                    slot = node;
                }
                struct kevent ev;
                EV_SET(&ev, fd, filter, EV_ADD | EV_ONESHOT, 0, 0, node);
                if (_kq < 0 || ::kevent(_kq, &ev, 1, nullptr, 0, nullptr) < 0) {   // a descriptor the kernel cannot watch (or a reactor stopping): ready at once, the read will say what is wrong
                    _fire(node);
                }
#else
                (void)fd; (void)write; (void)keep; (void)ch;
                static_assert(SGCL_REACTOR_KQUEUE, "the reactor is kqueue only for now (macOS, FreeBSD); epoll and IOCP are to come");
#endif
            }

            // The thread joined and the queue closed; a registration still
            // pending is dropped (its channel closed: a wait ends with
            // nothing); a wait registered while the thread is being joined
            // ends the same way. The next wait starts the reactor again.
            void stop() {
#if SGCL_REACTOR_KQUEUE
                {
                    std::lock_guard lock(_m);
                    if (!_running) {
                        return;
                    }
                    _stop = true;
                    struct kevent ev;
                    EV_SET(&ev, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
                    ::kevent(_kq, &ev, 1, nullptr, 0, nullptr);
                }
                _thread.join();
                std::lock_guard lock(_m);
                ::close(_kq);
                _kq = -1;
                _running = false;
                for (auto& [key, node] : _pending) {   // the waits still registered: ended with nothing
                    for (auto& w : node->waits) {
                        w->ch->close();
                    }
                    delete node;
                }
                _pending.clear();
#endif
            }

        private:
#if SGCL_REACTOR_KQUEUE
            using Key = std::pair<int, short>;

            struct KeyHash {
                size_t operator()(const Key& k) const noexcept {
                    return std::hash<long>()((long(k.first) << 16) ^ k.second);
                }
            };

            void _start() {
                std::lock_guard lock(_m);
                if (_running) {
                    return;
                }
                _kq = ::kqueue();
                struct kevent ev;
                EV_SET(&ev, 1, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);   // the wake for stop()
                ::kevent(_kq, &ev, 1, nullptr, 0, nullptr);
                _stop = false;
                _running = true;
                _thread = std::thread([this] { _run(); });
                scheduler_stop_hook2.store([] { reactor_instance().stop(); }, std::memory_order_release);
            }

            void _run() {
                struct kevent events[64];
                for (;;) {
                    int n = ::kevent(_kq, nullptr, 0, events, 64, nullptr);
                    if (n < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        std::lock_guard lock(_m);   // the queue is gone (closed, or never made: descriptors exhausted): the waits end with nothing, and every later one at once, until a stop and a start
                        for (auto& [key, node] : _pending) {
                            for (auto& w : node->waits) {
                                w->ch->close();
                            }
                            delete node;
                        }
                        _pending.clear();
                        ::close(_kq);
                        _kq = -1;
                        return;
                    }
                    for (int i = 0; i < n; ++i) {
                        auto& ev = events[i];
                        if (ev.filter == EVFILT_USER) {
                            continue;
                        }
                        _fire((IoNode*)ev.udata);
                    }
                    std::lock_guard lock(_m);
                    if (_stop) {
                        return;
                    }
                }
            }

            // The waits of a registration signalled and its node gone,
            // once: the kernel's event and a failed registration may both
            // name it. A wait that joined the registration after the
            // kernel's event and before this is signalled with the others:
            // the descriptor was ready a moment ago.
            void _fire(IoNode* node) {
                {
                    std::lock_guard lock(_m);
                    auto it = _pending.find(Key(node->fd, node->filter));
                    if (it == _pending.end() || it->second != node) {
                        return;
                    }
                    _pending.erase(it);
                }
                for (auto& w : node->waits) {
                    w->ch->try_send();
                    w->ch->close();
                }
                delete node;
            }

            int _kq = -1;
            std::unordered_map<Key, IoNode*, KeyHash> _pending;   // guarded by _m
#endif
            std::mutex _m;
            std::thread _thread;
            bool _stop = false;
            bool _running = false;   // the thread started and not yet joined (under _m)
        };

        inline Reactor& reactor_instance() {
            static Reactor reactor;
            return reactor;
        }
    }

    // A channel signalled once when fd can be read without blocking, then closed
    inline tracked_ptr<channel<void>> readable(int fd) {
        tracked_ptr<channel<void>> ch = make_tracked<channel<void>>(1);
        detail::reactor_instance().watch(fd, false, ch, ch.get());
        return ch;
    }

    // The same for a write
    inline tracked_ptr<channel<void>> writable(int fd) {
        tracked_ptr<channel<void>> ch = make_tracked<channel<void>>(1);
        detail::reactor_instance().watch(fd, true, ch, ch.get());
        return ch;
    }
}
