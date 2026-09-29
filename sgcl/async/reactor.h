//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/root_ptr.h"
#include "../core/detail/backoff.h"
#include "channel.h"
#include "event.h"
#include "scheduler.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// SGCL_REACTOR_FORCE_EPOLL: the epoll branch compiled on another system,
// for a syntax check of it against a stub <sys/epoll.h> (never a build
// that runs)
#if (defined(__APPLE__) || defined(__FreeBSD__)) && !defined(SGCL_REACTOR_FORCE_EPOLL)
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#define SGCL_REACTOR_KQUEUE 1
#define SGCL_REACTOR_EPOLL 0
#elif defined(__linux__) || defined(SGCL_REACTOR_FORCE_EPOLL)
#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>
#define SGCL_REACTOR_KQUEUE 0
#define SGCL_REACTOR_EPOLL 1
#else
#define SGCL_REACTOR_KQUEUE 0
#define SGCL_REACTOR_EPOLL 0
#endif

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // The reactor: the readiness of file descriptors. Two ways in.
    //
    // The descriptors of the io and net modules (io::detail::Descriptor)
    // are registered with the kernel once, at their first wait, edge-
    // triggered (kqueue's EV_CLEAR, epoll's EPOLLET), and stay so until
    // they are closed: a wait is no system call and takes no lock. Each
    // descriptor number has a slot here (PollSlot) with one word per
    // direction, the state of the waits on it, which the waiting task and
    // the reactor's thread change with compare-exchanges; Go's pollDesc
    // with its rg and wg, in this library's terms. A read that would block
    // tries the call, then parks on the word; an edge that comes between
    // the try and the park is kept on the word (Ready) and the park takes
    // it instead of sleeping, and the call is tried again.
    //
    // `readable(fd)` and `writable(fd)`, for any other descriptor: a
    // channel that gets one signal when fd can be read (or written)
    // without blocking, then is closed; a one-shot registration each
    // (kqueue's EV_ONESHOT), under a lock. A task writes `co_await
    // readable(fd)->receive()` and holds no thread until the data comes; a
    // select bounds it (`readable(fd)->on_receive(f), timeout(1s, g)`), a
    // stop token cancels it. The two ways do not mix on one descriptor
    // number: the kernel keeps one entry per number and filter, and the
    // second registration would take the first's. `exited(pid)` is the
    // same for the end of a process.
    //
    // Under both one thread on the kernel's queue (kqueue here, epoll on
    // Linux, IOCP to come), asleep in the kernel until something is ready.
    namespace detail {
        class Reactor;
        inline Reactor& reactor_instance();

        // One wait of the one-shot way: the channel to signal, held through
        // the object that holds it
        struct IoWait {
            tracked_ptr<void> keep;
            ChannelState<void>* ch = nullptr;
        };

        // One registration with the kernel of the one-shot way: the waits on
        // one descriptor in one direction, signalled together by its first
        // readiness. The kernel keeps one entry per descriptor and filter
        // (a second EV_ADD for the pair replaces the first entry's udata
        // rather than adding one), so two waits on one descriptor share the
        // registration here, or the first would never be signalled. The
        // kernel's udata is the registration's number, never a pointer: an
        // event may come for a registration already gone (cancelled while
        // the descriptor stays open, or taken from the kernel in one batch
        // with others just before a cancel), and a number that names
        // nothing any more is dropped without anything being read through
        // it.
        struct IoNode {
            uint64_t number = 0;
            std::vector<root_ptr<IoWait>> waits;
            size_t sweep_at = 8;   // the count of waits at which the given-up ones are dropped
        };

        // A wait on a registered descriptor that is not a coroutine in its
        // slot's own record: a thread (woken through its word), a channel
        // (closed at the wake, with a signal before it for readiness: a case
        // of a select), or a coroutine behind another waiter in the same
        // direction (on the overflow list). A managed object: a thread holds
        // its own on its stack while it waits, the slot's roots hold the
        // rest.
        struct PollWaiter {
            tracked_ptr<FrameWord> frame;          // a coroutine's frame, else null
            tracked_ptr<void> keep;                // what holds the channel
            ChannelState<void>* ch = nullptr;           // a channel's, else null
            tracked_ptr<PollWaiter> next;          // the overflow list's link
            std::atomic<uint32_t> signal = {0};    // a thread's: 1 once woken
        };

        // The slots come in chunks of 1024 descriptor numbers, made on the
        // first wait on a number of the chunk and kept for the process: the
        // kernel's udata names a slot by its number and generation, and an
        // event taken from the kernel just before a close may be handled
        // after it, so a slot's memory is never given back; a stale event
        // finds another generation there and is dropped (or, in the window
        // between its look and its step, makes a wait on the new descriptor
        // try its call once more, which an edge-triggered wait allows).
        inline constexpr unsigned PollChunkBits = 10;
        inline constexpr unsigned PollChunkSize = 1u << PollChunkBits;
        inline constexpr unsigned PollChunkCount = 4096;   // descriptor numbers below 4M

        // The first number past the table: PollChunkSize * PollChunkCount,
        // lowered only by a test, which has no way to open a descriptor
        // with a number that high (tests/net/poll.cpp)
        inline std::atomic<unsigned> poll_number_limit = {PollChunkSize * PollChunkCount};

        // The tracked words of a chunk's slots, in one managed object that
        // the chunk roots (the slots themselves are plain memory, which
        // holds no tracked word): the coroutine parked in a slot, the
        // waiter record of a thread or channel parked there, and the head
        // of the overflow list, per direction. A parked coroutine's frame
        // lives while it waits, as it did on the channel of a one-shot
        // registration: a detached task reading a connection nobody else
        // references is not collected under its read.
        struct PollRoots {
            tracked_ptr<FrameWord> frame[PollChunkSize][2];
            tracked_ptr<PollWaiter> waiter[PollChunkSize][2];
            tracked_ptr<PollWaiter> overflow[PollChunkSize][2];
        };

        // A coroutine made ready: gathered into the reactor's batch, or
        // enqueued at once by a waker that has none
        inline void poll_wake(tracked_ptr<FrameWord> frame, WakeBatch* batch) {
            if (batch) {
                batch->add(std::move(frame));
            } else {
                enqueue(std::move(frame), true);
            }
        }

        // A waiter record woken: a coroutine made ready, a channel closed
        // (with the ready bit when ready: the library tells a readiness
        // apart from a wait ended with nothing, and the event is set by the
        // close alone, so that a wait woken by it finds it set), a
        // thread's word set
        inline void poll_wake(PollWaiter& w, bool ready, WakeBatch* batch) {
            if (w.frame) {
                tracked_ptr<FrameWord> f = w.frame;
                w.frame = nullptr;
                poll_wake(std::move(f), batch);
            } else if (w.ch) {
                if (ready) {
                    w.ch->close_ready();
                } else {
                    w.ch->close();
                }
            } else {
                w.signal.store(1, std::memory_order_release);   // the thread's wait reads it with acquire
                w.signal.notify_one();                          // the record is held by the waker's pointer, alive under the call
            }
        }

        // The waits on one descriptor number, a word per direction (Read 0,
        // Write 1), the bits of which say:
        //
        //   Ready     an edge came and no waiter has taken it yet
        //   Claim     one waiter holds the direction: its record (the
        //             frame, or the waiter) is being filled, or is parked,
        //             or is being read by the one waking it
        //   Parked    the record is published: a waker may take it
        //   Overflow  waiters behind the one that holds the direction are
        //             on the overflow list (under the slot's lock)
        //
        // A waiter, after its call answered EAGAIN: claim() takes Ready if
        // it is there (the call is tried again, no sleep), else takes Claim;
        // it fills the record and publish() sets Parked, unless Ready came
        // meanwhile, which publish takes, giving up the claim (the call is
        // tried again). The reactor's event (fire) takes Parked and wakes
        // the record; with a claim not yet parked it sets Ready, which the
        // publish then sees; with nobody there it sets Ready for the next
        // wait. So an edge is never lost between a waiter's try and its
        // park, and no lock is taken by either side.
        //
        // The one that takes Parked reads the record and clears it while
        // Claim still holds the direction, then gives up Claim (and takes
        // Overflow) with one exchange: a waiter that claims next writes a
        // record nobody reads any more.
        //
        // One waiter per direction parks in the record; a second (tasks
        // accepting on one listener, reading one pipe) goes on the overflow
        // list, and is woken whenever the one ahead gives up the direction,
        // however (a wake, a readiness at its publish, its own taking back):
        // it then tries its call again. A waiter behind never takes Ready
        // and never parks when the direction was free or Ready was there as
        // it pushed: its try may have come before an edge the one ahead was
        // woken for. The list is Go's fdMutex queue in effect, where Go
        // lets one waiter per direction in and makes the others wait on the
        // mutex.
        //
        // A close, a deadline, a stop of the reactor wake the waiters
        // without readiness (interrupt): each sets its cause first, and the
        // waiter looks at the causes after its publication; both are
        // sequentially consistent, so either the interrupter sees the
        // waiter parked, or the waiter sees the cause and takes itself back
        // (take_back, remove).
        struct alignas(64) PollSlot {
            static constexpr uint32_t Ready = 1;
            static constexpr uint32_t Claim = 2;
            static constexpr uint32_t Parked = 4;
            static constexpr uint32_t Overflow = 8;

            enum class Claimed {
                ready,   // Ready taken: the call is tried again
                ours,    // the record is this waiter's to fill
                busy     // another waiter holds the direction: the overflow list
            };

            std::atomic<uint32_t> state[2] = {{0}, {0}};
            std::atomic<uint32_t> lock = {0};      // the overflow lists'
            std::atomic<uint32_t> gen = {0};       // moved by every close of an owned descriptor with this number
            std::atomic<uint64_t> armed = {0};     // the reactor's incarnation << 2, and a bit per direction registered in it
            PollRoots* roots = nullptr;            // rooted by the chunk
            unsigned index = 0;

            tracked_ptr<FrameWord>& frame(int dir) noexcept {
                return roots->frame[index][dir];
            }

            tracked_ptr<PollWaiter>& waiter(int dir) noexcept {
                return roots->waiter[index][dir];
            }

            tracked_ptr<PollWaiter>& overflow(int dir) noexcept {
                return roots->overflow[index][dir];
            }

            // Before a call's try: a Ready left by an edge that came while
            // nobody waited is dropped, since the try sees what the edge
            // brought (Go's pollReset, in prepareRead); an edge after the
            // try sets it again. Without it a wait after the try's EAGAIN
            // took the stale Ready and made one more call for nothing (a
            // stream 4 per cent slower, measured). Left when a waiter holds
            // the direction: the Ready is its publication's.
            void reset(int dir) noexcept {
                auto& st = state[dir];
                uint32_t s = st.load(std::memory_order_relaxed);
                while ((s & (Ready | Claim)) == Ready) {
                    // acquire: the try's reads of the kernel's buffers come after
                    // the Ready is dropped, never before it (an edge the try
                    // missed would then be dropped with the stale one)
                    if (st.compare_exchange_weak(s, s & ~Ready, std::memory_order_acquire, std::memory_order_relaxed)) {
                        return;
                    }
                }
            }

            // A waiter comes: Ready taken, or the direction claimed, or
            // held by another. Acquire on the claim: the record was
            // cleared by the last one to read it before it gave up Claim
            // with release, and this waiter's writes come after that.
            Claimed claim(int dir) noexcept {
                auto& st = state[dir];
                uint32_t s = st.load(std::memory_order_relaxed);
                for (;;) {
                    if (s & Claim) {
                        return Claimed::busy;   // Ready, if there, is for the one ahead: its publish takes it and it tries again
                    }
                    if (s & Ready) {
                        if (st.compare_exchange_weak(s, s & ~Ready, std::memory_order_acquire, std::memory_order_relaxed)) {
                            return Claimed::ready;
                        }
                    } else if (st.compare_exchange_weak(s, s | Claim, std::memory_order_acquire, std::memory_order_relaxed)) {
                        return Claimed::ours;
                    }
                }
            }

            // The record filled, published: true when parked (a wake will
            // come), false when Ready came while it was filled, taken here
            // with the claim given up and the waiters behind woken (the
            // call is tried again). Sequentially consistent: the store of
            // Parked releases the record to the waker, and orders the
            // waiter's look at the causes after it (interrupt)
            bool publish(int dir) {
                auto& st = state[dir];
                uint32_t s = st.load(std::memory_order_relaxed);
                while (!(s & Ready)) {
                    if (st.compare_exchange_weak(s, s | Parked, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                        return true;
                    }
                }
                _clear_record(dir);   // still this waiter's: Claim is held
                s = st.fetch_and(~(Ready | Claim | Overflow), std::memory_order_acq_rel);   // release: the record cleared before the next claim
                if (s & Overflow) {
                    _wake_overflow(dir, true, nullptr);
                }
                return false;
            }

            // A parked waiter taken back by itself (its look after the
            // publication found a cause): true when no wake will come,
            // false when a waker has taken it and the wake is on its way
            bool take_back(int dir) {
                auto& st = state[dir];
                uint32_t s = st.load(std::memory_order_relaxed);
                do {
                    if (!(s & Parked)) {
                        return false;
                    }
                } while (!st.compare_exchange_weak(s, s & ~Parked, std::memory_order_acquire, std::memory_order_relaxed));
                _clear_record(dir);
                s = st.fetch_and(~(Claim | Overflow), std::memory_order_acq_rel);
                if (s & Overflow) {
                    _wake_overflow(dir, true, nullptr);
                }
                return true;
            }

            // A waiter behind the one that holds the direction: on the
            // overflow list, true when it waits there; false when it should
            // try its call again instead (the direction free, or Ready not
            // taken yet), and it is not on the list. A level waiter (the
            // one-shot way on epoll, which looks at the level itself after
            // the push) always stays.
            bool push(int dir, const tracked_ptr<PollWaiter>& w, bool level = false) {
                _lock();
                w->next = overflow(dir);
                overflow(dir) = w;
                uint32_t s = state[dir].fetch_or(Overflow, std::memory_order_seq_cst);   // the publication, as publish's
                if (!level && (!(s & Claim) || (s & Ready))) {
                    overflow(dir) = w->next;   // still the head: pushed under the lock
                    w->next = nullptr;
                    _unlock();
                    return false;
                }
                _unlock();
                return true;
            }

            // A waiter of the overflow list taken off by itself: true when
            // it was still there (no wake will come)
            bool remove(int dir, PollWaiter* w) {
                _lock();
                for (tracked_ptr<PollWaiter>* link = &overflow(dir); *link; link = &(*link)->next) {
                    if (link->get() == w) {
                        *link = w->next;
                        w->next = nullptr;
                        _unlock();
                        return true;
                    }
                }
                _unlock();
                return false;
            }

            // The kernel's event for the direction, on the reactor's thread
            void fire(int dir, WakeBatch* batch) {
                auto& st = state[dir];
                uint32_t s = st.load(std::memory_order_relaxed);
                uint32_t n;
                do {
                    if (s & Parked) {
                        n = s & ~Parked;            // the parked one woken: its record read below, the claim given up after
                    } else if (s & Claim) {
                        n = s | Ready;              // one filling its record: its publish takes the readiness
                    } else if (s & Overflow) {
                        n = s & ~Overflow;          // waiters with nobody ahead: all woken, each tries again
                    } else {
                        n = s | Ready;              // nobody waits: the next wait takes the readiness
                    }
                    if (n == s) {
                        return;                     // Ready there already
                    }
                } while (!st.compare_exchange_weak(s, n, std::memory_order_acq_rel, std::memory_order_relaxed));
                if (s & Parked) {
                    _wake_parked(dir, true, batch);
                } else if (!(s & Claim) && (s & Overflow)) {
                    _wake_overflow(dir, true, batch);
                }
            }

            // The waiters of the direction woken without readiness, their
            // cause set before this (a close, a deadline, the reactor's
            // stop). A waiter between its claim and its publication is
            // left: it looks at the causes after its publication.
            void interrupt(int dir) {
                auto& st = state[dir];
                uint32_t s = st.load(std::memory_order_seq_cst);   // after the cause's store: the other half of the waiter's publish and look
                do {
                    if (!(s & (Parked | Overflow))) {
                        return;
                    }
                } while (!st.compare_exchange_weak(s, s & ~(Parked | Overflow), std::memory_order_seq_cst, std::memory_order_seq_cst));
                if (s & Parked) {
                    _wake_parked(dir, false, nullptr);
                }
                if (s & Overflow) {
                    _wake_overflow(dir, false, nullptr);
                }
            }

            // The number given back to the kernel (an owned descriptor's
            // close, before the ::close): a new generation, so that the
            // kernel's events still on their way for the old one are
            // dropped; nothing registered, so that the next descriptor with
            // the number registers again; no readiness kept. No waiter is
            // there: the close is made by the last operation.
            void clear() noexcept {
                gen.fetch_add(1, std::memory_order_acq_rel);
                armed.store(0, std::memory_order_release);
                state[0].store(0, std::memory_order_release);
                state[1].store(0, std::memory_order_release);
            }

        private:
            void _clear_record(int dir) {
                if (frame(dir)) {
                    frame(dir) = nullptr;
                }
                if (waiter(dir)) {
                    waiter(dir) = nullptr;
                }
            }

            // Parked taken by this waker (Claim still set): the record
            // read and cleared, Claim given up (and Overflow taken, the
            // waiters behind woken to try again), then the parked one woken
            void _wake_parked(int dir, bool ready, WakeBatch* batch) {
                tracked_ptr<FrameWord> f = frame(dir);
                tracked_ptr<PollWaiter> w = waiter(dir);
                _clear_record(dir);
                uint32_t s = state[dir].fetch_and(~(Claim | Overflow), std::memory_order_acq_rel);   // release: the record cleared before the next claim
                if (f) {
                    poll_wake(std::move(f), batch);
                } else if (w) {
                    poll_wake(*w, ready, batch);
                }
                if (s & Overflow) {
                    _wake_overflow(dir, true, batch);
                }
            }

            // The overflow list taken whole and every waiter on it woken
            void _wake_overflow(int dir, bool ready, WakeBatch* batch) {
                _lock();
                tracked_ptr<PollWaiter> w = overflow(dir);
                if (w) {
                    overflow(dir) = nullptr;
                }
                _unlock();
                while (w) {
                    tracked_ptr<PollWaiter> next = w->next;
                    w->next = nullptr;
                    poll_wake(*w, ready, batch);
                    w = next;
                }
            }

            // A spin lock: held for a link or an unlink, taken only by the
            // waiters behind another and by the ones waking them
            void _lock() noexcept {
                Backoff<64> backoff;
                uint32_t e = 0;
                while (!lock.compare_exchange_weak(e, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                    e = 0;
                    backoff();
                }
            }

            void _unlock() noexcept {
                lock.store(0, std::memory_order_release);
            }
        };

        // A chunk of slots, plain memory, never given back; the roots of
        // its tracked words beside it
        struct PollChunk {
            PollChunk()
            : roots(make_tracked<PollRoots>()) {
                for (unsigned i = 0; i < PollChunkSize; ++i) {
                    slots[i].roots = roots.get();
                    slots[i].index = i;
                }
            }

            PollSlot slots[PollChunkSize];
            root_ptr<PollRoots> roots;
        };

        class Reactor {
        public:
            Reactor() {
                scheduler_instance();   // made after the scheduler, so destroyed before it (timer.h: Timers)
            }

            // The chunks are kept (PollChunk): a descriptor destroyed by the
            // collector at the end of the program may still look at its slot
            ~Reactor() {
                stop();
            }

            // A one-shot registration: the channel signalled and closed when fd is ready
            void watch(int fd, bool write, const tracked_ptr<void>& keep, ChannelState<void>* ch) {
#if SGCL_REACTOR_KQUEUE
                _watch(fd, write ? EVFILT_WRITE : EVFILT_READ, 0, keep, ch);
#elif SGCL_REACTOR_EPOLL
                _watch_level(fd, write ? 1 : 0, keep, ch);
#else
                (void)fd; (void)write; (void)keep; (void)ch;
                static_assert(SGCL_REACTOR_KQUEUE || SGCL_REACTOR_EPOLL, "the reactor is kqueue and epoll only for now; IOCP is to come");
#endif
            }

            // The same for the exit of a process: the channel signalled
            // when the process with this id has ended (its status still
            // to be collected with waitpid); at once when it has ended
            // already, or never existed (the registration fails with
            // ESRCH). What Go 1.23 does with a pidfd on Linux, done here
            // with kqueue's EVFILT_PROC.
            void watch_exit(int pid, const tracked_ptr<void>& keep, ChannelState<void>* ch) {
#if SGCL_REACTOR_KQUEUE
                _watch(pid, EVFILT_PROC, NOTE_EXIT, keep, ch);
#elif SGCL_REACTOR_EPOLL
                // For now a thread per process, in waitid with WNOWAIT (the
                // status left for waitpid); a pidfd on the reactor is to
                // come with the Linux machine
                std::thread([hold = root_ptr<void>(keep), ch, pid] {
                    siginfo_t si;
                    while (::waitid(P_PID, (id_t)pid, &si, WEXITED | WNOWAIT) < 0 && errno == EINTR) {
                    }
                    ch->close_ready();   // the process ended
                }).detach();
#else
                (void)pid; (void)keep; (void)ch;
#endif
            }

            // The waits on fd ended with nothing, before it is
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
                if (auto s = find(fd)) {
                    s->interrupt(0);
                    s->interrupt(1);
                }
#endif
            }

            // The slot of a descriptor number, its chunk made on the first
            // call; null for a number past the table (a wait on it fails:
            // io::errc::unsupported)
            PollSlot* slot(int fd) {
                if (fd < 0 || unsigned(fd) >= poll_number_limit.load(std::memory_order_relaxed)) {
                    return nullptr;
                }
                if (auto s = find(fd)) [[likely]] {
                    return s;
                }
                auto& top = _chunks[unsigned(fd) >> PollChunkBits];
                auto made = new PollChunk();
                PollChunk* seen = nullptr;
                if (!top.compare_exchange_strong(seen, made, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    delete made;   // another thread made it first
                    return &seen->slots[unsigned(fd) & (PollChunkSize - 1)];
                }
                return &made->slots[unsigned(fd) & (PollChunkSize - 1)];
            }

            // The slot if its chunk is made, else null
            PollSlot* find(int fd) const noexcept {
                if (fd < 0 || unsigned(fd) >= PollChunkSize * PollChunkCount) {
                    return nullptr;
                }
                PollChunk* c = _chunks[unsigned(fd) >> PollChunkBits].load(std::memory_order_acquire);
                return c ? &c->slots[unsigned(fd) & (PollChunkSize - 1)] : nullptr;
            }

            // The queue's incarnation, the queue and its thread started when
            // there are none (0: none could be made, or a stop is being
            // joined). Every start is a new incarnation: a registration
            // made in an older one is made again (arm), and a wait parked
            // in an older one ended by the stop sees the number moved.
            uint64_t running() {
                uint64_t inc = _live.load(std::memory_order_acquire);
                if (inc) [[likely]] {
                    return inc;
                }
                std::lock_guard lock(_m);
                return _start() ? _live.load(std::memory_order_relaxed) : 0;
            }

            // The incarnation now, 0 when stopped: a waiter's look after its
            // publication (sequentially consistent, with the store of the
            // stop before its sweep)
            uint64_t live() const noexcept {
                return _live.load(std::memory_order_seq_cst);
            }

            // The descriptor registered for the direction in this
            // incarnation: once, by the first waiter that finds it not, and
            // never again until the number is closed or the reactor
            // restarted. 0, or the errno of a registration the kernel
            // refuses (a descriptor it cannot watch, one closed under the
            // wait), which the wait gives as its error: answered as a
            // readiness, as the one-shot way has it, a call that answers
            // EAGAIN on such a descriptor would be tried again forever.
            int arm(int fd, PollSlot& s, int dir, uint64_t inc) {
                uint64_t bit = uint64_t(1) << dir;
                uint64_t a = s.armed.load(std::memory_order_acquire);
                for (;;) {
                    bool current = (a >> 2) == inc;
                    if (current && (a & bit)) [[likely]] {
                        return 0;
                    }
#if SGCL_REACTOR_EPOLL
                    uint64_t n = (inc << 2) | 3;   // epoll: one entry per descriptor, both directions in it
#else
                    uint64_t n = (current ? a : (inc << 2)) | bit;
#endif
                    if (s.armed.compare_exchange_weak(a, n, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                }
                int e = _register(fd, s, dir);
                if (e) {
#if SGCL_REACTOR_EPOLL
                    s.armed.fetch_and(~uint64_t(3), std::memory_order_acq_rel);   // the next wait tries again
                    s.fire(0, nullptr);
                    s.fire(1, nullptr);   // a waiter of either direction that took the bits for a registration
#else
                    s.armed.fetch_and(~bit, std::memory_order_acq_rel);   // the next wait tries again
                    s.fire(dir, nullptr);   // and a waiter that took the bit for a registration and parked is woken: it tries its call, and its own wait registers and fails
#endif
                }
                return e;
            }

            // An owned descriptor's number given back to the kernel: called
            // right before its ::close, with no wait on it (the slot says
            // why: PollSlot::clear)
            void release(int fd) noexcept {
                if (auto s = find(fd)) {
#if SGCL_REACTOR_EPOLL
                    // epoll keeps an entry while any descriptor refers to the
                    // file (a dup, a child's copy before its exec): taken out
                    // before the close, as Go does
                    int q = _queue.load(std::memory_order_acquire);
                    if (q >= 0 && (s->armed.load(std::memory_order_acquire) & 3)) {
                        ::epoll_ctl(q, EPOLL_CTL_DEL, fd, nullptr);
                    }
#endif
                    s->clear();
                }
            }

            // The thread joined and the queue closed; a registration still
            // pending is dropped (its channel closed: a wait ends with
            // nothing), and so is every wait parked in a slot, which sees
            // the incarnation moved (cancelled); a wait registered while
            // the thread is being joined ends the same way. The next wait
            // starts the reactor again.
            void stop() {
#if SGCL_REACTOR_KQUEUE || SGCL_REACTOR_EPOLL
                std::lock_guard stopping(_stop_m);   // one stop at a time: a second (two threads stopping the scheduler, the destructor at exit beside a stop) would join the thread again; it waits and finds the reactor stopped
                {
                    std::lock_guard lock(_m);
                    if (!_running) {
                        return;
                    }
                    _stop = true;
                    _wake_thread();
                }
                _thread.join();
                std::vector<root_ptr<IoWait>> ended;
                {
                    std::lock_guard lock(_m);
                    _close_queue();
                    _running = false;
#if SGCL_REACTOR_KQUEUE
                    _take_all(ended);   // the waits still registered: ended with nothing
#endif
                }
                _end(ended, false);
                _sweep();
#endif
            }

        private:
            static constexpr uint64_t PollTag = uint64_t(1) << 63;   // a udata of a slot's registration, not of a one-shot one

            // The kernel's udata of a slot's registration: the tag, the
            // slot's generation, the number
            static uint64_t _udata(int fd, const PollSlot& s) noexcept {
                return PollTag | (uint64_t(s.gen.load(std::memory_order_relaxed) & 0x7fffffff) << 32) | uint32_t(fd);
            }

            // An event of a slot's registration, on the reactor's thread: the
            // slot fired when its generation is still the event's
            void _dispatch(uint64_t u, int dir, WakeBatch& batch) {
                int fd = int(uint32_t(u));
                uint32_t g = uint32_t(u >> 32) & 0x7fffffff;
                if (auto s = find(fd); s && (s->gen.load(std::memory_order_acquire) & 0x7fffffff) == g) {
                    s->fire(dir, &batch);
                }
            }

            // Every slot's waiters woken without readiness: the stop, or a
            // queue that failed, after the incarnation was taken back
            void _sweep() {
                for (auto& c : _chunks) {
                    if (auto chunk = c.load(std::memory_order_acquire)) {
                        for (auto& s : chunk->slots) {
                            s.interrupt(0);
                            s.interrupt(1);
                        }
                    }
                }
            }

            // Under _m: the queue closed, the incarnation taken back first
            // (sequentially consistent: the waiters' look after their
            // publication, against the sweep that follows)
            void _close_queue() {
                _live.store(0, std::memory_order_seq_cst);
                if (_kq >= 0) {
                    _queue.store(-1, std::memory_order_release);
                    ::close(_kq);
                    _kq = -1;
                }
#if SGCL_REACTOR_EPOLL
                if (_wake_fd >= 0) {
                    ::close(_wake_fd);
                    _wake_fd = -1;
                }
#endif
            }

#if SGCL_REACTOR_KQUEUE
            using Key = std::pair<int, short>;
            using Waits = std::vector<root_ptr<IoWait>>;

            // 0, or the errno of the registration
            int _register(int fd, PollSlot& s, int dir) {
                struct kevent ev;
                EV_SET(&ev, fd, dir ? EVFILT_WRITE : EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, (void*)(uintptr_t)_udata(fd, s));
                return ::kevent(_queue.load(std::memory_order_acquire), &ev, 1, nullptr, 0, nullptr) == 0 ? 0 : errno;
            }

            void _wake_thread() {
                if (_kq >= 0) {
                    struct kevent ev;
                    EV_SET(&ev, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
                    ::kevent(_kq, &ev, 1, nullptr, 0, nullptr);
                }
            }

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
            void _watch(int ident, short filter, unsigned fflags, const tracked_ptr<void>& keep, ChannelState<void>* ch) {
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
            // up) stays on the registration until the descriptor is ready,
            // which an idle descriptor read with a short deadline in a loop
            // may never be; so the closed ones are dropped each time the
            // count reaches twice what was left the time before. The
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
                _queue.store(kq, std::memory_order_release);
                _live.store(++_incarnations, std::memory_order_release);
                scheduler_stop_hook2.store([] { reactor_instance().stop(); }, std::memory_order_release);
                return true;
            }

            // The kernel's events. A slot's (the tag in its udata) is
            // handled at once, no lock taken, its wakes gathered into one
            // batch for the scheduler. A one-shot registration's, under
            // the lock, for the registration it names: an event whose
            // number is not the number of the node the table holds for its
            // pair (cancelled, or fired and registered again since) is
            // dropped. A wait that joined the registration after the
            // kernel's event and before this is signalled with the others:
            // the descriptor was ready a moment ago. The lock is taken only
            // for such events and for the wake of stop().
            void _run() {
                struct kevent events[64];
                Waits ready;
#if !defined(NDEBUG)
                const uintptr_t floor = dead_stack_floor();
#endif
                for (;;) {
#if !defined(NDEBUG)
                    // A build without NDEBUG: the dead stack zeroed before the
                    // wait, as the other threads of the library do theirs
                    // (scheduler.h: clear_dead_stack). The -O0 frames of a
                    // dispatch leave copies of the frames it woke (a wake
                    // batch's words, its temporaries) that an optimized
                    // build keeps in registers or nulls; the conservative
                    // scan of the parked reactor would keep them alive. A
                    // build with NDEBUG is left as it was (note 254)
                    clear_dead_stack(floor);
#endif
                    int n = ::kevent(_kq, nullptr, 0, events, 64, nullptr);
                    if (n < 0 && errno == EINTR) {
                        continue;
                    }
                    bool locked = n < 0;
                    if (n > 0) {
                        WakeBatch batch;
                        for (int i = 0; i < n; ++i) {
                            auto& ev = events[i];
                            uint64_t u = (uint64_t)(uintptr_t)ev.udata;
                            if (ev.filter == EVFILT_USER || !(u & PollTag)) {
                                locked = true;
                                continue;
                            }
                            _dispatch(u, ev.filter == EVFILT_WRITE ? 1 : 0, batch);
                        }
                    }
                    if (!locked) {
                        continue;
                    }
                    bool stop;
                    {
                        std::lock_guard lock(_m);
                        if (n < 0) {   // the queue has failed: the waits end with nothing, and the next wait joins this thread and makes a queue again (_start)
                            _take_all(ready);
                            _close_queue();
                            stop = true;
                        } else {
                            for (int i = 0; i < n; ++i) {
                                auto& ev = events[i];
                                if (ev.filter == EVFILT_USER || ((uint64_t)(uintptr_t)ev.udata & PollTag)) {
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
                    if (n < 0) {
                        _sweep();
                    }
                    if (stop) {
                        return;
                    }
                }
            }

            uint64_t _numbers = 0;   // the last registration's number (under _m)
            std::unordered_map<Key, IoNode, KeyHash> _pending;   // guarded by _m
#elif SGCL_REACTOR_EPOLL
            using Waits = std::vector<root_ptr<IoWait>>;

            // One entry per descriptor, both directions, edge-triggered
            // (Go's netpollopen); EEXIST: the entry made already (by a
            // waiter of the other direction in an older incarnation's
            // queue number, or a stale entry of a dup), set to this one
            int _register(int fd, PollSlot& s, int dir) {
                (void)dir;
                struct epoll_event ev = {};
                ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
                ev.data.u64 = _udata(fd, s);
                int q = _queue.load(std::memory_order_acquire);
                if (::epoll_ctl(q, EPOLL_CTL_ADD, fd, &ev) == 0) {
                    return 0;
                }
                if (errno != EEXIST) {
                    return errno;
                }
                return ::epoll_ctl(q, EPOLL_CTL_MOD, fd, &ev) == 0 ? 0 : errno;
            }

            void _wake_thread() {
                if (_wake_fd >= 0) {
                    uint64_t one = 1;
                    (void)!::write(_wake_fd, &one, sizeof(one));
                }
            }

            // The one-shot way on epoll: a channel waiter on the slot's
            // overflow list (never in its record, which is the
            // descriptors'), the level looked at after the push, so that a
            // descriptor ready already signals at once and one that becomes
            // ready after the look wakes the list
            void _watch_level(int fd, int dir, const tracked_ptr<void>& keep, ChannelState<void>* ch) {
                uint64_t inc = running();
                PollSlot* s = inc ? slot(fd) : nullptr;
                if (!s) {
                    if (inc) {
                        ch->close_ready();   // a number past the table: ready at once, the call after says what is
                    } else {
                        ch->close();
                    }
                    return;
                }
                if (arm(fd, *s, dir, inc)) {
                    ch->close_ready();   // not watched: ready at once, the call after says what is (the one-shot way's rule)
                    return;
                }
                tracked_ptr<PollWaiter> w = make_tracked<PollWaiter>();
                w->keep = keep;
                w->ch = ch;
                s->push(dir, w, true);
                struct pollfd p = {fd, short(dir ? POLLOUT : POLLIN), 0};
                bool now = ::poll(&p, 1, 0) > 0;
                if ((now || live() != inc) && s->remove(dir, w.get())) {
                    if (now) {
                        ch->close_ready();
                    } else {
                        ch->close();
                    }
                }
            }

            bool _start() {
                if (_running) {
                    if (_stop) {
                        return false;
                    }
                    if (_kq >= 0) {
                        return true;
                    }
                    _thread.join();
                    _running = false;
                }
                int q = ::epoll_create1(EPOLL_CLOEXEC);
                if (q < 0) {
                    return false;
                }
                int w = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
                struct epoll_event ev = {};
                ev.events = EPOLLIN;
                ev.data.u64 = 0;   // the wake for stop()
                if (w < 0 || ::epoll_ctl(q, EPOLL_CTL_ADD, w, &ev) < 0) {
                    if (w >= 0) {
                        ::close(w);
                    }
                    ::close(q);
                    return false;
                }
                _kq = q;
                _wake_fd = w;
                _stop = false;
                try {
                    _thread = std::thread([this] { _run(); });
                } catch (...) {
                    ::close(q);
                    ::close(w);
                    _kq = _wake_fd = -1;
                    throw;
                }
                _running = true;
                _queue.store(q, std::memory_order_release);
                _live.store(++_incarnations, std::memory_order_release);
                scheduler_stop_hook2.store([] { reactor_instance().stop(); }, std::memory_order_release);
                return true;
            }

            // The kernel's events, no lock but for the wake of stop(): a
            // hang-up or an error is a readiness of both directions (the
            // call says what is)
            void _run() {
                struct epoll_event events[64];
#if !defined(NDEBUG)
                const uintptr_t floor = dead_stack_floor();
#endif
                for (;;) {
#if !defined(NDEBUG)
                    // A build without NDEBUG: the dead stack zeroed before the
                    // wait, as the other threads of the library do theirs
                    // (scheduler.h: clear_dead_stack). The -O0 frames of a
                    // dispatch leave copies of the frames it woke (a wake
                    // batch's words, its temporaries) that an optimized
                    // build keeps in registers or nulls; the conservative
                    // scan of the parked reactor would keep them alive. A
                    // build with NDEBUG is left as it was (note 254)
                    clear_dead_stack(floor);
#endif
                    int n = ::epoll_wait(_kq, events, 64, -1);
                    if (n < 0 && errno == EINTR) {
                        continue;
                    }
                    if (n < 0) {
                        {
                            std::lock_guard lock(_m);
                            _close_queue();
                        }
                        _sweep();
                        return;
                    }
                    bool woken = false;
                    {
                        WakeBatch batch;
                        for (int i = 0; i < n; ++i) {
                            uint64_t u = events[i].data.u64;
                            if (u == 0) {
                                woken = true;
                                continue;
                            }
                            uint32_t e = events[i].events;
                            if (e & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                                _dispatch(u, 0, batch);
                            }
                            if (e & (EPOLLOUT | EPOLLHUP | EPOLLERR)) {
                                _dispatch(u, 1, batch);
                            }
                        }
                    }
                    if (woken) {
                        std::lock_guard lock(_m);
                        if (_stop) {
                            return;
                        }
                    }
                }
            }

            int _wake_fd = -1;
#endif
#if SGCL_REACTOR_KQUEUE || SGCL_REACTOR_EPOLL
            // The waits of the one-shot way ended, ready (the ready bit) or
            // with nothing, their channels closed; outside _m, since a
            // close wakes whoever waits on the channel
            static void _end(std::vector<root_ptr<IoWait>>& waits, bool ready) {
                for (auto& w : waits) {
                    if (ready) {
                        w->ch->close_ready();
                    } else {
                        w->ch->close();
                    }
                }
                waits.clear();
            }
#endif

            int _kq = -1;                               // the queue (under _m)
            std::atomic<int> _queue = {-1};             // the same, for the registrations made without the lock
            std::atomic<uint64_t> _live = {0};          // the running queue's incarnation, 0 when there is none
            uint64_t _incarnations = 0;                 // the last one given (under _m)
            std::atomic<PollChunk*> _chunks[PollChunkCount] = {};
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

    // An event set when fd can be read without blocking (event.h:
    // `co_await async::readable(fd)`, `.wait()` on a thread, `.on_set(f)`
    // in a select), or when the wait is ended with nothing (cancel_waits,
    // a close of the descriptor): a wait woken by it reads, and a read
    // that would block waits again. A wait given up before that (a select
    // lost to a timeout) is ended by setting the event: the reactor holds
    // it until fd is ready, and drops a set one at a later wait on the
    // descriptor.
    inline event readable(int fd) {
        tracked_ptr<detail::ChannelState<void>> ch = detail::make_linked_state<void>(1);
        detail::reactor_instance().watch(fd, false, ch, ch.get());
        return detail::EventAccess::make(std::move(ch));
    }

    // The same for a write
    inline event writable(int fd) {
        tracked_ptr<detail::ChannelState<void>> ch = detail::make_linked_state<void>(1);
        detail::reactor_instance().watch(fd, true, ch, ch.get());
        return detail::EventAccess::make(std::move(ch));
    }

    // The waits on fd ended with nothing: what a close of the descriptor
    // calls, before the number can be another descriptor's
    inline void cancel_waits(int fd) {
        detail::reactor_instance().cancel(fd);
    }

    // An event set when the process with this id has ended: `co_await
    // async::exited(pid)` holds no thread while a child runs, and waitpid
    // after it does not block (its status says how the child ended). A
    // process that has ended already, or that does not exist, sets it at
    // once.
    inline event exited(int pid) {
        tracked_ptr<detail::ChannelState<void>> ch = detail::make_linked_state<void>(1);
        detail::reactor_instance().watch_exit(pid, ch, ch.get());
        return detail::EventAccess::make(std::move(ch));
    }
}
