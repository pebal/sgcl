//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../../async/scheduler.h"
#include "../../../core/detail/bytes.h"
#include "../../../core/make_tracked.h"
#include "../../../core/root_ptr.h"
#include "../../../core/slice.h"
#include "../../../core/tracked_ptr.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

// Bytes of a body held on the managed heap in blocks of 8 KB (DESIGN 277:
// no system allocation on the path of a request): a response as its
// handler writes it (WriterImpl, both protocols) and the DATA of an HTTP/2
// stream until its reader takes it (h2::StreamState). The blocks are a
// type of their own, so they get pages of their own, and a list through
// their `next`: no container holds a tracked pointer. What a send takes is
// a slice of a block, the block its owner, so it lives while the send does.
namespace sgcl::net::http::detail {
    struct ByteChunk {
        static constexpr size_t Size = 8192;
        static constexpr size_t Room = Size - sizeof(tracked_ptr<void>) - 2 * sizeof(uint32_t);

        tracked_ptr<ByteChunk> next;
        uint32_t begin = 0;   // the first byte not yet taken
        uint32_t end = 0;     // the end of the bytes written
        std::byte bytes[Room];
    };

    static_assert(sizeof(ByteChunk) == ByteChunk::Size, "a block is 8 KB whole: a size that divides the page");

    // The blocks given back, kept for the next body on the same worker (as
    // TLS's RecordBlocks keeps its buffers, DESIGN 276 V2): at most Kept a
    // worker, the rest the collector's; nothing per connection. A list per
    // worker of the scheduler, in one managed object held by one root that
    // is never destroyed (as the method names of server_state.h): the
    // blocks are reached through it, no root of their own. Each worker's
    // list is its own (taken and given on that worker alone), so the lists
    // take no lock; a thread that is not a worker allocates and drops, as
    // before. Not zeroed on the way back: a body is no secret, a new one
    // writes over the block and its `end` bounds what can be read.
    struct ChunkPool {
        static constexpr uint32_t Kept = 16;   // 128 KB a worker
        static constexpr unsigned Workers = async::detail::Scheduler::MaxWorkers;

        struct alignas(64) List {
            tracked_ptr<ByteChunk> head;
            uint32_t count = 0;
            std::atomic<uint64_t> takes = {0};   // the statistics: blocks asked for, and found here
            std::atomic<uint64_t> hits = {0};
        };

        List lists[Workers];

        static ChunkPool& instance() {
            static root_ptr<ChunkPool>* pool = new root_ptr<ChunkPool>(make_tracked<ChunkPool>());
            return **pool;
        }

        // A block for a body: this worker's last given back, else a new one
        static tracked_ptr<ByteChunk> take() {
            const unsigned w = async::detail::Scheduler::worker_index();
            if (w >= Workers) {
                return make_tracked<ByteChunk>();
            }
            List& l = instance().lists[w];
            l.takes.fetch_add(1, std::memory_order_relaxed);
            if (!l.head) {
                return make_tracked<ByteChunk>();
            }
            l.hits.fetch_add(1, std::memory_order_relaxed);
            tracked_ptr<ByteChunk> c = l.head;
            l.head = c->next;
            --l.count;
            c->next = tracked_ptr<ByteChunk>();
            c->begin = 0;
            c->end = 0;
            return c;
        }

        // A block no body uses any more (every slice of it done): kept by
        // this worker up to Kept
        static void give(const tracked_ptr<ByteChunk>& c) {
            const unsigned w = async::detail::Scheduler::worker_index();
            if (w >= Workers) {
                return;
            }
            List& l = instance().lists[w];
            if (l.count >= Kept) {
                return;
            }
            c->next = l.head;
            l.head = c;
            ++l.count;
        }

        // The statistics of every worker together: blocks asked for, found
        struct Totals {
            uint64_t takes = 0;
            uint64_t hits = 0;
        };

        static Totals totals() noexcept {
            Totals t;
            for (auto& l : instance().lists) {
                t.takes += l.takes.load(std::memory_order_relaxed);
                t.hits += l.hits.load(std::memory_order_relaxed);
            }
            return t;
        }
    };

    // A queue of bytes over ByteChunk blocks: appended at the tail, taken
    // from the head, a block dropped once taken whole
    class ByteChunks {
    public:
        size_t size() const noexcept {
            return _size;
        }

        bool empty() const noexcept {
            return _size == 0;
        }

        void append(const void* data, size_t n) {
            const std::byte* p = static_cast<const std::byte*>(data);
            while (n) {
                if (!_tail || _tail->end == ByteChunk::Room) {
                    tracked_ptr fresh = ChunkPool::take();
                    if (_tail) {
                        _tail->next = fresh;
                    } else {
                        _head = fresh;
                    }
                    _tail = fresh;
                }
                const size_t k = std::min<size_t>(n, ByteChunk::Room - _tail->end);
                std::memcpy(_tail->bytes + _tail->end, p, k);
                _tail->end += uint32_t(k);
                _size += k;
                p += k;
                n -= k;
            }
        }

        void append(std::string_view s) {
            append(s.data(), s.size());
        }

        // Bytes read straight into the room at the tail (a block from the
        // pool when there is none): read(at, room) puts up to room bytes at
        // `at` and says how many, 0 at the end (or a failure), and it is
        // called until then. No buffer between the source and the blocks
        template<class Read>
        void append_read(Read&& read) {
            for (;;) {
                if (!_tail || _tail->end == ByteChunk::Room) {
                    tracked_ptr fresh = ChunkPool::take();
                    if (_tail) {
                        _tail->next = fresh;
                    } else {
                        _head = fresh;
                    }
                    _tail = fresh;
                }
                const size_t got = read(_tail->bytes + _tail->end, ByteChunk::Room - _tail->end);
                if (got == 0) {
                    return;
                }
                _tail->end += uint32_t(got);
                _size += got;
            }
        }

        // Up to n bytes from the head into out: how many
        size_t take(void* out, size_t n) {
            std::byte* o = static_cast<std::byte*>(out);
            size_t done = 0;
            while (done < n && _head) {
                const size_t k = std::min<size_t>(n - done, _head->end - _head->begin);
                std::memcpy(o + done, _head->bytes + _head->begin, k);
                _head->begin += uint32_t(k);
                done += k;
                _size -= k;
                if (_head->begin == _head->end) {
                    _pop();
                }
            }
            return done;
        }

        void clear() noexcept {
            _head = tracked_ptr<ByteChunk>();
            _tail = tracked_ptr<ByteChunk>();
            _size = 0;
        }

        // Every block given back to the worker's pool (no slice of them
        // left anywhere: the caller's word), the queue empty
        void release() {
            tracked_ptr<ByteChunk> c = _head;
            _head = tracked_ptr<ByteChunk>();
            _tail = tracked_ptr<ByteChunk>();
            _size = 0;
            while (c) {
                tracked_ptr<ByteChunk> next = c->next;
                ChunkPool::give(c);
                c = next;
            }
        }

        // The blocks handed over as their list (first and last), the queue
        // empty: for whoever gives them back to the pool later, once no
        // slice of them is left (an HTTP/2 connection, after the write
        // that sends them in place)
        void detach(tracked_ptr<ByteChunk>& first, tracked_ptr<ByteChunk>& last) noexcept {
            first = std::move(_head);
            last = std::move(_tail);
            _head = tracked_ptr<ByteChunk>();
            _tail = tracked_ptr<ByteChunk>();
            _size = 0;
        }

        // Every block of a list from detach given back to the worker's pool
        static void give_back(tracked_ptr<ByteChunk> c) {
            while (c) {
                tracked_ptr<ByteChunk> next = c->next;
                ChunkPool::give(c);
                c = next;
            }
        }

        // Each block's bytes not yet taken, in order, as a slice owned by
        // its block
        template<class F>
        void each(F&& f) const {
            for (tracked_ptr<ByteChunk> c = _head; c; c = c->next) {
                if (c->end > c->begin) {
                    f(slice<const byte>(tracked_ptr<const void>(c), c->bytes + c->begin, c->end - c->begin));
                }
            }
        }

        // Every byte appended to a std::string (a head's buffer)
        void copy_to(std::string& out) const {
            out.reserve(out.size() + _size);
            each([&](const slice<const byte>& s) {
                out.append(reinterpret_cast<const char*>(s.data()), s.size());
            });
        }

        // The first block, for a walk through `next` in a coroutine (no
        // callback across its awaits)
        const tracked_ptr<ByteChunk>& head() const noexcept {
            return _head;
        }

    private:
        tracked_ptr<ByteChunk> _head;
        tracked_ptr<ByteChunk> _tail;
        size_t _size = 0;

        // A block taken whole goes back to the pool (its bytes copied out)
        void _pop() {
            tracked_ptr<ByteChunk> done = _head;
            _head = _head->next;
            if (!_head) {
                _tail = tracked_ptr<ByteChunk>();
            }
            ChunkPool::give(done);
        }
    };

    // A response's body as its handler writes it: 64 bytes in place (a
    // short answer costs no block), past them ByteChunks
    class BodyBuffer {
    public:
        static constexpr size_t Inline = 64;

        size_t size() const noexcept {
            return _chunks.empty() ? _small_n : _chunks.size();
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        void append(const void* data, size_t n) {
            if (_chunks.empty() && _small_n + n <= Inline) {
                sgcl::detail::copy_bytes(_small + _small_n, data, n);
                _small_n += uint8_t(n);
                return;
            }
            if (_small_n) {
                _chunks.append(_small, _small_n);
                _small_n = 0;
            }
            _chunks.append(data, n);
        }

        void append(std::string_view s) {
            append(s.data(), s.size());
        }

        // Bytes read straight into the blocks (ByteChunks::append_read),
        // after what the buffer holds
        template<class Read>
        void append_read(Read&& read) {
            if (_small_n) {
                _chunks.append(_small, _small_n);
                _small_n = 0;
            }
            _chunks.append_read(std::forward<Read>(read));
        }

        void clear() noexcept {
            _small_n = 0;
            _chunks.clear();
        }

        // Empty, the blocks given back to the pool (every slice of them done)
        void release() {
            _small_n = 0;
            _chunks.release();
        }

        // The blocks handed over (ByteChunks::detach), the in-place bytes
        // dropped: the buffer empty
        void detach(tracked_ptr<ByteChunk>& first, tracked_ptr<ByteChunk>& last) noexcept {
            _small_n = 0;
            _chunks.detach(first, last);
        }

        // The bytes as slices in order (the in-place ones copied by the
        // caller before the buffer changes: a slice without an owner)
        template<class F>
        void each(F&& f) const {
            if (_small_n) {
                f(slice<const byte>(reinterpret_cast<const byte*>(_small), _small_n));
            }
            _chunks.each(f);
        }

        void copy_to(std::string& out) const {
            out.append(_small, _small_n);
            _chunks.copy_to(out);
        }

        // The first n bytes, as a copy (a response past its declared
        // length is cut)
        void copy_to(std::string& out, size_t n) const {
            each([&](const slice<const byte>& s) {
                const size_t k = std::min(n, s.size());
                out.append(reinterpret_cast<const char*>(s.data()), k);
                n -= k;
            });
        }

        // The in-place bytes and the blocks, for a walk in a coroutine
        std::string_view small() const noexcept {
            return std::string_view(_small, _small_n);
        }

        const ByteChunks& chunks() const noexcept {
            return _chunks;
        }

    private:
        ByteChunks _chunks;
        char _small[Inline];
        uint8_t _small_n = 0;
    };
}
