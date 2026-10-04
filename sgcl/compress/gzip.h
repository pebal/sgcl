//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/stream.h"
#include "../async/blocking.h"
#include "../io/file.h"
#include "../io/fs.h"
#include "../io/functions.h"

namespace sgcl::compress {
    // gzip's header (RFC 1952): every field optional. The name and the
    // comment are ISO 8859-1 in the format; a program's are UTF-8 and are
    // converted both ways where ISO 8859-1 holds them, and are written as
    // their UTF-8 bytes where it does not, as gzip(1) writes a file's name
    // (detail::gzip_text_bytes); read, bytes that are UTF-8 are taken as
    // they are. A NUL in either is invalid_argument when written.
    struct gzip_header {
        string name;
        string comment;
        optional<time::datetime> modified;
        vector<byte> extra;
        uint8_t os = 255;   // 255: unknown, as Go writes
    };
}

namespace sgcl::compress::detail {
    struct GzipFormat {
        static constexpr const char* name = "gzip";
        static constexpr bool members = true;
        static constexpr bool refuses = true;
        static constexpr size_t MaxHeader = size_t(1) << 20;
        gzip_header head;
        hash::crc32 sum;
        uint32_t size = 0;

        template<class Out>
        const char* start(Out& out, int lvl) noexcept {
            auto name = gzip_text_bytes(head.name);
            auto comment = gzip_text_bytes(head.comment);
            if (name.find('\0') != std::string::npos) {
                return "gzip: a name with a NUL";
            }
            if (comment.find('\0') != std::string::npos) {
                return "gzip: a comment with a NUL";
            }
            if (head.extra.size() > 65535) {
                return "gzip: an extra field longer than 65535 bytes";
            }
            uint8_t flags = 0;
            if (!head.extra.empty()) flags |= 4;
            if (!name.empty()) flags |= 8;
            if (!comment.empty()) flags |= 16;
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
                out.insert(out.end(), name.begin(), name.end());
                out.push_back(0);
            }
            if (flags & 16) {
                out.insert(out.end(), comment.begin(), comment.end());
                out.push_back(0);
            }
            return nullptr;
        }

        SGCL_INLINE_HOT void update(const uint8_t* p, size_t n) noexcept {
            sum.update(slice<const byte>(reinterpret_cast<const byte*>(p), n));
            size += uint32_t(n);
        }

        template<class Out>
        SGCL_INLINE_HOT void finish(Out& out) noexcept {
            put_le32(out, sum.value());
            put_le32(out, size);
        }

        // Potentially throwing: a name or a comment of a gzip stream in
        // memory may pass 2 GiB (gzip_text); a stream's header stops at
        // MaxHeader
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
                auto text = gzip_text(p + at, size_t(end - (p + at)));
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

        SGCL_INLINE_HOT Parsed trailer(const uint8_t* p, size_t n) noexcept {
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

        SGCL_INLINE_HOT void reset() noexcept {
            sum = hash::crc32();
            size = 0;
        }
    };
}

namespace sgcl::compress {
    // gzip (RFC 1952): DEFLATE between a header — a name, a comment, a
    // time, an extra field — and the CRC-32 and the length of the data;
    // .gz files and HTTP's Content-Encoding. A gzip stream may be several
    // members one after another (cat a.gz b.gz), which a reader reads as
    // one stream, as gunzip and Go do, unless given gzip::single_member.
    class gzip {
    public:
        using error = compress::error;
        using header = gzip_header;

        struct options {
            compress::level level;
            gzip::header header;
        };

        struct single_member_t {
            explicit single_member_t() = default;
        };
        static constexpr single_member_t single_member{};

        class writer;
        class reader;

        // What compress_file and decompress_file take besides the path
        struct file_options {
            compress::level level;   // compress_file: 6 by default
            bool keep = true;        // the original stays; false removes it once the other is whole, as gzip(1) without -k
        };

        // A file compressed beside itself, as gzip(1): path + ".gz", with
        // the file's name in the header (its bytes as they are past ISO
        // 8859-1, as gzip(1) writes them) and its time and mode on the new
        // file. A .gz there already is written over. (The overloads without
        // options stand for a default argument, which a nested struct with
        // member initializers cannot be inside its class.)
        static expected<void, error> compress_file(const string& path);
        static expected<void, error> compress_file(const string& path, const file_options& o);
        static async::task<expected<void, error>> async_compress_file(string path) noexcept;
        static async::task<expected<void, error>> async_compress_file(string path, file_options o) noexcept;

        // The reverse, as gunzip: path without its ".gz" (another name is
        // errc::invalid_argument), every member, the time of the .gz on the
        // file made
        static expected<void, error> decompress_file(const string& path);
        static expected<void, error> decompress_file(const string& path, const file_options& o);
        static async::task<expected<void, error>> async_decompress_file(string path) noexcept;
        static async::task<expected<void, error>> async_decompress_file(string path, file_options o) noexcept;

        SGCL_INLINE_HOT static vector<byte> compress(const slice<const byte>& data) noexcept {
            return compress(data, options{});
        }

        // A header the format cannot write — a name or a comment with a
        // NUL, an extra field past 65535 bytes — is the program's mistake:
        // std::invalid_argument (the writer's first write reports it as
        // errc::invalid_argument)
        SGCL_INLINE_HOT static vector<byte> compress(const slice<const byte>& data, const options& o) {
            detail::GzipFormat f;
            f.head = o.header;
            return detail::compress_all(detail::bytes(data), data.size(), o.level.value(), f, {});
        }

        SGCL_INLINE_HOT static vector<byte> compress(const string& text) noexcept {
            return compress(io::detail::bytes_of(text), options{});
        }

        SGCL_INLINE_HOT static vector<byte> compress(const string& text, const options& o) {
            return compress(io::detail::bytes_of(text), o);
        }

        // A literal, a character array, a std::string_view: the text's
        // bytes as the string's overload takes them (an exact match, else
        // the two conversions, to a string and to bytes, tie)
        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT static vector<byte> compress(const T& text) noexcept {
            return compress(slice<const byte>(text), options{});
        }

        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT static vector<byte> compress(const T& text, const options& o) {
            return compress(slice<const byte>(text), o);
        }

        // Every member, one after another
        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress(const slice<const byte>& data) {
            return decompress(data, limits{});
        }

        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) {
            // the length of the last member is the size to expect
            size_t hint = data.size() >= 18 ? detail::le32(detail::bytes(data) + data.size() - 4) : 0;
            return detail::decompress_all(detail::bytes(data), data.size(), l, detail::GzipFormat(), {}, std::min<size_t>(hint, size_t(l.max_size)));
        }
    };

    // Writes the header before the first bytes; a header the format
    // cannot write (a NUL in the name or the comment, an extra field past
    // 65535 bytes) fails that first write (or close) with
    // errc::invalid_argument
    class gzip::writer final
    : public detail::DeflateWriter<detail::GzipFormat> {
    public:
        writer(writer&&) noexcept = default;   // the other left closed (detail/stream.h)

        SGCL_INLINE_HOT writer& operator=(writer&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        SGCL_INLINE_HOT explicit writer(const io::writer& out) noexcept
        : writer(out, options{}) {
        }

        SGCL_INLINE_HOT writer(const io::writer& out, const options& o) noexcept
        : DeflateWriter(out, o.level.value(), _format(o), {}) {
        }

    private:
        SGCL_INLINE_HOT static detail::GzipFormat _format(const options& o) noexcept {
            detail::GzipFormat f;
            f.head = o.header;
            return f;
        }
    };

    class gzip::reader final
    : public detail::InflateReader<detail::GzipFormat> {
    public:
        reader(reader&&) noexcept = default;   // the other left closed (detail/stream.h)

        SGCL_INLINE_HOT reader& operator=(reader&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        SGCL_INLINE_HOT explicit reader(const io::reader& in) noexcept
        : InflateReader(in, {}) {
        }

        SGCL_INLINE_HOT reader(const io::reader& in, gzip::single_member_t) noexcept
        : InflateReader(in, {}, true) {
        }

        // The header of the current member, read now if it was not yet
        SGCL_INLINE_HOT expected<gzip::header, error> header() {
            auto f = _header_now();
            if (!f) {
                return unexpected<error>(f.error());
            }
            return (*f)->head;
        }

        async::task<expected<gzip::header, error>> async_header() noexcept {
            auto f = co_await _async_header_now();
            if (!f) {
                co_return unexpected<error>(f.error());
            }
            co_return (*f)->head;
        }
    };
}

namespace sgcl::compress::detail {
    // A failure of the copy into the gzip writer: the writer's own (a
    // header it cannot write) as its code, a failure of a file as
    // errc::io; neither has a place in any data (the file read is not
    // compressed, the .gz is what is written)
    inline error gzip_io_error(const io::error& e) noexcept {
        if (&e.code().category() == &compress_category()) {
            return no_place(errc(e.code().value()), string(e.message()));
        }
        return no_place(e);
    }

    // Compressing a file: no error has a place, as nothing compressed is
    // read; every failure is of a file or of the writer
    inline expected<void, error> gzip_compress_file(const string& path, compress::level level, bool keep) {
        auto info = io::stat(path);
        if (!info) {
            return unexpected(no_place(info.error()));
        }
        auto in = io::open(path);
        if (!in) {
            return unexpected(no_place(in.error()));
        }
        const string out_path = path + ".gz";
        auto out = io::create(out_path, info->mode);
        if (!out) {
            (void)in->close();
            return unexpected(no_place(out.error()));
        }
        gzip::options o;
        o.level = level;
        std::string_view name = path.view();
        if (auto slash = name.rfind('/'); slash != std::string_view::npos) {
            name.remove_prefix(slash + 1);
        }
        o.header.name = string(name);
        o.header.modified = time::datetime::from_unix_nano(int64_t(info->modified.time_since_epoch().count()), time::zone::utc());
        o.header.os = 3;   // Unix, as gzip(1) there writes
        expected<void, error> r;
        {
            gzip::writer z(*out, o);
            auto copied = io::copy(z, *in);
            if (!copied) {
                r = unexpected(gzip_io_error(copied.error()));
            }
            if (auto c = z.close(); r && !c) {
                r = unexpected(gzip_io_error(c.error()));
            }
        }
        (void)in->close();
        if (auto c = out->close(); r && !c) {
            r = unexpected(no_place(c.error()));
        }
        if (!r) {
            (void)io::remove(out_path);
            return r;
        }
        (void)io::set_modified(out_path, info->modified);
        if (!keep) {
            if (auto removed = io::remove(path); !removed) {
                return unexpected(no_place(removed.error()));
            }
        }
        return {};
    }

    // Decompressing a file: an error of the .gz's data (or of its read)
    // where the reader found it; a name, a file that does not open, a
    // failure of the file written with no place
    inline expected<void, error> gzip_decompress_file(const string& path, bool keep) {
        std::string_view name = path.view();
        if (name.size() <= 3 || name.substr(name.size() - 3) != ".gz") {
            return unexpected(no_place(errc::invalid_argument, string("gzip: a name that does not end in .gz: ") + path));
        }
        auto info = io::stat(path);
        if (!info) {
            return unexpected(no_place(info.error()));
        }
        auto in = io::open(path);
        if (!in) {
            return unexpected(no_place(in.error()));
        }
        const string out_path = string(name.substr(0, name.size() - 3));
        auto out = io::create(out_path, info->mode);
        if (!out) {
            (void)in->close();
            return unexpected(no_place(out.error()));
        }
        expected<void, error> r;
        {
            gzip::reader z(*in);
            auto copied = io::copy(*out, z);
            if (!copied) {
                // the data's error or in's, as the reader keeps it where it
                // was found; else the file written failed
                r = unexpected(z.last_error() ? *z.last_error() : no_place(copied.error()));
            }
        }
        (void)in->close();
        if (auto c = out->close(); r && !c) {
            r = unexpected(no_place(c.error()));
        }
        if (!r) {
            (void)io::remove(out_path);
            return r;
        }
        (void)io::set_modified(out_path, info->modified);
        if (!keep) {
            if (auto removed = io::remove(path); !removed) {
                return unexpected(no_place(removed.error()));
            }
        }
        return {};
    }
}

namespace sgcl::compress {
    namespace detail {
        inline async::task<expected<void, error>> gzip_compress_file_task(string path, compress::level l, bool keep) noexcept {
            co_return co_await async::spawn_blocking([path, l, keep] { return gzip_compress_file(path, l, keep); });
        }

        inline async::task<expected<void, error>> gzip_decompress_file_task(string path, bool keep) noexcept {
            co_return co_await async::spawn_blocking([path, keep] { return gzip_decompress_file(path, keep); });
        }
    }

    SGCL_INLINE_HOT expected<void, error> gzip::compress_file(const string& path) {
        return compress_file(path, file_options{});
    }

    SGCL_INLINE_HOT expected<void, error> gzip::compress_file(const string& path, const file_options& o) {
        return detail::gzip_compress_file(path, o.level, o.keep);
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> gzip::async_compress_file(string path) noexcept {
        return async_compress_file(std::move(path), file_options{});
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> gzip::async_compress_file(string path, file_options o) noexcept {
        return detail::gzip_compress_file_task(std::move(path), o.level, o.keep);
    }

    SGCL_INLINE_HOT expected<void, error> gzip::decompress_file(const string& path) {
        return decompress_file(path, file_options{});
    }

    SGCL_INLINE_HOT expected<void, error> gzip::decompress_file(const string& path, const file_options& o) {
        return detail::gzip_decompress_file(path, o.keep);
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> gzip::async_decompress_file(string path) noexcept {
        return async_decompress_file(std::move(path), file_options{});
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> gzip::async_decompress_file(string path, file_options o) noexcept {
        return detail::gzip_decompress_file_task(std::move(path), o.keep);
    }
}
