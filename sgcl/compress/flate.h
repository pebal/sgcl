//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/stream.h"

namespace sgcl::compress {
    // DEFLATE (RFC 1951) with nothing around it: the data of zip, PNG and
    // HTTP's "deflate" as browsers read it. The whole of the data in memory
    // either way, or a stream each way: a writer that compresses what is
    // written to it into another writer, a reader of what the data read
    // from another reader decompresses to. A dictionary is data both sides
    // agree on in advance, which the first matches may refer to (Go's
    // NewWriterDict).
    class flate {
    public:
        using error = compress::error;

        struct options {
            compress::level level;
            slice<const byte> dictionary;
        };

        class writer;
        class reader;

        static vector<byte> compress(const slice<const byte>& data) noexcept {
            return compress(data, options{});
        }

        static vector<byte> compress(const slice<const byte>& data, const options& o) noexcept {
            return detail::compress_all(detail::bytes(data), data.size(), o.level.value(), detail::FlateFormat(), o.dictionary);
        }

        static vector<byte> compress(const string& text) noexcept {
            return compress(io::detail::bytes_of(text), options{});
        }

        static vector<byte> compress(const string& text, const options& o) noexcept {
            return compress(io::detail::bytes_of(text), o);
        }

        // A literal, a character array, a std::string_view: the text's
        // bytes as the string's overload takes them (an exact match, else
        // the two conversions, to a string and to bytes, tie)
        template<sgcl::detail::TextArgument T>
        static vector<byte> compress(const T& text) noexcept {
            return compress(slice<const byte>(text), options{});
        }

        template<sgcl::detail::TextArgument T>
        static vector<byte> compress(const T& text, const options& o) noexcept {
            return compress(slice<const byte>(text), o);
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept {
            return decompress(data, options{}, limits{});
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) noexcept {
            return decompress(data, options{}, l);
        }

        // With the dictionary it was compressed with
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const options& o) noexcept {
            return decompress(data, o, limits{});
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const options& o, const limits& l) noexcept {
            return detail::decompress_all(detail::bytes(data), data.size(), l, detail::FlateFormat(), o.dictionary);
        }
    };

    // What is written, compressed into out; flush() makes all of it
    // decodable at once, close() ends the stream and leaves out open
    class flate::writer final
    : public detail::DeflateWriter<detail::FlateFormat> {
    public:
        writer(writer&&) noexcept = default;   // the other left closed (detail/stream.h)

        writer& operator=(writer&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        explicit writer(const io::writer& out) noexcept
        : writer(out, options{}) {
        }

        writer(const io::writer& out, const options& o) noexcept
        : DeflateWriter(out, o.level.value(), detail::FlateFormat(), o.dictionary) {
        }
    };

    // What the data read from in decompresses to. It may read past the end
    // of the DEFLATE data (it reads its input a block at a time): a format
    // with more after it gives the reader a limit_reader over its part.
    class flate::reader final
    : public detail::InflateReader<detail::FlateFormat> {
    public:
        reader(reader&&) noexcept = default;   // the other left closed (detail/stream.h)

        reader& operator=(reader&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        explicit reader(const io::reader& in) noexcept
        : InflateReader(in, {}) {
        }

        reader(const io::reader& in, const options& o) noexcept
        : InflateReader(in, o.dictionary) {
        }
    };
}
