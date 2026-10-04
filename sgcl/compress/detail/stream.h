//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "block.h"
#include "copy.h"
#include "deflate.h"
#include "inflate.h"
#include "../error.h"
#include "../level.h"
#include "../limits.h"
#include "../../async/scheduler.h"
#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/utf8.h"
#include "../../core/vector.h"
#include "../../hash/adler32.h"
#include "../../hash/crc32.h"
#include "../../io/stream.h"
#include "../../time/datetime.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace sgcl::compress::detail {
    using namespace sgcl::detail;

    SGCL_INLINE_HOT const uint8_t* bytes(const slice<const byte>& s) noexcept {
        return reinterpret_cast<const uint8_t*>(s.data());
    }

    SGCL_INLINE_HOT slice<const byte> view(const std::vector<uint8_t>& v) noexcept {
        return slice<const byte>(reinterpret_cast<const byte*>(v.data()), v.size());
    }

    // The plain memory a whole compress gathers its output in (the coders
    // write into a std::vector), lent by the thread: a vector kept from one
    // call to the next, so that a compress in memory allocates its result
    // and not, again, the room it was written in (a malloc and its growth a
    // call before). Two are kept, for the second buffer of xz; a call made
    // while both are out gets one of its own. One grown past KeepBytes is
    // let go at the end of its call.
    class LentOutput {
    public:
        LentOutput() noexcept {
            auto& k = _kept();
            for (auto& slot : k.slots) {
                if (!slot.lent) {
                    slot.lent = true;
                    _slot = &slot;
                    _out.swap(slot.room);
                    break;
                }
            }
            _out.clear();
        }

        LentOutput(const LentOutput&) = delete;
        LentOutput& operator=(const LentOutput&) = delete;

        SGCL_INLINE_HOT ~LentOutput() {
            if (_slot) {
                if (_out.capacity() <= KeepBytes) {
                    _out.clear();
                    _slot->room.swap(_out);
                }
                _slot->lent = false;
            }
        }

        SGCL_INLINE_HOT std::vector<uint8_t>& out() noexcept {
            return _out;
        }

    private:
        static constexpr size_t KeepBytes = size_t(4) << 20;

        struct Slot {
            std::vector<uint8_t> room;
            bool lent = false;
        };

        struct Kept {
            Slot slots[2];
        };

        static Kept& _kept() noexcept {
            thread_local Kept kept;
            return kept;
        }

        std::vector<uint8_t> _out;
        Slot* _slot = nullptr;
    };

    // The Deflater of a whole compress in memory, lent by the thread as
    // LentOutput's room is: kept from one call to the next and reset to the
    // call's level, so that a small compress does not allocate and clear
    // its window and tables (some 0.9 MB at level 6) each time. The kept
    // one holds the largest tables of the levels the thread has used, for
    // the thread's life. A call made while it is out gets one of its own.
    // Only for a compress that ends within the call: a stream's Deflater
    // may live across a suspension or move to another thread.
    class LentDeflater {
    public:
        LentDeflater(int level, const uint8_t* dictionary, size_t dictionary_size, size_t input_size) noexcept {
            auto& k = _kept();
            if (k.lent) {
                _own = std::make_unique<Deflater>(level, dictionary, dictionary_size);
                _deflater = _own.get();
                return;
            }
            if (k.deflater) {
                k.deflater->reset(level, dictionary, dictionary_size, input_size);
            } else {
                k.deflater = std::make_unique<Deflater>(level, dictionary, dictionary_size);
            }
            k.lent = true;
            _kept_by = &k;
            _deflater = k.deflater.get();
        }

        LentDeflater(const LentDeflater&) = delete;
        LentDeflater& operator=(const LentDeflater&) = delete;

        SGCL_INLINE_HOT ~LentDeflater() {
            if (_kept_by) {
                _kept_by->lent = false;
            }
        }

        SGCL_INLINE_HOT Deflater& operator*() const noexcept {
            return *_deflater;
        }

        SGCL_INLINE_HOT Deflater* operator->() const noexcept {
            return _deflater;
        }

        // the thread's own Deflater (not one made because it was out)
        SGCL_INLINE_HOT bool kept() const noexcept {
            return _kept_by != nullptr;
        }

    private:
        struct Kept {
            std::unique_ptr<Deflater> deflater;
            bool lent = false;
        };

        static Kept& _kept() noexcept {
            thread_local Kept kept;
            return kept;
        }

        Deflater* _deflater = nullptr;
        std::unique_ptr<Deflater> _own;
        Kept* _kept_by = nullptr;
    };

    // The move assignment of a reader or a writer of the module: the
    // object made anew from the other by its move constructor, which
    // leaves the other closed; a move onto itself changes nothing
    template<class T>
    SGCL_INLINE_HOT T& move_into(T& to, T&& from) noexcept {
        static_assert(std::is_final_v<T> && std::is_nothrow_move_constructible_v<T>);
        if (&to != &from) {
            to.~T();
            ::new (static_cast<void*>(std::addressof(to))) T(std::move(from));
        }
        return to;
    }

    // What a reader moved from says to every read: it has no stream (its
    // stream went with the move), as a closed one; no place in any data
    inline error moved_from_error(const char* format) noexcept {
        error e(io::error(io::errc::closed, "read", format), 0);
        return std::move(ErrorAccess::without_place(e));
    }

    SGCL_INLINE_HOT vector<byte> to_vector(const uint8_t* p, size_t n) noexcept {
        auto b = reinterpret_cast<const byte*>(p);
        return vector<byte>(b, b + n);
    }

    // ISO 8859-1 to UTF-8, and back (nullopt for a character past U+00FF
    // or text that is not UTF-8). The first may throw: more than 2 GiB of
    // ISO 8859-1 is a text past string's 4 GiB (length_error)
    inline string latin1_to_utf8(const uint8_t* p, size_t n) {
        std::string s;
        s.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            if (p[i] < 0x80) {
                s += char(p[i]);
            } else {
                s += char(0xC0 | (p[i] >> 6));
                s += char(0x80 | (p[i] & 0x3F));
            }
        }
        return string(s);
    }

    inline optional<std::string> utf8_to_latin1(const string& text) noexcept {
        std::string s;
        auto v = text.view();
        for (size_t i = 0; i < v.size(); ++i) {
            uint8_t c = uint8_t(v[i]);
            if (c < 0x80) {
                s += char(c);
            } else if ((c & 0xE0) == 0xC0 && c >= 0xC2 && i + 1 < v.size() && (uint8_t(v[i + 1]) & 0xC0) == 0x80) {
                uint32_t cp = (uint32_t(c & 0x1F) << 6) | (uint8_t(v[i + 1]) & 0x3F);
                if (cp > 0xFF) {
                    return nullopt;
                }
                s += char(cp);
                ++i;
            } else {
                return nullopt;
            }
        }
        return s;
    }

    // A name or a comment of gzip's header as written: ISO 8859-1, as the
    // format has it (RFC 1952, 2.3.1), where every character fits and the
    // bytes cannot be taken for UTF-8 when read; else the text's own bytes,
    // as gzip(1) writes a file's name — UTF-8 past U+00FF, and a name that
    // is not UTF-8 as it is. Read (gzip_text), bytes that are UTF-8 are
    // taken as they are and others as ISO 8859-1, so whatever a program
    // writes reads back the same: é is E9, which is not UTF-8; Ã© would be
    // C3 A9, which is (é), so it goes as its UTF-8, C3 83 C2 A9
    SGCL_INLINE_HOT std::string gzip_text_bytes(const string& text) noexcept {
        auto latin1 = utf8_to_latin1(text);
        if (latin1 && !utf8::valid(*latin1)) {
            return std::move(*latin1);
        }
        return std::string(text.view());   // ASCII, or the UTF-8 (or other) bytes
    }

    // May throw as latin1_to_utf8 does
    SGCL_INLINE_HOT string gzip_text(const uint8_t* p, size_t n) {
        std::string_view v(reinterpret_cast<const char*>(p), n);
        if (utf8::valid(v)) {
            return string(v);
        }
        return latin1_to_utf8(p, n);
    }

    template<class Out>
    SGCL_INLINE_HOT void put_le32(Out& out, uint32_t v) noexcept {
        uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        out.insert(out.end(), b, b + 4);
    }

    template<class Out>
    SGCL_INLINE_HOT void put_be32(Out& out, uint32_t v) noexcept {
        uint8_t b[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
        out.insert(out.end(), b, b + 4);
    }

    SGCL_INLINE_HOT uint32_t le32(const uint8_t* p) noexcept {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }

    SGCL_INLINE_HOT uint32_t be32(const uint8_t* p) noexcept {
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | uint32_t(p[3]);
    }

    // What a header or a trailer parse says about the bytes it was given
    struct Parsed {
        enum : uint8_t { more, ok, failed } status = more;
        size_t size = 0;             // the bytes of the header or trailer when ok
        errc code = errc::invalid_header;
        const char* text = nullptr;

        SGCL_INLINE_HOT static Parsed need() noexcept {
            return {};
        }

        SGCL_INLINE_HOT static Parsed done(size_t n) noexcept {
            return {ok, n, errc::invalid_header, nullptr};
        }

        static Parsed fail(errc c, const char* t) noexcept {
            return {failed, 0, c, t};
        }
    };

    // The three wrappings of DEFLATE (gzip's GzipFormat in gzip.h): what
    // goes before the data and after it, and the checksum between. A
    // format is a value the stream keeps:
    //   name                       "gzip", for the messages
    //   start(out) / finish(out)   the writer's header and trailer; start gives what
    //                              is wrong with a header it cannot write (nullptr: written)
    //   refuses                    whether start may refuse a header
    //   header(p, n) / trailer(p, n)   the reader's, parsed from the bytes there are
    //   update(p, n)               the checksum over the data
    //   members                    whether another member may follow the trailer
    struct FlateFormat {
        static constexpr const char* name = "flate";
        static constexpr bool members = false;
        static constexpr bool refuses = false;

        template<class Out>
        SGCL_INLINE_HOT const char* start(Out&, int) noexcept {
            return nullptr;
        }

        SGCL_INLINE_HOT void update(const uint8_t*, size_t) noexcept {
        }

        template<class Out>
        SGCL_INLINE_HOT void finish(Out&) noexcept {
        }

        SGCL_INLINE_HOT Parsed header(const uint8_t*, size_t) noexcept {
            return Parsed::done(0);
        }

        SGCL_INLINE_HOT Parsed trailer(const uint8_t*, size_t) noexcept {
            return Parsed::done(0);
        }

        SGCL_INLINE_HOT void reset() noexcept {
        }
    };

    struct ZlibFormat {
        static constexpr const char* name = "zlib";
        static constexpr bool members = false;
        static constexpr bool refuses = false;
        optional<uint32_t> dictionary_id;   // of the dictionary given (the writer's), or the one the stream asks for (the reader's)
        hash::adler32 sum;

        template<class Out>
        const char* start(Out& out, int lvl) noexcept {
            // CMF: deflate, a window of 32 KB; FLG: the level's hint, FDICT, the check bits
            uint8_t cmf = 0x78;
            uint8_t hint = lvl == level::store || lvl == level::huffman_only || lvl == level::fastest ? 0 : lvl < level::standard ? 1 : lvl == level::standard ? 2 : 3;
            uint8_t flg = uint8_t(hint << 6);
            if (dictionary_id) {
                flg |= 0x20;
            }
            flg = uint8_t(flg | (31 - ((cmf * 256 + flg) % 31)) % 31);
            out.push_back(cmf);
            out.push_back(flg);
            if (dictionary_id) {
                put_be32(out, *dictionary_id);
            }
            return nullptr;
        }

        SGCL_INLINE_HOT void update(const uint8_t* p, size_t n) noexcept {
            sum.update(slice<const byte>(reinterpret_cast<const byte*>(p), n));
        }

        template<class Out>
        SGCL_INLINE_HOT void finish(Out& out) noexcept {
            put_be32(out, sum.value());
        }

        Parsed header(const uint8_t* p, size_t n) noexcept {
            if (n < 2) {
                return Parsed::need();
            }
            uint8_t cmf = p[0], flg = p[1];
            if ((cmf & 15) != 8 || (cmf >> 4) > 7) {
                return Parsed::fail(errc::invalid_header, "zlib: not a deflate stream with a window of 32 KB or less");
            }
            if ((cmf * 256u + flg) % 31) {
                return Parsed::fail(errc::invalid_header, "zlib: header check bits do not match");
            }
            if (flg & 0x20) {
                if (n < 6) {
                    return Parsed::need();
                }
                dictionary_id = be32(p + 2);
                return Parsed::done(6);
            }
            dictionary_id = nullopt;
            return Parsed::done(2);
        }

        SGCL_INLINE_HOT Parsed trailer(const uint8_t* p, size_t n) noexcept {
            if (n < 4) {
                return Parsed::need();
            }
            if (be32(p) != sum.value()) {
                return Parsed::fail(errc::checksum, "zlib: Adler-32 mismatch");
            }
            return Parsed::done(4);
        }

        SGCL_INLINE_HOT void reset() noexcept {
            sum = hash::adler32();
        }
    };

    // The compressing half of a stream: what is written is compressed,
    // in pieces of 64 KB of input, and what the pieces make is written to
    // out as it comes. The task's forms yield between the pieces, so that
    // a large write keeps its worker without starving the others.
    // flush() makes everything so far decodable at once (a sync flush);
    // close() writes the last block and the trailer and leaves out open.
    // Every error given is kept for good (a failure of out, and a write or
    // a flush after close); the close gives it too.
    template<class Format>
    class DeflateWriter
    : public io::mixin::writer<DeflateWriter<Format>> {
    public:
        using io::mixin::writer<DeflateWriter<Format>>::write;
        using io::mixin::writer<DeflateWriter<Format>>::async_write;
    private:
        static constexpr size_t Piece = size_t(64) << 10;

    public:

        SGCL_INLINE_HOT DeflateWriter(const io::writer& out, int level, const Format& format, const slice<const byte>& dictionary) noexcept
        : _out(out)
        , _format(format)
        , _level(level)
        , _dictionary(bytes(dictionary), bytes(dictionary) + dictionary.size())
        , _deflater(std::make_unique<Deflater>(level, bytes(dictionary), dictionary.size())) {
        }

        DeflateWriter(const DeflateWriter&) = delete;
        DeflateWriter& operator=(const DeflateWriter&) = delete;

        // The other left closed, its stream and its encoder gone with the
        // move (its settings kept: the format, the level, the dictionary):
        // its writes give io::errc::closed, its close does nothing, and a
        // reset gives it a new stream. The derived class assigns through
        // move_into.
        SGCL_INLINE_HOT DeflateWriter(DeflateWriter&& o) noexcept
        : _out(std::move(o._out))
        , _format(o._format)
        , _level(o._level)
        , _dictionary(o._dictionary)
        , _deflater(std::move(o._deflater))
        , _pending(std::move(o._pending))
        , _error(std::move(o._error))
        , _started(o._started)
        , _closed(o._closed) {
            o._out = io::writer();
            o._pending = ManagedOutput();
            o._error = nullopt;
            o._started = true;
            o._closed = true;
        }

        DeflateWriter& operator=(DeflateWriter&&) = delete;

        expected<size_t, io::error> write(const slice<const byte>& data) {
            if (auto e = _check("write")) {
                return io::detail::fail(*e);
            }
            auto p = bytes(data);
            size_t n = data.size();
            while (n) {
                size_t k = std::min(n, Piece);
                _format.update(p, k);
                _deflater->write(p, k, _pending);
                p += k;
                n -= k;
                if (auto e = _drain()) {
                    return io::detail::fail(*e);
                }
            }
            return data.size();
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept {
            if (auto e = _check("write")) {
                co_return io::detail::fail(*e);
            }
            auto p = bytes(data);
            size_t n = data.size();
            while (n) {
                size_t k = std::min(n, Piece);
                _format.update(p, k);
                _deflater->write(p, k, _pending);
                p += k;
                n -= k;
                if (auto e = co_await _async_drain()) {
                    co_return io::detail::fail(*e);
                }
                if (n) {
                    co_await async::yield();
                }
            }
            co_return data.size();
        }

        SGCL_INLINE_HOT expected<void, io::error> flush() {
            if (auto e = _check("flush")) {
                return io::detail::fail(*e);
            }
            _deflater->flush(_pending);
            if (auto e = _drain()) {
                return io::detail::fail(*e);
            }
            return {};
        }

        async::task<expected<void, io::error>> async_flush() noexcept {
            if (auto e = _check("flush")) {
                co_return io::detail::fail(*e);
            }
            _deflater->flush(_pending);
            if (auto e = co_await _async_drain()) {
                co_return io::detail::fail(*e);
            }
            co_return expected<void, io::error>();
        }

        // The last block and the trailer; out stays open. A second close does nothing.
        expected<void, io::error> close() {
            if (_closed && !_error) {
                return {};
            }
            if (auto e = _check("close")) {
                return io::detail::fail(*e);
            }
            _closed = true;
            _deflater->finish(_pending);
            _format.finish(_pending);
            if (auto e = _drain()) {
                return io::detail::fail(*e);
            }
            return {};
        }

        async::task<expected<void, io::error>> async_close() noexcept {
            if (_closed && !_error) {
                co_return expected<void, io::error>();
            }
            if (auto e = _check("close")) {
                co_return io::detail::fail(*e);
            }
            _closed = true;
            _deflater->finish(_pending);
            _format.finish(_pending);
            if (auto e = co_await _async_drain()) {
                co_return io::detail::fail(*e);
            }
            co_return expected<void, io::error>();
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _closed;
        }

        // The first error given (a failure of out, a header the format
        // cannot write, a write or a flush after close), kept: every write,
        // flush and close after it gives it at once and writes nothing, so
        // a stream may be written freely and checked once, at the close
        // (as buffered_writer, tar::writer and zip::writer)
        SGCL_INLINE_HOT const optional<io::error>& last_error() const noexcept {
            return _error;
        }

        // A new stream into out with the same settings: the encoder's
        // memory kept, nothing of the old stream carried over
        void reset(const io::writer& out) noexcept {
            _out = out;
            if (_deflater) {
                _deflater->reset(_dictionary.data(), _dictionary.size());
            } else {
                _deflater = std::make_unique<Deflater>(_level, _dictionary.data(), _dictionary.size());   // moved from: its encoder went with the move
            }
            _format.reset();
            _pending.clear();
            _started = false;
            _closed = false;
            _error = nullopt;
        }

    protected:
        SGCL_INLINE_HOT Format& _format_ref() noexcept {
            return _format;
        }

    private:
        // the header before the first bytes, and the errors that stop everything
        optional<io::error> _check(const char* op) noexcept {
            if (_error) {
                return _error;
            }
            if (_closed) {
                _error = io::error(io::errc::closed, op, Format::name);   // kept as every error
                return _error;
            }
            if (!_started) {
                _started = true;
                if (auto wrong = _format.start(_pending, _level)) {
                    // what is wrong with the header in the place of the format's name:
                    // "write gzip: a name with a NUL: invalid argument"
                    _error = io::error(make_error_code(errc::invalid_argument), op, string(wrong));
                    return _error;
                }
            }
            return nullopt;
        }

        optional<io::error> _drain() {
            if (_pending.empty()) {
                return nullopt;
            }
            auto w = _out.write(_pending.bytes());
            _pending.clear();
            if (!w) {
                _error = w.error();
                return _error;
            }
            return nullopt;
        }

        async::task<optional<io::error>> _async_drain() noexcept {
            if (_pending.empty()) {
                co_return nullopt;
            }
            auto w = co_await _out.async_write(_pending.bytes());   // a task's write may run on the pool: the bytes it is given are managed, the block's own
            _pending.clear();
            if (!w) {
                _error = w.error();
                co_return _error;
            }
            co_return nullopt;
        }

        io::writer _out;
        Format _format;
        int _level;
        std::vector<uint8_t> _dictionary;
        std::unique_ptr<Deflater> _deflater;
        ManagedOutput _pending;   // what the encoder made, in managed memory: given to the writes as it is (block.h)
        optional<io::error> _error;
        bool _started = false;
        bool _closed = false;
    };

    // The decompressing half: a reader of what the data read from in
    // decompresses to. The data is read into an input buffer and decoded
    // into a window of 64 KB whose first half is the history the back
    // references reach; the decoded bytes go from the window to the
    // caller. The checksum is checked at the trailer, and a gzip stream
    // goes on into the members that follow unless it is told to stop at
    // the first. A failure is the error of the read that reaches it and
    // of every read after, last_error() the whole of it.
    template<class Format>
    class InflateReader
    : public io::mixin::reader<InflateReader<Format>> {
    public:
    private:
        static constexpr size_t InputBytes = size_t(16) << 10;

    public:

        SGCL_INLINE_HOT InflateReader(const io::reader& in, const slice<const byte>& dictionary, bool single_member = false) noexcept
        : _in(in)
        , _dictionary(bytes(dictionary), bytes(dictionary) + dictionary.size())
        , _state(std::make_unique<InflateState>())
        , _window(2 * WindowSize + MaxMatch + 8)
        , _input(InputBytes)
        , _single(single_member) {
            _state->reset();
        }

        InflateReader(const InflateReader&) = delete;
        InflateReader& operator=(const InflateReader&) = delete;

        // The other left without a stream, its decoder and its buffers gone
        // with the move (its dictionary kept): its reads give
        // io::errc::closed, its close closes nothing, and a reset gives it a
        // new stream. The derived class assigns through move_into.
        InflateReader(InflateReader&& o) noexcept
        : _in(std::move(o._in))
        , _format(std::move(o._format))
        , _dictionary(o._dictionary)
        , _state(std::move(o._state))
        , _window(std::move(o._window))
        , _history(o._history)
        , _wide(o._wide)
        , _input(std::move(o._input))
        , _pos(o._pos)
        , _from(o._from)
        , _in_begin(o._in_begin)
        , _in_end(o._in_end)
        , _consumed(o._consumed)
        , _single(o._single)
        , _source_ended(o._source_ended)
        , _ended(o._ended)
        , _phase(o._phase)
        , _error(std::move(o._error))
        , _since_yield(o._since_yield) {
            o._in = io::reader();
            o._input = InputBuffer<InputBytes>(InputBytes);
            o._window = std::vector<uint8_t>();
            o._pos = o._from = o._in_begin = o._in_end = 0;
            o._ended = true;
            o._error = moved_from_error(Format::name);
        }

        InflateReader& operator=(InflateReader&&) = delete;

        expected<size_t, io::error> read(const slice<byte>& out) {
            for (;;) {
                if (size_t n = _hand_out(out)) {
                    return n;
                }
                if (_error) {
                    return io::detail::fail(to_io_error(*_error, Format::name));
                }
                if (_ended || out.empty()) {
                    return 0;
                }
                if (_advance()) {
                    if (auto e = _fill()) {
                        return io::detail::fail(*e);
                    }
                }
            }
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
            for (;;) {
                if (size_t n = _hand_out(out)) {
                    // decoding is work, not a wait: every 64 KB handed out
                    // the task lets the worker go, so that a read of it all
                    // does not starve the other tasks
                    _since_yield += n;
                    if (_since_yield >= YieldEvery) {
                        _since_yield = 0;
                        co_await async::yield();
                    }
                    co_return n;
                }
                if (_error) {
                    co_return io::detail::fail(to_io_error(*_error, Format::name));
                }
                if (_ended || out.empty()) {
                    co_return 0;
                }
                if (_advance()) {
                    if (auto e = co_await _async_fill()) {
                        co_return io::detail::fail(*e);
                    }
                }
            }
        }

        // Closes in, as buffered_reader's close does
        SGCL_INLINE_HOT expected<void, io::error> close() {
            return _in.close();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_close() noexcept {
            return _in.async_close();
        }

        SGCL_INLINE_HOT const optional<error>& last_error() const noexcept {
            return _error;
        }

        // A new stream from in: the decoder's memory kept
        void reset(const io::reader& in) noexcept {
            _in = in;
            if (!_state) {
                // moved from: the decoder and the window went with the move
                _state = std::make_unique<InflateState>();
                _window.assign(2 * _history + MaxMatch + 8, 0);
            }
            _state->reset();
            _format.reset();
            _pos = _from = 0;
            _in_begin = _in_end = 0;
            _consumed = 0;
            _source_ended = false;
            _ended = false;
            _phase = Phase::header;
            _error = nullopt;
        }

    protected:
        // The header of the current member, reading as far as it takes
        expected<Format*, error> _header_now() {
            while (_phase == Phase::header && !_error) {
                if (_advance()) {
                    if (auto e = _fill()) {
                        return unexpected<error>(_error ? *_error : error(*e, _offset()));
                    }
                }
            }
            if (_error) {
                return unexpected<error>(*_error);
            }
            return &_format;
        }

        async::task<expected<Format*, error>> _async_header_now() noexcept {
            while (_phase == Phase::header && !_error) {
                if (_advance()) {
                    if (auto e = co_await _async_fill()) {
                        co_return unexpected<error>(_error ? *_error : error(*e, _offset()));
                    }
                }
            }
            if (_error) {
                co_return unexpected<error>(*_error);
            }
            co_return &_format;
        }

        SGCL_INLINE_HOT Format& _format_ref() noexcept {
            return _format;
        }

        SGCL_INLINE_HOT const Format& _format_ref() const noexcept {
            return _format;
        }

    private:
        enum class Phase : uint8_t {
            header,
            data,
            trailer,
            between   // after a member's trailer: another, or the end
        };

        SGCL_INLINE_HOT uint64_t _offset() const noexcept {
            return _consumed + _in_begin;
        }

        void _fail(errc code, const char* text) noexcept {
            _error = error(code, _offset(), text ? string(text) : string());
        }

        // The decoded bytes not yet handed out, into out
        SGCL_INLINE_HOT size_t _hand_out(const slice<byte>& out) noexcept {
            size_t n = std::min(out.size(), _pos - _from);
            if (n) {
                copy_out(out.data(), _window.data() + _from, n);
                _from += n;
            }
            return n;
        }

        // Works on the input held; true when it needs more of it
        bool _advance() noexcept {
            const uint8_t* base = _input.data();
            for (;;) {
                switch (_phase) {
                    case Phase::header: {
                        auto r = _format.header(base + _in_begin, _in_end - _in_begin);
                        if (r.status == Parsed::failed) {
                            _fail(r.code, r.text);
                            return false;
                        }
                        if (r.status == Parsed::more) {
                            if (_source_ended) {
                                _fail(errc::unexpected_end, "unexpected end in the header");
                                return false;
                            }
                            return true;
                        }
                        _in_begin += r.size;
                        if (!_start_data()) {
                            return false;
                        }
                        _phase = Phase::data;
                        break;
                    }
                    case Phase::data: {
                        if (_pos != _from) {
                            return false;   // hand out first
                        }
                        const size_t window_bytes = 2 * _history;
                        if (_pos + MaxMatch + 8 > window_bytes) {
                            // everything handed out: the last 32 KB (Deflate64's 64 KB) stay as the history
                            size_t keep = std::min<size_t>(_pos, _history);
                            sgcl::detail::move_bytes(_window.data(), _window.data() + _pos - keep, keep);
                            _pos = _from = keep;
                        }
                        const uint8_t* in = base + _in_begin;
                        size_t before = _pos;
                        auto st = _wide ? inflate64(*_state, in, base + _in_end, _window.data(), _pos, window_bytes)
                                        : inflate(*_state, in, base + _in_end, _window.data(), _pos, window_bytes);
                        _in_begin = size_t(in - base);
                        _format.update(_window.data() + before, _pos - before);
                        if (st == InflateStatus::failed) {
                            _fail(_state->error, _state->error_text);
                            return false;
                        }
                        if (st == InflateStatus::done) {
                            _phase = Phase::trailer;
                            break;
                        }
                        if (st == InflateStatus::need_input && _pos == before) {
                            if (_source_ended) {
                                _fail(errc::unexpected_end, "unexpected end of the compressed data");
                                return false;
                            }
                            return true;
                        }
                        break;
                    }
                    case Phase::trailer: {
                        auto r = _format.trailer(base + _in_begin, _in_end - _in_begin);
                        if (r.status == Parsed::failed) {
                            _fail(r.code, r.text);
                            return false;
                        }
                        if (r.status == Parsed::more) {
                            if (_source_ended) {
                                _fail(errc::unexpected_end, "unexpected end in the trailer");
                                return false;
                            }
                            return true;
                        }
                        _in_begin += r.size;
                        _phase = Phase::between;
                        break;
                    }
                    case Phase::between: {
                        if (!Format::members || _single) {
                            _ended = true;
                            return false;
                        }
                        if (_in_begin == _in_end) {
                            if (_source_ended) {
                                _ended = true;
                                return false;
                            }
                            return true;
                        }
                        // another member: the last one's bytes handed out
                        // first (its window starts over), then its decoder
                        // afresh
                        if (_pos != _from) {
                            return false;
                        }
                        _state->reset();
                        _format.reset();
                        _phase = Phase::header;
                        break;
                    }
                }
            }
        }

        // The data of a member starts: a zlib stream that asks for a
        // dictionary gets the one given, when it is that one
        bool _start_data() noexcept {
            // a member's history is its own: none carried from the one before
            // (whose bytes were all handed out before its trailer led here)
            _pos = _from = 0;
            if constexpr (requires { _format.dictionary_id; }) {
                if (_format.dictionary_id) {
                    if (_dictionary.empty()) {
                        _fail(errc::dictionary_required, "zlib: the stream needs a preset dictionary");
                        return false;
                    }
                    hash::adler32 a;
                    a.update(slice<const byte>(reinterpret_cast<const byte*>(_dictionary.data()), _dictionary.size()));
                    if (a.value() != *_format.dictionary_id) {
                        _fail(errc::dictionary_required, "zlib: the dictionary given is not the one the stream needs");
                        return false;
                    }
                    _preload();
                }
            } else {
                if (!_dictionary.empty()) {
                    _preload();
                }
            }
            return true;
        }

        SGCL_INLINE_HOT void _preload() noexcept {
            size_t n = std::min<size_t>(_dictionary.size(), WindowSize);
            sgcl::detail::copy_bytes(_window.data(), _dictionary.data() + _dictionary.size() - n, n);
            _pos = _from = n;
        }

        // room for more input: what is left moved to the front, the buffer
        // grown when a header needs more than it holds
        size_t _make_room() noexcept {
            if (_in_begin) {
                sgcl::detail::move_bytes(_input.data(), _input.data() + _in_begin, _in_end - _in_begin);
                _consumed += _in_begin;
                _in_end -= _in_begin;
                _in_begin = 0;
            }
            if (_in_end == _input.size()) {
                _input.resize(_input.size() * 2, _in_end);
            }
            return _input.size() - _in_end;
        }

        SGCL_INLINE_HOT optional<io::error> _fill() {
            size_t room = _make_room();
            auto r = _in.read(_input.room(_in_end, room));
            return _took(r);
        }

        async::task<optional<io::error>> _async_fill() noexcept {
            _input.to_managed(_in_end);   // the read may run on the pool: into managed memory, which the slice holds
            size_t room = _make_room();
            auto r = co_await _in.async_read(_input.room(_in_end, room));
            co_return _took(r);
        }

        SGCL_INLINE_HOT optional<io::error> _took(const expected<size_t, io::error>& r) noexcept {
            if (!r) {
                _error = error(r.error(), _offset());
                return r.error();
            }
            if (*r == 0) {
                _source_ended = true;
            }
            _in_end += *r;
            return nullopt;
        }

        io::reader _in;
        Format _format;
        std::vector<uint8_t> _dictionary;
        std::unique_ptr<InflateState> _state;
        std::vector<uint8_t> _window;
        size_t _history = WindowSize;   // how far back a match reaches
        bool _wide = false;             // Deflate64 (zip method 9)
        InputBuffer<InputBytes> _input;   // plain until a task's first read, managed from then on (block.h)
        size_t _pos = 0;          // the end of the decoded bytes in the window
        size_t _from = 0;         // the first not yet handed out
        size_t _in_begin = 0;     // the input not yet decoded: [_in_begin, _in_end)
        size_t _in_end = 0;
        uint64_t _consumed = 0;   // the input bytes before the buffer's start
        bool _single = false;
        bool _source_ended = false;
        bool _ended = false;
        Phase _phase = Phase::header;
        optional<error> _error;
        static constexpr size_t YieldEvery = size_t(64) << 10;
        size_t _since_yield = 0;   // bytes handed out by the task's reads since it last let the worker go

        template<class F>
        friend void size_input(InflateReader<F>& r, size_t n) noexcept;
        template<class F>
        friend void use_deflate64(InflateReader<F>& r) noexcept;
    };

    // Before the first read: the data is Deflate64 (a zip entry of method
    // 9), its window twice 64 KB
    template<class F>
    SGCL_INLINE_HOT void use_deflate64(InflateReader<F>& r) noexcept {
        r._wide = true;
        r._history = Window64Size;
        r._window.assign(2 * Window64Size + MaxMatch + 8, 0);
    }

    // Before the first read: the reader's input n bytes (a zip entry's
    // compressed size in the block that holds it, when less than 16 KB), so
    // that a small entry's reader makes no more (block.h: managed_bytes)
    template<class F>
    SGCL_INLINE_HOT void size_input(InflateReader<F>& r, size_t n) noexcept {
        r._input.resize(n, 0);
    }

    // The whole of data compressed, in memory; only a gzip header can be
    // refused (std::invalid_argument)
    template<class Format>
    vector<byte> compress_all(const uint8_t* p, size_t n, int level, Format format, const slice<const byte>& dictionary) noexcept(!Format::refuses) {
        LentOutput lent;   // the thread's room, kept from call to call
        std::vector<uint8_t>& out = lent.out();
        out.reserve(n / 2 + 64);
        // a header the format cannot write (a NUL in a gzip name)
        // is the program's mistake here: compress returns no error. Only
        // gzip's can be refused; flate's and zlib's start never fails
        auto wrong = format.start(out, level);
        if constexpr (Format::refuses) {
            if (wrong) {
                throw std::invalid_argument(std::string("compress::") + wrong);
            }
        }
        format.update(p, n);
        LentDeflater d(level, bytes(dictionary), dictionary.size(), n);
        d->write(p, n, out);
        d->finish(out);
        format.finish(out);
        return to_vector(out.data(), out.size());
    }

    // The whole of data decompressed, in memory, every member of a gzip
    // stream one after another, straight into the vector returned; the
    // limit checked as the output grows
    template<class Format>
    expected<vector<byte>, error> decompress_all(const uint8_t* p, size_t n, const limits& l, Format format, const slice<const byte>& dictionary, size_t size_hint = 0) noexcept(noexcept(format.header(p, n))) {
        // a dictionary lies in front of the output as its history: room for
        // it past what the limit counts
        const size_t history_room = std::min<size_t>(dictionary.size(), WindowSize);
        const size_t slack = MaxMatch + 16 + history_room;
        // the room the limit allows past its size, saturated: limits{UINT64_MAX} is no limit
        const uint64_t ceiling = l.max_size > UINT64_MAX - slack ? UINT64_MAX : l.max_size + slack;
        // a hint from the data (gzip's length) is believed only as far as
        // DEFLATE can expand the input (1032 to 1); doubling does the rest
        uint64_t guess = size_hint ? std::min<uint64_t>(size_hint, uint64_t(n) * 1032 + 64) : uint64_t(n) * 4;
        guess = std::min<uint64_t>(guess, uint64_t(64) << 20);
        size_t capacity = size_t(std::min<uint64_t>(std::max<uint64_t>(guess, 1024) + slack, ceiling));
        vector<byte> result;
        result.resize(capacity);
        auto out = [&]() noexcept {
            return reinterpret_cast<uint8_t*>(result.data());
        };
        size_t total = 0;
        size_t at = 0;
        auto state = std::make_unique<InflateState>();
        bool first = true;
        for (;;) {
            if (!first && (!Format::members || at == n)) {
                break;
            }
            first = false;
            auto h = format.header(p + at, n - at);
            if (h.status != Parsed::ok) {
                return unexpected<error>(error(h.status == Parsed::more ? errc::unexpected_end : h.code, at, h.text ? string(h.text) : string()));
            }
            at += h.size;
            size_t member_start = total;
            size_t history = 0;
            if constexpr (requires { format.dictionary_id; }) {
                if (format.dictionary_id) {
                    hash::adler32 a;
                    a.update(dictionary);
                    if (dictionary.empty() || a.value() != *format.dictionary_id) {
                        return unexpected<error>(error(errc::dictionary_required, at, "zlib: the stream needs a preset dictionary"));
                    }
                }
            }
            // a dictionary is history in front of the output: laid there,
            // and taken off at the end (zlib's only when the stream asks)
            bool preset = !dictionary.empty();
            if constexpr (requires { format.dictionary_id; }) {
                preset = bool(format.dictionary_id);
            }
            if (preset) {
                history = history_room;
                sgcl::detail::copy_bytes(out() + total, bytes(dictionary) + dictionary.size() - history, history);
                total += history;
            }
            state->reset();
            const uint8_t* in = p + at;
            for (;;) {
                size_t before = total;
                auto st = inflate(*state, in, p + n, out(), total, capacity);
                format.update(out() + before, total - before);
                if (st == InflateStatus::done) {
                    break;
                }
                if (st == InflateStatus::failed) {
                    return unexpected<error>(error(state->error, uint64_t(in - p), state->error_text ? string(state->error_text) : string()));
                }
                if (st == InflateStatus::need_input) {
                    return unexpected<error>(error(errc::unexpected_end, n, "unexpected end of the compressed data"));
                }
                // need_room: grown, up to the limit
                if (capacity >= ceiling || capacity - slack >= l.max_size) {
                    return unexpected<error>(error(errc::too_large, uint64_t(in - p), "decompressed data past the limit"));
                }
                capacity = size_t(std::min<uint64_t>({uint64_t(capacity) * 2, ceiling, uint64_t(SIZE_MAX)}));
                result.resize(capacity);
            }
            if (history) {
                sgcl::detail::move_bytes(out() + member_start, out() + member_start + history, total - member_start - history);
                total -= history;
            }
            if (total > l.max_size) {
                return unexpected<error>(error(errc::too_large, uint64_t(in - p), "decompressed data past the limit"));
            }
            at = size_t(in - p);
            auto t = format.trailer(p + at, n - at);
            if (t.status != Parsed::ok) {
                return unexpected<error>(error(t.status == Parsed::more ? errc::unexpected_end : t.code, at, t.text ? string(t.text) : string()));
            }
            at += t.size;
            format.reset();
        }
        result.resize(total);
        return result;
    }
}
