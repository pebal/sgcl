//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
#include "../../core/detail/handle_word.h"
#include "../../core/expected.h"
#include "../../core/make_tracked.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../io/stream.h"

#include <algorithm>
#include <cassert>
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

    // The text of n bytes, written by the codec straight into the
    // string's object, taken for the size the codec says: one pass and
    // each character written once, no staging copy and no allocation but
    // the string. The size is exact for base64, base32 and hex and a
    // bound for ascii85 (a 'z' is one character for four), whose string
    // keeps the class of the bound with the length it wrote. A result
    // longer than a string can hold (4 G characters) is length_error.
    template<class Codec>
    string encode_text(const Codec& c, const uint8_t* p, size_t n) {
        size_t bound = c.encode_bound(n);
        if (bound > string::max_size()) {
            throw length_error("sgcl: an encoding longer than a string can hold");
        }
        return sgcl::detail::StringAccess::bounded<string>(bound, [&](char* chars) {
            char* o = chars;
            auto q = p;
            c.encode_groups(q, p + n, o, chars + bound);
            c.encode_final(q, size_t(p + n - q), o);
            return size_t(o - chars);
        });
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
    expected<vector<byte>, error> decode_text(const Codec& c, const string& text) noexcept {
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
    expected<size_t, error> decode_to(const Codec& c, const slice<byte>& out, const char* p, size_t n) {
        if (out.size() < c.decode_bound(p, n)) {
            throw length_error("sgcl: the buffer is smaller than the most the text decodes to");
        }
        return decode_into(c, p, n, reinterpret_cast<uint8_t*>(out.data()), out.size());
    }

    template<class Codec>
    expected<size_t, error> decode_to(const Codec& c, const slice<byte>& out, const string& text) {
        return decode_to(c, out, text.data(), text.size());
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

        CodecWriter(const Codec& codec, const io::writer& out) noexcept
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

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept {
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

        async::task<expected<void, io::error>> async_close() noexcept {
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
        CodecReader(const Codec& codec, const io::reader& in) noexcept
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

        async::task<expected<size_t, io::error>> async_read(slice<byte> buffer) noexcept {
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

        expected<size_t, io::error> _failure() const noexcept {
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
        optional<expected<size_t, io::error>> _step(const slice<byte>& buffer) noexcept {
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

        void _received(const expected<size_t, io::error>& r) noexcept {
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

    // The tag of a stream handle's constructor from a state the library made
    struct CodecMade {
        explicit CodecMade() noexcept = default;
    };

    // The one way from a state to a handle: encoder_to, decoder_from,
    // dumper_to (base64.h, base32.h, hex.h, ascii85.h)
    struct CodecAccess {
        template<class H, class S>
        static H make(S&& state) noexcept {
            return H(CodecMade{}, std::forward<S>(state));   // the state as make_tracked gave it: held from here on
        }
    };

    // The public streams of the codecs as handles (a base64 encoder, a hex
    // dumper): one tracked word to the state (CodecWriter, CodecReader,
    // HexDumperState), copied and passed by value, the copies sharing one
    // stream. Made by the library alone, the state with the handle; a
    // default-constructed handle holds none (`!h`), and an operation on it
    // is a contract violation. A stream made of one (io::writer(h),
    // io::copy) binds the state, not the handle (io/stream.h: IsStreamHandle).
    // Each public type derives from one of these two and adds nothing,
    // so that its layout is the word's (req::handle).
    template<class Derived, class State>
    class WriterHandle
    : public io::mixin::writer<Derived> {
    public:
        using io::mixin::writer<Derived>::write;
        using io::mixin::writer<Derived>::async_write;

        WriterHandle() noexcept = default;

        expected<size_t, io::error> write(const slice<const byte>& data) const {
            return _get().write(data);
        }

        async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const noexcept {
            return _get().async_write(data);
        }

        // The rest written (the last group, the short line), the writer
        // underneath left open; a write after it is errc::closed
        expected<void, io::error> close() const {
            return _get().close();
        }

        async::task<expected<void, io::error>> async_close() const noexcept {
            return _get().async_close();
        }

        bool is_closed() const noexcept {
            return _get().is_closed();
        }

        // Whether this handle holds a stream
        explicit operator bool() const noexcept {
            return (bool)_s;
        }

        // The same stream: the same state
        friend bool operator==(const Derived& a, const Derived& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct io::detail::HandleAccess;
        friend struct sgcl::detail::HandleWord;
        friend struct CodecAccess;

        WriterHandle(CodecMade, tracked_ptr<State> s) noexcept
        : _s(std::move(s)) {
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        WriterHandle(sgcl::detail::FromWord, const tracked_ptr<State>& w) noexcept
        : _s(w) {
        }

        tracked_ptr<State>& _handle_word() noexcept {
            return _s;
        }

        const tracked_ptr<State>& _handle_word() const noexcept {
            return _s;
        }

        State& _get() const noexcept {
            assert(_s && "an empty encoding stream: made by encoder_to or dumper_to");
            return *_s;
        }

        const tracked_ptr<State>& _stream_state() const noexcept {
            return _s;
        }

        tracked_ptr<State> _s;
    };

    template<class Derived, class State>
    class ReaderHandle
    : public io::mixin::reader<Derived> {
    public:
        ReaderHandle() noexcept = default;

        expected<size_t, io::error> read(const slice<byte>& buffer) const {
            return _get().read(buffer);
        }

        async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const noexcept {
            return _get().async_read(buffer);
        }

        // Where the text went wrong, or why the reader under it failed
        const optional<error>& last_error() const noexcept {
            return _get().last_error();
        }

        explicit operator bool() const noexcept {
            return (bool)_s;
        }

        friend bool operator==(const Derived& a, const Derived& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct io::detail::HandleAccess;
        friend struct sgcl::detail::HandleWord;
        friend struct CodecAccess;

        ReaderHandle(CodecMade, tracked_ptr<State> s) noexcept
        : _s(std::move(s)) {
        }

        ReaderHandle(sgcl::detail::FromWord, const tracked_ptr<State>& w) noexcept
        : _s(w) {
        }

        tracked_ptr<State>& _handle_word() noexcept {
            return _s;
        }

        const tracked_ptr<State>& _handle_word() const noexcept {
            return _s;
        }

        State& _get() const noexcept {
            assert(_s && "an empty encoding stream: made by decoder_from");
            return *_s;
        }

        const tracked_ptr<State>& _stream_state() const noexcept {
            return _s;
        }

        tracked_ptr<State> _s;
    };
}
