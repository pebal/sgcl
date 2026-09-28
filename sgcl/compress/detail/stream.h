//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "block.h"
#include "deflate.h"
#include "inflate.h"
#include "../error.h"
#include "../level.h"
#include "../../async/scheduler.h"
#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../hash/adler32.h"
#include "../../hash/crc32.h"
#include "../../io/stream.h"
#include "../../time/datetime.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace sgcl::compress {
    // A limit on what a decompression of data in memory makes: data from
    // outside may decompress a thousand times over (a "bomb"), so the
    // functions of the module stop at max_size bytes with errc::too_large.
    // limits{UINT64_MAX} lifts it. A stream has none: its reader decides
    // how much it reads (io::limit_reader over it). max_memory bounds the
    // memory a decoder takes because the data asks for it (LZMA's
    // dictionary, whose size the header gives), in memory and in a
    // stream alike: more is errc::too_large before anything is allocated.
    // max_entries bounds the entries an archive's header may list (7z):
    // its table is made before any entry is read.
    struct limits {
        uint64_t max_size = uint64_t(1) << 30;
        uint64_t max_memory = uint64_t(1) << 30;
        uint64_t max_entries = 1000000;
    };

    // gzip's header (RFC 1952): every field optional. The name and the
    // comment are ISO 8859-1 in the format; a program's are UTF-8 and are
    // converted both ways, a character past U+00FF being invalid_argument
    // when written.
    struct gzip_header {
        string name;
        string comment;
        optional<time::datetime> modified;
        vector<byte> extra;
        uint8_t os = 255;   // 255: unknown, as Go writes
    };
}

namespace sgcl::compress::detail {
    using namespace sgcl::detail;

    inline const uint8_t* bytes(const slice<const byte>& s) noexcept {
        return reinterpret_cast<const uint8_t*>(s.data());
    }

    inline slice<const byte> view(const std::vector<uint8_t>& v) noexcept {
        return slice<const byte>(reinterpret_cast<const byte*>(v.data()), v.size());
    }

    inline vector<byte> to_vector(const uint8_t* p, size_t n) {
        auto b = reinterpret_cast<const byte*>(p);
        return vector<byte>(b, b + n);
    }

    // ISO 8859-1 to UTF-8, and back (nullopt for a character past U+00FF
    // or text that is not UTF-8)
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

    inline optional<std::string> utf8_to_latin1(const string& text) {
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

    inline void put_le32(std::vector<uint8_t>& out, uint32_t v) {
        uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        out.insert(out.end(), b, b + 4);
    }

    inline void put_be32(std::vector<uint8_t>& out, uint32_t v) {
        uint8_t b[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
        out.insert(out.end(), b, b + 4);
    }

    inline uint32_t le32(const uint8_t* p) noexcept {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }

    inline uint32_t be32(const uint8_t* p) noexcept {
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | uint32_t(p[3]);
    }

    // What a header or a trailer parse says about the bytes it was given
    struct Parsed {
        enum : uint8_t { more, ok, failed } status = more;
        size_t size = 0;             // the bytes of the header or trailer when ok
        errc code = errc::invalid_header;
        const char* text = nullptr;

        static Parsed need() noexcept {
            return {};
        }

        static Parsed done(size_t n) noexcept {
            return {ok, n, errc::invalid_header, nullptr};
        }

        static Parsed fail(errc c, const char* t) noexcept {
            return {failed, 0, c, t};
        }
    };

    // The three wrappings of DEFLATE: what goes before the data and after
    // it, and the checksum between. A format is a value the stream keeps:
    //   name                       "gzip", for the messages
    //   start(out) / finish(out)   the writer's header and trailer
    //   header(p, n) / trailer(p, n)   the reader's, parsed from the bytes there are
    //   update(p, n)               the checksum over the data
    //   members                    whether another member may follow the trailer
    struct FlateFormat {
        static constexpr const char* name = "flate";
        static constexpr bool members = false;

        optional<error> start(std::vector<uint8_t>&, int) {
            return nullopt;
        }

        void update(const uint8_t*, size_t) noexcept {
        }

        void finish(std::vector<uint8_t>&) {
        }

        Parsed header(const uint8_t*, size_t) noexcept {
            return Parsed::done(0);
        }

        Parsed trailer(const uint8_t*, size_t) noexcept {
            return Parsed::done(0);
        }

        void reset() noexcept {
        }
    };

    struct ZlibFormat {
        static constexpr const char* name = "zlib";
        static constexpr bool members = false;
        optional<uint32_t> dictionary_id;   // of the dictionary given (the writer's), or the one the stream asks for (the reader's)
        hash::adler32 sum;

        optional<error> start(std::vector<uint8_t>& out, int lvl) {
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
            return nullopt;
        }

        void update(const uint8_t* p, size_t n) noexcept {
            sum.update(slice<const byte>(reinterpret_cast<const byte*>(p), n));
        }

        void finish(std::vector<uint8_t>& out) {
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

        Parsed trailer(const uint8_t* p, size_t n) noexcept {
            if (n < 4) {
                return Parsed::need();
            }
            if (be32(p) != sum.value()) {
                return Parsed::fail(errc::checksum, "zlib: Adler-32 mismatch");
            }
            return Parsed::done(4);
        }

        void reset() noexcept {
            sum = hash::adler32();
        }
    };

    struct GzipFormat {
        static constexpr const char* name = "gzip";
        static constexpr bool members = true;
        static constexpr size_t MaxHeader = size_t(1) << 20;
        gzip_header head;
        hash::crc32 sum;
        uint32_t size = 0;

        optional<error> start(std::vector<uint8_t>& out, int lvl) {
            auto name = utf8_to_latin1(head.name);
            auto comment = utf8_to_latin1(head.comment);
            if (!name || !comment) {
                return error(errc::invalid_argument, 0, "gzip: a header string with a character past U+00FF, which ISO 8859-1 cannot write");
            }
            if (name->find('\0') != std::string::npos || comment->find('\0') != std::string::npos) {
                return error(errc::invalid_argument, 0, "gzip: a header string with a NUL");
            }
            if (head.extra.size() > 65535) {
                return error(errc::invalid_argument, 0, "gzip: extra field longer than 65535 bytes");
            }
            uint8_t flags = 0;
            if (!head.extra.empty()) flags |= 4;
            if (!name->empty()) flags |= 8;
            if (!comment->empty()) flags |= 16;
            out.push_back(0x1F);
            out.push_back(0x8B);
            out.push_back(8);
            out.push_back(flags);
            int64_t t = head.modified ? head.modified->unix() : 0;
            put_le32(out, t > 0 && t <= int64_t(UINT32_MAX) ? uint32_t(t) : 0);
            out.push_back(lvl == level::smallest ? 2 : lvl == level::fastest ? 4 : 0);
            out.push_back(head.os);
            if (flags & 4) {
                out.push_back(uint8_t(head.extra.size()));
                out.push_back(uint8_t(head.extra.size() >> 8));
                auto e = reinterpret_cast<const uint8_t*>(head.extra.data());
                out.insert(out.end(), e, e + head.extra.size());
            }
            if (flags & 8) {
                out.insert(out.end(), name->begin(), name->end());
                out.push_back(0);
            }
            if (flags & 16) {
                out.insert(out.end(), comment->begin(), comment->end());
                out.push_back(0);
            }
            return nullopt;
        }

        void update(const uint8_t* p, size_t n) noexcept {
            sum.update(slice<const byte>(reinterpret_cast<const byte*>(p), n));
            size += uint32_t(n);
        }

        void finish(std::vector<uint8_t>& out) {
            put_le32(out, sum.value());
            put_le32(out, size);
        }

        Parsed header(const uint8_t* p, size_t n) {
            if (n < 10) {
                return Parsed::need();
            }
            if (p[0] != 0x1F || p[1] != 0x8B) {
                return Parsed::fail(errc::invalid_header, "gzip: not a gzip stream");
            }
            if (p[2] != 8) {
                return Parsed::fail(errc::unsupported, "gzip: compression method other than deflate");
            }
            uint8_t flags = p[3];
            if (flags & 0xE0) {
                return Parsed::fail(errc::invalid_header, "gzip: reserved header flags set");
            }
            size_t at = 10;
            gzip_header h;
            uint32_t mtime = le32(p + 4);
            if (mtime) {
                h.modified = time::datetime::from_unix(int64_t(mtime), time::zone::utc());
            }
            h.os = p[9];
            if (flags & 4) {
                if (n < at + 2) {
                    return n >= MaxHeader ? Parsed::fail(errc::too_large, "gzip: header longer than 1 MiB") : Parsed::need();
                }
                size_t xlen = size_t(p[at]) | size_t(p[at + 1]) << 8;
                at += 2;
                if (n < at + xlen) {
                    return Parsed::need();
                }
                h.extra = to_vector(p + at, xlen);
                at += xlen;
            }
            for (int field = 0; field < 2; ++field) {
                if (!(flags & (field == 0 ? 8 : 16))) {
                    continue;
                }
                auto end = static_cast<const uint8_t*>(std::memchr(p + at, 0, n - at));
                if (!end) {
                    return n >= MaxHeader ? Parsed::fail(errc::too_large, "gzip: header longer than 1 MiB") : Parsed::need();
                }
                auto text = latin1_to_utf8(p + at, size_t(end - (p + at)));
                (field == 0 ? h.name : h.comment) = text;
                at = size_t(end - p) + 1;
            }
            if (flags & 2) {
                if (n < at + 2) {
                    return Parsed::need();
                }
                // the header's CRC-16: the low half of the CRC-32 of the header before it
                uint16_t want = uint16_t(p[at] | p[at + 1] << 8);
                if (uint16_t(hash::crc32::of(slice<const byte>(reinterpret_cast<const byte*>(p), at))) != want) {
                    return Parsed::fail(errc::checksum, "gzip: header CRC-16 mismatch");
                }
                at += 2;
            }
            head = h;
            sum = hash::crc32();
            size = 0;
            return Parsed::done(at);
        }

        Parsed trailer(const uint8_t* p, size_t n) noexcept {
            if (n < 8) {
                return Parsed::need();
            }
            if (le32(p) != sum.value()) {
                return Parsed::fail(errc::checksum, "gzip: CRC-32 mismatch");
            }
            if (le32(p + 4) != size) {
                return Parsed::fail(errc::corrupt, "gzip: length in the trailer does not match the data");
            }
            return Parsed::done(8);
        }

        void reset() noexcept {
            sum = hash::crc32();
            size = 0;
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

        DeflateWriter(const io::writer& out, int level, const Format& format, const slice<const byte>& dictionary)
        : _out(out)
        , _format(format)
        , _level(level)
        , _dictionary(bytes(dictionary), bytes(dictionary) + dictionary.size())
        , _deflater(std::make_unique<Deflater>(level, bytes(dictionary), dictionary.size())) {
        }

        DeflateWriter(const DeflateWriter&) = delete;
        DeflateWriter& operator=(const DeflateWriter&) = delete;
        DeflateWriter(DeflateWriter&&) noexcept = default;
        DeflateWriter& operator=(DeflateWriter&&) noexcept = default;

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

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) {
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

        expected<void, io::error> flush() {
            if (auto e = _check("flush")) {
                return io::detail::fail(*e);
            }
            _deflater->flush(_pending);
            if (auto e = _drain()) {
                return io::detail::fail(*e);
            }
            return {};
        }

        async::task<expected<void, io::error>> async_flush() {
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

        async::task<expected<void, io::error>> async_close() {
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

        bool is_closed() const noexcept {
            return _closed;
        }

        // The first error given (a failure of out, a header the format
        // cannot write, a write or a flush after close), kept: every write,
        // flush and close after it gives it at once and writes nothing, so
        // a stream may be written freely and checked once, at the close
        // (as buffered_writer, tar::writer and zip::writer)
        const optional<io::error>& last_error() const noexcept {
            return _error;
        }

        // A new stream into out with the same settings: the encoder's
        // memory kept, nothing of the old stream carried over
        void reset(const io::writer& out) {
            _out = out;
            _deflater->reset(_dictionary.data(), _dictionary.size());
            _format.reset();
            _pending.clear();
            _started = false;
            _closed = false;
            _error = nullopt;
        }

    protected:
        Format& _format_ref() noexcept {
            return _format;
        }

    private:
        // the header before the first bytes, and the errors that stop everything
        optional<io::error> _check(const char* op) {
            if (_error) {
                return _error;
            }
            if (_closed) {
                _error = io::error(io::errc::closed, op, Format::name);   // kept as every error
                return _error;
            }
            if (!_started) {
                _started = true;
                if (auto e = _format.start(_pending, _level)) {
                    _error = io::error(make_error_code(e->code()), op, Format::name);
                    return _error;
                }
            }
            return nullopt;
        }

        optional<io::error> _drain() {
            if (_pending.empty()) {
                return nullopt;
            }
            auto w = _out.write(view(_pending));
            _pending.clear();
            if (!w) {
                _error = w.error();
                return _error;
            }
            return nullopt;
        }

        async::task<optional<io::error>> _async_drain() {
            if (_pending.empty()) {
                co_return nullopt;
            }
            auto w = co_await _out.async_write(_stage.stage(_pending));   // a task's write may run on the pool: the bytes it is given are managed
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
        std::vector<uint8_t> _pending;
        OutputStage _stage;   // the pending bytes for a task's write (block.h)
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

        InflateReader(const io::reader& in, const slice<const byte>& dictionary, bool single_member = false)
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
        InflateReader(InflateReader&&) noexcept = default;
        InflateReader& operator=(InflateReader&&) noexcept = default;

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

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
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
        expected<void, io::error> close() {
            return _in.close();
        }

        async::task<expected<void, io::error>> async_close() {
            return _in.async_close();
        }

        const optional<error>& last_error() const noexcept {
            return _error;
        }

        // A new stream from in: the decoder's memory kept
        void reset(const io::reader& in) {
            _in = in;
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

        async::task<expected<Format*, error>> _async_header_now() {
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

        Format& _format_ref() noexcept {
            return _format;
        }

        const Format& _format_ref() const noexcept {
            return _format;
        }

    private:
        enum class Phase : uint8_t {
            header,
            data,
            trailer,
            between   // after a member's trailer: another, or the end
        };

        uint64_t _offset() const noexcept {
            return _consumed + _in_begin;
        }

        void _fail(errc code, const char* text) {
            _error = error(code, _offset(), text ? string(text) : string());
        }

        // The decoded bytes not yet handed out, into out
        size_t _hand_out(const slice<byte>& out) noexcept {
            size_t n = std::min(out.size(), _pos - _from);
            if (n) {
                std::memcpy(out.data(), _window.data() + _from, n);
                _from += n;
            }
            return n;
        }

        // Works on the input held; true when it needs more of it
        bool _advance() {
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
                            std::memmove(_window.data(), _window.data() + _pos - keep, keep);
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
        bool _start_data() {
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

        void _preload() noexcept {
            size_t n = std::min<size_t>(_dictionary.size(), WindowSize);
            std::memcpy(_window.data(), _dictionary.data() + _dictionary.size() - n, n);
            _pos = _from = n;
        }

        // room for more input: what is left moved to the front, the buffer
        // grown when a header needs more than it holds
        size_t _make_room() {
            if (_in_begin) {
                std::memmove(_input.data(), _input.data() + _in_begin, _in_end - _in_begin);
                _consumed += _in_begin;
                _in_end -= _in_begin;
                _in_begin = 0;
            }
            if (_in_end == _input.size()) {
                _input.resize(_input.size() * 2, _in_end);
            }
            return _input.size() - _in_end;
        }

        optional<io::error> _fill() {
            size_t room = _make_room();
            auto r = _in.read(_input.room(_in_end, room));
            return _took(r);
        }

        async::task<optional<io::error>> _async_fill() {
            _input.to_managed(_in_end);   // the read may run on the pool: into managed memory, which the slice holds
            size_t room = _make_room();
            auto r = co_await _in.async_read(_input.room(_in_end, room));
            co_return _took(r);
        }

        optional<io::error> _took(const expected<size_t, io::error>& r) {
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
        friend void size_input(InflateReader<F>& r, size_t n);
        template<class F>
        friend void use_deflate64(InflateReader<F>& r);
    };

    // Before the first read: the data is Deflate64 (a zip entry of method
    // 9), its window twice 64 KB
    template<class F>
    void use_deflate64(InflateReader<F>& r) {
        r._wide = true;
        r._history = Window64Size;
        r._window.assign(2 * Window64Size + MaxMatch + 8, 0);
    }

    // Before the first read: the reader's input n bytes (a zip entry's
    // compressed size in the block that holds it, when less than 16 KB), so
    // that a small entry's reader makes no more (block.h: managed_bytes)
    template<class F>
    void size_input(InflateReader<F>& r, size_t n) {
        r._input.resize(n, 0);
    }

    // The whole of data compressed, in memory
    template<class Format>
    vector<byte> compress_all(const uint8_t* p, size_t n, int level, Format format, const slice<const byte>& dictionary) {
        std::vector<uint8_t> out;
        out.reserve(n / 2 + 64);
        // a header the format cannot write (a gzip name past ISO 8859-1)
        // is the program's mistake here: compress returns no error
        if (auto e = format.start(out, level)) {
            throw std::invalid_argument(std::string("compress: ") + std::string(e->message().view()));
        }
        format.update(p, n);
        Deflater d(level, bytes(dictionary), dictionary.size());
        d.write(p, n, out);
        d.finish(out);
        format.finish(out);
        return to_vector(out.data(), out.size());
    }

    // The whole of data decompressed, in memory, every member of a gzip
    // stream one after another, straight into the vector returned; the
    // limit checked as the output grows
    template<class Format>
    expected<vector<byte>, error> decompress_all(const uint8_t* p, size_t n, const limits& l, Format format, const slice<const byte>& dictionary, size_t size_hint = 0) {
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
                std::memcpy(out() + total, bytes(dictionary) + dictionary.size() - history, history);
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
                std::memmove(out() + member_start, out() + member_start + history, total - member_start - history);
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
