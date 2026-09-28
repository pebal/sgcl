//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "flate.h"
#include "detail/path.h"
#include "detail/source.h"
#include "../core/utf8.h"
#include "../io/file.h"
#include "../time/date.h"

#include <sys/stat.h>

namespace sgcl::compress::zip {
    using error = compress::error;

    // How an entry's data is kept (APPNOTE 4.4.5): the values of the
    // format, of which the library reads and writes the first two and
    // reads Deflate64 (PKWARE's enhanced deflate, as 7-Zip writes it)
    enum class method : uint16_t {
        store = 0,
        deflate = 8,
        deflate64 = 9
    };

    // One entry of an archive: what its central directory record says
    struct entry {
        string name;                  // "dir/file.txt", '/' between the parts; a directory ends in '/'
        string comment;
        time::datetime modified;      // the extended timestamp when there is one, else the DOS fields read as UTC (as Go reads them); written in its own zone
        uint64_t size = 0;            // decompressed
        uint64_t compressed_size = 0;
        uint32_t crc32 = 0;
        zip::method method = zip::method::deflate;
        io::permissions mode = io::permissions(0644);
        bool symlink = false;         // the data is the target (Unix attributes)
        vector<byte> extra;           // the central record's extra field, as it is
        uint64_t offset = 0;          // where its local header starts in the archive (read)

        bool is_directory() const noexcept {
            return !name.empty() && name.view().back() == '/';
        }

        bool is_symlink() const noexcept {
            return symlink;
        }

        // Whether the name may be joined to a directory without leaving
        // it, as Go's filepath.IsLocal (the rule tar's entries use too):
        // not empty, not absolute, no NUL, no '\' (APPNOTE's names use
        // '/'), no ".." that climbs above the start. The only check
        // between an archive from outside and the file system until an
        // extract comes.
        bool is_local() const noexcept {
            return compress::detail::is_local_path(name.view());
        }
    };
}

namespace sgcl::compress::zip::detail {
    using namespace sgcl::compress::detail;

    inline constexpr uint32_t LocalSignature = 0x04034b50;
    inline constexpr uint32_t CentralSignature = 0x02014b50;
    inline constexpr uint32_t EndSignature = 0x06054b50;
    inline constexpr uint32_t End64Signature = 0x06064b50;
    inline constexpr uint32_t Locator64Signature = 0x07064b50;
    inline constexpr uint32_t DescriptorSignature = 0x08074b50;
    inline constexpr size_t EndSize = 22;
    inline constexpr size_t MaxComment = 65535;

    inline uint16_t le16(const uint8_t* p) noexcept {
        return uint16_t(p[0] | p[1] << 8);
    }

    inline uint64_t le64(const uint8_t* p) noexcept {
        return uint64_t(le32(p)) | uint64_t(le32(p + 4)) << 32;
    }

    inline void put16(std::vector<uint8_t>& o, uint32_t v) {
        o.push_back(uint8_t(v));
        o.push_back(uint8_t(v >> 8));
    }

    inline void put64(std::vector<uint8_t>& o, uint64_t v) {
        put_le32(o, uint32_t(v));
        put_le32(o, uint32_t(v >> 32));
    }

    // The upper half of code page 437, for names without the UTF-8 flag
    // that are not UTF-8 either (APPNOTE appendix D)
    inline constexpr uint16_t Cp437[128] = {
        0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
        0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
        0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
        0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
        0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
        0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
        0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
        0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0};

    // UTF-8 as core has it: overlongs and surrogates are not UTF-8 (a
    // name with them goes the way of code page 437)
    inline bool valid_utf8(const uint8_t* p, size_t n) noexcept {
        return sgcl::utf8::valid(std::string_view(reinterpret_cast<const char*>(p), n));
    }

    // A name or a comment: UTF-8 as it is (flagged, or valid anyway, as
    // most writers make it without the flag), else code page 437
    inline string name_of(const uint8_t* p, size_t n, bool utf8) {
        if (utf8 || valid_utf8(p, n)) {
            return string(std::string_view(reinterpret_cast<const char*>(p), n));
        }
        std::string s;
        for (size_t i = 0; i < n; ++i) {
            uint32_t c = p[i] < 0x80 ? p[i] : Cp437[p[i] - 0x80];
            if (c < 0x80) {
                s += char(c);
            } else if (c < 0x800) {
                s += char(0xC0 | (c >> 6));
                s += char(0x80 | (c & 0x3F));
            } else {
                s += char(0xE0 | (c >> 12));
                s += char(0x80 | ((c >> 6) & 0x3F));
                s += char(0x80 | (c & 0x3F));
            }
        }
        return string(s);
    }

    // The DOS date and time (APPNOTE 4.4.6), which name no zone: read as
    // UTC, as Go reads them
    inline time::datetime from_dos(uint16_t date, uint16_t tm) {
        // the fields as they are, normalized as Go's time.Date does: a
        // month or a day of 0 is the one before (0/0/1980 is 1979-11-30)
        int64_t months = int64_t(1980 + (date >> 9)) * 12 + ((date >> 5) & 15) - 1;
        int64_t year = months >= 0 ? months / 12 : (months - 11) / 12;
        unsigned month = unsigned(months - year * 12) + 1;
        int64_t days = time::detail::days_from_civil(year, month) + int64_t(date & 31) - 1;
        int64_t seconds = days * 86400 + int64_t(tm >> 11) * 3600 + int64_t((tm >> 5) & 63) * 60 + int64_t(tm & 31) * 2;
        return time::datetime::from_unix(seconds, time::zone::utc());
    }

    inline pair<uint16_t, uint16_t> to_dos(const time::datetime& t) noexcept {
        int year = std::clamp(t.year(), 1980, 2107);
        uint16_t date = uint16_t((year - 1980) << 9 | int(t.month()) << 5 | t.day());
        uint16_t tm = uint16_t(t.hour() << 11 | t.minute() << 5 | t.second() / 2);
        if (t.year() < 1980) {
            date = uint16_t(0 << 9 | 1 << 5 | 1);
            tm = 0;
        }
        return {date, tm};
    }

    // Opening an archive, as the reads it asks for: the tail (the end of
    // central directory record and its comment), maybe the ZIP64 record,
    // and the central directory. A driver (a thread's or a task's) does
    // the reads; this parses what they bring.
    class Opener {
    public:
        struct Read {
            uint64_t offset;
            size_t size;
        };

        explicit Opener(uint64_t size) noexcept
        : _size(size) {
        }

        // The next read, or nullopt when done (the entries, or an error)
        optional<Read> next() const noexcept {
            switch (_step) {
                case Step::tail: {
                    size_t n = size_t(std::min<uint64_t>(_size, EndSize + MaxComment + 20));
                    return Read{_size - n, n};
                }
                case Step::end64:
                    return Read{_end64_offset, 56};
                case Step::directory:
                case Step::directory_retry: {
                    // the directory in pieces, never the whole of it in one buffer
                    uint64_t start = _base + _cd_offset + _dir_read;
                    return Read{start, size_t(std::min<uint64_t>(_directory_end - start, DirectoryPiece))};
                }
                default:
                    return nullopt;
            }
        }

        void feed(const uint8_t* p, size_t n) {
            const Bytes b{p, n};
            switch (_step) {
                case Step::tail:
                    _tail(b);
                    break;
                case Step::end64:
                    _end64(b);
                    break;
                case Step::directory:
                case Step::directory_retry:
                    _directory(b);
                    break;
                default:
                    break;
            }
        }

        const optional<error>& failure() const noexcept {
            return _error;
        }

        vector<entry>& entries() noexcept {
            return _entries;
        }

        const string& comment() const noexcept {
            return _comment;
        }

    private:
        // What a read brought
        struct Bytes {
            const uint8_t* p;
            size_t n;

            const uint8_t* data() const noexcept {
                return p;
            }

            size_t size() const noexcept {
                return n;
            }

            bool empty() const noexcept {
                return n == 0;
            }

            const uint8_t* begin() const noexcept {
                return p;
            }

            const uint8_t* end() const noexcept {
                return p + n;
            }
        };

        enum class Step : uint8_t {
            tail,
            end64,
            directory,
            directory_retry,
            done
        };

        void _fail(errc c, uint64_t at, const char* text) {
            _error = error(c, at, string(text));
            _step = Step::done;
        }

        void _tail(const Bytes& b) {
            uint64_t tail_at = _size - b.size();
            if (b.size() < EndSize) {
                return _fail(errc::invalid_header, 0, "zip: not a zip file");
            }
            // the last signature; its comment must fit in the file (a
            // truncated comment is a truncated archive, as Go has it)
            size_t found = SIZE_MAX;
            for (size_t i = b.size() - EndSize + 1; i-- > 0;) {
                if (le32(b.data() + i) == EndSignature) {
                    found = i;
                    break;
                }
            }
            if (found == SIZE_MAX) {
                return _fail(errc::invalid_header, 0, "zip: not a zip file (no end of central directory)");
            }
            if (found + EndSize + le16(b.data() + found + 20) > b.size()) {
                return _fail(errc::invalid_header, tail_at + found, "zip: comment past the end of the file");
            }
            const uint8_t* e = b.data() + found;
            _end_at = tail_at + found;
            _count = le16(e + 10);
            _cd_size = le32(e + 12);
            _cd_offset = le32(e + 16);
            size_t clen = le16(e + 20);
            _comment = name_of(e + 22, clen, true);
            // a ZIP64 locator right before it
            if (found >= 20 && le32(b.data() + found - 20) == Locator64Signature) {
                _end64_offset = le64(b.data() + found - 20 + 8);
                if (_end_at < 56 || _end64_offset > _end_at - 56) {
                    return _fail(errc::invalid_header, _end_at, "zip: ZIP64 end record past the end record");
                }
                _step = Step::end64;
                return;
            }
            _directory_at(_end_at);
        }

        void _end64(const Bytes& b) {
            if (b.size() < 56 || le32(b.data()) != End64Signature) {
                return _fail(errc::invalid_header, _end64_offset, "zip: invalid ZIP64 end of central directory");
            }
            _count = le64(b.data() + 32);
            _cd_size = le64(b.data() + 40);
            _cd_offset = le64(b.data() + 48);
            _directory_at(_end64_offset);
        }

        // Where the directory ends and what it says fix the base: the
        // bytes an archive has in front of it (a self-extractor's)
        void _directory_at(uint64_t directory_end) {
            if (_cd_size > directory_end || _cd_offset > directory_end - _cd_size) {
                return _fail(errc::invalid_header, directory_end, "zip: central directory past its end record");
            }
            // A base read from the sizes may be wrong: when the directory's
            // offset alone finds a record, that is taken first (base 0),
            // as Go does; the base the sizes give is the second try
            _computed_base = directory_end - _cd_size - _cd_offset;
            _base = 0;
            _directory_end = directory_end;
            _step = Step::directory;
        }

        // The records from the directory's start to its end record, as many
        // as there are (Go's reading, which a wrong size or base in the end
        // record does not stop), read in pieces: a record cut between two is
        // kept for the next, and the reading stops at the first bytes that
        // are no record. The count is checked at the end; nothing is
        // reserved past what the directory's bytes can hold (46 a record).
        void _directory(const Bytes& b) {
            if (_dir_read == 0) {
                if (!b.empty() && (b.size() < 4 || le32(b.data()) != CentralSignature)) {
                    if (_step == Step::directory && _computed_base != 0) {
                        // no record at the offset itself: the archive has bytes in front of it
                        _base = _computed_base;
                        _step = Step::directory_retry;
                        _dir_read = 0;
                        return;
                    }
                    return _fail(errc::invalid_header, _base + _cd_offset, "zip: no central directory where the end record points");
                }
                _entries.clear();
                _entries.reserve(size_t(std::min<uint64_t>(_count, (_directory_end - _base - _cd_offset) / 46)));
            }
            _dir_read += b.size();
            _pending.insert(_pending.end(), b.begin(), b.end());
            const bool more = !b.empty() && _base + _cd_offset + _dir_read < _directory_end;
            size_t at = 0;
            bool stopped = false;
            while (at + 46 <= _pending.size()) {
                const uint8_t* r = _pending.data() + at;
                if (le32(r) != CentralSignature) {
                    stopped = true;
                    break;
                }
                size_t nlen = le16(r + 28), xlen = le16(r + 30), clen = le16(r + 32);
                if (at + 46 + nlen + xlen + clen > _pending.size()) {
                    if (more) {
                        break;   // the rest of it comes with the next piece
                    }
                    return _fail(errc::invalid_header, _record_offset(at), "zip: central record past the directory");
                }
                if (!_record(r, nlen, xlen, clen, _record_offset(at))) {
                    return;
                }
                at += 46 + nlen + xlen + clen;
            }
            if (!stopped && at + 4 <= _pending.size() && le32(_pending.data() + at) != CentralSignature) {
                stopped = true;
            }
            _parsed += at;
            _pending.erase(_pending.begin(), _pending.begin() + std::ptrdiff_t(at));
            if (more && !stopped) {
                return;
            }
            if (uint16_t(_entries.size()) != uint16_t(_count)) {   // compared in 16 bits, as Go does
                return _fail(errc::invalid_header, _record_offset(0), "zip: fewer central records than the end record counts");
            }
            _pending.clear();
            _pending.shrink_to_fit();
            _step = Step::done;
        }

        uint64_t _record_offset(size_t at) const noexcept {
            return _base + _cd_offset + _parsed + at;
        }

        // One central record into an entry
        bool _record(const uint8_t* r, size_t nlen, size_t xlen, size_t clen, uint64_t at) {
            entry e;
            uint16_t made_by = le16(r + 4);
            uint16_t flags = le16(r + 8);
            e.method = zip::method(le16(r + 10));
            uint16_t tm = le16(r + 12), date = le16(r + 14);
            e.crc32 = le32(r + 16);
            e.compressed_size = le32(r + 20);
            e.size = le32(r + 24);
            uint32_t external = le32(r + 38);
            e.offset = le32(r + 42);
            const uint8_t* name = r + 46;
            e.name = name_of(name, nlen, flags & 0x800);
            const uint8_t* x = name + nlen;
            e.extra = to_vector(x, xlen);
            e.comment = name_of(x + xlen, clen, flags & 0x800);
            bool have_time = false;
            if (!_extras(e, x, xlen, have_time, at)) {
                return false;
            }
            if (!have_time) {
                e.modified = from_dos(date, tm);
            }
            uint8_t creator = uint8_t(made_by >> 8);
            if (creator == 3 || creator == 19) {
                // Unix and macOS: the mode and the type in the upper half of the attributes
                uint32_t mode = external >> 16;
                e.mode = io::permissions(mode & 07777);
                e.symlink = (mode & S_IFMT) == S_IFLNK;
            } else if (e.is_directory()) {
                e.mode = io::permissions(0755);
            }
            e.offset += _base;
            _entries.push_back(std::move(e));
            return true;
        }

        // ZIP64 sizes and offset (in that order, those whose field is all
        // ones), and the time from the fields that carry one (the last of
        // them wins, as in Go)
        bool _extras(entry& e, const uint8_t* x, size_t n, bool& have_time, uint64_t at) {
            size_t i = 0;
            while (i + 4 <= n) {
                uint16_t id = le16(x + i), len = le16(x + i + 2);
                const uint8_t* d = x + i + 4;
                if (i + 4 + len > n) {
                    break;   // a torn last field is left alone, as Go does
                }
                if (id == 0x0001) {
                    size_t k = 0;
                    auto take = [&](uint64_t& field) {
                        if (field == 0xFFFFFFFF) {
                            if (k + 8 > len) {
                                return false;
                            }
                            field = le64(d + k);
                            k += 8;
                        }
                        return true;
                    };
                    if (!take(e.size) || !take(e.compressed_size) || !take(e.offset)) {
                        _fail(errc::invalid_header, at, "zip: ZIP64 extra field too short");
                        return false;
                    }
                } else if (id == 0x5455 && len >= 5 && (d[0] & 1)) {
                    // the extended timestamp: the time since 1970
                    e.modified = time::datetime::from_unix(int64_t(le32(d + 1)), time::zone::utc());   // unsigned, as Go reads it
                    have_time = true;
                } else if ((id == 0x000d || id == 0x5855) && len >= 8) {
                    // UNIX and Info-ZIP's Unix field: the access time, then the modification time
                    e.modified = time::datetime::from_unix(int64_t(le32(d + 4)), time::zone::utc());
                    have_time = true;
                } else if (id == 0x000a && len >= 4) {
                    // NTFS: attributes after four reserved bytes; tag 1 holds
                    // the times in 100 ns since 1601, the modification first
                    size_t k = 4;
                    while (k + 4 <= len) {
                        uint16_t tag = le16(d + k), size = le16(d + k + 2);
                        k += 4;
                        if (k + size > len) {
                            break;
                        }
                        if (tag == 1 && size == 24) {
                            uint64_t ticks = le64(d + k);
                            int64_t seconds = int64_t(ticks / 10000000) - 11644473600;
                            int64_t ns = int64_t(ticks % 10000000) * 100;
                            // nanoseconds in 64 bits reach 1678..2262; past that, the seconds alone
                            e.modified = seconds > -9000000000 && seconds < 9000000000
                                ? time::datetime::from_unix_nano(seconds * 1000000000 + ns, time::zone::utc())
                                : time::datetime::from_unix(seconds, time::zone::utc());
                            have_time = true;
                        }
                        k += size;
                    }
                }
                i += 4 + len;
            }
            return true;
        }

        uint64_t _size;
        Step _step = Step::tail;
        uint64_t _end_at = 0;
        uint64_t _end64_offset = 0;
        uint64_t _count = 0;
        uint64_t _cd_size = 0;
        uint64_t _cd_offset = 0;
        uint64_t _base = 0;
        uint64_t _directory_end = 0;
        uint64_t _computed_base = 0;
        uint64_t _dir_read = 0;          // the directory's bytes read so far
        uint64_t _parsed = 0;            // of those, the ones parsed into entries
        std::vector<uint8_t> _pending;   // a record cut between two pieces
        static constexpr size_t DirectoryPiece = size_t(256) << 10;
        string _comment;
        vector<entry> _entries;
        optional<error> _error;
    };

    struct Archive {
        Source source;
        vector<entry> entries;
        string comment;
    };

    // The data of one entry, read: its local header found, the bytes of
    // the entry decompressed, counted and checked against the central
    // record's size and CRC-32 at the end
    class EntryReader final
    : public io::mixin::reader<EntryReader> {
    public:
        EntryReader(const tracked_ptr<Archive>& archive, const entry& e)
        : _archive(archive), _entry(e) {
        }

        // Neither copied nor moved: the inflating reader refers to the
        // range reader beside it
        EntryReader(const EntryReader&) = delete;
        EntryReader& operator=(const EntryReader&) = delete;

        expected<size_t, io::error> read(const slice<byte>& out) {
            if (!_opened) {
                _opened = true;
                uint8_t head[30];
                auto r = _archive->source.read_at(head, 30, _entry.offset);
                if (!r) {
                    return _io_fail(r.error());
                }
                if (auto e = _header(head, *r)) {
                    return io::detail::fail(*e);
                }
                std::vector<uint8_t> name(_name_length);
                auto n = _archive->source.read_at(name.data(), name.size(), _entry.offset + 30);
                if (!n) {
                    return _io_fail(n.error());
                }
                if (auto e = _name(name.data(), name.size(), *n)) {
                    return io::detail::fail(*e);
                }
            }
            if (_error) {
                return io::detail::fail(_io_error());
            }
            auto n = _inflating ? _flate->read(out) : _raw_read(out);
            if (auto e = _took(n, out)) {
                return io::detail::fail(*e);
            }
            return n;
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            if (!_archive->source.file) {
                // bytes in memory: nothing waits and nothing runs on the
                // pool, so the thread's read serves (no managed block, no
                // frame a call); decompressing is work, so every 64 KB the
                // task lets the worker go
                auto n = read(out);
                if (n && (_since_yield += *n) >= YieldEvery) {
                    _since_yield = 0;
                    co_await async::yield();
                }
                co_return n;
            }
            if (!_opened) {
                _opened = true;
                if (auto head = co_await _async_head(); !head) {
                    co_return io::detail::fail(head);
                }
            }
            if (_error) {
                co_return io::detail::fail(_io_error());
            }
            auto n = _inflating ? co_await _flate->async_read(out) : co_await _async_raw_read(out);
            if (auto e = _took(n, out)) {
                co_return io::detail::fail(*e);
            }
            co_return n;
        }

        const optional<error>& last_error() const noexcept {
            return _error;
        }

    private:
        static constexpr size_t HeadBytes = 512;   // the local header and a name of up to 482 bytes; a longer name goes through it in pieces
        using HeadBlock = ByteBlock<HeadBytes>;

        // The local header and its name, read by a task: a file's reads
        // run on the pool, so they go into a managed block of the reader's
        // own (made by the first), which the slices given to them hold
        async::task<expected<void, io::error>> _async_head() {
            if (!_head) {
                _head = make_tracked<HeadBlock>();
            }
            slice<byte> block(tracked_ptr<const void>(_head), _head->data(), HeadBytes);
            auto p = reinterpret_cast<const uint8_t*>(_head->data());
            auto r = co_await _archive->source.async_read_at(block.first(30), _entry.offset);
            if (!r) {
                co_return io::detail::fail(_io_fail(r.error()));
            }
            if (auto e = _header(p, *r)) {
                co_return io::detail::fail(*e);
            }
            const uint64_t at = _entry.offset + 30;
            if (_name_length <= HeadBytes) {
                auto n = co_await _archive->source.async_read_at(block.first(_name_length), at);
                if (!n) {
                    co_return io::detail::fail(_io_fail(n.error()));
                }
                if (auto e = _name(p, _name_length, *n)) {
                    co_return io::detail::fail(*e);
                }
                co_return expected<void, io::error>();
            }
            std::vector<uint8_t> name(_name_length);
            size_t got = 0;
            while (got < name.size()) {
                size_t k = std::min(name.size() - got, HeadBytes);
                auto n = co_await _archive->source.async_read_at(block.first(k), at + got);
                if (!n) {
                    co_return io::detail::fail(_io_fail(n.error()));
                }
                std::memcpy(name.data() + got, p, *n);
                got += *n;
                if (*n < k) {
                    break;
                }
            }
            if (auto e = _name(name.data(), name.size(), got)) {
                co_return io::detail::fail(*e);
            }
            co_return expected<void, io::error>();
        }

        expected<size_t, io::error> _io_fail(const io::error& e) {
            _error = error(e, _entry.offset);
            return io::detail::fail(e);
        }

        optional<io::error> _fail(errc code, uint64_t at, const std::string& text) {
            _error = error(code, at, string("zip: entry " + std::string(_entry.name.view()) + ": " + text));
            return _io_error();
        }

        // What a read through an io handle reports: the error's code, the
        // entry's name as the path ("read dir/a.txt: checksum mismatch")
        io::error _io_error() const {
            if (_error->io_error()) {
                return *_error->io_error();
            }
            return io::error(make_error_code(_error->code()), "read", _entry.name);
        }

        // The local header: where the data starts (its own name and extra
        // lengths; its sizes and CRC are the central record's business)
        optional<io::error> _header(const uint8_t* head, size_t got) {
            if (got < 30 || le32(head) != LocalSignature) {
                return _fail(errc::invalid_header, _entry.offset, "no local header where the directory points");
            }
            if (le16(head + 6) & 1) {
                return _fail(errc::unsupported, _entry.offset, "encrypted");
            }
            _name_length = le16(head + 26);
            _utf8 = le16(head + 6) & 0x800;
            size_t xlen = le16(head + 28);
            _start = _entry.offset + 30 + _name_length + xlen;
            if (_start > _archive->source.size || _entry.compressed_size > _archive->source.size - _start) {
                return _fail(errc::corrupt, _entry.offset, "data past the end of the archive");
            }
            return nullopt;
        }

        // The local name is the central one (in the form both were read),
        // and the data's reader is set up
        optional<io::error> _name(const uint8_t* name, size_t size, size_t got) {
            if (got < size || name_of(name, size, _utf8) != _entry.name) {
                return _fail(errc::corrupt, _entry.offset, "the local name differs from the central one");
            }
            if (_entry.method == method::deflate || _entry.method == method::deflate64) {
                _inflating = true;
                _range.emplace(_archive, _start, _entry.compressed_size);
                _flate.emplace(io::reader(*_range));   // a member, referred to: no object of its own
                if (_entry.method == method::deflate64) {
                    use_deflate64(*_flate);
                }
                if (_entry.compressed_size <= 8192) {
                    // a small entry's input: the block of 1 or 8 KB that holds it, not 16 KB
                    size_input(*_flate, _entry.compressed_size <= 1024 ? 1024 : 8192);
                }
            } else if (_entry.method != method::store) {
                return _fail(errc::unsupported, _entry.offset, "method " + std::to_string(unsigned(_entry.method)));
            } else if (_entry.compressed_size != _entry.size) {
                return _fail(errc::corrupt, _entry.offset, "a stored entry whose sizes differ");
            }
            return nullopt;
        }

        // bytes of the data as they are (store)
        expected<size_t, io::error> _raw_read(const slice<byte>& out) {
            size_t n = size_t(std::min<uint64_t>(out.size(), _entry.compressed_size - _raw));
            auto r = _archive->source.read_at(reinterpret_cast<uint8_t*>(out.data()), n, _start + _raw);
            if (r) {
                _raw += *r;
                if (*r < n) {
                    return io::detail::fail(*_fail(errc::unexpected_end, _start + _raw, "data cut short"));
                }
            }
            return r;
        }

        async::task<expected<size_t, io::error>> _async_raw_read(slice<byte> out) {
            size_t n = size_t(std::min<uint64_t>(out.size(), _entry.compressed_size - _raw));
            auto r = co_await _archive->source.async_read_at(out.first(n), _start + _raw);   // the caller's slice, its owner with it
            if (r) {
                _raw += *r;
                if (*r < n) {
                    co_return io::detail::fail(*_fail(errc::unexpected_end, _start + _raw, "data cut short"));
                }
            }
            co_return r;
        }

        // What a read gave: counted and checksummed, the sizes and the
        // CRC-32 checked at the end
        optional<io::error> _took(const expected<size_t, io::error>& n, const slice<byte>& out) {
            if (!n) {
                if (!_error) {
                    if (_inflating && _flate->last_error()) {
                        _error = *_flate->last_error();
                    } else {
                        _error = error(n.error(), _start);
                    }
                }
                return n.error();
            }
            if (*n) {
                _count += *n;
                if (_count > _entry.size) {
                    return _fail(errc::corrupt, _start, "more data than the directory says");
                }
                _crc.update(out.first(*n));
                return nullopt;
            }
            if (out.empty()) {
                return nullopt;
            }
            if (_count != _entry.size) {
                return _fail(errc::corrupt, _start, "less data than the directory says");
            }
            if (_crc.value() != _entry.crc32) {
                return _fail(errc::checksum, _start, "CRC-32 mismatch");
            }
            return nullopt;
        }

        // The compressed bytes of the entry, for the inflater
        class RangeReader final
        : public io::mixin::reader<RangeReader> {
        public:
            RangeReader(const tracked_ptr<Archive>& a, uint64_t start, uint64_t size)
            : _archive(a), _at(start), _left(size) {
            }

            expected<size_t, io::error> read(const slice<byte>& out) {
                size_t n = size_t(std::min<uint64_t>(out.size(), _left));
                auto r = _archive->source.read_at(reinterpret_cast<uint8_t*>(out.data()), n, _at);
                if (r) {
                    _at += *r;
                    _left -= *r;
                }
                return r;
            }

            async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
                size_t n = size_t(std::min<uint64_t>(out.size(), _left));
                auto r = co_await _archive->source.async_read_at(out.first(n), _at);   // the inflater's managed input, its owner with it
                if (r) {
                    _at += *r;
                    _left -= *r;
                }
                co_return r;
            }

        private:
            tracked_ptr<Archive> _archive;
            uint64_t _at;
            uint64_t _left;
        };

        tracked_ptr<Archive> _archive;
        entry _entry;
        tracked_ptr<HeadBlock> _head;   // a task's read of the local header and the name (made by the first)
        optional<RangeReader> _range;   // before _flate, which reads through it
        optional<flate::reader> _flate;   // holds io::reader(*_range), a reference to a member: it never leaves this object (no copy, no move, never returned)
        hash::crc32 _crc;
        uint64_t _start = 0;
        size_t _name_length = 0;
        bool _utf8 = false;
        uint64_t _raw = 0;
        uint64_t _count = 0;
        bool _opened = false;
        bool _inflating = false;
        static constexpr size_t YieldEvery = size_t(64) << 10;
        size_t _since_yield = 0;   // bytes a task's reads of an archive in memory handed out since it last let the worker go
        optional<error> _error;
    };
}

namespace sgcl::compress::zip {
    // An archive to read: its central directory read when it is opened,
    // the entries then open in any order and any number of times at once.
    // A value, copied cheaply (the entries are shared); it lives where a
    // tracked_ptr may.
    class archive {
    public:
        static expected<archive, error> open(const string& path) {
            auto f = io::open(path);
            if (!f) {
                return unexpected<error>(error(f.error(), 0));
            }
            auto a = open(*f);
            if (a) {
                a->_impl->source.owned = true;
            } else {
                (void)f->close();
            }
            return a;
        }

        static async::task<expected<archive, error>> async_open(string path) {
            auto f = co_await io::async_open(path);
            if (!f) {
                co_return unexpected<error>(error(f.error(), 0));
            }
            auto a = co_await async_open(*f);
            if (a) {
                a->_impl->source.owned = true;
            } else {
                (void)f->close();
            }
            co_return a;
        }

        static expected<archive, error> open(const io::file& file) {
            auto st = file.stat();
            if (!st) {
                return unexpected<error>(error(st.error(), 0));
            }
            detail::Source s;
            s.file = file;
            s.size = st->size;
            return _open(s);
        }

        static async::task<expected<archive, error>> async_open(io::file file) {
            auto st = file.stat();
            if (!st) {
                co_return unexpected<error>(error(st.error(), 0));
            }
            detail::Source s;
            s.file = file;
            s.size = st->size;
            detail::Opener o(s.size);
            // the reads run on the pool: into one managed buffer for the
            // open (made anew only when a read needs more), which the
            // slices given to them hold
            vector<byte> b;
            while (auto r = o.next()) {
                if (b.size() < r->size) {
                    b = vector<byte>(r->size);
                }
                auto got = co_await s.async_read_at(b.as_slice(0, r->size), r->offset);
                if (!got) {
                    co_return unexpected<error>(error(got.error(), r->offset));
                }
                o.feed(reinterpret_cast<const uint8_t*>(b.data()), *got);
            }
            co_return _finish(s, o);
        }

        // An archive in memory; a buffer of unmanaged memory is the
        // caller's to keep while the archive is used
        static expected<archive, error> from(const slice<const byte>& data) {
            detail::Source s;
            s.memory = data;
            s.size = data.size();
            return _open(s);
        }

        slice<const entry> entries() const noexcept {
            return _impl->entries.as_slice();
        }

        // The first entry of that name
        optional<entry> find(const string& name) const {
            for (auto& e : _impl->entries) {
                if (e.name == name) {
                    return e;
                }
            }
            return nullopt;
        }

        const string& comment() const noexcept {
            return _impl->comment;
        }

        // A reader of the entry's data, decompressed and checked
        expected<io::reader, error> reader(const entry& e) const {
            return io::reader(make_tracked<detail::EntryReader>(_impl, e));
        }

        expected<io::reader, error> reader(const string& name) const {
            auto e = find(name);
            if (!e) {
                return unexpected<error>(error(errc::invalid_argument, 0, string("zip: no entry " + std::string(name.view()))));
            }
            return reader(*e);
        }

        // The entry's data whole; its size against the limit before anything is read
        expected<vector<byte>, error> read(const entry& e) const {
            return read(e, limits{});
        }

        expected<vector<byte>, error> read(const entry& e, const limits& l) const {
            if (e.size > l.max_size) {
                return unexpected<error>(error(errc::too_large, e.offset, string("zip: entry " + std::string(e.name.view()) + ": larger than the limit")));
            }
            detail::EntryReader r(_impl, e);
            vector<byte> out;
            out.resize(size_t(e.size));
            size_t got = 0;
            std::byte probe[1];
            for (;;) {
                // past the size, a read of one byte more: 0 runs the end's checks
                auto room = got < out.size() ? slice<byte>(out.data() + got, out.size() - got) : slice<byte>(probe, 1);
                auto n = r.read(room);
                if (!n) {
                    return unexpected<error>(r.last_error() ? *r.last_error() : error(n.error(), e.offset));
                }
                if (*n == 0) {
                    break;
                }
                got += *n;
            }
            return out;
        }

        async::task<expected<vector<byte>, error>> async_read(const entry& e) const {
            return async_read(e, limits{});
        }

        async::task<expected<vector<byte>, error>> async_read(entry e, limits l) const {
            if (e.size > l.max_size) {
                co_return unexpected<error>(error(errc::too_large, e.offset, string("zip: entry " + std::string(e.name.view()) + ": larger than the limit")));
            }
            auto r = make_tracked<detail::EntryReader>(_impl, e);
            vector<byte> out;
            out.resize(size_t(e.size));
            size_t got = 0;
            std::byte probe[1];
            for (;;) {
                // the vector's slice holds it (a stored entry's read of a
                // file runs on the pool); the probe past the size is read
                // into by no file (0 bytes are left to read), it only runs
                // the end's checks
                auto room = got < out.size() ? out.as_slice(got) : slice<byte>(probe, 1);
                auto n = co_await r->async_read(room);
                if (!n) {
                    co_return unexpected<error>(r->last_error() ? *r->last_error() : error(n.error(), e.offset));
                }
                if (*n == 0) {
                    break;
                }
                got += *n;
                // the work of decompressing: every 64 KB the task lets the worker go
                if ((got >> 16) != ((got - *n) >> 16)) {
                    co_await async::yield();
                }
            }
            co_return out;
        }

        expected<vector<byte>, error> read(const string& name) const {
            return read(name, limits{});
        }

        expected<vector<byte>, error> read(const string& name, const limits& l) const {
            auto e = find(name);
            if (!e) {
                return unexpected<error>(error(errc::invalid_argument, 0, string("zip: no entry " + std::string(name.view()))));
            }
            return read(*e, l);
        }

        // Closes the file the archive opened itself (from a path); a file
        // given to open() is the caller's to close
        expected<void, error> close() {
            if (_impl->source.owned && _impl->source.file) {
                auto r = _impl->source.file.close();
                if (!r) {
                    return unexpected<error>(error(r.error(), 0));
                }
            }
            return {};
        }

    private:
        explicit archive(const tracked_ptr<detail::Archive>& impl) noexcept
        : _impl(impl) {
        }

        static expected<archive, error> _open(const detail::Source& s) {
            detail::Opener o(s.size);
            while (auto r = o.next()) {
                std::vector<uint8_t> b(r->size);
                auto got = s.read_at(b.data(), b.size(), r->offset);
                if (!got) {
                    return unexpected<error>(error(got.error(), r->offset));
                }
                o.feed(b.data(), *got);
            }
            return _finish(s, o);
        }

        static expected<archive, error> _finish(const detail::Source& s, detail::Opener& o) {
            if (o.failure()) {
                return unexpected<error>(*o.failure());
            }
            auto impl = make_tracked<detail::Archive>();
            impl->source = s;
            impl->entries = std::move(o.entries());
            impl->comment = o.comment();
            return archive(tracked_ptr<detail::Archive>(std::move(impl)));
        }

        tracked_ptr<detail::Archive> _impl;
    };
}

namespace sgcl::compress::zip::detail {
    // The state a writer and the writer of its current entry share
    struct WriterState {
        io::writer out;
        uint64_t offset = 0;              // the bytes written to out
        std::vector<uint8_t> central;     // the central records so far
        uint64_t count = 0;
        string comment;
        bool closed = false;
        optional<io::error> error;        // the first error given, as the entry writers give it: nothing is written after it
        optional<compress::error> failure;   // the same, as the archive's writer gives it
        uint64_t current = 0;             // the number of the entry being written (1 based; 0: none)
        std::vector<uint8_t> head;        // the local header a task's create made, counted in offset, written with the entry's first bytes (one write for both)
        OutputStage stage;                // the bytes of a task's write (a local header, a descriptor, the directory): managed, as the write may run on the pool (block.h)

        // e kept as the first error, unless one is kept already; the one kept
        const io::error& keep(const io::error& e, const compress::error& archive) {
            if (!error) {
                error = e;
                failure = archive;
            }
            return *error;
        }

        const io::error& keep(const io::error& e) {
            return keep(e, compress::error(e, offset));
        }
    };

    // Counts what goes to out, and keeps its first failure: after it
    // nothing more goes to out, and every write gives that failure
    class Sink final
    : public io::mixin::writer<Sink> {
    public:
        explicit Sink(const tracked_ptr<WriterState>& s) noexcept
        : _s(s) {
        }

        expected<size_t, io::error> write(const slice<const byte>& d) {
            if (_s->error) {
                return io::detail::fail(*_s->error);
            }
            if (!_s->head.empty()) {
                // a task's create, a thread's write after it: the header first
                auto h = _s->out.write(view(_s->head));
                _s->head.clear();
                if (!h) {
                    return io::detail::fail(_s->keep(h.error()));
                }
            }
            auto r = _s->out.write(d);
            if (!r) {
                return io::detail::fail(_s->keep(r.error()));
            }
            _s->offset += *r;
            return r;
        }

        // A task's: a waiting local header goes out in the same write, the
        // two staged together, unless the data is large (a stored entry's
        // own bytes, written as they are, after the header)
        async::task<expected<size_t, io::error>> async_write(slice<const byte> d) {
            if (_s->error) {
                co_return io::detail::fail(*_s->error);
            }
            if (_s->head.empty()) {
                co_return co_await _async_put(d, 0);
            }
            if (_s->head.size() + d.size() > HeadWithData) {
                auto h = co_await _async_put(_s->stage.stage(_s->head), _s->head.size());
                if (!h) {
                    co_return h;
                }
                co_return co_await _async_put(d, 0);
            }
            co_return co_await _async_put(_s->stage.stage(_s->head, d), _s->head.size());
        }

        // A task's write of bytes the writer made (a descriptor, the
        // directory), staged with a waiting local header
        async::task<expected<size_t, io::error>> async_write_made(const std::vector<uint8_t>& made) {
            if (_s->error) {
                co_return io::detail::fail(*_s->error);
            }
            co_return co_await _async_put(_s->stage.stage(_s->head, view(made)), _s->head.size());
        }

    private:
        static constexpr size_t HeadWithData = size_t(32) << 10;

        // b written; its first `head` bytes were the waiting header, counted
        // in the offset when it was made. A failure kept, as every one.
        async::task<expected<size_t, io::error>> _async_put(slice<const byte> b, size_t head) {
            auto r = co_await _s->out.async_write(b);
            if (head) {
                _s->head.clear();
            }
            if (!r) {
                co_return io::detail::fail(_s->keep(r.error()));
            }
            size_t n = *r - std::min(*r, head);
            _s->offset += n;
            co_return n;
        }

        tracked_ptr<WriterState> _s;
    };

    // The writer of one entry: its data compressed (or stored) to out,
    // counted and checksummed; its end writes the data descriptor and
    // the central record
    class EntryWriter final
    : public io::mixin::writer<EntryWriter> {
    public:
        using io::mixin::writer<EntryWriter>::write;
        using io::mixin::writer<EntryWriter>::async_write;

        EntryWriter(const tracked_ptr<WriterState>& s, const entry& e, uint64_t number, uint64_t header_offset, bool utf8)
        : _s(s), _sink(s), _entry(e), _number(number), _header(header_offset), _utf8(utf8) {
            if (e.method == method::deflate && !e.is_directory()) {
                _deflate.emplace(io::writer(_sink));   // a member, referred to: no object of its own
            }
        }

        // Neither copied nor moved: the deflating writer refers to the
        // sink beside it
        EntryWriter(const EntryWriter&) = delete;
        EntryWriter& operator=(const EntryWriter&) = delete;

        expected<size_t, io::error> write(const slice<const byte>& d) {
            if (auto e = _check(d)) {
                return io::detail::fail(*e);
            }
            _crc.update(d);
            _size += d.size();
            auto r = _deflate ? _deflate->write(d) : _sink.write(d);
            if (!r) {
                return r;
            }
            return d.size();
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> d) {
            if (auto e = _check(d)) {
                co_return io::detail::fail(*e);
            }
            _crc.update(d);
            _size += d.size();
            auto r = _deflate ? co_await _deflate->async_write(d) : co_await _sink.async_write(d);
            if (!r) {
                co_return r;
            }
            co_return d.size();
        }

        // Ends the entry; optional, as the next create or the writer's
        // close end it. A kept failure of out is the result, a second
        // time too.
        expected<void, io::error> close() {
            if (_done) {
                return _kept();
            }
            _done = true;
            if (_deflate) {
                if (auto r = _deflate->close(); !r) {
                    return r;
                }
            }
            return _end();
        }

        async::task<expected<void, io::error>> async_close() {
            if (_done) {
                co_return _kept();
            }
            _done = true;
            if (_deflate) {
                if (auto r = co_await _deflate->async_close(); !r) {
                    co_return r;
                }
            }
            auto d = _finish();
            auto r = co_await _sink.async_write_made(d);
            if (!r) {
                co_return io::detail::fail(r);
            }
            co_return expected<void, io::error>();
        }

    private:
        expected<void, io::error> _kept() const {
            if (_s->error) {
                return io::detail::fail(*_s->error);
            }
            return {};
        }

        optional<io::error> _check(const slice<const byte>& d) {
            if (_s->error) {
                return _s->error;
            }
            if (_done || _s->current != _number) {
                return _s->keep(io::error(io::errc::closed, "write", "zip entry"));
            }
            if (!d.empty() && _entry.is_directory()) {
                return _s->keep(io::error(make_error_code(errc::invalid_argument), "write", "zip directory entry"),
                                compress::error(errc::invalid_argument, _s->offset, string("zip: entry " + std::string(_entry.name.view()) + ": data for a directory")));
            }
            return nullopt;
        }

        // the descriptor after the data, and the central record kept
        expected<void, io::error> _end() {
            auto d = _finish();
            auto r = _sink.write(view(d));
            if (!r) {
                return io::detail::fail(r);
            }
            return {};
        }

        std::vector<uint8_t> _finish() {
            uint64_t data_end = _s->offset;
            uint64_t csize = data_end - _data_start();
            bool zip64 = _size >= 0xFFFFFFFF || csize >= 0xFFFFFFFF;
            std::vector<uint8_t> d;
            put_le32(d, DescriptorSignature);
            put_le32(d, _crc.value());
            if (zip64) {
                put64(d, csize);
                put64(d, _size);
            } else {
                put_le32(d, uint32_t(csize));
                put_le32(d, uint32_t(_size));
            }
            _central(csize);
            _s->current = 0;
            return d;
        }

        uint64_t _data_start() const noexcept {
            return _header + 30 + _entry.name.size() + (_timestamp() ? 9 : 0);
        }

        // the extended timestamp, unsigned as Go reads it: 1970..2106
        bool _timestamp() const noexcept {
            int64_t t = _entry.modified.unix();
            return t >= 0 && t <= int64_t(UINT32_MAX);
        }

        void _central(uint64_t csize) {
            bool big_size = _size >= 0xFFFFFFFF, big_csize = csize >= 0xFFFFFFFF, big_offset = _header >= 0xFFFFFFFF;
            bool zip64 = big_size || big_csize || big_offset;
            auto& o = _s->central;
            auto [date, tm] = to_dos(_entry.modified);
            uint32_t mode = uint32_t(_entry.mode) | (_entry.is_directory() ? S_IFDIR : _entry.symlink ? S_IFLNK : S_IFREG);
            put_le32(o, CentralSignature);
            put16(o, (3u << 8) | (zip64 ? 45 : 20));
            put16(o, zip64 ? 45 : 20);
            put16(o, 0x08 | (_utf8 ? 0x800 : 0));
            put16(o, uint16_t(_deflate ? method::deflate : method::store));
            put16(o, tm);
            put16(o, date);
            put_le32(o, _crc.value());
            put_le32(o, big_csize ? 0xFFFFFFFF : uint32_t(csize));
            put_le32(o, big_size ? 0xFFFFFFFF : uint32_t(_size));
            put16(o, uint32_t(_entry.name.size()));
            size_t z64 = (big_size ? 8 : 0) + (big_csize ? 8 : 0) + (big_offset ? 8 : 0);
            size_t xlen = (zip64 ? 4 + z64 : 0) + (_timestamp() ? 9 : 0);
            put16(o, uint32_t(xlen));
            put16(o, uint32_t(_entry.comment.size()));
            put16(o, 0);
            put16(o, 0);
            // the Unix mode in the upper half, and in the low byte the DOS
            // bits Go and Info-ZIP set: 0x10 a directory, 0x01 read-only
            uint32_t dos = (_entry.is_directory() ? 0x10u : 0u) | ((uint32_t(_entry.mode) & 0222) == 0 ? 0x01u : 0u);
            put_le32(o, mode << 16 | dos);
            put_le32(o, big_offset ? 0xFFFFFFFF : uint32_t(_header));
            auto n = _entry.name.view();
            o.insert(o.end(), n.begin(), n.end());
            if (zip64) {
                put16(o, 0x0001);
                put16(o, uint32_t(z64));
                if (big_size) put64(o, _size);
                if (big_csize) put64(o, csize);
                if (big_offset) put64(o, _header);
            }
            if (_timestamp()) {
                put16(o, 0x5455);
                put16(o, 5);
                o.push_back(1);
                put_le32(o, uint32_t(_entry.modified.unix()));
            }
            auto c = _entry.comment.view();
            o.insert(o.end(), c.begin(), c.end());
            ++_s->count;
        }

        tracked_ptr<WriterState> _s;
        Sink _sink;   // before _deflate, which writes through it
        entry _entry;
        uint64_t _number;
        uint64_t _header;
        bool _utf8;
        optional<flate::writer> _deflate;   // holds io::writer(_sink), a reference to a member: it never leaves this object (no copy, no move, never returned)
        hash::crc32 _crc;
        uint64_t _size = 0;
        bool _done = false;
    };
}

namespace sgcl::compress::zip {
    // An archive written entry after entry, as Go's zip.Writer: create()
    // gives a writer of the entry's data, which the next create() (or an
    // entry writer's close(), or the writer's close()) ends; add() writes
    // a whole entry. close() writes the central directory (ZIP64 when the
    // entries, the sizes or the offsets need it) and leaves out open. The
    // local headers carry no sizes (the data descriptor after the data
    // has them), so out need not seek. The first failure of out is kept:
    // every write, create, add and close after it gives it at once and
    // writes nothing (create still gives an entry writer, whose writes
    // give it), so an archive may be written freely and checked once, at
    // the close; last_error() holds it.
    class writer {
    public:
        explicit writer(const io::writer& out)
        : _s(make_tracked<detail::WriterState>()) {
            _s->out = out;
        }

        // Moved, not copied: a copy would share the archive's state and
        // not its current entry
        writer(const writer&) = delete;
        writer& operator=(const writer&) = delete;
        writer(writer&&) noexcept = default;
        writer& operator=(writer&&) noexcept = default;

        // A deflated entry named `name`, modified now
        expected<io::writer, error> create(const string& name) {
            entry e;
            e.name = name;
            e.modified = time::now();
            if (e.is_directory()) {
                e.method = method::store;
                e.mode = io::permissions(0755);
            }
            return create(e);
        }

        // With the method, the time, the mode, the comment of e. An error
        // is the entry's own (a name too long, a method not written) or a
        // create after close, and is kept as every error; once an error is
        // kept (of out, or the caller's), create gives an entry writer
        // whose writes give it, so a program that writes freely and checks
        // at the close finds it there
        expected<io::writer, error> create(const entry& e) {
            _end_current();   // an error kept
            return _create(e, false);
        }

        async::task<expected<io::writer, error>> async_create(entry e) {
            co_await _async_end_current();   // an error kept
            co_return _create(e, true);
        }

        // A whole entry
        expected<void, error> add(const string& name, const slice<const byte>& data) {
            auto w = create(name);
            if (!w) {
                return unexpected<error>(w.error());
            }
            if (auto r = w->write(data); !r) {
                return _kept();
            }
            return _end_current();
        }

        async::task<expected<void, error>> async_add(string name, slice<const byte> data) {
            entry e;
            e.name = name;
            e.modified = time::now();
            if (e.is_directory()) {
                e.method = method::store;
                e.mode = io::permissions(0755);
            }
            auto w = co_await async_create(e);
            if (!w) {
                co_return unexpected<error>(w.error());
            }
            if (auto r = co_await w->async_write(data); !r) {
                co_return _kept();
            }
            co_return co_await _async_end_current();
        }

        expected<void, error> set_comment(const string& comment) {
            if (_s->failure) {
                return _kept();
            }
            if (comment.size() > detail::MaxComment) {
                _s->keep(io::error(make_error_code(errc::invalid_argument), "set_comment", "zip"),
                         error(errc::invalid_argument, 0, string("zip: comment longer than 65535 bytes")));
                return _kept();
            }
            _s->comment = comment;
            return {};
        }

        // The current entry ended, the central directory written; out
        // stays open. The kept error is the result, of this close and of
        // every one after it.
        expected<void, error> close() {
            if (!_s->closed) {
                _end_current();
                _s->closed = true;
                auto tail = _directory();
                detail::Sink(_s).write(detail::view(tail));
            }
            return _kept();
        }

        async::task<expected<void, error>> async_close() {
            if (!_s->closed) {
                co_await _async_end_current();
                _s->closed = true;
                auto tail = _directory();
                co_await detail::Sink(_s).async_write_made(tail);
            }
            co_return _kept();
        }

        // The first error the writer gave (a failure of out, or the
        // caller's), kept (as the reader's last_error): what every
        // operation after it gives
        const optional<error>& last_error() const noexcept {
            return _s->failure;
        }

    private:
        expected<void, error> _kept() const {
            if (_s->failure) {
                return unexpected<error>(*_s->failure);
            }
            return {};
        }

        // An entry's writer, or the entry's own error kept
        expected<io::writer, error> _create(const entry& e, bool task) {
            if (!_s->failure) {
                if (auto r = _check(e); !r) {
                    _s->keep(io::error(make_error_code(r.error().code()), "create", e.name), r.error());
                    return unexpected<error>(r.error());
                }
            }
            return io::writer(_begin(e, task));
        }

        expected<void, error> _check(const entry& e) {
            if (_s->closed) {
                return unexpected<error>(error(errc::invalid_argument, _s->offset, string("zip: create after close")));
            }
            if (e.name.size() > 65535 || e.comment.size() > 65535) {
                return unexpected<error>(error(errc::invalid_argument, _s->offset, string("zip: name or comment longer than 65535 bytes")));
            }
            if (e.method != method::store && e.method != method::deflate) {
                return unexpected<error>(error(errc::unsupported, _s->offset, string("zip: only store and deflate are written")));
            }
            return {};
        }

        // The entry's local header written (or its failure kept) and its writer
        // (a task's: the header waits for the entry's first bytes, its data
        // or its descriptor, and goes out in one write with them; a failure
        // of that write is kept as every one)
        tracked_ptr<detail::EntryWriter> _begin(const entry& e, bool task) {
            bool utf8 = false;
            for (char c : e.name.view()) {
                utf8 |= uint8_t(c) >= 0x80;
            }
            for (char c : e.comment.view()) {
                utf8 |= uint8_t(c) >= 0x80;
            }
            uint64_t header = _s->offset;
            std::vector<uint8_t> h;
            auto [date, tm] = detail::to_dos(e.modified);
            int64_t t = e.modified.unix();
            bool stamp = t >= 0 && t <= int64_t(UINT32_MAX);
            bool deflate = e.method == method::deflate && !e.is_directory();
            detail::put_le32(h, detail::LocalSignature);
            detail::put16(h, 20);
            detail::put16(h, 0x08 | (utf8 ? 0x800 : 0));
            detail::put16(h, uint16_t(deflate ? method::deflate : method::store));
            detail::put16(h, tm);
            detail::put16(h, date);
            detail::put_le32(h, 0);
            detail::put_le32(h, 0);
            detail::put_le32(h, 0);
            detail::put16(h, uint32_t(e.name.size()));
            detail::put16(h, stamp ? 9 : 0);
            auto n = e.name.view();
            h.insert(h.end(), n.begin(), n.end());
            if (stamp) {
                detail::put16(h, 0x5455);
                detail::put16(h, 5);
                h.push_back(1);
                detail::put_le32(h, uint32_t(t));
            }
            if (!task) {
                detail::Sink(_s).write(detail::view(h));   // a failure kept in _s->error
            } else if (!_s->error) {
                _s->offset += h.size();
                _s->head = std::move(h);
            }
            entry copy = e;
            if (!deflate) {
                copy.method = method::store;
            }
            uint64_t number = ++_number;
            _s->current = number;
            _current = make_tracked<detail::EntryWriter>(_s, copy, number, header, utf8);
            return _current;
        }

        expected<void, error> _end_current() {
            if (!_current) {
                return {};
            }
            auto c = _current;
            _current = nullptr;
            c->close();   // an error kept
            return _kept();
        }

        async::task<expected<void, error>> _async_end_current() {
            if (!_current) {
                co_return expected<void, error>();
            }
            auto c = _current;
            _current = nullptr;
            co_await c->async_close();   // an error kept
            co_return _kept();
        }

        // The central records, and the end records after them
        std::vector<uint8_t> _directory() {
            std::vector<uint8_t> o = std::move(_s->central);
            uint64_t cd_offset = _s->offset;
            uint64_t cd_size = o.size();
            uint64_t count = _s->count;
            bool zip64 = count >= 0xFFFF || cd_size >= 0xFFFFFFFF || cd_offset >= 0xFFFFFFFF;
            if (zip64) {
                uint64_t end64 = cd_offset + cd_size;
                detail::put_le32(o, detail::End64Signature);
                detail::put64(o, 44);
                detail::put16(o, (3u << 8) | 45);
                detail::put16(o, 45);
                detail::put_le32(o, 0);
                detail::put_le32(o, 0);
                detail::put64(o, count);
                detail::put64(o, count);
                detail::put64(o, cd_size);
                detail::put64(o, cd_offset);
                detail::put_le32(o, detail::Locator64Signature);
                detail::put_le32(o, 0);
                detail::put64(o, end64);
                detail::put_le32(o, 1);
            }
            detail::put_le32(o, detail::EndSignature);
            detail::put16(o, 0);
            detail::put16(o, 0);
            detail::put16(o, zip64 ? 0xFFFF : uint32_t(count));
            detail::put16(o, zip64 ? 0xFFFF : uint32_t(count));
            detail::put_le32(o, zip64 ? 0xFFFFFFFF : uint32_t(cd_size));
            detail::put_le32(o, zip64 ? 0xFFFFFFFF : uint32_t(cd_offset));
            detail::put16(o, uint32_t(_s->comment.size()));
            auto c = _s->comment.view();
            o.insert(o.end(), c.begin(), c.end());
            return o;
        }

        tracked_ptr<detail::WriterState> _s;
        tracked_ptr<detail::EntryWriter> _current;
        uint64_t _number = 0;
    };
}
