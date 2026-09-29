//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "bzip2.h"
#include "gzip.h"
#include "xz.h"
#include "detail/block.h"
#include "detail/files.h"
#include "../io/detail/path.h"
#include "../async/scheduler.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/vector.h"
#include "../io/fs.h"
#include "../io/stream.h"
#include "../time/datetime.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sgcl::compress::tar {
    using error = compress::error;

    // What an entry of the archive is. A type flag the list has no name
    // for is a file, as POSIX says of ustar ('7', contiguous, among them);
    // GNU's 'D' (a directory with the listing of an incremental dump as
    // its data) is a directory, the listing stepped over.
    enum class kind : uint8_t {
        file,
        directory,
        symlink,
        hardlink,
        char_device,
        block_device,
        fifo
    };

    // One entry: what the headers before its data say, the ustar header,
    // the pax records (the archive's global ones and the entry's own) and
    // GNU's long names merged into one. The names are the archive's bytes
    // as they are: nothing is checked or cleaned when the archive is read,
    // and is_local() says whether a name may be joined to a directory.
    // size is the bytes of data the entry has in the archive, which the
    // reader gives and the writer takes: 0 for everything but a file (the
    // size a header of a directory or a link may carry is not data, where
    // Go gives it).
    struct entry {
        string name;
        string link_name;                   // the target of a symlink or a hard link
        tar::kind type = tar::kind::file;
        uint64_t size = 0;
        io::permissions mode = io::permissions::none;
        time::datetime modified;            // in UTC, to the nanosecond a pax record gives
        optional<time::datetime> accessed;  // when the archive has them: pax, GNU, star
        optional<time::datetime> changed;
        int64_t uid = 0;
        int64_t gid = 0;
        string user_name;
        string group_name;
        uint32_t dev_major = 0;             // of a char_device or a block_device
        uint32_t dev_minor = 0;
        vector<pair<string, string>> pax;   // the pax records none of the fields above holds, in order ("comment", "SCHILY.xattr.user.a")

        // Whether name, and link_name where there is one, stay inside the
        // directory the archive is unpacked to (Go's filepath.IsLocal): not
        // empty, not absolute, no ".." that climbs out of it, no backslash
        // (a separator on Windows). A symlink's target is taken from the
        // directory of the link, a hard link's from the archive's root.
        bool is_local() const;

        friend bool operator==(const entry& a, const entry& b) = default;
    };

    class reader;
    class writer;
}

namespace sgcl::compress::detail {
    using namespace sgcl::detail;

    inline constexpr size_t TarBlock = 512;
    inline constexpr size_t TarMaxSpecial = size_t(1) << 20;   // a pax header, a GNU long name: 1 MiB, as Go
    inline constexpr int64_t TarMaxOctal11 = (int64_t(1) << 33) - 1;   // eleven octal digits: the size and the time of ustar
    inline constexpr int64_t TarMaxOctal7 = (int64_t(1) << 21) - 1;    // seven: the ids, the mode, the devices

    inline uint64_t tar_padding(uint64_t n) noexcept {
        return (TarBlock - n % TarBlock) % TarBlock;
    }

    // A text field: to its first NUL, or the whole field
    inline std::string_view tar_string(const uint8_t* p, size_t n) noexcept {
        auto end = static_cast<const uint8_t*>(std::memchr(p, 0, n));
        return std::string_view(reinterpret_cast<const char*>(p), end ? size_t(end - p) : n);
    }

    // An octal field: spaces and NULs around the digits, as every tar
    // writes some; false for anything else. An empty field is 0.
    inline bool tar_octal(const uint8_t* p, size_t n, int64_t& out) noexcept {
        size_t b = 0, e = n;
        while (b < e && (p[b] == ' ' || p[b] == 0)) {
            ++b;
        }
        while (e > b && (p[e - 1] == ' ' || p[e - 1] == 0)) {
            --e;
        }
        uint64_t x = 0;
        for (size_t i = b; i < e; ++i) {
            if (p[i] == 0) {
                break;   // the digits end at a NUL inside, as a string field would
            }
            if (p[i] < '0' || p[i] > '7' || (x >> 61)) {
                return false;
            }
            x = x << 3 | uint64_t(p[i] - '0');
        }
        out = int64_t(x);
        return true;
    }

    // A numeric field: octal, or base-256 (GNU, star) when the first
    // byte's top bit is set, the next bit the sign of a two's complement
    // number; false for a number past 64 bits
    inline bool tar_numeric(const uint8_t* p, size_t n, int64_t& out) noexcept {
        if (n && (p[0] & 0x80)) {
            uint8_t inv = (p[0] & 0x40) ? 0xFF : 0;
            uint64_t x = 0;
            for (size_t i = 0; i < n; ++i) {
                uint8_t c = uint8_t(p[i] ^ inv);
                if (i == 0) {
                    c &= 0x7F;
                }
                if (x >> 56) {
                    return false;
                }
                x = x << 8 | c;
            }
            if (x >> 63) {
                return false;
            }
            out = inv ? ~int64_t(x) : int64_t(x);
            return true;
        }
        return tar_octal(p, n, out);
    }

    // A decimal of a pax record: an optional sign and digits, in 64 bits
    inline bool tar_decimal(std::string_view s, int64_t& out) noexcept {
        bool negative = false;
        if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
            negative = s[0] == '-';
            s.remove_prefix(1);
        }
        if (s.empty()) {
            return false;
        }
        uint64_t x = 0;
        for (char c : s) {
            if (c < '0' || c > '9') {
                return false;
            }
            if (x > (uint64_t(INT64_MAX) + 1 - uint64_t(c - '0')) / 10) {
                return false;
            }
            x = x * 10 + uint64_t(c - '0');
        }
        if (!negative && x > uint64_t(INT64_MAX)) {
            return false;
        }
        out = negative ? int64_t(0 - x) : int64_t(x);
        return true;
    }

    // Seconds and a part of one, saturated at the ends of a datetime
    inline time::datetime tar_time(int64_t seconds, int64_t nanos) noexcept {
        __int128 v = (__int128)seconds * 1000000000 + nanos;
        int64_t ns = v < INT64_MIN ? INT64_MIN : v > INT64_MAX ? INT64_MAX : int64_t(v);
        return time::datetime::from_unix_nano(ns, time::zone::utc());
    }

    // A pax time: seconds, and a fraction of any length after a dot (the
    // digits past the ninth dropped); the fraction of "-1.5" is taken
    // away, as the seconds are negative
    inline bool tar_pax_time(std::string_view s, time::datetime& out) noexcept {
        auto dot = s.find('.');
        std::string_view whole = s.substr(0, dot);
        int64_t seconds;
        if (!tar_decimal(whole, seconds)) {
            return false;
        }
        int64_t nanos = 0;
        if (dot != std::string_view::npos) {
            std::string_view frac = s.substr(dot + 1);
            int digits = 0;
            for (char c : frac) {
                if (c < '0' || c > '9') {
                    return false;
                }
                if (digits < 9) {
                    nanos = nanos * 10 + (c - '0');
                    ++digits;
                }
            }
            for (; digits < 9; ++digits) {
                nanos *= 10;
            }
            if (whole[0] == '-') {
                nanos = -nanos;
            }
        }
        out = tar_time(seconds, nanos);
        return true;
    }

    inline int64_t tar_floor_div(int64_t a, int64_t b) noexcept {
        int64_t q = a / b;
        return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
    }

    // A time as a pax record writes it: the seconds, and the fraction
    // without its trailing zeros; before 1970 the sign in front of both
    inline std::string tar_format_pax_time(int64_t ns) {
        int64_t seconds = tar_floor_div(ns, 1000000000);
        // the fraction as the remainder brought into [0, 1e9): no product
        // of the seconds that could overflow near the ends of the range
        int64_t frac = ((ns % 1000000000) + 1000000000) % 1000000000;
        if (frac == 0) {
            return std::to_string(seconds);
        }
        std::string s;
        if (seconds < 0) {
            s = "-";
            seconds = -(seconds + 1);
            frac = 1000000000 - frac;
        }
        s += std::to_string(seconds);
        std::string f = std::to_string(frac);
        s += '.';
        s.append(9 - f.size(), '0');
        s += f;
        while (s.back() == '0') {
            s.pop_back();
        }
        return s;
    }

    using TarRecords = std::vector<std::pair<std::string, std::string>>;

    inline bool tar_known_key(std::string_view k) noexcept {
        return k == "path" || k == "linkpath" || k == "size" || k == "uid" || k == "gid" || k == "uname" || k == "gname" || k == "mtime" || k == "atime" || k == "ctime";
    }

    // A record the format allows: a key with no '=' and no NUL, and no NUL
    // in the value of the four records that stand for ustar's strings
    // (other values may hold one: an extended attribute's does)
    inline bool tar_valid_record(std::string_view k, std::string_view v) noexcept {
        if (k.empty() || k.find('=') != std::string_view::npos || k.find('\0') != std::string_view::npos) {
            return false;
        }
        if (k == "path" || k == "linkpath" || k == "uname" || k == "gname") {
            return v.find('\0') == std::string_view::npos;
        }
        return true;
    }

    // The records of an extended header, "%d %s=%s\n" each, the length
    // counting itself; false for anything else. A key again is the last
    // one's value, in the place of the first.
    inline bool tar_parse_records(const uint8_t* p, size_t n, TarRecords& out) {
        std::string_view s(reinterpret_cast<const char*>(p), n);
        while (!s.empty()) {
            auto space = s.find(' ');
            // at most 19 digits: a longer number could wrap in 64 bits (Go and Python refuse it too)
            if (space == std::string_view::npos || space == 0 || space > 19) {
                return false;
            }
            uint64_t len = 0;
            for (size_t i = 0; i < space; ++i) {
                if (s[i] < '0' || s[i] > '9') {
                    return false;
                }
                len = len * 10 + uint64_t(s[i] - '0');
            }
            if (len < 5 || len > s.size() || len <= space + 1) {
                return false;
            }
            std::string_view record = s.substr(space + 1, size_t(len) - space - 2);
            if (s[size_t(len) - 1] != '\n') {
                return false;
            }
            auto eq = record.find('=');
            if (eq == std::string_view::npos) {
                return false;
            }
            std::string_view k = record.substr(0, eq), v = record.substr(eq + 1);
            if (!tar_valid_record(k, v)) {
                return false;
            }
            auto at = std::find_if(out.begin(), out.end(), [&](const auto& r) { return r.first == k; });
            if (at != out.end()) {
                at->second = std::string(v);
            } else {
                out.emplace_back(std::string(k), std::string(v));
            }
            s.remove_prefix(size_t(len));
        }
        return true;
    }

    inline void tar_append_record(std::string& out, std::string_view k, std::string_view v) {
        size_t size = k.size() + v.size() + 3;   // ' ', '=', '\n'
        size_t digits = std::to_string(size).size();
        size_t total = size + digits;
        if (std::to_string(total).size() != digits) {
            ++total;
        }
        out += std::to_string(total);
        out += ' ';
        out.append(k);
        out += '=';
        out.append(v);
        out += '\n';
    }

    // Whether a path stays below the directory it is joined to: its
    // elements walked, a ".." one level up, climbing out of the start
    // not local (Go's filepath.IsLocal, the same as cleaning the path
    // lexically and looking for a leading "..")
    inline bool tar_is_local(std::string_view p) noexcept {
        return sgcl::io::detail::is_local_path(p);
    }

    inline bool tar_is_ascii(std::string_view s) noexcept {
        for (char c : s) {
            if (uint8_t(c) >= 0x80) {
                return false;
            }
        }
        return true;
    }

    inline std::string tar_entry_text(std::string_view name) {
        std::string s = "tar: entry ";
        s.append(name);
        s += ": ";
        return s;
    }
}

namespace sgcl::compress::tar {
    inline bool entry::is_local() const {
        std::string_view n = name.view();
        if (!detail::tar_is_local(n)) {
            return false;
        }
        std::string_view l = link_name.view();
        if (type == tar::kind::symlink) {
            if (l.empty() || l[0] == '/') {
                return false;
            }
            // the target is found from the link's directory ("a/link/" is
            // the link "a/link")
            std::string_view link = n;
            while (!link.empty() && link.back() == '/') {
                link.remove_suffix(1);
            }
            auto slash = link.rfind('/');
            std::string joined(slash == std::string_view::npos ? std::string_view() : link.substr(0, slash + 1));
            joined.append(l);
            return detail::tar_is_local(joined);
        }
        if (type == tar::kind::hardlink) {
            return detail::tar_is_local(l);
        }
        return true;
    }

    // Reads an archive entry by entry, as Go's tar.Reader: next() goes to
    // the next entry's header, past whatever of the current one's data
    // was not read, and the reads give that entry's data, exactly its
    // size, then 0 — the stream functions of the mixin (read_all, copy_to)
    // read one entry. The archive may be v7, ustar, pax (an entry's
    // extended header and the global ones, which apply to every entry
    // after them), GNU (long names and links, base-256 numbers, the
    // access and change times) or star; a header's checksum is taken
    // unsigned or signed, as old Sun tar wrote it. The end is two blocks
    // of zeros, one block of zeros and the end of the data, or the end of
    // the data where a header would start, as Go takes it.
    //
    // An extended header or a GNU long name longer than 1 MiB is
    // errc::too_large, and the global records altogether no more; a size
    // that is negative or not a number is invalid_header; data that ends
    // inside an entry is unexpected_end, naming the entry. These stop the
    // reader: every call after gives the same error. A sparse file (GNU
    // 'S', a pax GNU.sparse record) and a GNU multi-volume part are
    // errc::unsupported, which stops only that entry: next() goes on to
    // the one after it.
    class reader final
    : public io::mixin::reader<tar::reader> {
    public:
    private:
        static constexpr size_t BufferBytes = size_t(64) << 10;
        static constexpr size_t ManagedBufferBytes = size_t(32) << 10;   // a task's: the largest block of whole pages

    public:

        explicit reader(const io::reader& in)
        : _in(in)
        , _buf(BufferBytes) {
        }

        reader(const reader&) = delete;
        reader& operator=(const reader&) = delete;
        reader(reader&&) noexcept = default;
        reader& operator=(reader&&) noexcept = default;

        // The next entry, nullopt at the end of the archive
        expected<optional<entry>, error> next() {
            for (;;) {
                switch (_step()) {
                    case Step::need:
                        _fill();
                        break;
                    case Step::found:
                        return optional<entry>(std::move(_found));
                    case Step::end:
                        return optional<entry>();
                    case Step::failed:
                        return unexpected<error>(*_error);
                }
            }
        }

        async::task<expected<optional<entry>, error>> async_next() {
            for (;;) {
                switch (_step()) {
                    case Step::need:
                        co_await _async_fill();
                        break;
                    case Step::found:
                        co_return optional<entry>(std::move(_found));
                    case Step::end:
                        co_return optional<entry>();
                    case Step::failed:
                        co_return unexpected<error>(*_error);
                }
            }
        }

        // The current entry's data; 0 at its end, and before the first
        // entry or after the last
        expected<size_t, io::error> read(const slice<byte>& out) {
            if (_stopped) {
                return io::detail::fail(_stream_error());
            }
            if (_phase != Phase::data || _remaining == 0 || out.empty()) {
                return size_t(0);
            }
            size_t want = size_t(std::min<uint64_t>(out.size(), _remaining));
            if (size_t n = _hand_out(out, want)) {
                return n;
            }
            if (_source_ended) {
                _truncated();
                return io::detail::fail(_stream_error());
            }
            auto r = _in.read(out.first(want));
            return _took_data(r);
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            if (_stopped) {
                co_return io::detail::fail(_stream_error());
            }
            if (_phase != Phase::data || _remaining == 0 || out.empty()) {
                co_return size_t(0);
            }
            size_t want = size_t(std::min<uint64_t>(out.size(), _remaining));
            if (size_t n = _hand_out(out, want)) {
                co_return n;
            }
            if (_source_ended) {
                _truncated();
                co_return io::detail::fail(_stream_error());
            }
            auto r = co_await _in.async_read(out.first(want));
            co_return _took_data(r);
        }

        // Closes in, as buffered_reader's close does
        expected<void, io::error> close() {
            return _in.close();
        }

        async::task<expected<void, io::error>> async_close() {
            return _in.async_close();
        }

        // The whole of the last failure: its code, the byte of the archive
        // where it was found, the entry it was in
        const optional<error>& last_error() const noexcept {
            return _error;
        }

    private:
        enum class Step : uint8_t { need, found, end, failed };

        enum class Phase : uint8_t {
            header,    // a header next
            special,   // the data of an extended header or a GNU long name
            sparse,    // the extension blocks of a GNU sparse header, then its data skipped
            data,      // an entry given: its data read by read(), or skipped by next()
            skip,      // the rest of the data and the padding skipped
            ended
        };

        size_t _avail() const noexcept {
            return _e - _b;
        }

        void _take(size_t n) noexcept {
            _b += n;
            _at += n;
        }

        Step _fail(errc code, std::string text, bool stop = true) {
            _error = error(code, _at, string(text));
            _stopped = stop;
            return Step::failed;
        }

        // What a read of the stream reports: the source's error as it was,
        // or the code of the module's category and the entry it was in
        io::error _stream_error() const {
            if (_error && _error->io_error()) {
                return *_error->io_error();
            }
            std::string where = "tar";
            if (!_name.empty()) {
                where += " entry ";
                where += _name;
            }
            return io::error(make_error_code(_error ? _error->code() : errc::corrupt), "read", string(where));
        }

        void _truncated() {
            _fail(errc::unexpected_end, detail::tar_entry_text(_name) + "unexpected end of data");
        }

        size_t _hand_out(const slice<byte>& out, size_t want) noexcept {
            size_t n = std::min(want, _avail());
            if (n) {
                std::memcpy(out.data(), _buf.data() + _b, n);
                _take(n);
                _remaining -= n;
            }
            return n;
        }

        expected<size_t, io::error> _took_data(const expected<size_t, io::error>& r) {
            if (!r) {
                _error = error(r.error(), _at);
                _stopped = true;
                return io::detail::fail(r.error());
            }
            if (*r == 0) {
                _source_ended = true;
                _truncated();
                return io::detail::fail(_stream_error());
            }
            _at += *r;
            _remaining -= *r;
            return *r;
        }

        // Room at the end of the buffer: what is left moved to the front,
        // the buffer grown for the data of an extended header, and back to
        // its size after one
        size_t _make_room() {
            if (_b) {
                std::memmove(_buf.data(), _buf.data() + _b, _e - _b);
                _e -= _b;
                _b = 0;
            }
            size_t base = _buf.managed() ? ManagedBufferBytes : BufferBytes;
            size_t want = std::max(base, _phase == Phase::special ? size_t(_special_size) : size_t(0));
            if (_buf.size() < want || (_buf.size() > want && _e <= want)) {
                _buf.resize(want, _e);
            }
            return _buf.size() - _e;
        }

        void _fill() {
            size_t room = _make_room();
            _took(_in.read(_buf.room(_e, room)));
        }

        async::task<void> _async_fill() {
            _buf.to_managed(_e);   // the read may run on the pool: into managed memory, which the slice holds
            size_t room = _make_room();
            _took(co_await _in.async_read(_buf.room(_e, room)));
        }

        void _took(const expected<size_t, io::error>& r) {
            if (!r) {
                _error = error(r.error(), _at + _avail());
                _stopped = true;
                return;
            }
            if (*r == 0) {
                _source_ended = true;
            }
            _e += *r;
        }

        // Works on the bytes held until an entry is found, the archive
        // ends, it fails, or more bytes are needed
        Step _step() {
            for (;;) {
                if (_stopped) {
                    return Step::failed;
                }
                switch (_phase) {
                    case Phase::ended:
                        return Step::end;
                    case Phase::data:
                        _phase = Phase::skip;
                        break;
                    case Phase::sparse: {
                        if (_avail() < detail::TarBlock) {
                            if (_source_ended) {
                                return _fail(errc::unexpected_end, detail::tar_entry_text(_name) + "unexpected end in its sparse map");
                            }
                            return Step::need;
                        }
                        bool more = _buf.data()[_b + 504] != 0;
                        _take(detail::TarBlock);
                        if (!more) {
                            _phase = Phase::skip;
                        }
                        break;
                    }
                    case Phase::skip: {
                        size_t n = size_t(std::min<uint64_t>(_avail(), _remaining));
                        _take(n);
                        _remaining -= n;
                        if (_remaining) {
                            if (_source_ended) {
                                return _fail(errc::unexpected_end, detail::tar_entry_text(_name) + "unexpected end of data");
                            }
                            return Step::need;
                        }
                        n = size_t(std::min<uint64_t>(_avail(), _padding));
                        _take(n);
                        _padding -= n;
                        if (_padding) {
                            if (_source_ended) {
                                // the padding cut short: the end of the archive, as Go takes it
                                _phase = Phase::ended;
                                return Step::end;
                            }
                            return Step::need;
                        }
                        _phase = Phase::header;
                        break;
                    }
                    case Phase::header: {
                        if (_avail() < detail::TarBlock) {
                            if (!_source_ended) {
                                return Step::need;
                            }
                            if (_avail() == 0) {
                                _phase = Phase::ended;
                                return Step::end;
                            }
                            return _fail(errc::unexpected_end, "tar: unexpected end in a header");
                        }
                        if (auto s = _header(_buf.data() + _b)) {
                            return *s;
                        }
                        break;
                    }
                    case Phase::special: {
                        if (_avail() < _special_size) {
                            if (_source_ended) {
                                return _fail(errc::unexpected_end, "tar: unexpected end in an extended header");
                            }
                            return Step::need;
                        }
                        if (auto s = _special(_buf.data() + _b, size_t(_special_size))) {
                            return *s;
                        }
                        _take(size_t(_special_size));
                        _remaining = 0;
                        _padding = detail::tar_padding(_special_size);
                        _phase = Phase::skip;
                        break;
                    }
                }
            }
        }

        // The data of an extended header or a GNU long name, whole in the buffer
        optional<Step> _special(const uint8_t* p, size_t n) {
            switch (_special_flag) {
                case 'x': {
                    detail::TarRecords r;
                    if (!detail::tar_parse_records(p, n, r)) {
                        return _fail(errc::invalid_header, "tar: malformed pax record");
                    }
                    _local = std::move(r);   // a second extended header before an entry replaces the first, as Go takes it
                    return nullopt;
                }
                case 'g': {
                    detail::TarRecords r;
                    if (!detail::tar_parse_records(p, n, r)) {
                        return _fail(errc::invalid_header, "tar: malformed pax record in a global header");
                    }
                    // an empty value takes the global record away
                    for (auto& [k, v] : r) {
                        auto at = std::find_if(_globals.begin(), _globals.end(), [&](const auto& g) { return g.first == k; });
                        if (v.empty()) {
                            if (at != _globals.end()) {
                                _globals.erase(at);
                            }
                        } else if (at != _globals.end()) {
                            at->second = v;
                        } else {
                            _globals.emplace_back(k, v);
                        }
                    }
                    size_t total = 0;
                    for (auto& [k, v] : _globals) {
                        total += k.size() + v.size();
                    }
                    if (total > detail::TarMaxSpecial) {
                        return _fail(errc::too_large, "tar: global pax records longer than 1 MiB");
                    }
                    return nullopt;
                }
                case 'L':
                    _long_name = std::string(detail::tar_string(p, n));
                    return nullopt;
                default:
                    _long_link = std::string(detail::tar_string(p, n));
                    return nullopt;
            }
        }

        enum class Format : uint8_t { v7, ustar, gnu, star };

        // A header block: an extended header or a long name to read, or
        // an entry with everything before it merged in
        optional<Step> _header(const uint8_t* h) {
            if (std::all_of(h, h + detail::TarBlock, [](uint8_t c) { return c == 0; })) {
                _take(detail::TarBlock);
                if (_zero_block) {
                    _phase = Phase::ended;
                    return Step::end;
                }
                _zero_block = true;
                return nullopt;
            }
            if (_zero_block) {
                return _fail(errc::invalid_header, "tar: a header after a block of zeros");
            }
            int64_t sum;
            if (!detail::tar_octal(h + 148, 8, sum)) {
                return _fail(errc::invalid_header, "tar: header checksum not a number");
            }
            int64_t unsigned_sum = 0, signed_sum = 0;
            for (size_t i = 0; i < detail::TarBlock; ++i) {
                uint8_t c = i >= 148 && i < 156 ? uint8_t(' ') : h[i];
                unsigned_sum += c;
                signed_sum += int8_t(c);
            }
            if (sum != unsigned_sum && sum != signed_sum) {
                return _fail(errc::invalid_header, "tar: header checksum mismatch");
            }
            Format format = Format::v7;
            if (std::memcmp(h + 257, "ustar\0", 6) == 0) {
                format = std::memcmp(h + 508, "tar\0", 4) == 0 ? Format::star : Format::ustar;
            } else if (std::memcmp(h + 257, "ustar  \0", 8) == 0) {
                format = Format::gnu;
            }
            uint8_t flag = h[156];
            int64_t size;
            if (!detail::tar_numeric(h + 124, 12, size)) {
                return _fail(errc::invalid_header, "tar: size not a number");
            }
            if (size < 0) {
                return _fail(errc::invalid_header, "tar: negative size");
            }
            if (flag == 'x' || flag == 'g' || flag == 'L' || flag == 'K') {
                if (uint64_t(size) > detail::TarMaxSpecial) {
                    return _fail(errc::too_large, flag == 'L' || flag == 'K' ? "tar: GNU long name longer than 1 MiB" : "tar: pax header longer than 1 MiB");
                }
                _take(detail::TarBlock);
                _special_flag = flag;
                _special_size = uint64_t(size);
                _phase = Phase::special;
                return nullopt;
            }
            return _entry(h, format, flag, size);
        }

        optional<Step> _entry(const uint8_t* h, Format format, uint8_t flag, int64_t size) {
            entry e;
            std::string name(detail::tar_string(h, 100));
            std::string link(detail::tar_string(h + 157, 100));
            std::string uname, gname;
            int64_t mode, uid, gid, mtime, major = 0, minor = 0;
            if (!detail::tar_numeric(h + 100, 8, mode) || !detail::tar_numeric(h + 108, 8, uid) || !detail::tar_numeric(h + 116, 8, gid) || !detail::tar_numeric(h + 136, 12, mtime)) {
                return _fail(errc::invalid_header, detail::tar_entry_text(name) + "a numeric field of its header not a number");
            }
            e.modified = detail::tar_time(mtime, 0);
            if (format != Format::v7) {
                uname = detail::tar_string(h + 265, 32);
                gname = detail::tar_string(h + 297, 32);
                if (!detail::tar_numeric(h + 329, 8, major) || !detail::tar_numeric(h + 337, 8, minor)) {
                    return _fail(errc::invalid_header, detail::tar_entry_text(name) + "a device number not a number");
                }
                std::string prefix;
                if (format == Format::ustar) {
                    prefix = detail::tar_string(h + 345, 155);
                } else if (format == Format::star) {
                    prefix = detail::tar_string(h + 345, 131);
                    int64_t a = 0, c = 0;
                    if ((h[476] && !detail::tar_numeric(h + 476, 12, a)) || (h[488] && !detail::tar_numeric(h + 488, 12, c))) {
                        return _fail(errc::invalid_header, detail::tar_entry_text(name) + "a time of its star header not a number");
                    }
                    if (h[476]) {
                        e.accessed = detail::tar_time(a, 0);
                    }
                    if (h[488]) {
                        e.changed = detail::tar_time(c, 0);
                    }
                } else {
                    // GNU's times, when there are any. Go before 1.8 wrote
                    // a ustar prefix there in a GNU header: text that is no
                    // number is taken as that prefix, as Go takes it now.
                    int64_t a = 0, c = 0;
                    bool ok = (!h[345] || detail::tar_numeric(h + 345, 12, a)) && (!h[357] || detail::tar_numeric(h + 357, 12, c));
                    if (ok && h[345]) {
                        e.accessed = detail::tar_time(a, 0);
                    }
                    if (ok && h[357]) {
                        e.changed = detail::tar_time(c, 0);
                    }
                    if (!ok && detail::tar_is_ascii(detail::tar_string(h + 345, 155))) {
                        prefix = detail::tar_string(h + 345, 155);
                    }
                }
                if (!prefix.empty()) {
                    name = prefix + "/" + name;
                }
            }
            if (major < 0 || major > int64_t(UINT32_MAX) || minor < 0 || minor > int64_t(UINT32_MAX)) {
                return _fail(errc::invalid_header, detail::tar_entry_text(name) + "a device number out of range");
            }

            // the pax records: the global ones, the entry's over them; an
            // empty value leaves the header's field as it is
            detail::TarRecords records = _globals;
            for (auto& [k, v] : _local) {
                auto at = std::find_if(records.begin(), records.end(), [&](const auto& r) { return r.first == k; });
                if (at != records.end()) {
                    at->second = v;
                } else {
                    records.emplace_back(k, v);
                }
            }
            _local.clear();
            bool sparse = false;
            std::string sparse_name;
            for (auto& [k, v] : records) {
                if (v.empty()) {
                    continue;
                }
                bool ok = true;
                if (k == "path") {
                    name = v;
                } else if (k == "linkpath") {
                    link = v;
                } else if (k == "uname") {
                    uname = v;
                } else if (k == "gname") {
                    gname = v;
                } else if (k == "uid") {
                    ok = detail::tar_decimal(v, uid);
                } else if (k == "gid") {
                    ok = detail::tar_decimal(v, gid);
                } else if (k == "size") {
                    ok = detail::tar_decimal(v, size) && size >= 0;
                } else if (k == "mtime") {
                    ok = detail::tar_pax_time(v, e.modified);
                } else if (k == "atime") {
                    time::datetime t;
                    ok = detail::tar_pax_time(v, t);
                    e.accessed = t;
                } else if (k == "ctime") {
                    time::datetime t;
                    ok = detail::tar_pax_time(v, t);
                    e.changed = t;
                } else if (k.starts_with("GNU.sparse.")) {
                    sparse = true;
                    if (k == "GNU.sparse.name") {
                        sparse_name = v;   // the file's name, the header's being GNUSparseFile.0/...
                    }
                } else {
                    e.pax.push_back(pair<string, string>(string(k), string(v)));
                }
                if (!ok) {
                    return _fail(errc::invalid_header, detail::tar_entry_text(name) + "pax record " + k + " not a valid value");
                }
            }
            // GNU's long names over everything
            if (!_long_name.empty()) {
                name = std::move(_long_name);
            }
            if (!_long_link.empty()) {
                link = std::move(_long_link);
            }
            _long_name.clear();
            _long_link.clear();

            _take(detail::TarBlock);
            _name = name;
            switch (flag) {
                case '1': e.type = tar::kind::hardlink; break;
                case '2': e.type = tar::kind::symlink; break;
                case '3': e.type = tar::kind::char_device; break;
                case '4': e.type = tar::kind::block_device; break;
                case '5': case 'D': e.type = tar::kind::directory; break;
                case '6': e.type = tar::kind::fifo; break;
                case 0: e.type = !name.empty() && name.back() == '/' ? tar::kind::directory : tar::kind::file; break;   // v7 marks a directory by its slash
                default: e.type = tar::kind::file; break;
            }
            // the types that are a header alone have no data, whatever their size says
            bool header_only = flag >= '1' && flag <= '6';
            _remaining = header_only ? 0 : uint64_t(size);
            _padding = detail::tar_padding(_remaining);
            if (flag == 'S' || flag == 'M' || sparse) {
                // stepped over by the next call: the extension blocks of a
                // GNU sparse map first, then the data
                if (!sparse_name.empty()) {
                    _name = sparse_name;
                }
                _phase = format == Format::gnu && flag == 'S' && h[482] ? Phase::sparse : Phase::skip;
                return _fail(errc::unsupported, detail::tar_entry_text(_name) + (flag == 'M' ? "GNU multi-volume parts are not supported" : "sparse files are not supported"), false);
            }
            // a GNU dump directory's data (the names in it, for an
            // incremental dump) is no file's: stepped over, as a
            // directory has none
            bool dump_dir = flag == 'D';
            e.name = string(name);
            e.link_name = string(link);
            e.size = dump_dir ? 0 : _remaining;
            e.mode = io::permissions(unsigned(uint64_t(mode) & 07777));
            e.uid = uid;
            e.gid = gid;
            e.user_name = string(uname);
            e.group_name = string(gname);
            e.dev_major = uint32_t(major);
            e.dev_minor = uint32_t(minor);
            _found = std::move(e);
            _phase = dump_dir ? Phase::skip : Phase::data;
            return Step::found;
        }

        io::reader _in;
        detail::InputBuffer<ManagedBufferBytes> _buf;   // plain until a task's first read, managed from then on (detail/block.h)
        size_t _b = 0;               // the bytes held and not yet used: [_b, _e)
        size_t _e = 0;
        uint64_t _at = 0;            // the archive's offset of _buf[_b]
        uint64_t _remaining = 0;     // of the current entry's data
        uint64_t _padding = 0;       // after it, to the block's end
        uint64_t _special_size = 0;
        uint8_t _special_flag = 0;
        bool _zero_block = false;
        bool _source_ended = false;
        bool _stopped = false;
        Phase _phase = Phase::header;
        std::string _name;           // of the current entry, for the messages
        std::string _long_name;
        std::string _long_link;
        detail::TarRecords _local;
        detail::TarRecords _globals;
        entry _found;
        optional<error> _error;
    };

    // Writes an archive entry by entry, as Go's tar.Writer: write_header
    // then exactly the entry's size in bytes through write (more is
    // errc::invalid_argument, nothing of it written), and close() the two
    // blocks of zeros that end the archive, out left open. A header is
    // ustar when the entry fits in one; otherwise a pax extended header
    // goes before it, with a record for each field that does not: a name
    // past ustar's (100 bytes, or a prefix of 155 and a name of 100 split
    // at a slash) or not ASCII, a link name likewise, a user or group name
    // past 32 bytes or not ASCII, an id past 2097151 or negative, a size
    // of 8 GiB or more, a time with a fraction of a second or outside
    // eleven octal digits (1970 to 2242), an access or change time, and
    // the entry's own records (entry::pax). With an extended header a
    // name past 100 bytes goes into it rather than into the prefix, and
    // the records are in the order of their keys, as Go writes them (the
    // same entries make Go's bytes). A header before the last entry's
    // data is whole, or a close, is errc::invalid_argument, and so is an
    // entry the format cannot hold: an empty name, a NUL in a name, a size
    // for a type that has no data, a device number past 2097151, a pax
    // record that is malformed, given twice or one of the fields'. Every
    // error the writer gives is kept as its first: a failure of out, and
    // the caller's too (a header or a write it cannot take, a write after
    // close); every write_header, write and close after it gives that
    // error at once and writes nothing, so an archive may be written
    // freely and checked once, at the close.
    class writer final
    : public io::mixin::writer<tar::writer> {
    public:
        using io::mixin::writer<tar::writer>::write;
        using io::mixin::writer<tar::writer>::async_write;

        explicit writer(const io::writer& out)
        : _out(out) {
        }

        writer(const writer&) = delete;
        writer& operator=(const writer&) = delete;
        writer(writer&&) noexcept = default;
        writer& operator=(writer&&) noexcept = default;

        expected<void, error> write_header(const entry& e) {
            if (_header_kept(e) || _drain()) {
                return unexpected<error>(*_error);
            }
            return {};
        }

        async::task<expected<void, error>> async_write_header(entry e) {
            if (_header_kept(e) || co_await _async_drain()) {
                co_return unexpected<error>(*_error);
            }
            co_return expected<void, error>();
        }

        // The current entry's data: all of it, or nothing when it goes
        // past the entry's size
        expected<size_t, io::error> write(const slice<const byte>& data) {
            if (auto x = _check_write(data.size())) {
                return io::detail::fail(*x);
            }
            if (data.empty()) {
                return size_t(0);
            }
            return _wrote(_out.write(data), data.size());
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            if (auto x = _check_write(data.size())) {
                co_return io::detail::fail(*x);
            }
            if (data.empty()) {
                co_return size_t(0);
            }
            co_return _wrote(co_await _out.async_write(data), data.size());
        }

        // The last entry's padding and two blocks of zeros; out stays
        // open. A second close does nothing; the kept error is the result
        // of every close after it.
        expected<void, io::error> close() {
            if (auto x = _check_close()) {
                return x->ok ? expected<void, io::error>() : io::detail::fail(x->e);
            }
            if (auto x = _drain()) {
                return io::detail::fail(*x);
            }
            _closed = true;
            return {};
        }

        async::task<expected<void, io::error>> async_close() {
            if (auto x = _check_close()) {
                co_return x->ok ? expected<void, io::error>() : io::detail::fail(x->e);
            }
            if (auto x = co_await _async_drain()) {
                co_return io::detail::fail(*x);
            }
            _closed = true;
            co_return expected<void, io::error>();
        }

        // The first error the writer gave, kept (as the reader's
        // last_error): what every header, write and close after it gives
        const optional<error>& last_error() const noexcept {
            return _error;
        }

    private:
        struct Closing {
            bool ok;
            io::error e;
        };

        std::string _where() const {
            return _name.empty() ? std::string("tar") : "tar entry " + _name;
        }

        // The first error kept, in the two forms the writer gives it: the
        // archive's (write_header, last_error) and the stream's (write, close)
        const io::error& _keep(const error& e, const io::error& stream) {
            if (!_error) {
                _error = e;
                _stream_error = stream;
            }
            return *_stream_error;
        }

        const io::error& _keep(const io::error& stream) {
            return _keep(error(stream, _written), stream);
        }

        optional<io::error> _check_write(size_t n) {
            if (_error) {
                return _stream_error;
            }
            if (_closed) {
                return _keep(io::error(io::errc::closed, "write", "tar"));
            }
            if (n > _remaining) {
                return _keep(error(errc::invalid_argument, _written, string(detail::tar_entry_text(_name) + "a write past its size")),
                             io::error(make_error_code(errc::invalid_argument), "write", string(_where())));
            }
            return nullopt;
        }

        // The header of e made into _pending, or its error kept
        bool _header_kept(const entry& e) {
            if (_error) {
                return true;
            }
            if (auto x = _header(e)) {
                _keep(*x, x->io_error() ? *x->io_error() : io::error(make_error_code(x->code()), "write_header", string(_where())));
                return true;
            }
            return false;
        }

        expected<size_t, io::error> _wrote(const expected<size_t, io::error>& w, size_t n) {
            if (!w) {
                return io::detail::fail(_keep(w.error()));
            }
            _remaining -= n;
            _written += n;
            return n;
        }

        // A close that ends here (done before, or failing), or nullopt with
        // the padding and the two blocks of zeros made into _pending
        optional<Closing> _check_close() {
            if (_error) {
                return Closing{false, *_stream_error};
            }
            if (_closed) {
                return Closing{true, {}};
            }
            if (_remaining) {
                return Closing{false, _keep(error(errc::invalid_argument, _written, string(detail::tar_entry_text(_name) + std::to_string(_remaining) + " bytes of its data not written")),
                                            io::error(make_error_code(errc::invalid_argument), "close", string(_where())))};
            }
            _pending.assign(size_t(_padding) + 2 * detail::TarBlock, 0);
            _padding = 0;
            return nullopt;
        }

        optional<io::error> _drain() {
            auto w = _out.write(slice<const byte>(reinterpret_cast<const byte*>(_pending.data()), _pending.size()));
            return _drained(w);
        }

        async::task<optional<io::error>> _async_drain() {
            auto w = co_await _out.async_write(_stage.stage(_pending));   // a task's write may run on the pool: the bytes it is given are managed
            co_return _drained(w);
        }

        optional<io::error> _drained(const expected<size_t, io::error>& w) {
            size_t n = _pending.size();
            _pending.clear();
            if (!w) {
                return _keep(w.error());
            }
            _written += n;
            return nullopt;
        }

        static void _octal(uint8_t* field, size_t width, uint64_t v) {
            // width - 1 digits and a NUL
            for (size_t i = width - 1; i-- > 0;) {
                field[i] = uint8_t('0' + (v & 7));
                v >>= 3;
            }
            field[width - 1] = 0;
        }

        static void _text(uint8_t* field, size_t width, std::string_view s) {
            std::memcpy(field, s.data(), std::min(width, s.size()));
        }

        // A name that ustar holds as a prefix and a name split at a slash:
        // the position of the slash, 0 for none
        static size_t _split(std::string_view s) noexcept {
            if (s.size() <= 100) {
                return 0;
            }
            size_t i = std::min<size_t>(155, s.size() - 2);
            for (; i > 0 && s[i] != '/'; --i) {
            }
            return i > 0 && s.size() - i - 1 <= 100 ? i : 0;
        }

        // A string field: what a reader of ustar alone sees of a name only
        // pax holds, as Go's writer makes it — the ASCII characters, cut to
        // the field, and a cut name that would end in a slash (a directory
        // to such a reader) cut before its slashes. A string that fits is
        // written as it is.
        static void _plain(uint8_t* field, size_t width, std::string_view s) {
            std::string t;
            for (char c : s) {
                if (uint8_t(c) < 0x80 && c != 0) {
                    t += c;
                }
            }
            if (t.size() > width) {
                t.resize(width);
                if (t.back() == '/') {
                    while (!t.empty() && t.back() == '/') {
                        t.pop_back();
                    }
                }
            }
            _text(field, width, t);
        }

        static void _checksum(uint8_t* h) {
            std::memset(h + 148, ' ', 8);
            unsigned sum = 0;
            for (size_t i = 0; i < detail::TarBlock; ++i) {
                sum += h[i];
            }
            _octal(h + 148, 7, sum);
            h[155] = ' ';
        }

        static void _magic(uint8_t* h) {
            std::memcpy(h + 257, "ustar\0" "00", 8);
        }

        optional<error> _invalid(std::string_view name, const char* text) const {
            return error(errc::invalid_argument, _written, string(detail::tar_entry_text(name) + text));
        }

        // The header of e, and a pax header before it when needed, made
        // into _pending after the last entry's padding
        optional<error> _header(const entry& e) {
            if (_closed) {
                return error(io::error(io::errc::closed, "write_header", "tar"), _written);
            }
            if (_remaining) {
                return error(errc::invalid_argument, _written, string(detail::tar_entry_text(_name) + std::to_string(_remaining) + " bytes of its data not written"));
            }
            std::string_view name = e.name.view(), link = e.link_name.view(), uname = e.user_name.view(), gname = e.group_name.view();
            if (name.empty()) {
                return error(errc::invalid_argument, _written, string("tar: an entry with no name"));
            }
            for (auto s : {name, link, uname, gname}) {
                if (s.find('\0') != std::string_view::npos) {
                    return _invalid(name, "a NUL in a name");
                }
            }
            bool has_data = e.type == tar::kind::file;
            if (!has_data && e.size) {
                return _invalid(name, "a size for a type that has no data");
            }
            if (e.size > uint64_t(INT64_MAX)) {
                return _invalid(name, "a size past 2^63 - 1");
            }
            if (e.dev_major > detail::TarMaxOctal7 || e.dev_minor > detail::TarMaxOctal7) {
                return _invalid(name, "a device number past 2097151");
            }
            for (size_t i = 0; i < e.pax.size(); ++i) {
                auto k = e.pax[i].first.view();
                if (!detail::tar_valid_record(k, e.pax[i].second.view()) || detail::tar_known_key(k) || k.starts_with("GNU.sparse.")) {
                    return _invalid(name, "a pax record that is malformed or one of the entry's fields");
                }
                for (size_t j = 0; j < i; ++j) {
                    if (e.pax[j].first.view() == k) {
                        return _invalid(name, "a pax record given twice");
                    }
                }
            }

            uint8_t h[detail::TarBlock] = {};
            detail::TarRecords records;
            if (!detail::tar_is_ascii(link) || link.size() > 100) {
                records.emplace_back("linkpath", link);
            }
            _plain(h + 157, 100, link);
            if (e.size > uint64_t(detail::TarMaxOctal11)) {
                records.emplace_back("size", std::to_string(e.size));
            }
            for (auto [key, id] : {std::pair<const char*, int64_t>("uid", e.uid), std::pair<const char*, int64_t>("gid", e.gid)}) {
                if (id < 0 || id > detail::TarMaxOctal7) {
                    records.emplace_back(key, std::to_string(id));
                }
            }
            for (auto [key, s] : {std::pair<const char*, std::string_view>("uname", uname), std::pair<const char*, std::string_view>("gname", gname)}) {
                if (!detail::tar_is_ascii(s) || s.size() > 32) {
                    records.emplace_back(key, s);
                }
            }
            int64_t mtime_ns = e.modified.unix_nano();
            int64_t mtime = detail::tar_floor_div(mtime_ns, 1000000000);
            bool mtime_fits = mtime_ns % 1000000000 == 0 && mtime >= 0 && mtime <= detail::TarMaxOctal11;
            if (!mtime_fits) {
                records.emplace_back("mtime", detail::tar_format_pax_time(mtime_ns));
            }
            if (e.accessed) {
                records.emplace_back("atime", detail::tar_format_pax_time(e.accessed->unix_nano()));
            }
            if (e.changed) {
                records.emplace_back("ctime", detail::tar_format_pax_time(e.changed->unix_nano()));
            }
            for (auto& [k, v] : e.pax) {
                records.emplace_back(k.view(), v.view());
            }
            // The name: in the name field, or split into the prefix and
            // the name at a slash when there is no extended header anyway;
            // with one, a long name goes into a record, as Go writes it
            size_t split = detail::tar_is_ascii(name) && records.empty() ? _split(name) : 0;
            if (detail::tar_is_ascii(name) && name.size() <= 100) {
                _text(h, 100, name);
            } else if (split) {
                _text(h + 345, 155, name.substr(0, split));
                _text(h, 100, name.substr(split + 1));
            } else {
                records.emplace_back("path", name);
                _plain(h, 100, name);
            }
            // in the order of the keys, as Go writes them
            std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            std::string pax;
            for (auto& [k, v] : records) {
                detail::tar_append_record(pax, k, v);
            }
            if (pax.size() > detail::TarMaxSpecial) {
                return _invalid(name, "pax records longer than 1 MiB");
            }

            int64_t field_mtime = mtime >= 0 && mtime <= detail::TarMaxOctal11 ? mtime : 0;
            _octal(h + 100, 8, unsigned(e.mode) & 07777);
            _octal(h + 108, 8, uint64_t(e.uid >= 0 && e.uid <= detail::TarMaxOctal7 ? e.uid : 0));
            _octal(h + 116, 8, uint64_t(e.gid >= 0 && e.gid <= detail::TarMaxOctal7 ? e.gid : 0));
            _octal(h + 124, 12, e.size <= uint64_t(detail::TarMaxOctal11) ? e.size : 0);
            _octal(h + 136, 12, uint64_t(field_mtime));
            static constexpr char flags[] = {'0', '5', '2', '1', '3', '4', '6'};
            h[156] = uint8_t(flags[size_t(e.type)]);
            _magic(h);
            _plain(h + 265, 32, uname);
            _plain(h + 297, 32, gname);
            _octal(h + 329, 8, e.dev_major);
            _octal(h + 337, 8, e.dev_minor);
            _checksum(h);

            _pending.assign(size_t(_padding), 0);
            if (!pax.empty()) {
                // the extended header: named after the entry, in a
                // directory of its own (what a reader of ustar alone
                // unpacks), everything else zero, as Go writes it
                std::string base(name);
                while (base.size() > 1 && base.back() == '/') {
                    base.pop_back();
                }
                auto slash = base.find_last_of('/');
                std::string xname = (slash == std::string::npos ? std::string() : base.substr(0, slash + 1)) + "PaxHeaders.0/" + (slash == std::string::npos ? base : base.substr(slash + 1));
                uint8_t x[detail::TarBlock] = {};
                _plain(x, 100, xname);
                _octal(x + 100, 8, 0);
                _octal(x + 108, 8, 0);
                _octal(x + 116, 8, 0);
                _octal(x + 124, 12, pax.size());
                _octal(x + 136, 12, 0);
                x[156] = 'x';
                _magic(x);
                _checksum(x);
                _pending.insert(_pending.end(), x, x + detail::TarBlock);
                _pending.insert(_pending.end(), pax.begin(), pax.end());
                _pending.resize(_pending.size() + size_t(detail::tar_padding(pax.size())), 0);
            }
            _pending.insert(_pending.end(), h, h + detail::TarBlock);
            _name = std::string(name);
            _remaining = e.size;
            _padding = detail::tar_padding(e.size);
            return nullopt;
        }

        io::writer _out;
        std::vector<uint8_t> _pending;
        detail::OutputStage _stage;   // the pending bytes for a task's write (detail/block.h)
        uint64_t _written = 0;      // to out: the offset of the errors
        uint64_t _remaining = 0;    // of the current entry's data
        uint64_t _padding = 0;      // after it
        std::string _name;
        optional<error> _error;               // the first error given, kept
        optional<io::error> _stream_error;    // the same, as write and close give it
        bool _closed = false;
    };
}

namespace sgcl::compress::detail {
    // What a tar file is wrapped in, by its first bytes: gzip (1f 8b),
    // xz (fd 37 7a 58 5a 00), bzip2 ("BZh"), or nothing
    enum class TarWrap : uint8_t {
        none,
        gzip,
        xz,
        bzip2
    };

    inline expected<TarWrap, error> tar_wrap_of(const string& path) {
        auto f = io::open(path);
        if (!f) {
            return unexpected(error(f.error(), 0));
        }
        unsigned char head[6] = {};
        size_t got = 0;
        while (got < sizeof head) {
            auto n = f->read(slice<byte>(reinterpret_cast<byte*>(head + got), sizeof head - got));
            if (!n) {
                (void)f->close();
                return unexpected(error(n.error(), 0));
            }
            if (*n == 0) {
                break;
            }
            got += *n;
        }
        (void)f->close();
        if (got >= 2 && head[0] == 0x1f && head[1] == 0x8b) {
            return TarWrap::gzip;
        }
        if (got >= 6 && head[0] == 0xfd && head[1] == '7' && head[2] == 'z' && head[3] == 'X' && head[4] == 'Z' && head[5] == 0) {
            return TarWrap::xz;
        }
        if (got >= 3 && head[0] == 'B' && head[1] == 'Z' && head[2] == 'h') {
            return TarWrap::bzip2;
        }
        return TarWrap::none;
    }

    // The archive's bytes as a stream, unwrapped
    inline io::reader tar_input(const io::file& f, TarWrap w) {
        switch (w) {
            case TarWrap::gzip: return io::reader(gzip::reader(f));
            case TarWrap::xz: return io::reader(xz::reader(f));
            case TarWrap::bzip2: return io::reader(bzip2::reader(f));
            default: return io::reader(f);
        }
    }

    // What a tar file is to be wrapped in, by its name: .tar.gz and .tgz
    // gzip, .tar.xz and .txz xz; .tar.bz2 and .tbz2 have no writer here
    inline TarWrap tar_wrap_by_name(std::string_view name) {
        auto ends = [&](std::string_view s) { return name.size() >= s.size() && name.substr(name.size() - s.size()) == s; };
        if (ends(".tar.gz") || ends(".tgz")) {
            return TarWrap::gzip;
        }
        if (ends(".tar.xz") || ends(".txz")) {
            return TarWrap::xz;
        }
        if (ends(".tar.bz2") || ends(".tbz2") || ends(".tbz")) {
            return TarWrap::bzip2;
        }
        return TarWrap::none;
    }

    // One pass over the archive: every entry given to f with the reader
    // standing at its data; the reader's error, or f's, ends it
    template<class F>
    expected<void, error> tar_each(const string& path, TarWrap w, F f) {
        auto file = io::open(path);
        if (!file) {
            return unexpected(error(file.error(), 0));
        }
        tar::reader r(tar_input(*file, w));
        for (;;) {
            auto e = r.next();
            if (!e) {
                (void)file->close();
                return unexpected(e.error());
            }
            if (!*e) {
                break;
            }
            if (auto done = f(**e, r); !done) {
                (void)file->close();
                return done;
            }
        }
        (void)file->close();
        return {};
    }

    inline expected<void, error> tar_extract(const string& archive_path, const string& directory, uint64_t max_size) {
        auto w = tar_wrap_of(archive_path);
        if (!w) {
            return unexpected(w.error());
        }
        // every name checked, and the sizes summed, before anything is written
        uint64_t total = 0;
        auto checked = tar_each(archive_path, *w, [&](const tar::entry& e, tar::reader&) -> expected<void, error> {
            if (!e.is_local()) {
                return unexpected(error(errc::insecure_path, 0, string("tar: an entry's name or link leaves the directory: ") + e.name));
            }
            total += e.size;
            if (max_size && total > max_size) {
                return unexpected(error(errc::too_large, 0, string("tar: the files are larger than max_size")));
            }
            return {};
        });
        if (!checked) {
            return checked;
        }
        TreeWriter out;
        if (auto started = out.start(directory); !started) {
            return started;
        }
        auto written = tar_each(archive_path, *w, [&](const tar::entry& e, tar::reader& r) -> expected<void, error> {
            std::string_view name = e.name.view();
            switch (e.type) {
                case tar::kind::directory:
                    return out.directory(name, e.mode);
                case tar::kind::file:
                    return out.file(name, e.mode, optional<time::datetime>(e.modified), r, 0);
                case tar::kind::symlink:
                    out.symlink(name, e.link_name.view());
                    return {};
                case tar::kind::hardlink:
                    out.hardlink(name, e.link_name.view());
                    return {};
                default:
                    return {};   // a device or a fifo: made by root only, left out as Go's tools leave them
            }
        });
        if (!written) {
            return written;
        }
        return out.finish();
    }

    inline expected<void, error> tar_create(const string& directory, const string& archive_path, compress::level level) {
        const TarWrap w = tar_wrap_by_name(archive_path.view());
        if (w == TarWrap::bzip2) {
            return unexpected(error(errc::unsupported, 0, string("tar: bzip2 is read, not written: ") + archive_path));
        }
        auto tree = list_tree(directory);
        if (!tree) {
            return unexpected(tree.error());
        }
        auto file = io::create(archive_path);
        if (!file) {
            return unexpected(error(file.error(), 0));
        }
        auto write_all = [&](const io::writer& sink) -> expected<void, error> {
            tar::writer tw(sink);
            for (const auto& t : *tree) {
                tar::entry e;
                e.mode = io::permissions(unsigned(t.mode) & 07777);
                e.modified = datetime_of(t.modified);
                if (t.type == io::file_type::directory) {
                    e.name = string(t.name + "/");
                    e.type = tar::kind::directory;
                } else if (t.type == io::file_type::symlink) {
                    e.name = string(t.name);
                    e.type = tar::kind::symlink;
                    e.link_name = string(t.target);
                } else {
                    e.name = string(t.name);
                    e.size = t.size;
                }
                if (auto h = tw.write_header(e); !h) {
                    return h;
                }
                if (t.type == io::file_type::regular) {
                    auto in = io::open(string(t.path));
                    if (!in) {
                        return unexpected(error(in.error(), 0));
                    }
                    auto copied = io::copy(tw, *in);
                    (void)in->close();
                    if (!copied) {
                        return unexpected(archive_error(copied.error()));
                    }
                    if (uint64_t(*copied) != t.size) {   // the file changed while it was read
                        return unexpected(error(errc::invalid_argument, 0, string("tar: a file changed size while it was archived: ") + string(t.name)));
                    }
                }
            }
            if (auto c = tw.close(); !c) {
                return unexpected(archive_error(c.error()));
            }
            return {};
        };
        expected<void, error> r;
        if (w == TarWrap::gzip) {
            gzip::writer z(*file, gzip::options{level, {}});
            r = write_all(io::writer(z));
            if (auto c = z.close(); r && !c) {
                r = unexpected(archive_error(c.error()));
            }
        } else if (w == TarWrap::xz) {
            xz::writer z(*file, xz::options{level});
            r = write_all(io::writer(z));
            if (auto c = z.close(); r && !c) {
                r = unexpected(archive_error(c.error()));
            }
        } else {
            r = write_all(io::writer(*file));
        }
        if (auto c = file->close(); r && !c) {
            r = unexpected(error(c.error(), 0));
        }
        if (!r) {
            (void)io::remove(archive_path);   // no half archive left behind
        }
        return r;
    }
}

namespace sgcl::compress::tar {
    // What extract and create take besides the paths
    struct options {
        compress::level level;                    // create: the level of gzip or xz around the archive, 6 by default
        uint64_t max_size = limits{}.max_size;    // extract: the files' bytes together past which nothing is written (errc::too_large); 1 GiB, 0: none
    };

    namespace detail {
        inline async::task<expected<void, error>> tar_extract_task(string archive_path, string directory, uint64_t max) {
            co_return co_await async::spawn_blocking([archive_path, directory, max] { return compress::detail::tar_extract(archive_path, directory, max); });
        }

        inline async::task<expected<void, error>> tar_create_task(string directory, string archive_path, compress::level l) {
            co_return co_await async::spawn_blocking([directory, archive_path, l] { return compress::detail::tar_create(directory, archive_path, l); });
        }
    }

    // The archive unpacked under the directory: tar::extract("site.tar.gz",
    // "site"). gzip, xz and bzip2 around the archive are read by its first
    // bytes, whatever its name. Every name is checked before anything is
    // written: one that would leave the directory, or a link whose target
    // would, is errc::insecure_path and nothing is written; so is a total
    // past options::max_size (errc::too_large; 1 GiB by default, as
    // decompress has it: an archive comes from outside, tar -x has no
    // bound, this one has). Directories, files with their mode and time,
    // symbolic and hard links (made last); a device or a fifo is left out.
    // A file there already is written over.
    inline expected<void, error> extract(const string& archive_path, const string& directory, const options& o = {}) {
        return compress::detail::tar_extract(archive_path, directory, o.max_size);
    }

    // The same in a task, on the blocking pool
    inline async::task<expected<void, error>> async_extract(string archive_path, string directory, options o = {}) {
        return detail::tar_extract_task(std::move(archive_path), std::move(directory), o.max_size);
    }

    // The directory packed into the archive: tar::create("site",
    // "site.tar.gz"). The name says what wraps the archive: .tar.gz and
    // .tgz gzip, .tar.xz and .txz xz (at options::level), anything else
    // none (.tar.bz2 is errc::unsupported: bzip2 is read, not written).
    // The entries are named from the directory ("index.html", "css/",
    // "css/site.css"), as Go's AddFS names them, in lexical order, with
    // their mode and time; symbolic links as links, not followed; a
    // socket, a device or a fifo left out. A failure removes the file.
    inline expected<void, error> create(const string& directory, const string& archive_path, const options& o = {}) {
        return compress::detail::tar_create(directory, archive_path, o.level);
    }

    inline async::task<expected<void, error>> async_create(string directory, string archive_path, options o = {}) {
        return detail::tar_create_task(std::move(directory), std::move(archive_path), o.level);
    }
}
