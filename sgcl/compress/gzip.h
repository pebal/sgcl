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
        // the file's name in the header and its time and mode on the new
        // file. A .gz there already is written over. (The overloads without
        // options stand for a default argument, which a nested struct with
        // member initializers cannot be inside its class.)
        static expected<void, error> compress_file(const string& path);
        static expected<void, error> compress_file(const string& path, const file_options& o);
        static async::task<expected<void, error>> async_compress_file(string path);
        static async::task<expected<void, error>> async_compress_file(string path, file_options o);

        // The reverse, as gunzip: path without its ".gz" (another name is
        // errc::invalid_argument), every member, the time of the .gz on the
        // file made
        static expected<void, error> decompress_file(const string& path);
        static expected<void, error> decompress_file(const string& path, const file_options& o);
        static async::task<expected<void, error>> async_decompress_file(string path);
        static async::task<expected<void, error>> async_decompress_file(string path, file_options o);

        static vector<byte> compress(const slice<const byte>& data) {
            return compress(data, options{});
        }

        // A header the format cannot write — a name or a comment past ISO
        // 8859-1 or with a NUL, an extra field past 65535 bytes — is the
        // program's mistake: std::invalid_argument (the writer's first
        // write reports it as errc::invalid_argument)
        static vector<byte> compress(const slice<const byte>& data, const options& o) {
            detail::GzipFormat f;
            f.head = o.header;
            return detail::compress_all(detail::bytes(data), data.size(), o.level.value(), f, {});
        }

        static vector<byte> compress(const string& text) {
            return compress(io::detail::bytes_of(text), options{});
        }

        static vector<byte> compress(const string& text, const options& o) {
            return compress(io::detail::bytes_of(text), o);
        }

        // A literal, a character array, a std::string_view: the text's
        // bytes as the string's overload takes them (an exact match, else
        // the two conversions, to a string and to bytes, tie)
        template<sgcl::detail::TextArgument T>
        static vector<byte> compress(const T& text) {
            return compress(slice<const byte>(text), options{});
        }

        template<sgcl::detail::TextArgument T>
        static vector<byte> compress(const T& text, const options& o) {
            return compress(slice<const byte>(text), o);
        }

        // Every member, one after another
        static expected<vector<byte>, error> decompress(const slice<const byte>& data) {
            return decompress(data, limits{});
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) {
            // the length of the last member is the size to expect
            size_t hint = data.size() >= 18 ? detail::le32(detail::bytes(data) + data.size() - 4) : 0;
            return detail::decompress_all(detail::bytes(data), data.size(), l, detail::GzipFormat(), {}, std::min<size_t>(hint, size_t(l.max_size)));
        }
    };

    // Writes the header before the first bytes; a name or a comment that
    // ISO 8859-1 cannot write fails that first write (or close) with
    // errc::invalid_argument
    class gzip::writer final
    : public detail::DeflateWriter<detail::GzipFormat> {
    public:
        explicit writer(const io::writer& out)
        : writer(out, options{}) {
        }

        writer(const io::writer& out, const options& o)
        : DeflateWriter(out, o.level.value(), _format(o), {}) {
        }

    private:
        static detail::GzipFormat _format(const options& o) {
            detail::GzipFormat f;
            f.head = o.header;
            return f;
        }
    };

    class gzip::reader final
    : public detail::InflateReader<detail::GzipFormat> {
    public:
        explicit reader(const io::reader& in)
        : InflateReader(in, {}) {
        }

        reader(const io::reader& in, gzip::single_member_t)
        : InflateReader(in, {}, true) {
        }

        // The header of the current member, read now if it was not yet
        expected<gzip::header, error> header() {
            auto f = _header_now();
            if (!f) {
                return unexpected<error>(f.error());
            }
            return (*f)->head;
        }

        async::task<expected<gzip::header, error>> async_header() {
            auto f = co_await _async_header_now();
            if (!f) {
                co_return unexpected<error>(f.error());
            }
            co_return (*f)->head;
        }
    };
}

namespace sgcl::compress::detail {
    inline error gzip_io_error(const io::error& e) {
        if (&e.code().category() == &compress_category()) {
            return error(errc(e.code().value()), 0, string(e.message()));
        }
        return error(e, 0);
    }

    inline expected<void, error> gzip_compress_file(const string& path, compress::level level, bool keep) {
        auto info = io::stat(path);
        if (!info) {
            return unexpected(error(info.error(), 0));
        }
        auto in = io::open(path);
        if (!in) {
            return unexpected(error(in.error(), 0));
        }
        const string out_path = path + ".gz";
        auto out = io::create(out_path, info->mode);
        if (!out) {
            (void)in->close();
            return unexpected(error(out.error(), 0));
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
            r = unexpected(error(c.error(), 0));
        }
        if (!r) {
            (void)io::remove(out_path);
            return r;
        }
        (void)io::set_modified(out_path, info->modified);
        if (!keep) {
            if (auto removed = io::remove(path); !removed) {
                return unexpected(error(removed.error(), 0));
            }
        }
        return {};
    }

    inline expected<void, error> gzip_decompress_file(const string& path, bool keep) {
        std::string_view name = path.view();
        if (name.size() <= 3 || name.substr(name.size() - 3) != ".gz") {
            return unexpected(error(errc::invalid_argument, 0, string("gzip: a name that does not end in .gz: ") + path));
        }
        auto info = io::stat(path);
        if (!info) {
            return unexpected(error(info.error(), 0));
        }
        auto in = io::open(path);
        if (!in) {
            return unexpected(error(in.error(), 0));
        }
        const string out_path = string(name.substr(0, name.size() - 3));
        auto out = io::create(out_path, info->mode);
        if (!out) {
            (void)in->close();
            return unexpected(error(out.error(), 0));
        }
        expected<void, error> r;
        {
            gzip::reader z(*in);
            auto copied = io::copy(*out, z);
            if (!copied) {
                r = unexpected(gzip_io_error(copied.error()));
            }
        }
        (void)in->close();
        if (auto c = out->close(); r && !c) {
            r = unexpected(error(c.error(), 0));
        }
        if (!r) {
            (void)io::remove(out_path);
            return r;
        }
        (void)io::set_modified(out_path, info->modified);
        if (!keep) {
            if (auto removed = io::remove(path); !removed) {
                return unexpected(error(removed.error(), 0));
            }
        }
        return {};
    }
}

namespace sgcl::compress {
    namespace detail {
        inline async::task<expected<void, error>> gzip_compress_file_task(string path, compress::level l, bool keep) {
            co_return co_await async::spawn_blocking([path, l, keep] { return gzip_compress_file(path, l, keep); });
        }

        inline async::task<expected<void, error>> gzip_decompress_file_task(string path, bool keep) {
            co_return co_await async::spawn_blocking([path, keep] { return gzip_decompress_file(path, keep); });
        }
    }

    inline expected<void, error> gzip::compress_file(const string& path) {
        return compress_file(path, file_options{});
    }

    inline expected<void, error> gzip::compress_file(const string& path, const file_options& o) {
        return detail::gzip_compress_file(path, o.level, o.keep);
    }

    inline async::task<expected<void, error>> gzip::async_compress_file(string path) {
        return async_compress_file(std::move(path), file_options{});
    }

    inline async::task<expected<void, error>> gzip::async_compress_file(string path, file_options o) {
        return detail::gzip_compress_file_task(std::move(path), o.level, o.keep);
    }

    inline expected<void, error> gzip::decompress_file(const string& path) {
        return decompress_file(path, file_options{});
    }

    inline expected<void, error> gzip::decompress_file(const string& path, const file_options& o) {
        return detail::gzip_decompress_file(path, o.keep);
    }

    inline async::task<expected<void, error>> gzip::async_decompress_file(string path) {
        return async_decompress_file(std::move(path), file_options{});
    }

    inline async::task<expected<void, error>> gzip::async_decompress_file(string path, file_options o) {
        return detail::gzip_decompress_file_task(std::move(path), o.keep);
    }
}
