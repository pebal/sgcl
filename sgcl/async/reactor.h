//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/root_ptr.h"
#include "channel.h"
#include "scheduler.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
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

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // The reactor: the readiness of file descriptors, as channels. `readable(fd)`
    // is a channel that gets one signal when fd can be read without
    // blocking (data, or the end of the stream), then is closed;
    // `writable(fd)` the same for a write. A task writes `co_await
    // readable(fd)->receive()` and holds no thread until the data
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
        // here, or the first would never be signalled. The kernel's udata
        // is the registration's number, never a pointer: an event may come
        // for a registration already gone (cancelled while the descriptor
        // stays open, or taken from the kernel in one batch with others
        // just before a cancel), and a number that names nothing any more
        // is dropped without anything being read through it.
        struct IoNode {
            uint64_t number = 0;
            std::vector<root_ptr<IoWait>> waits;
            size_t sweep_at = 8;   // the count of waits at which the given-up ones are dropped
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
            void watch(int fd, bool write, const tracked_ptr<void>& keep, channel<void>* ch) {
#if SGCL_REACTOR_KQUEUE
                _watch(fd, write ? EVFILT_WRITE : EVFILT_READ, 0, keep, ch);
#else
                (void)fd; (void)write; (void)keep; (void)ch;
                static_assert(SGCL_REACTOR_KQUEUE, "the reactor is kqueue only for now (macOS, FreeBSD); epoll and IOCP are to come");
#endif
            }

            // The same for the exit of a process: the channel signalled
            // when the process with this id has ended (its status still
            // to be collected with waitpid); at once when it has ended
            // already, or never existed (the registration fails with
            // ESRCH). What Go 1.23 does with a pidfd on Linux, done here
            // with kqueue's EVFILT_PROC.
            void watch_exit(int pid, const tracked_ptr<void>& keep, channel<void>* ch) {
#if SGCL_REACTOR_KQUEUE
                _watch(pid, EVFILT_PROC, NOTE_EXIT, keep, ch);
#else
                (void)pid; (void)keep; (void)ch;
                static_assert(SGCL_REACTOR_KQUEUE, "the reactor is kqueue only for now (macOS, FreeBSD); epoll with a pidfd is to come");
#endif
            }

            // The waits on a descriptor ended with nothing, before it is
            // closed: the kernel drops the registration of a closed
            // descriptor silently, and a node left here would take the
            // waits of the next descriptor with the same number (a pipe
            // closed by a wait_delay, its number reused by the next pipe).
            // The kernel's entry is left to the close: an event it gives
            // meanwhile carries a number that names nothing and is dropped.
            void cancel(int fd) {
#if SGCL_REACTOR_KQUEUE
                std::vector<root_ptr<IoWait>> ended;
                {
                    std::lock_guard lock(_m);
                    for (short filter : {EVFILT_READ, EVFILT_WRITE}) {
                        auto it = _pending.find(Key(fd, filter));
                        if (it != _pending.end()) {
                            _take(it, ended);
                        }
                    }
                }
                _end(ended, false);
#else
                (void)fd;
#endif
            }

            // The thread joined and the queue closed; a registration still
            // pending is dropped (its channel closed: a wait ends with
            // nothing); a wait registered while the thread is being joined
            // ends the same way. The next wait starts the reactor again.
            void stop() {
#if SGCL_REACTOR_KQUEUE
                std::lock_guard stopping(_stop_m);   // one stop at a time: a second (two threads stopping the scheduler, the destructor at exit beside a stop) would join the thread again; it waits and finds the reactor stopped
                {
                    std::lock_guard lock(_m);
                    if (!_running) {
                        return;
                    }
                    _stop = true;
                    if (_kq >= 0) {
                        struct kevent ev;
                        EV_SET(&ev, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
                        ::kevent(_kq, &ev, 1, nullptr, 0, nullptr);
                    }
                }
                _thread.join();
                std::vector<root_ptr<IoWait>> ended;
                {
                    std::lock_guard lock(_m);
                    if (_kq >= 0) {
                        ::close(_kq);
                        _kq = -1;
                    }
                    _running = false;
                    _take_all(ended);   // the waits still registered: ended with nothing
                }
                _end(ended, false);
#endif
            }

        private:
#if SGCL_REACTOR_KQUEUE
            using Key = std::pair<int, short>;
            using Waits = std::vector<root_ptr<IoWait>>;

            // One registration for an identifier (a descriptor, a process
            // id) and a filter: a second wait on the pair rides on the
            // first, and renews the kernel's entry with the node's number:
            // a wait registered between cancel_waits and the ::close of a
            // descriptor leaves a node whose entry the close drops, and a
            // later wait on the number that only joined it would never be
            // signalled, nor any wait on the number in that direction after
            // it (an EV_ADD of an entry in place changes nothing, and one
            // fired already is armed again, its number still the node's).
            // The kernel's entry is made under the lock, so that
            // the number it carries is the number of the node the table
            // holds for the pair: an EV_ADD made after the lock would race
            // a cancel and a new wait on the pair, and could leave the
            // kernel naming a node gone and the new one never signalled.
            void _watch(int ident, short filter, unsigned fflags, const tracked_ptr<void>& keep, channel<void>* ch) {
                root_ptr<IoWait> wait = make_tracked<IoWait>();
                wait->keep = keep;
                wait->ch = ch;
                Waits ended;
                bool ready = false;
                {
                    std::lock_guard lock(_m);
                    if (_start()) {
                        Key key(ident, filter);
                        auto it = _pending.find(key);
                        if (it != _pending.end()) {   // registered already: this wait rides on it
                            _join(it->second, std::move(wait));
                        } else {   // the node made whole before it enters the table, so that a bad_alloc leaves no empty one there
                            IoNode node;
                            node.number = ++_numbers;
                            node.waits.push_back(std::move(wait));
                            it = _pending.emplace(key, std::move(node)).first;
                        }
                        struct kevent ev;
                        EV_SET(&ev, ident, filter, EV_ADD | EV_ONESHOT, fflags, 0, (void*)(uintptr_t)it->second.number);
                        if (::kevent(_kq, &ev, 1, nullptr, 0, nullptr) == 0) {
                            return;
                        }
                        _take(it, ended);   // a descriptor the kernel cannot watch, a process that is gone: ready at once, the call after will say what is
                        ready = true;
                    } else {   // no queue (kqueue() failed: descriptors exhausted): the wait ends with nothing, and the next one tries again
                        ended.push_back(std::move(wait));
                    }
                }
                _end(ended, ready);
            }

            // A wait added to a registration. A wait given up (its channel
            // closed by the one who waited, which is how a wait is given
            // up: net's descriptor does it when a deadline passes) stays on
            // the registration until the descriptor is ready, which an idle descriptor read with a short deadline in
            // a loop may never be; so the closed ones are dropped each time
            // the count reaches twice what was left the time before. The
            // registration then holds at most about twice the waits still
            // open, at a constant cost per wait.
            static void _join(IoNode& node, root_ptr<IoWait> wait) {
                if (node.waits.size() >= node.sweep_at) {
                    std::erase_if(node.waits, [](const root_ptr<IoWait>& w) { return w->ch->closed(); });
                    node.sweep_at = std::max<size_t>(8, 2 * node.waits.size());
                }
                node.waits.push_back(std::move(wait));
            }

            // Under _m: the node's waits moved out and the node gone
            template<class It>
            void _take(It it, Waits& out) {
                for (auto& w : it->second.waits) {
                    out.push_back(std::move(w));
                }
                _pending.erase(it);
            }

            void _take_all(Waits& out) {
                for (auto& [key, node] : _pending) {
                    for (auto& w : node.waits) {
                        out.push_back(std::move(w));
                    }
                }
                _pending.clear();
            }

            // The waits signalled (ready) or ended with nothing, and their
            // channels closed; outside _m, since a close wakes whoever
            // waits on the channel
            static void _end(Waits& waits, bool ready) {
                for (auto& w : waits) {
                    if (ready) {
                        w->ch->try_send();
                    }
                    w->ch->close();
                }
                waits.clear();
            }

            struct KeyHash {
                size_t operator()(const Key& k) const noexcept {
                    return std::hash<long>()((long(k.first) << 16) ^ k.second);
                }
            };

            // Under _m: the queue and its thread, made by the first wait;
            // false when there is none. A kqueue() that fails (EMFILE)
            // leaves the reactor as it was, not running, so that the next
            // wait tries again; a queue that failed under the thread
            // (_run) is joined here and made again. A stop being joined
            // takes no new queue: its waits end with nothing, as stop()
            // says.
            bool _start() {
                if (_running) {
                    if (_stop) {
                        return false;
                    }
                    if (_kq >= 0) {
                        return true;
                    }
                    _thread.join();   // _run ended on a failed queue: its last step under _m set _kq
                    _running = false;
                }
                int kq = ::kqueue();
                if (kq < 0) {
                    return false;
                }
                struct kevent ev;
                EV_SET(&ev, 1, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);   // the wake for stop()
                if (::kevent(kq, &ev, 1, nullptr, 0, nullptr) < 0) {
                    ::close(kq);
                    return false;
                }
                _kq = kq;
                _stop = false;
                try {
                    _thread = std::thread([this] { _run(); });
                } catch (...) {
                    ::close(kq);
                    _kq = -1;
                    throw;
                }
                _running = true;
                scheduler_stop_hook2.store([] { reactor_instance().stop(); }, std::memory_order_release);
                return true;
            }

            // The kernel's events for the registrations they name, one
            // batch under one lock: an event whose number is not the
            // number of the node the table holds for its pair (cancelled,
            // or fired and registered again since) is dropped. A wait that
            // joined the registration after the kernel's event and before
            // this is signalled with the others: the descriptor was ready
            // a moment ago.
            void _run() {
                struct kevent events[64];
                Waits ready;
                for (;;) {
                    int n = ::kevent(_kq, nullptr, 0, events, 64, nullptr);
                    if (n < 0 && errno == EINTR) {
                        continue;
                    }
                    bool stop;
                    {
                        std::lock_guard lock(_m);
                        if (n < 0) {   // the queue has failed: the waits end with nothing, and the next wait joins this thread and makes a queue again (_start)
                            _take_all(ready);
                            ::close(_kq);
                            _kq = -1;
                            stop = true;
                        } else {
                            for (int i = 0; i < n; ++i) {
                                auto& ev = events[i];
                                if (ev.filter == EVFILT_USER) {
                                    continue;
                                }
                                auto it = _pending.find(Key(int(ev.ident), ev.filter));
                                if (it != _pending.end() && it->second.number == (uint64_t)(uintptr_t)ev.udata) {
                                    _take(it, ready);
                                }
                            }
                            stop = _stop;
                        }
                    }
                    _end(ready, n >= 0);
                    if (stop) {
                        return;
                    }
                }
            }

            int _kq = -1;
            uint64_t _numbers = 0;   // the last registration's number (under _m)
            std::unordered_map<Key, IoNode, KeyHash> _pending;   // guarded by _m
#endif
            std::mutex _m;
            std::mutex _stop_m;   // held through a whole stop(), taken before _m
            std::thread _thread;
            bool _stop = false;
            bool _running = false;   // the thread started and not yet joined (under _m)
        };

        inline Reactor& reactor_instance() {
            static Reactor reactor;
            return reactor;
        }
    }

    // A channel signalled once when fd can be read without blocking, then
    // closed. A wait given up before that (a select lost to a timeout) is
    // ended by closing the channel: the reactor holds the channel until fd
    // is ready, and drops a closed one at a later wait on the descriptor.
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

    // The waits on fd ended with nothing: what a close of the descriptor
    // calls, before the number can be another descriptor's
    inline void cancel_waits(int fd) {
        detail::reactor_instance().cancel(fd);
    }

    // A channel signalled once when the process with this id has ended,
    // then closed: `co_await exited(pid)->receive()` holds no
    // thread while a child runs, and waitpid after it does not block. A
    // process that has ended already, or that does not exist, signals at
    // once.
    inline tracked_ptr<channel<void>> exited(int pid) {
        tracked_ptr<channel<void>> ch = make_tracked<channel<void>>(1);
        detail::reactor_instance().watch_exit(pid, ch, ch.get());
        return ch;
    }
}
