//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../core/array.h"
#include "../../core/vector.h"
#include "../../core/aliases.h"
#include "../../core/config.h"
#include "../../core/detail/bytes.h"
#include "../../core/expected.h"
#include "../../core/make_tracked.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../io/stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;
    // What the byte codecs (base64, base32, hex, ascii85) share: the whole
    // text in and out, a caller's buffer in and out, and a stream each
    // way. A codec is a value with
    //   GroupBytes, MaxGroupChars    the bytes of a whole group, the most
    //                                characters one takes
    //   name()                       "base64", for the messages
    //   encode_bound(n)              the most characters n bytes take
    //   decode_bound(text, n)        the most bytes a text decodes to
    //   encode_groups, encode_final  whole groups while there is room, and
    //                                the short group at the end
    //   decoding, feed, finish       the state of a decoding given its
    //                                input in pieces, and its end
    // (radix.h, ascii85.h); this is the rest, once.

    // How a step of decoding ended: everything given was taken, the output
    // had no room for the next group (which is left untaken), or the
    // input is not valid (the error set)
    enum class FeedStatus : uint8_t {
        more,
        room,
        failed
    };

    // The block a stream encodes into or decodes from: the block of io's
    // buffered streams, 8 KB, eight to a page, no header
    using CodecBlock = array<byte, config::io_buffer_size>;

    // The text of n bytes. Laid out once into a std::string of the size
    // the codec says and copied into the string at the end: one pass of
    // the codec and one copy, where a string grown a character at a time
    // costs ten times the writing. A result longer than a string can hold
    // (4 G characters) is length_error, as std::string's own limit is.
    template<class Codec>
    string encode_text(const Codec& c, const uint8_t* p, size_t n) {
        size_t bound = c.encode_bound(n);
        if (bound > string::max_size()) {
            throw length_error("sgcl: an encoding longer than a string can hold");
        }
        std::string s;
        s.resize(bound);
        char* o = s.data();
        auto q = p;
        c.encode_groups(q, p + n, o, s.data() + bound);
        c.encode_final(q, size_t(p + n - q), o);
        return string(s.data(), size_t(o - s.data()));
    }

    // Into a caller's buffer, which holds encode_bound(n): the characters
    // written
    template<class Codec>
    size_t encode_into(const Codec& c, const slice<char>& out, const uint8_t* p, size_t n) {
        size_t bound = c.encode_bound(n);
        if (out.size() < bound) {
            throw length_error("sgcl: the buffer is smaller than the encoding");
        }
        char* o = out.data();
        auto q = p;
        c.encode_groups(q, p + n, o, out.data() + out.size());
        c.encode_final(q, size_t(p + n - q), o);
        return size_t(o - out.data());
    }

    // The bytes of a text into room bytes at out, room at least the
    // codec's bound for the text: the bytes written, or the error. The
    // bound is what every group the text starts can write, so the room
    // cannot run out before the text ends or fails; the check below is
    // the proof held to account.
    template<class Codec>
    expected<size_t, error> decode_into(const Codec& c, const char* p, size_t n, uint8_t* out, size_t room) {
        typename Codec::decoding d;
        optional<error> e;
        auto in = p;
        auto o = out;
        auto status = c.feed(d, in, p + n, o, out + room, e);
        if (status == FeedStatus::more) {
            status = c.finish(d, o, out + room, e);
        }
        if (status == FeedStatus::failed) {
            return unexpected<error>(std::move(*e));
        }
        if (status == FeedStatus::room) {
            throw logic_error("sgcl: a decoding ran out of the room its bound gave");
        }
        return size_t(o - out);
    }

    template<class Codec>
    expected<vector<byte>, error> decode_text(const Codec& c, const string& text) {
        vector<byte> out;
        out.resize(c.decode_bound(text.data(), text.size()));
        auto r = decode_into(c, text.data(), text.size(), reinterpret_cast<uint8_t*>(out.data()), out.size());
        if (!r) {
            return unexpected<error>(std::move(r.error()));
        }
        out.resize(*r);
        return out;
    }

    template<class Codec>
    expected<size_t, error> decode_to(const Codec& c, const slice<byte>& out, const string& text) {
        if (out.size() < c.decode_bound(text.data(), text.size())) {
            throw length_error("sgcl: the buffer is smaller than the most the text decodes to");
        }
        return decode_into(c, text.data(), text.size(), reinterpret_cast<uint8_t*>(out.data()), out.size());
    }

    // The encoder as a stream: a writer whose bytes go out encoded to
    // another writer. A write encodes the whole groups it has into the
    // block and writes the block; the bytes that do not make a group yet
    // wait in the carry for the next write; close() writes the last,
    // short group with its padding. close() does not close the writer
    // underneath, which usually goes on (a PEM block's end line, a MIME
    // part's boundary), and a write after it is errc::closed. A failure of
    // the writer underneath is kept for good, as Go's encoder keeps it: the
    // group that was being written is gone with it, so every later write
    // and close reports that failure rather than going on without it.
    template<class Codec>
    class CodecWriter
    : public io::mixin::writer<CodecWriter<Codec>> {
    public:
        using io::mixin::writer<CodecWriter<Codec>>::write;
        using io::mixin::writer<CodecWriter<Codec>>::async_write;

        CodecWriter(const Codec& codec, const io::writer& out)
        : _codec(codec), _out(out), _block(make_tracked<CodecBlock>()) {
        }

        expected<size_t, io::error> write(const slice<const byte>& data) {
            if (_error) {
                return io::detail::fail(*_error);
            }
            if (_closed) {
                return io::detail::fail(io::error(io::errc::closed, "write", _codec.name()));
            }
            auto p = reinterpret_cast<const uint8_t*>(data.data());
            auto end = p + data.size();
            for (;;) {
                size_t n = _fill(p, end);
                if (n == 0) {
                    return data.size();
                }
                auto w = _out.write(_chunk(n));
                if (!w) {
                    _error = w.error();
                    return io::detail::fail(w);
                }
            }
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            if (_error) {
                co_return io::detail::fail(*_error);
            }
            if (_closed) {
                co_return io::detail::fail(io::error(io::errc::closed, "write", _codec.name()));
            }
            auto p = reinterpret_cast<const uint8_t*>(data.data());
            auto end = p + data.size();
            for (;;) {
                size_t n = _fill(p, end);
                if (n == 0) {
                    co_return data.size();
                }
                auto w = co_await _out.async_write(_chunk(n));
                if (!w) {
                    _error = w.error();
                    co_return io::detail::fail(w);
                }
            }
        }

        // Writes the last group; the writer underneath stays open
        expected<void, io::error> close() {
            if (_closed || _error) {
                _closed = true;
                return _error ? expected<void, io::error>(io::detail::fail(*_error)) : expected<void, io::error>();
            }
            _closed = true;
            size_t n = _final();
            if (n) {
                auto w = _out.write(_chunk(n));
                if (!w) {
                    _error = w.error();
                    return io::detail::fail(w);
                }
            }
            return {};
        }

        async::task<expected<void, io::error>> async_close() {
            if (_closed || _error) {
                _closed = true;
                co_return _error ? expected<void, io::error>(io::detail::fail(*_error)) : expected<void, io::error>();
            }
            _closed = true;
            size_t n = _final();
            if (n) {
                auto w = co_await _out.async_write(_chunk(n));
                if (!w) {
                    _error = w.error();
                    co_return io::detail::fail(w);
                }
            }
            co_return expected<void, io::error>();
        }

        bool is_closed() const noexcept {
            return _closed;
        }

    private:
        char* _chars() const noexcept {
            return reinterpret_cast<char*>(_block->data());
        }

        slice<const byte> _chunk(size_t n) const noexcept {
            return slice<const byte>(_block, _block->data(), n);
        }

        // The characters of the carry and of the input from p that fit the
        // block; the bytes left over when fewer than a group remain go to
        // the carry and p to the end. 0: nothing to write yet.
        size_t _fill(const uint8_t*& p, const uint8_t* end) noexcept {
            char* o = _chars();
            char* o_end = o + _block->size();
            if (_carried) {
                while (_carried < Codec::GroupBytes && p != end) {
                    _carry[_carried++] = *p++;
                }
                if (_carried < Codec::GroupBytes) {
                    return 0;
                }
                const uint8_t* c = _carry;
                _codec.encode_groups(c, _carry + Codec::GroupBytes, o, o_end);
                _carried = 0;
            }
            _codec.encode_groups(p, end, o, o_end);
            if (size_t(end - p) < Codec::GroupBytes) {
                while (p != end) {
                    _carry[_carried++] = *p++;
                }
            }
            return size_t(o - _chars());
        }

        size_t _final() noexcept {
            char* o = _chars();
            _codec.encode_final(_carry, _carried, o);
            _carried = 0;
            return size_t(o - _chars());
        }

        Codec _codec;
        io::writer _out;
        tracked_ptr<CodecBlock> _block;
        uint8_t _carry[Codec::GroupBytes] {};
        uint8_t _carried = 0;
        optional<io::error> _error;   // the first failure of the writer under it, for good
        bool _closed = false;
    };

    // The decoder as a stream: a reader of the bytes another reader's
    // text decodes to. The text is read into the block and decoded
    // straight into the caller's buffer; a buffer smaller than a group's
    // bytes gets them through a small pending array. The state of the
    // decoding holds a group cut between two reads, so the text may come
    // in pieces of any size. An invalid text is an error of the read that
    // reaches it — after the bytes before it were handed out — and of
    // every read after; last_error() says where, as an error
    // with the offset in the text.
    template<class Codec>
    class CodecReader
    : public io::mixin::reader<CodecReader<Codec>> {
    public:
        CodecReader(const Codec& codec, const io::reader& in)
        : _codec(codec), _in(in), _block(make_tracked<CodecBlock>()) {
        }

        expected<size_t, io::error> read(const slice<byte>& buffer) {
            if (buffer.empty()) {
                return 0;
            }
            for (;;) {
                if (auto r = _step(buffer)) {
                    return std::move(*r);
                }
                _received(_in.read(_room()));
            }
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> buffer) {
            if (buffer.empty()) {
                co_return 0;
            }
            for (;;) {
                if (auto r = _step(buffer)) {
                    co_return std::move(*r);
                }
                _received(co_await _in.async_read(_room()));
            }
        }

        // Where the text went wrong, or why the reader under it failed
        const optional<error>& last_error() const noexcept {
            return _error;
        }

    private:
        const char* _chars() const noexcept {
            return reinterpret_cast<const char*>(_block->data());
        }

        slice<byte> _room() const noexcept {
            return slice<byte>(_block, _block->data(), _block->size());
        }

        expected<size_t, io::error> _failure() const {
            return io::detail::fail(to_io_error(*_error, _codec.name()));
        }

        size_t _hand_out(const slice<byte>& buffer) noexcept {
            size_t n = std::min(buffer.size(), size_t(_pending_end - _pending_begin));
            copy_bytes(buffer.data(), _pending + _pending_begin, n);
            _pending_begin += uint8_t(n);
            return n;
        }

        // One step: the bytes the block holds decoded into the buffer, or
        // nullopt when more text is needed
        optional<expected<size_t, io::error>> _step(const slice<byte>& buffer) {
            if (_pending_begin < _pending_end) {
                return expected<size_t, io::error>(_hand_out(buffer));
            }
            if (_error) {
                return _failure();
            }
            auto out = reinterpret_cast<uint8_t*>(buffer.data());
            auto out_end = out + buffer.size();
            while (_begin < _end) {
                auto o = out;
                auto p = _chars() + _begin;
                auto status = _codec.feed(_state, p, _chars() + _end, o, out_end, _error);
                _begin = size_t(p - _chars());
                if (o != out) {
                    return expected<size_t, io::error>(size_t(o - out));
                }
                if (status == FeedStatus::failed) {
                    return _failure();
                }
                if (status == FeedStatus::room) {
                    uint8_t* q = _pending;
                    status = _codec.feed(_state, p, _chars() + _end, q, _pending + sizeof _pending, _error);
                    _begin = size_t(p - _chars());
                    _pending_begin = 0;
                    _pending_end = uint8_t(q - _pending);
                    if (_pending_end) {
                        return expected<size_t, io::error>(_hand_out(buffer));
                    }
                    if (status == FeedStatus::failed) {
                        return _failure();
                    }
                }
            }
            if (_eof) {
                if (!_finished) {
                    _finished = true;
                    uint8_t* q = _pending;
                    if (_codec.finish(_state, q, _pending + sizeof _pending, _error) == FeedStatus::failed) {
                        return _failure();
                    }
                    _pending_begin = 0;
                    _pending_end = uint8_t(q - _pending);
                    if (_pending_end) {
                        return expected<size_t, io::error>(_hand_out(buffer));
                    }
                }
                return expected<size_t, io::error>(0);
            }
            return nullopt;
        }

        void _received(const expected<size_t, io::error>& r) {
            if (!r) {
                _error = error(r.error(), _state.offset);
            } else if (*r == 0) {
                _eof = true;
            } else {
                _begin = 0;
                _end = *r;
            }
        }

        Codec _codec;
        io::reader _in;
        tracked_ptr<CodecBlock> _block;
        typename Codec::decoding _state;
        optional<error> _error;
        size_t _begin = 0, _end = 0;   // the text of the block not decoded yet
        uint8_t _pending[Codec::MaxGroupBytes] {};
        uint8_t _pending_begin = 0, _pending_end = 0;
        bool _eof = false;
        bool _finished = false;
    };
}
