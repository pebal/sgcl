//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/path.h"
#include "detail/block.h"
#include "detail/sevenzip_folder.h"
#include "detail/sevenzip_writer.h"
#include "../async/blocking.h"
#include "../async/generator.h"
#include "../core/generator.h"
#include "../io/file.h"
#include "../time/datetime.h"

namespace sgcl::compress::sevenzip {
    using error = compress::error;

    // One entry of an archive: what its header says
    struct entry {
        string name;                                  // UTF-8 ("dir/file.txt"; the archive's UTF-16LE converted)
        uint64_t size = 0;                            // decompressed
        bool is_directory = false;
        bool is_anti = false;                         // an anti-item: an update's mark that the name was deleted
        optional<uint32_t> crc;                       // the CRC-32 of the data (none for directories and empty files)
        optional<time::datetime> modified;
        optional<time::datetime> created;
        optional<time::datetime> accessed;
        uint32_t attributes = 0;                      // Windows' in the low 16 bits; with 0x8000, POSIX st_mode in the high 16
        bool encrypted = false;                       // its folder is encrypted (7zAES): its data needs the password
        uint32_t folder = UINT32_MAX;                 // where its data lies (read): the folder, none for an entry without data
        uint64_t offset = 0;                          // and the byte of the folder's output where it starts

        // A symbolic link (POSIX attributes): the data is the target
        bool is_symlink() const noexcept {
            return (attributes & 0x8000) && ((attributes >> 16) & 0170000) == 0120000;
        }

        // Whether the name may be joined to a directory without leaving
        // it, as Go's filepath.IsLocal (the rule of zip and tar): not
        // empty, not absolute, no NUL, no '\' (7z's names use '/'), no ".."
        // that climbs above the start
        bool is_local() const noexcept {
            return compress::detail::is_local_path(name.view());
        }
    };
}

namespace sgcl::compress::sevenzip {
    // The coder of a written archive's folders (BZip2 is read, not written:
    // the module has no compressor of it)
    enum class method : uint8_t {
        lzma2,
        lzma,
        ppmd,
        deflate,
        copy
    };

    // How an archive is written, and what reading one needs (the password,
    // the limits); a reader takes only those two
    struct options {
        sevenzip::method method = sevenzip::method::lzma2;
        compress::level level;       // 0..9; LZMA and LZMA2 as xz's levels, PPMd's order and memory as 7-Zip's
        bool solid = true;           // entries one after another in a folder
        uint64_t solid_block = 0;    // the data a folder takes before a new one starts; 0: 7-Zip's for the settings
        bool auto_filters = true;    // a branch converter or Delta by what the entry's first bytes are (not with Copy)
        optional<string> password;   // 7zAES: to read encrypted entries and headers; to write, every folder encrypted
        bool encrypt_header = true;  // with a password, the header too (the names hidden), as 7-Zip's -mhe=on
        limits limit;                // reading: max_size, max_memory, max_entries
    };

    // What an entry is besides its name and data
    struct entry_info {
        optional<time::datetime> modified;   // none: now
        optional<time::datetime> created;
        optional<time::datetime> accessed;
        optional<io::permissions> mode;      // none: 0644, a directory's 0755, a link's 0777
        bool symlink = false;                // the data is the link's target
        optional<uint32_t> attributes;       // the archive's attributes as they are (Windows' and POSIX's), past mode
    };
}

namespace sgcl::compress::sevenzip::detail {
    using namespace sgcl::compress::detail;
    namespace fmt = sevenzip_format;

    struct Archive {
        Source source;
        fmt::Streams streams;
        vector<entry> entries;
        std::unique_ptr<sevenzip_aes::Keys> keys;   // the password's keys (plain memory, the password zeroed when dropped)
    };

    // A failure of the data of an encrypted folder whose decoders were
    // made (a password given, the properties sound): damaged data and a
    // wrong password decrypt alike, so every failure of the data there is
    // one error, the same words in every path
    inline error password_error(const error& e) {
        switch (e.code()) {
            case errc::corrupt:
            case errc::checksum:
            case errc::unexpected_end:
                return error(errc::wrong_password, e.offset(), string("7z: wrong password"));
            default:
                return e;
        }
    }

    // FILETIME: 100 ns since 1601-01-01 UTC
    inline time::datetime from_filetime(uint64_t t) {
        constexpr int64_t Epoch = 11644473600LL;   // seconds from 1601 to 1970
        int64_t seconds = int64_t(t / 10000000) - Epoch;
        int64_t rest = int64_t(t % 10000000) * 100;
        // within datetime's nanoseconds (1678..2262); past them, whole seconds
        if (seconds > -9000000000LL && seconds < 9000000000LL) {
            return time::datetime::from_unix_nano(seconds * 1000000000LL + rest, time::zone::utc());
        }
        return time::datetime::from_unix(seconds, time::zone::utc());
    }

    // The reads an open asks for, and the parse of what they bring: the
    // signature header, the header, and the streams of a packed header
    class Opener {
    public:
        Opener(uint64_t size, const limits& l, sevenzip_aes::Keys* keys = nullptr) noexcept
        : _size(size), _limits(l), _keys(keys) {
        }

        const optional<error>& failure() const noexcept {
            return _error;
        }

        // The signature header: where the header lies
        bool signature(const uint8_t* p, size_t n) {
            if (n < fmt::SignatureSize) {
                return _fail(errc::unexpected_end, 0, "7z: shorter than the signature header");
            }
            if (std::memcmp(p, fmt::Signature, 6) != 0) {
                return _fail(errc::invalid_header, 0, "7z: not a 7z archive");
            }
            if (p[6] != 0) {
                return _fail(errc::unsupported, 6, "7z: an archive of a later major version");
            }
            if (xz_crc(p + 12, 20) != le32(p + 8)) {
                return _fail(errc::checksum, 8, "7z: signature header CRC-32 mismatch");
            }
            _next_offset = le64(p + 12);
            _next_size = le64(p + 20);
            _next_crc = le32(p + 28);
            if (_next_size == 0) {
                _empty = true;
                return true;
            }
            uint64_t at = fmt::SignatureSize + _next_offset;
            if (_next_offset > _size || at > _size || _next_size > _size - at) {
                return _fail(errc::unexpected_end, 12, "7z: the header lies past the archive's end");
            }
            if (_next_size > MaxHeader) {
                return _fail(errc::too_large, 20, "7z: a header past 64 MiB");
            }
            return true;
        }

        bool empty() const noexcept {
            return _empty;
        }

        uint64_t header_offset() const noexcept {
            return fmt::SignatureSize + _next_offset;
        }

        uint64_t header_size() const noexcept {
            return _next_size;
        }

        // The header's bytes; true when they are the plain header. A packed
        // header leaves its streams in packed(), to be decoded (decode())
        bool header(const uint8_t* p, size_t n, bool& plain) {
            if (n != _next_size) {
                return _fail(errc::unexpected_end, header_offset(), "7z: the archive ends in its header");
            }
            if (xz_crc(p, n) != _next_crc) {
                return _fail(errc::checksum, header_offset(), "7z: header CRC-32 mismatch");
            }
            return _header(p, n, header_offset(), plain);
        }

        // The header a packed one decodes to (from `source`, whose first
        // byte is the archive's byte `base`)
        bool decode(const Source& source, bool& plain, uint64_t base = 0) {
            if (++_rounds > 4) {
                return _fail(errc::corrupt, header_offset(), "7z: a header packed too many times");
            }
            const auto& s = _packed;
            if (s.folders.empty()) {
                return _fail(errc::corrupt, header_offset(), "7z: a packed header without its folder");
            }
            const bool encrypted = s.folders[0].encrypted();
            uint64_t size = s.folders[0].size();
            if (size > MaxHeader) {
                return _fail(errc::too_large, header_offset(), "7z: a packed header past 64 MiB decompressed");
            }
            limits l = _limits;
            fmt::Streams copy = s;
            for (auto& off : copy.pack_offsets) {
                if (off < base) {
                    return _fail(errc::corrupt, header_offset(), "7z: a packed header before its streams");
                }
                off -= base;
            }
            sevenzip_folder::Decoder d(source, copy, 0, l, _keys);
            bool decrypting = false;
            auto failed = [&](const error& e) {
                _error = decrypting ? password_error(e) : e;
                return false;
            };
            if (d.failure()) {
                return failed(d.failure()->to_error());
            }
            decrypting = encrypted;
            std::vector<uint8_t> out(static_cast<size_t>(size));
            size_t got = 0;
            while (got < out.size()) {
                size_t k;
                if (!d.read(out.data() + got, out.size() - got, k)) {
                    return failed(d.failure()->to_error());
                }
                if (k == 0) {
                    return failed(error(errc::corrupt, header_offset(), string("7z: a packed header shorter than its size")));
                }
                got += k;
            }
            if (s.folders[0].has_crc && xz_crc(out.data(), out.size()) != s.folders[0].crc) {
                return failed(error(errc::checksum, header_offset(), string("7z: the packed header's CRC-32 does not match")));
            }
            if (!_header(out.data(), out.size(), header_offset(), plain)) {
                return decrypting ? failed(*_error) : false;
            }
            return true;
        }

        const fmt::Streams& packed() const noexcept {
            return _packed;
        }

        fmt::Streams& streams() noexcept {
            return _streams;
        }

        // The entries of the files read
        bool entries(vector<entry>& out) {
            out.clear();
            out.reserve(_files.size());
            size_t sub = 0;
            size_t folder = 0;
            uint64_t in_folder = 0;   // substreams of the current folder taken
            uint64_t offset = 0;
            auto& s = _streams;
            for (auto& f : _files) {
                entry e;
                auto name = fmt::utf16_to_utf8(f.name);
                if (!name) {
                    return _fail(errc::corrupt, header_offset(), "7z: a name with an unpaired surrogate");
                }
                e.name = string(*name);
                if (f.has_attributes) {
                    e.attributes = f.attributes;
                }
                if (f.has_time[0]) e.created = from_filetime(f.time[0]);
                if (f.has_time[1]) e.accessed = from_filetime(f.time[1]);
                if (f.has_time[2]) e.modified = from_filetime(f.time[2]);
                if (f.empty_stream) {
                    // no data: a directory, unless the header says an empty file
                    e.is_directory = !f.empty_file;
                    e.is_anti = f.anti;
                } else {
                    while (folder < s.folders.size() && in_folder == s.folders[folder].substreams) {
                        ++folder;
                        in_folder = 0;
                        offset = 0;
                    }
                    if (folder == s.folders.size()) {
                        return _fail(errc::corrupt, header_offset(), "7z: the files do not match the streams");
                    }
                    e.folder = uint32_t(folder);
                    e.offset = offset;
                    e.size = s.sub_sizes[sub];
                    if (s.sub_has_crc[sub]) {
                        e.crc = s.sub_crc[sub];
                    }
                    e.encrypted = s.folders[folder].encrypted();
                    offset += e.size;
                    ++in_folder;
                    ++sub;
                    if (f.has_attributes && (f.attributes & 0x10)) {
                        e.is_directory = true;
                    }
                }
                out.push_back(std::move(e));
            }
            return true;
        }

    private:
        static constexpr uint64_t MaxHeader = uint64_t(64) << 20;

        static uint32_t xz_crc(const uint8_t* p, size_t n) noexcept {
            return hash::crc32::of(slice<const byte>(reinterpret_cast<const byte*>(p), n));
        }

        static uint64_t le64(const uint8_t* p) noexcept {
            return uint64_t(le32(p)) | uint64_t(le32(p + 4)) << 32;
        }

        bool _fail(errc code, uint64_t at, const char* text) {
            if (!_error) {
                _error = error(code, at, string(text));
            }
            return false;
        }

        bool _header(const uint8_t* p, size_t n, uint64_t origin, bool& plain) {
            fmt::HeaderReader r(p, n, origin, _limits);
            uint8_t id;
            if (!r.byte(id)) {
                _error = *r.failure();
                return false;
            }
            if (id == fmt::kEncodedHeader) {
                _packed = fmt::Streams();
                if (!r.streams(_packed)) {
                    _error = *r.failure();
                    return false;
                }
                plain = false;
                return true;
            }
            if (id != fmt::kHeader) {
                return _fail(errc::corrupt, origin, "7z: not a header");
            }
            plain = true;
            for (;;) {
                if (!r.byte(id)) {
                    break;
                }
                if (id == fmt::kEnd) {
                    return true;
                }
                if (id == fmt::kArchiveProperties) {
                    for (;;) {
                        uint64_t type, size;
                        if (!r.number(type) || type == 0) {
                            break;
                        }
                        if (!r.number(size) || !r.skip(size)) {
                            break;
                        }
                    }
                } else if (id == fmt::kAdditionalStreamsInfo) {
                    fmt::Streams extra;
                    if (!r.streams(extra)) {
                        break;
                    }
                } else if (id == fmt::kMainStreamsInfo) {
                    if (!r.streams(_streams)) {
                        break;
                    }
                } else if (id == fmt::kFilesInfo) {
                    if (!r.files(_streams, _files)) {
                        break;
                    }
                } else {
                    r.fail(errc::corrupt, "7z: an unexpected property in the header");
                    break;
                }
            }
            _error = r.failure() ? *r.failure() : error(errc::corrupt, origin, string("7z: corrupt header"));
            return false;
        }

        uint64_t _size;
        limits _limits;
        sevenzip_aes::Keys* _keys = nullptr;
        uint64_t _next_offset = 0;
        uint64_t _next_size = 0;
        uint32_t _next_crc = 0;
        bool _empty = false;
        int _rounds = 0;
        fmt::Streams _packed;
        fmt::Streams _streams;
        std::vector<fmt::File> _files;
        optional<error> _error;
    };

    inline std::string entry_text(const entry& e, const char* what) {
        return "7z: entry " + std::string(e.name.view()) + ": " + what;
    }

    // What reads an entry's data from its folder's decoder: a count of the
    // bytes left and the CRC-32 checked at the end
    struct Portion {
        uint64_t left = 0;
        hash::crc32 crc;
        bool checked = false;

        // After the entry's last byte: its CRC
        optional<error> finish(const entry& e) {
            if (checked) {
                return nullopt;
            }
            checked = true;
            if (e.crc && crc.value() != *e.crc) {
                return error(errc::checksum, 0, string(entry_text(e, "checksum mismatch")));
            }
            return nullopt;
        }
    };

    // The data of one entry: its folder decoded from the start, the bytes
    // before the entry dropped
    class EntryReader final
    : public io::mixin::reader<EntryReader> {
    public:
        EntryReader(const tracked_ptr<Archive>& archive, const entry& e, const limits& l)
        : _archive(archive), _entry(e), _limits(l) {
            _portion.left = e.size;
        }

        EntryReader(const EntryReader&) = delete;
        EntryReader& operator=(const EntryReader&) = delete;

        expected<size_t, io::error> read(const slice<byte>& out) {
            if (_error) {
                return io::detail::fail(to_io_error(*_error, "7z"));
            }
            if (!_opened) {
                _opened = true;
                if (_entry.folder != UINT32_MAX) {
                    _decoder = std::make_unique<sevenzip_folder::Decoder>(_archive->source, _archive->streams, _entry.folder, _limits, _archive->keys.get());
                    _decrypting = _entry.encrypted && !_decoder->failure();
                    if (_decoder->failure() || !_decoder->skip(_entry.offset)) {
                        return _fail(_decoder->failure()->to_error());
                    }
                }
            }
            if (_portion.left == 0 || out.empty()) {
                if (auto e = _portion.finish(_entry)) {
                    return _fail(*e);
                }
                return 0;
            }
            size_t got;
            size_t n = size_t(std::min<uint64_t>(out.size(), _portion.left));
            if (!_decoder->read(reinterpret_cast<uint8_t*>(out.data()), n, got)) {
                return _fail(_decoder->failure()->to_error());
            }
            if (got == 0) {
                return _fail(error(errc::corrupt, 0, string(entry_text(_entry, "the folder ends before the entry"))));
            }
            _portion.crc.update(slice<const byte>(out.data(), got));
            _portion.left -= got;
            if (_portion.left == 0) {
                _decoder.reset();   // its plain memory now, not when the collector takes the reader
                if (auto e = _portion.finish(_entry)) {
                    return _fail(*e);
                }
            }
            return got;
        }

        // A task's: an archive in memory decodes on the worker, letting it
        // go every 64 KB; an archive in a file decodes on the blocking pool,
        // where its reads of the file wait (the job holds this reader and
        // the slice of the caller's buffer, so a task let go of meanwhile
        // leaves the job nothing freed)
        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            if (!_archive->source.file) {
                auto n = read(out);
                if (n && (_since_yield += *n) >= YieldEvery) {
                    _since_yield = 0;
                    co_await async::yield();
                }
                co_return n;
            }
            tracked_ptr<EntryReader> self(this);
            co_return co_await async::spawn_blocking([self, out]() {
                return self->read(out);
            });
        }

        const optional<error>& last_error() const noexcept {
            return _error;
        }

    private:
        static constexpr size_t YieldEvery = size_t(64) << 10;

        expected<size_t, io::error> _fail(const error& e) {
            _error = _decrypting ? password_error(e) : e;
            _decoder.reset();
            return io::detail::fail(to_io_error(*_error, "7z"));
        }

        tracked_ptr<Archive> _archive;
        entry _entry;
        limits _limits;
        std::unique_ptr<sevenzip_folder::Decoder> _decoder;
        Portion _portion;
        bool _opened = false;
        bool _decrypting = false;   // an encrypted folder's decoders made: its failures are a wrong password
        size_t _since_yield = 0;
        optional<error> _error;
    };

    // The walk's one decoder per folder, shared by the readers it hands
    // out; a reader serves only while its entry is the current one
    class Walk {
    public:
        explicit Walk(const tracked_ptr<Archive>& a, const limits& l)
        : archive(a), limits_(l) {
        }

        // The decoder brought to entry e: the rest of the entry before it
        // dropped, a new decoder at a new folder
        void advance(const entry& e) {
            ++generation;
            current = e;
            portion = Portion();
            portion.left = e.size;
            if (e.folder == UINT32_MAX) {
                return;
            }
            if (!decoder || folder != e.folder) {
                folder = e.folder;
                decoder = std::make_unique<sevenzip_folder::Decoder>(archive->source, archive->streams, e.folder, limits_, archive->keys.get());
                decrypting = e.encrypted && !decoder->failure();
            }
            if (!decoder->failure() && decoder->done() < e.offset) {
                (void)decoder->skip(e.offset - decoder->done());
            }
        }

        tracked_ptr<Archive> archive;
        limits limits_;
        std::unique_ptr<sevenzip_folder::Decoder> decoder;
        uint32_t folder = UINT32_MAX;
        bool decrypting = false;
        uint64_t generation = 0;
        entry current;
        Portion portion;
    };

    class WalkReader final
    : public io::mixin::reader<WalkReader> {
    public:
        WalkReader(const tracked_ptr<Walk>& walk)
        : _walk(walk), _generation(walk->generation) {
        }

        WalkReader(const WalkReader&) = delete;
        WalkReader& operator=(const WalkReader&) = delete;

        expected<size_t, io::error> read(const slice<byte>& out) {
            if (_error) {
                return io::detail::fail(to_io_error(*_error, "7z"));
            }
            Walk& w = *_walk;
            if (w.generation != _generation) {
                return io::detail::fail(io::error(io::errc::closed, "read", "7z"));   // the walk went on
            }
            const entry& e = w.current;
            if (w.portion.left == 0 || out.empty()) {
                if (auto err = w.portion.finish(e)) {
                    return _fail(*err);
                }
                return 0;
            }
            if (w.decoder->failure()) {
                return _fail(w.decoder->failure()->to_error());
            }
            size_t got;
            size_t n = size_t(std::min<uint64_t>(out.size(), w.portion.left));
            if (!w.decoder->read(reinterpret_cast<uint8_t*>(out.data()), n, got)) {
                return _fail(w.decoder->failure()->to_error());
            }
            if (got == 0) {
                return _fail(error(errc::corrupt, 0, string(entry_text(e, "the folder ends before the entry"))));
            }
            w.portion.crc.update(slice<const byte>(out.data(), got));
            w.portion.left -= got;
            if (w.portion.left == 0) {
                if (auto err = w.portion.finish(e)) {
                    return _fail(*err);
                }
            }
            return got;
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            if (!_walk->archive->source.file) {
                auto n = read(out);
                if (n && (_since_yield += *n) >= (size_t(64) << 10)) {
                    _since_yield = 0;
                    co_await async::yield();
                }
                co_return n;
            }
            tracked_ptr<WalkReader> self(this);
            co_return co_await async::spawn_blocking([self, out]() {
                return self->read(out);
            });
        }

        const optional<error>& last_error() const noexcept {
            return _error;
        }

    private:
        expected<size_t, io::error> _fail(const error& e) {
            _error = _walk->current.encrypted && _walk->decrypting ? password_error(e) : e;
            return io::detail::fail(to_io_error(*_error, "7z"));
        }

        tracked_ptr<Walk> _walk;
        uint64_t _generation;
        size_t _since_yield = 0;
        optional<error> _error;
    };
}

namespace sgcl::compress::sevenzip {
    // An archive to read: its header read when it is opened, the entries
    // then open in any order and any number of times at once. A value,
    // copied cheaply (the entries are shared); it lives where a tracked_ptr
    // may. In a solid archive an entry's data lies inside a folder with
    // the entries before it: reader(e) and read(e) decode the folder from
    // its start, so reading every entry that way costs the square of a
    // folder's size; walk() reads them all in the archive's order with one
    // decoder a folder.
    class archive {
    public:
        static expected<archive, error> open(const string& path) {
            return open(path, limits{});
        }

        static expected<archive, error> open(const string& path, const limits& l) {
            return open(path, _with(l));
        }

        // With a password (encrypted entries and headers, 7zAES) and the
        // limits; the rest of the options is the writer's
        static expected<archive, error> open(const string& path, const options& o) {
            auto f = io::open(path);
            if (!f) {
                return unexpected<error>(error(f.error(), 0));
            }
            auto a = open(*f, o);
            if (a) {
                a->_impl->source.owned = true;
            } else {
                (void)f->close();
            }
            return a;
        }

        static async::task<expected<archive, error>> async_open(string path) {
            return async_open(std::move(path), limits{});
        }

        static async::task<expected<archive, error>> async_open(string path, limits l) {
            return async_open(std::move(path), _with(l));
        }

        static async::task<expected<archive, error>> async_open(string path, options o) {
            auto f = co_await io::async_open(path);
            if (!f) {
                co_return unexpected<error>(error(f.error(), 0));
            }
            auto a = co_await async_open(*f, std::move(o));
            if (a) {
                a->_impl->source.owned = true;
            } else {
                (void)f->close();
            }
            co_return a;
        }

        static expected<archive, error> open(const io::file& file) {
            return open(file, limits{});
        }

        static expected<archive, error> open(const io::file& file, const limits& l) {
            return open(file, _with(l));
        }

        static expected<archive, error> open(const io::file& file, const options& o) {
            auto st = file.stat();
            if (!st) {
                return unexpected<error>(error(st.error(), 0));
            }
            detail::Source s;
            s.file = file;
            s.size = st->size;
            return _open(s, o);
        }

        static async::task<expected<archive, error>> async_open(io::file file) {
            return async_open(std::move(file), limits{});
        }

        // The reads of the signature header and the header into managed
        // memory (they run on the pool, the slices hold it); a packed
        // header's streams read the same way, then decoded from memory
        static async::task<expected<archive, error>> async_open(io::file file, limits l) {
            return async_open(std::move(file), _with(l));
        }

        static async::task<expected<archive, error>> async_open(io::file file, options opt) {
            auto st = file.stat();
            if (!st) {
                co_return unexpected<error>(error(st.error(), 0));
            }
            detail::Source s;
            s.file = file;
            s.size = st->size;
            auto keys = _keys(opt);
            detail::Opener o(s.size, opt.limit, keys.get());
            vector<byte> head(detail::fmt::SignatureSize);
            auto got = co_await s.async_read_at(head.as_slice(), 0);
            if (!got) {
                co_return unexpected<error>(error(got.error(), 0));
            }
            if (!o.signature(reinterpret_cast<const uint8_t*>(head.data()), *got)) {
                co_return unexpected<error>(*o.failure());
            }
            if (!o.empty()) {
                vector<byte> h(size_t(o.header_size()));
                auto hg = co_await s.async_read_at(h.as_slice(), o.header_offset());
                if (!hg) {
                    co_return unexpected<error>(error(hg.error(), o.header_offset()));
                }
                bool plain = false;
                if (!o.header(reinterpret_cast<const uint8_t*>(h.data()), *hg, plain)) {
                    co_return unexpected<error>(*o.failure());
                }
                while (!plain) {
                    // the packed header's streams into memory, at the offsets they have
                    const auto& p = o.packed();
                    uint64_t first = p.pack_offsets.empty() ? 0 : p.pack_offsets.front();
                    uint64_t total = 0;
                    for (auto v : p.pack_sizes) {
                        total += v;
                    }
                    if (first > s.size || total > s.size - first || total > (uint64_t(64) << 20)) {
                        co_return unexpected<error>(error(errc::corrupt, o.header_offset(), string("7z: a packed header past the archive's end")));
                    }
                    vector<byte> packed(static_cast<size_t>(total));
                    auto pg = co_await s.async_read_at(packed.as_slice(), first);
                    if (!pg) {
                        co_return unexpected<error>(error(pg.error(), first));
                    }
                    detail::Source m;
                    m.memory = packed.as_slice();
                    m.size = packed.size();
                    if (!o.decode(m, plain, first)) {
                        co_return unexpected<error>(*o.failure());
                    }
                }
            }
            co_return _finish(s, o, std::move(keys));
        }

        // An archive in memory; a buffer of unmanaged memory is the
        // caller's to keep while the archive is used
        static expected<archive, error> from(const slice<const byte>& data) {
            return from(data, limits{});
        }

        static expected<archive, error> from(const slice<const byte>& data, const limits& l) {
            return from(data, _with(l));
        }

        static expected<archive, error> from(const slice<const byte>& data, const options& o) {
            detail::Source s;
            s.memory = data;
            s.size = data.size();
            return _open(s, o);
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

        // A reader of the entry's data, decompressed and checked (its
        // folder decoded from the start: see the class's note)
        expected<io::reader, error> reader(const entry& e) const {
            return reader(e, limits{});
        }

        expected<io::reader, error> reader(const entry& e, const limits& l) const {
            return io::reader(make_tracked<detail::EntryReader>(_impl, e, l));
        }

        expected<io::reader, error> reader(const string& name) const {
            auto e = find(name);
            if (!e) {
                return unexpected<error>(_missing(name));
            }
            return reader(*e);
        }

        // The entry's data whole; its size against the limit before anything is read
        expected<vector<byte>, error> read(const entry& e) const {
            return read(e, limits{});
        }

        expected<vector<byte>, error> read(const entry& e, const limits& l) const {
            if (e.size > l.max_size) {
                return unexpected<error>(error(errc::too_large, 0, string(detail::entry_text(e, "larger than the limit"))));
            }
            detail::EntryReader r(_impl, e, l);
            vector<byte> out;
            out.resize(size_t(e.size));
            size_t got = 0;
            std::byte probe[1];
            for (;;) {
                auto room = got < out.size() ? slice<byte>(out.data() + got, out.size() - got) : slice<byte>(probe, 1);
                auto n = r.read(room);
                if (!n) {
                    return unexpected<error>(r.last_error() ? *r.last_error() : error(n.error(), 0));
                }
                if (*n == 0) {
                    break;
                }
                got += *n;
            }
            return out;
        }

        expected<vector<byte>, error> read(const string& name) const {
            return read(name, limits{});
        }

        expected<vector<byte>, error> read(const string& name, const limits& l) const {
            auto e = find(name);
            if (!e) {
                return unexpected<error>(_missing(name));
            }
            return read(*e, l);
        }

        // A task's: an archive in a file read on the blocking pool (the job
        // holds the archive and the result), one in memory on the worker
        async::task<expected<vector<byte>, error>> async_read(const entry& e) const {
            return async_read(e, limits{});
        }

        async::task<expected<vector<byte>, error>> async_read(entry e, limits l) const {
            archive self = *this;
            if (!_impl->source.file) {
                co_return self.read(e, l);
            }
            co_return co_await async::spawn_blocking([self, e, l]() {
                return self.read(e, l);
            });
        }

        // Every entry in the archive's order, each with a reader of its
        // data, one decoder a folder: `for (auto& [e, r] : a.walk())`. A
        // reader serves until the walk goes on (then errc::closed); what
        // it has not read of its entry is decoded and dropped then. An
        // entry that cannot be read (encrypted with no password or a wrong
        // one, an unsupported method, a damaged folder) has a reader that
        // fails, and the walk goes on.
        generator<pair<entry, io::reader>> walk() const {
            return _walk(_impl, limits{});
        }

        generator<pair<entry, io::reader>> walk(const limits& l) const {
            return _walk(_impl, l);
        }

        // The same for a task: `while (auto v = co_await g.next())`
        async::generator<pair<entry, io::reader>> async_walk() const {
            return _async_walk(_impl, limits{});
        }

        async::generator<pair<entry, io::reader>> async_walk(const limits& l) const {
            return _async_walk(_impl, l);
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

        static error _missing(const string& name) {
            return error(errc::invalid_argument, 0, string("7z: no entry " + std::string(name.view())));
        }

        static generator<pair<entry, io::reader>> _walk(tracked_ptr<detail::Archive> impl, limits l) {
            tracked_ptr<detail::Walk> w = make_tracked<detail::Walk>(impl, l);
            for (auto& e : impl->entries) {
                w->advance(e);
                co_yield pair<entry, io::reader>(e, io::reader(make_tracked<detail::WalkReader>(w)));
            }
            ++w->generation;
            w->decoder.reset();
        }

        static async::generator<pair<entry, io::reader>> _async_walk(tracked_ptr<detail::Archive> impl, limits l) {
            tracked_ptr<detail::Walk> w = make_tracked<detail::Walk>(impl, l);
            for (size_t i = 0; i < impl->entries.size(); ++i) {
                entry e = impl->entries[i];
                if (impl->source.file) {
                    // the drop of the last entry's rest reads the file: on the pool
                    co_await async::spawn_blocking([w, e]() {
                        w->advance(e);
                        return 0;
                    });
                } else {
                    w->advance(e);
                }
                co_yield pair<entry, io::reader>(e, io::reader(make_tracked<detail::WalkReader>(w)));
            }
            ++w->generation;
            w->decoder.reset();
        }

        static options _with(const limits& l) {
            options o;
            o.limit = l;
            return o;
        }

        // The password's keys, in plain memory (the password's bytes zeroed when they go)
        static std::unique_ptr<detail::sevenzip_aes::Keys> _keys(const options& o) {
            if (!o.password) {
                return nullptr;
            }
            return std::make_unique<detail::sevenzip_aes::Keys>(o.password->view());
        }

        static expected<archive, error> _open(const detail::Source& s, const options& opt) {
            auto keys = _keys(opt);
            detail::Opener o(s.size, opt.limit, keys.get());
            uint8_t head[detail::fmt::SignatureSize];
            auto got = s.read_at(head, sizeof(head), 0);
            if (!got) {
                return unexpected<error>(error(got.error(), 0));
            }
            if (!o.signature(head, *got)) {
                return unexpected<error>(*o.failure());
            }
            if (!o.empty()) {
                std::vector<uint8_t> h(size_t(o.header_size()));
                auto hg = s.read_at(h.data(), h.size(), o.header_offset());
                if (!hg) {
                    return unexpected<error>(error(hg.error(), o.header_offset()));
                }
                bool plain = false;
                if (!o.header(h.data(), *hg, plain)) {
                    return unexpected<error>(*o.failure());
                }
                while (!plain) {
                    if (!o.decode(s, plain)) {
                        return unexpected<error>(*o.failure());
                    }
                }
            }
            return _finish(s, o, std::move(keys));
        }

        static expected<archive, error> _finish(const detail::Source& s, detail::Opener& o, std::unique_ptr<detail::sevenzip_aes::Keys> keys) {
            if (o.failure()) {
                return unexpected<error>(*o.failure());
            }
            auto impl = make_tracked<detail::Archive>();
            impl->source = s;
            if (!o.entries(impl->entries)) {
                return unexpected<error>(*o.failure());
            }
            impl->streams = std::move(o.streams());
            impl->keys = std::move(keys);
            return archive(tracked_ptr<detail::Archive>(std::move(impl)));
        }

        tracked_ptr<detail::Archive> _impl;
    };
}

namespace sgcl::compress::sevenzip::detail {
    namespace writing = sevenzip_writing;

    // Where a written archive goes: a file, written in order and its
    // signature header at its start last (pwrite), or a buffer, sought back
    // to it. The archive starts where the file or the buffer stands.
    struct Sink {
        io::file file;
        io::buffer buffer;                      // when there is no file
        uint64_t base = 0;
        bool owned = false;

        expected<size_t, io::error> write(const slice<const byte>& b) {
            return file ? file.write(b) : buffer.write(b);
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> b) {
            if (file) {
                co_return co_await file.async_write(b);
            }
            co_return buffer.write(b);
        }

        expected<void, io::error> write_start(const std::vector<uint8_t>& s) {
            slice<const byte> b(reinterpret_cast<const byte*>(s.data()), s.size());
            if (file) {
                auto r = file.write_at(b, base);
                if (!r) {
                    return io::detail::fail(r);
                }
                return {};
            }
            auto at = buffer.seek(int64_t(base));
            if (!at) {
                return io::detail::fail(at);
            }
            (void)buffer.write(b);
            (void)buffer.seek(0, io::seek_from::end);
            return {};
        }
    };

    // The state of a writer: its sink, its encoder (plain memory), the
    // records of what was written (std strings in plain memory), the entry
    // and the folder being written, the first error
    class WriterState {
    public:
        static constexpr size_t FlushAt = size_t(256) << 10;
        static constexpr size_t Head = 4096;   // the first bytes an entry's filter is judged by

        WriterState(Sink sink, const options& o)
        : _sink(std::move(sink)), _o(o) {
        }

        optional<error> error_;

        void fail(const error& e) {
            if (!error_) {
                error_ = e;
            }
        }

        bool ok() const noexcept {
            return !error_;
        }

        bool closed = false;
        uint64_t current = 0;   // the id of the entry being written (0: none)

        // The options checked and the encoder made, the signature header's
        // place taken
        void start() {
            int level = _o.level.value();
            if (level < 0) {
                fail(error(errc::invalid_argument, 0, string("7z: a level of 0..9")));
                return;
            }
            if (uint8_t(_o.method) > uint8_t(method::copy)) {
                fail(error(errc::invalid_argument, 0, string("7z: an unknown method")));
                return;
            }
            _encoder = std::make_unique<writing::FolderEncoder>(writing::Method(_o.method), level);
            _block = _o.solid_block ? _o.solid_block : _encoder->solid_block();
            if (_o.password) {
                // the key made once (2^19 rounds of SHA-256), in plain memory;
                // the password's copy in the options dropped
                sevenzip_aes::Props p;
                p.k = sevenzip_aes::WriteRounds;
                p.salt_size = 16;
                sevenzip_aes::random_bytes(p.salt, 16);
                sevenzip_aes::Password pw(_o.password->view());
                _encoder->encrypt(sevenzip_aes::derive(pw, p), p.salt);
                _o.password.reset();
            }
            _out.assign(fmt::SignatureSize, 0);   // the signature header, written again at the end
        }

        // A new entry (the one before ended); its id
        uint64_t begin(const string& name, const entry_info& info, bool directory) {
            end();
            if (!ok()) {
                return 0;
            }
            if (closed) {
                fail(error(io::error(io::errc::closed, "create", "7z"), _position()));
                return 0;
            }
            std::string n(name.view());
            while (n.size() > 1 && n.back() == '/') {
                n.pop_back();
            }
            writing::FileRecord r;
            if (n.empty() || n.find('\0') != std::string::npos || !writing::utf8_to_utf16(n, r.name)) {
                fail(error(errc::invalid_argument, 0, string("7z: an entry name that is empty, has a NUL or is not UTF-8")));
                return 0;
            }
            r.directory = directory;
            r.has_time[2] = true;
            r.time[2] = info.modified ? writing::filetime_of_unix_nano(info.modified->unix_nano()) : writing::filetime_now();
            if (info.created) {
                r.has_time[0] = true;
                r.time[0] = writing::filetime_of_unix_nano(info.created->unix_nano());
            }
            if (info.accessed) {
                r.has_time[1] = true;
                r.time[1] = writing::filetime_of_unix_nano(info.accessed->unix_nano());
            }
            if (info.attributes) {
                r.attributes = *info.attributes;
            } else {
                unsigned mode = info.mode ? unsigned(*info.mode) : directory ? 0755u : info.symlink ? 0777u : 0644u;
                uint32_t type = directory ? 040000 : info.symlink ? 0120000 : 0100000;
                r.attributes = (directory ? 0x10u : 0x20u) | ((mode & 0200) ? 0u : 1u) | 0x8000u | ((type | (mode & 07777)) << 16);
            }
            _entry = std::move(r);
            _entry_crc = hash::crc32();
            _head.clear();
            _decided = false;
            _open = true;
            current = ++_ids;
            if (directory) {
                end();
            }
            return current;
        }

        // Data of entry id
        optional<io::error> write(uint64_t id, const uint8_t* p, size_t n) {
            if (!ok()) {
                return _io_error();
            }
            if (id != current || !_open || closed) {
                fail(error(io::error(io::errc::closed, "write", "7z"), _position()));
                return _io_error();
            }
            if (_entry.directory && n) {
                fail(error(errc::invalid_argument, 0, string("7z: data for a directory")));
                return _io_error();
            }
            _entry_crc.update(slice<const byte>(reinterpret_cast<const byte*>(p), n));
            _entry.size += n;
            if (!_decided) {
                size_t k = std::min(n, Head - _head.size());
                _head.insert(_head.end(), p, p + k);
                p += k;
                n -= k;
                if (_head.size() < Head) {
                    return nullopt;
                }
                _decide();
            }
            if (n) {
                _code(p, n);
            }
            return nullopt;
        }

        // The entry being written ended: its record made
        void end() {
            if (!_open) {
                return;
            }
            _open = false;
            current = 0;
            if (!ok()) {
                return;
            }
            if (_entry.size || !_head.empty()) {
                if (!_decided) {
                    _decide();
                }
                _entry.stream = true;
                _entry.crc = _entry_crc.value();
                _folder.sizes.push_back(_entry.size);
                _folder.crcs.push_back(_entry.crc);
            }
            _files.push_back(std::move(_entry));
            _entry = writing::FileRecord();
        }

        // The bytes made and not yet written
        std::vector<uint8_t>& out() noexcept {
            return _out;
        }

        optional<io::error> flush() {
            if (_out.empty() || !ok()) {
                return ok() ? optional<io::error>() : _io_error();
            }
            auto r = _sink.write(slice<const byte>(reinterpret_cast<const byte*>(_out.data()), _out.size()));
            _written += _out.size();
            _out.clear();
            if (!r) {
                fail(error(r.error(), _written));
                return _io_error();
            }
            return nullopt;
        }

        // A task's: the bytes through a managed block the write's slice holds
        async::task<optional<io::error>> async_flush() {
            if (_out.empty() || !ok()) {
                co_return ok() ? optional<io::error>() : _io_error();
            }
            size_t at = 0;
            while (at < _out.size()) {
                if (!_managed) {
                    _managed = make_tracked<ByteBlock<32768>>();
                }
                size_t k = std::min<size_t>(_out.size() - at, 32768);
                std::memcpy(_managed->data(), _out.data() + at, k);
                auto r = co_await _sink.async_write(slice<const byte>(tracked_ptr<const void>(_managed), _managed->data(), k));
                if (!r) {
                    _written += at;
                    _out.clear();
                    fail(error(r.error(), _written));
                    co_return _io_error();
                }
                at += k;
            }
            _written += _out.size();
            _out.clear();
            co_return nullopt;
        }

        // The last folder, the header packed, the signature header at the start
        void finish() {
            end();
            if (!ok()) {
                return;
            }
            _end_folder();
            auto plain = writing::header(_folders, _files);
            uint64_t pack = _position() - fmt::SignatureSize;
            std::vector<uint8_t> encoded;
            if (!_files.empty()) {
                std::vector<uint8_t> packed;
                bool hide = _encoder->key() && _o.encrypt_header;
                writing::packed_header(plain, pack, packed, encoded, hide ? _encoder->key() : nullptr, _encoder->salt());
                _out.insert(_out.end(), packed.begin(), packed.end());
                pack += packed.size();
                _out.insert(_out.end(), encoded.begin(), encoded.end());
            } else {
                pack = 0;
            }
            _start = writing::signature(pack, encoded);
        }

        optional<io::error> write_start() {
            if (!ok()) {
                return _io_error();
            }
            auto r = _sink.write_start(_start);
            if (!r) {
                fail(error(r.error(), 0));
                return _io_error();
            }
            return nullopt;
        }

        // The encoder and the records given back: plain memory the collector
        // does not see, which would wait for it otherwise
        void release() {
            _encoder.reset();
            std::vector<writing::FileRecord>().swap(_files);
            std::vector<writing::FolderRecord>().swap(_folders);
            std::vector<uint8_t>().swap(_out);
            std::vector<uint8_t>().swap(_head);
        }

        optional<error> close_sink() {
            if (_sink.owned && _sink.file) {
                auto r = _sink.file.close();
                if (!r && ok()) {
                    fail(error(r.error(), _written));
                }
            }
            return error_;
        }

        io::error _io_error() const {
            return to_io_error(*error_, "7z");
        }

    private:
        uint64_t _position() const noexcept {
            return _written + _out.size();
        }

        // The entry's filter judged by its first bytes, and the folder it
        // goes into: a new one when the archive is not solid, the filter
        // differs from the folder's, or the folder took its block
        void _decide() {
            _decided = true;
            writing::FilterChoice f;
            bool coded = _o.method != method::copy;   // as 7-Zip: a filter before every coder but Copy
            if (_o.auto_filters && coded && !_entry.directory && (_entry.attributes >> 16 & 0170000) != 0120000) {
                f = writing::detect(_head.data(), _head.size());
            }
            if (_folder_open && (!_o.solid || !(f == _folder_filter) || _encoder->unpacked() >= _block)) {
                _end_folder();
            }
            if (!_folder_open) {
                _encoder->start(f);
                _folder_filter = f;
                _folder_open = true;
                _folder = writing::FolderRecord();
                _folder_start = _position();
            }
            if (!_head.empty()) {
                _code(_head.data(), _head.size());
            }
        }

        void _code(const uint8_t* p, size_t n) {
            _encoder->write(p, n, _out);
        }

        void _end_folder() {
            if (!_folder_open) {
                return;
            }
            _encoder->finish(_out);
            _folder.coders = _encoder->coders();
            _folder.coder_sizes = _encoder->coder_sizes();
            _folder.unpacked = _encoder->unpacked();
            _folder.packed = _position() - _folder_start;
            _folders.push_back(std::move(_folder));
            _folder = writing::FolderRecord();
            _folder_open = false;
        }

        Sink _sink;
        options _o;
        std::unique_ptr<writing::FolderEncoder> _encoder;
        uint64_t _block = 0;
        std::vector<uint8_t> _out;
        uint64_t _written = 0;
        tracked_ptr<ByteBlock<32768>> _managed;
        std::vector<writing::FileRecord> _files;
        std::vector<writing::FolderRecord> _folders;
        writing::FileRecord _entry;
        hash::crc32 _entry_crc;
        std::vector<uint8_t> _head;
        bool _decided = false;
        bool _open = false;
        uint64_t _ids = 0;
        bool _folder_open = false;
        writing::FilterChoice _folder_filter;
        writing::FolderRecord _folder;
        uint64_t _folder_start = 0;
        std::vector<uint8_t> _start;
    };

    // The writer of one entry's data: a managed object of its own holding
    // the writer's state; its writes end when the next entry begins
    class EntryWriter final
    : public io::mixin::writer<EntryWriter> {
    public:
        using io::mixin::writer<EntryWriter>::write;
        using io::mixin::writer<EntryWriter>::async_write;

        EntryWriter(const tracked_ptr<WriterState>& s, uint64_t id)
        : _state(s), _id(id) {
        }

        expected<size_t, io::error> write(const slice<const byte>& data) {
            auto& s = *_state;
            if (auto e = s.write(_id, reinterpret_cast<const uint8_t*>(data.data()), data.size())) {
                return io::detail::fail(*e);
            }
            if (s.out().size() >= WriterState::FlushAt) {
                if (auto e = s.flush()) {
                    return io::detail::fail(*e);
                }
            }
            return data.size();
        }

        // A task's: the work in portions of 64 KB, the worker let go
        // between them, the output written through a managed block
        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            size_t at = 0;
            do {
                size_t k = std::min<size_t>(data.size() - at, Portion);
                if (auto e = _state->write(_id, reinterpret_cast<const uint8_t*>(data.data()) + at, k)) {
                    co_return io::detail::fail(*e);
                }
                at += k;
                if (_state->out().size() >= WriterState::FlushAt) {
                    if (auto e = co_await _state->async_flush()) {
                        co_return io::detail::fail(*e);
                    }
                }
                if (at < data.size()) {
                    co_await async::yield();
                }
            } while (at < data.size());
            co_return data.size();
        }

        // Ends the entry (the next create ends it too)
        expected<void, io::error> close() {
            if (_state->current == _id) {
                _state->end();
            }
            if (!_state->ok()) {
                return io::detail::fail(_state->_io_error());
            }
            return {};
        }

    private:
        static constexpr size_t Portion = size_t(64) << 10;

        tracked_ptr<WriterState> _state;
        uint64_t _id;
    };
}

namespace sgcl::compress::sevenzip {
    // An archive written entry after entry, as 7-Zip writes it: the data of
    // entries into folders (solid by default, a new folder at a new filter
    // or past the block), encoded as it comes, never held whole; at close
    // the header, packed with LZMA, and the signature header at the start,
    // which is why the output must seek (a file, or an io::buffer).
    // Every error is kept as the first — a failure of the output, and the
    // caller's own (a name that is not UTF-8, data for a directory, a write
    // to an entry that ended, anything after close): everything after it is
    // refused and close() returns it. Moved, not copied.
    class writer {
    public:
        explicit writer(const string& path)
        : writer(path, options{}) {
        }

        // A file made at the path (closed by close()); failing to make it
        // is the writer's first error
        writer(const string& path, const options& o) {
            detail::Sink s;
            auto f = io::create(path);
            if (f) {
                s.file = *f;
                s.owned = true;
            }
            _init(std::move(s), o);
            if (!f) {
                _state->fail(error(f.error(), 0));
            }
        }

        explicit writer(const io::file& file)
        : writer(file, options{}) {
        }

        // Into a file from where it stands; the file is the caller's to close
        writer(const io::file& file, const options& o) {
            detail::Sink s;
            s.file = file;
            auto at = file.seek(0, io::seek_from::current);
            s.base = at ? *at : 0;
            _init(std::move(s), o);
            if (!at) {
                _state->fail(error(at.error(), 0));
            }
        }

        explicit writer(const io::buffer& b)
        : writer(b, options{}) {
        }

        // Into a buffer from its write position; the writer holds the
        // buffer (a handle: the same buffer as the caller's)
        writer(const io::buffer& b, const options& o) {
            detail::Sink s;
            s.buffer = b;
            s.base = *s.buffer.tell();
            _init(std::move(s), o);
        }

        writer(const writer&) = delete;
        writer& operator=(const writer&) = delete;
        writer(writer&&) noexcept = default;
        writer& operator=(writer&&) noexcept = default;

        // A writer of the entry's data (the entry before it ended). No
        // error here: one is kept, and the entry writer's writes give it.
        io::writer create(const string& name) {
            return create(name, entry_info{});
        }

        io::writer create(const string& name, const entry_info& info) {
            uint64_t id = _state->begin(name, info, false);
            return io::writer(make_tracked<detail::EntryWriter>(_state, id));
        }

        // A whole entry
        void add(const string& name, const slice<const byte>& data) {
            add(name, data, entry_info{});
        }

        void add(const string& name, const slice<const byte>& data, const entry_info& info) {
            uint64_t id = _state->begin(name, info, false);
            if (id) {
                detail::EntryWriter w(_state, id);
                (void)w.write(data);
                _state->end();
            }
        }

        void add_directory(const string& name) {
            add_directory(name, entry_info{});
        }

        void add_directory(const string& name, const entry_info& info) {
            (void)_state->begin(name, info, true);
        }

        // The last entry and folder ended, the header and the signature
        // header written; a file made from a path closed. The first error,
        // if any. A second close does nothing more.
        expected<void, error> close() {
            auto& s = *_state;
            if (!s.closed) {
                s.finish();
                (void)s.flush();
                (void)s.write_start();
                s.closed = true;
                (void)s.close_sink();
                s.release();
            }
            if (s.error_) {
                return unexpected<error>(*s.error_);
            }
            return {};
        }

        async::task<expected<void, error>> async_close() {
            tracked_ptr<detail::WriterState> keep = _state;
            auto& s = *keep;
            if (!s.closed) {
                s.finish();
                (void)co_await s.async_flush();
                (void)s.write_start();
                s.closed = true;
                (void)s.close_sink();
                s.release();
            }
            if (s.error_) {
                co_return unexpected<error>(*s.error_);
            }
            co_return expected<void, error>();
        }

        // The first error, kept
        const optional<error>& last_error() const noexcept {
            return _state->error_;
        }

    private:
        void _init(detail::Sink s, const options& o) {
            _state = make_tracked<detail::WriterState>(std::move(s), o);
            _state->start();
        }

        tracked_ptr<detail::WriterState> _state;
    };
}
