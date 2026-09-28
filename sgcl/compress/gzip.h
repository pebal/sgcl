//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/stream.h"

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
