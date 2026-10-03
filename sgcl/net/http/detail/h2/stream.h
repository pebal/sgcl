//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "hpack.h"
#include "../chunks.h"
#include "../../headers.h"
#include "../../../../async/channel.h"
#include "../../../../async/coroutine.h"
#include "../../../../core/aliases.h"
#include "../../../../core/expected.h"
#include "../../../../core/make_tracked.h"
#include "../../../../core/slice.h"
#include "../../../../core/tracked_ptr.h"
#include "../../../../io/error.h"

#include <cstring>
#include <mutex>
#include <string>
#include <system_error>

// One HTTP/2 stream as the rest of net::http sees it, on either side: the
// body that comes in (Body reads it as it reads a body off the wire in
// HTTP/1.1) and the way out (the response of the server's WriterImpl, the
// request of the client). The connection behind it is a StreamOwner: the
// server's connection (serve.h) or the client's (transport.h), each over
// its machine (connection.h) and its lock.
namespace sgcl::net::http::detail::h2 {
    // A field block to send, encoded by the connection under its lock (the
    // encoder's table is the connection's; the blocks go in the order they
    // were encoded). Made on the stack by whoever sends: nothing allocated
    struct FieldBlock {
        virtual void encode(Encoder& e, std::string& out) const noexcept = 0;

    protected:
        ~FieldBlock() = default;
    };

    // The connection, as its streams use it (virtual in detail: the
    // server's and the client's connections)
    class StreamOwner {
    public:
        virtual ~StreamOwner() = default;

        // HEADERS (and its CONTINUATIONs): not flow-controlled, queued at
        // once; an error when the stream or the connection is gone
        virtual expected<void, io::error> send_headers(uint32_t id, const FieldBlock& block, bool end_stream) = 0;

        // DATA, as the windows allow: the task ends when the last byte is
        // queued (END_STREAM with it when asked)
        virtual async::task<expected<void, io::error>> send_data(uint32_t id, slice<const byte> data, bool end_stream) noexcept = 0;

        // The same for a piece of a body's block (BodyBuffer), which the
        // connection may send in place, not copied: the block stays
        // unchanged until retire() has it and the connection gives it back
        // to the pool. The default copies, as send_data
        virtual async::task<expected<void, io::error>> send_block(uint32_t id, slice<const byte> data, bool end_stream) noexcept {
            return send_data(id, std::move(data), end_stream);
        }

        // A body's blocks once all of them are queued (send_now,
        // send_block): the connection gives them back to the worker's pool
        // when what it queued of them is written. The default gives them
        // back at once (it copied them)
        virtual void retire(BodyBuffer& body) {
            body.release();
        }

        // HEADERS (when a block is given; END_STREAM on them with
        // headers_end) and as much of `data` as the windows allow, in one
        // step: one hold of the connection's lock and one wake of its
        // writer, so that a response small enough goes out in one write
        // (over TLS one record), not a write for its HEADERS and another
        // for its DATA (`data` null: none; its blocks may be sent in place,
        // as by send_block, and go to retire() after). The result is the bytes of `data` taken (all of
        // them: END_STREAM with the last when end_stream; none and
        // end_stream: an empty DATA with END_STREAM); the rest is sent by
        // send_data. The owner without it (the default) sends the HEADERS
        // alone and takes nothing
        virtual expected<size_t, io::error> send_now(uint32_t id, const FieldBlock* block, bool headers_end, const BodyBuffer* data, bool end_stream) {
            if (block) {
                if (auto r = send_headers(id, *block, headers_end); !r) {
                    return unexpected(r.error());
                }
            }
            (void)data;
            (void)end_stream;
            return size_t(0);
        }

        // n bytes of the stream's body taken by its reader: the peer's
        // window given back
        virtual void consumed(uint32_t id, size_t n) = 0;

        // RST_STREAM with the code (a body given up: CANCEL)
        virtual void reset(uint32_t id, ErrorCode code) = 0;
    };

    // The error a stream reset gives its reader and writer
    inline io::error stream_reset_error(const char* op, ErrorCode code) noexcept {
        (void)code;
        return io::error(std::make_error_code(std::errc::connection_reset), op, "HTTP/2 stream");
    }

    // A stream's state: made by the connection when the stream opens, held
    // (one word) by the connection's table of its streams, by the Body that
    // reads it and by the writer that answers on it. The incoming bytes are
    // kept here, unmanaged, until read; the connection adds to them under
    // its lock and then this one's, a reader takes them under this one's
    // alone and gives the window back after (the order is always the
    // connection's lock before this one)
    struct StreamState {
        uint32_t id;
        tracked_ptr<StreamOwner> owner;
        async::channel<void> readable;      // bytes, the end or a reset came (a signal, one held)
        async::channel<void> writable;      // the window opened, or a reset
        tracked_ptr<void> request;          // the server's request (RequestImpl), for a handler that waits its turn
        http::headers trailers;             // set once, with the end

        StreamState(uint32_t id, tracked_ptr<StreamOwner> owner) noexcept
        : id(id), owner(std::move(owner)), readable(1), writable(1) {
        }

        // --- the connection's side (under its lock) -------------------------

        void add(const uint8_t* p, size_t n, bool end) {
            {
                std::lock_guard<std::mutex> g(_lock);
                if (n) {
                    _data.append(p, n);
                }
                _ended = _ended || end;
            }
            readable.try_send();
        }

        void end_with(http::headers fields) {
            {
                std::lock_guard<std::mutex> g(_lock);
                trailers = std::move(fields);
                _ended = true;
            }
            readable.try_send();
        }

        // `why`: the error the reader gets, when it is not the reset's (the
        // client's deadline: ETIMEDOUT; the connection lost)
        void reset_by(ErrorCode code, optional<io::error> why = nullopt) {
            {
                std::lock_guard<std::mutex> g(_lock);
                if (_reset) {
                    return;
                }
                _reset = true;
                _code = code;
                _why = std::move(why);
                _data.release();
            }
            readable.try_send();
            writable.try_send();
        }

        void window_opened() {
            writable.try_send();
        }

        // --- the reader's side -------------------------------------------------

        // Bytes of the body into out, 0 at its end; the window given back
        // for what was taken
        async::task<expected<size_t, io::error>> read(slice<byte> out) noexcept {
            for (;;) {
                size_t n = 0;
                bool ended = false;
                {
                    std::lock_guard<std::mutex> g(_lock);
                    if (_reset) {
                        co_return unexpected(_why ? *_why : stream_reset_error("read", _code));
                    }
                    n = _data.take(out.data(), out.size());
                    ended = _ended;
                }
                if (n) {
                    owner->consumed(id, n);
                    co_return n;
                }
                if (ended || out.empty()) {
                    co_return size_t(0);
                }
                co_await readable.receive();
            }
        }

        // The stream's end awaited, once every byte of the body was read (a
        // length met, trailers still to come): nothing more is to come but
        // the end; a byte more is the connection's to judge (§8.1.1), a
        // reset is the error
        async::task<expected<void, io::error>> wait_end() noexcept {
            for (;;) {
                {
                    std::lock_guard<std::mutex> g(_lock);
                    if (_reset) {
                        co_return unexpected(_why ? *_why : stream_reset_error("read", _code));
                    }
                    if (_ended) {
                        co_return expected<void, io::error>();
                    }
                }
                co_await readable.receive();
            }
        }

        // What came is dropped (its window given back); whether the body
        // had come to its end
        bool drop_buffered() {
            size_t n = 0;
            bool ended = false;
            {
                std::lock_guard<std::mutex> g(_lock);
                n = _data.size();
                _data.release();
                ended = _ended && !_reset;
            }
            if (n) {
                owner->consumed(id, n);
            }
            return ended;
        }

        bool ended() const noexcept {
            std::lock_guard<std::mutex> g(_lock);
            return _ended && _data.empty();
        }

        bool was_reset() const noexcept {
            std::lock_guard<std::mutex> g(_lock);
            return _reset;
        }

        const http::headers& trailers_ref() const noexcept {
            return trailers;
        }

    private:
        mutable std::mutex _lock;
        detail::ByteChunks _data;   // the DATA come, not yet read: managed blocks (DESIGN 277)
        bool _ended = false;
        bool _reset = false;
        ErrorCode _code = ErrorCode::no_error;
        optional<io::error> _why;
    };
}
