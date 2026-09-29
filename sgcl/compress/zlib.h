//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/stream.h"

namespace sgcl::compress {
    // zlib (RFC 1950): DEFLATE between a two-byte header and the Adler-32
    // of the data; PNG's image data and PDF's streams. A stream made with
    // a preset dictionary names it by its Adler-32 (dictionary_id), and
    // reading it without that dictionary is errc::dictionary_required.
    class zlib {
    public:
        using error = compress::error;

        struct options {
            compress::level level;
            slice<const byte> dictionary;
        };

        class writer;
        class reader;

        static vector<byte> compress(const slice<const byte>& data) {
            return compress(data, options{});
        }

        static vector<byte> compress(const slice<const byte>& data, const options& o) {
            return detail::compress_all(detail::bytes(data), data.size(), o.level.value(), _format(o), o.dictionary);
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

        static expected<vector<byte>, error> decompress(const slice<const byte>& data) {
            return decompress(data, options{}, limits{});
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) {
            return decompress(data, options{}, l);
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const options& o) {
            return decompress(data, o, limits{});
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const options& o, const limits& l) {
            return detail::decompress_all(detail::bytes(data), data.size(), l, detail::ZlibFormat(), o.dictionary);
        }

        // The Adler-32 of the dictionary a stream was made with, from its
        // header; nullopt for a stream made without one (or not zlib)
        static optional<uint32_t> dictionary_id(const slice<const byte>& data) noexcept {
            detail::ZlibFormat f;
            auto r = f.header(detail::bytes(data), data.size());
            return r.status == detail::Parsed::ok ? f.dictionary_id : nullopt;
        }

    private:
        static detail::ZlibFormat _format(const options& o) {
            detail::ZlibFormat f;
            if (!o.dictionary.empty()) {
                hash::adler32 a;
                a.update(o.dictionary);
                f.dictionary_id = a.value();
            }
            return f;
        }

        friend class writer;
    };

    class zlib::writer final
    : public detail::DeflateWriter<detail::ZlibFormat> {
    public:
        explicit writer(const io::writer& out)
        : writer(out, options{}) {
        }

        writer(const io::writer& out, const options& o)
        : DeflateWriter(out, o.level.value(), zlib::_format(o), o.dictionary) {
        }
    };

    class zlib::reader final
    : public detail::InflateReader<detail::ZlibFormat> {
    public:
        explicit reader(const io::reader& in)
        : InflateReader(in, {}) {
        }

        reader(const io::reader& in, const options& o)
        : InflateReader(in, o.dictionary) {
        }

        // The Adler-32 of the dictionary the stream asks for, once its
        // header is read (nullopt before, and for a stream with none)
        optional<uint32_t> dictionary_id() const noexcept {
            return _format_ref().dictionary_id;
        }
    };
}
