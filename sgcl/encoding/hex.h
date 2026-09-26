//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/codec.h"
#include "detail/radix.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../io/stream.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace sgcl::encoding {
    namespace detail { using namespace sgcl::detail; }
    namespace detail {
        // Base16 (RFC 4648 section 8) is the radix codec of four bits, two
        // characters a byte and nothing to pad; the decoding takes either
        // case, as the section says a decoder may
        inline constexpr Radix<4> HexLower {"hex", "0123456789abcdef", -1, true};
        inline constexpr Radix<4> HexUpper {"hex", "0123456789ABCDEF", -1, true};

        // One line of a dump: the offset in eight hexadecimal digits (more
        // past 4 GB), the sixteen bytes in two columns of eight, and the
        // bytes again as characters, a dot for what is not printable ASCII.
        // The line of fewer bytes at the end keeps the columns where they
        // are. At most 79 characters; the end of what was written.
        inline char* dump_line(char* o, uint64_t offset, const uint8_t* p, size_t n) noexcept {
            static constexpr char Digits[] = "0123456789abcdef";
            int digits = 8;
            while (digits < 16 && (offset >> (4 * digits)) != 0) {
                ++digits;
            }
            for (int i = digits - 1; i >= 0; --i) {
                *o++ = Digits[(offset >> (4 * i)) & 15];
            }
            *o++ = ' ';
            *o++ = ' ';
            for (size_t i = 0; i < 16; ++i) {
                if (i < n) {
                    *o++ = Digits[p[i] >> 4];
                    *o++ = Digits[p[i] & 15];
                } else {
                    *o++ = ' ';
                    *o++ = ' ';
                }
                *o++ = ' ';
                if (i == 7) {
                    *o++ = ' ';
                }
            }
            *o++ = ' ';
            *o++ = '|';
            for (size_t i = 0; i < n; ++i) {
                *o++ = p[i] >= 0x20 && p[i] < 0x7F ? char(p[i]) : '.';
            }
            *o++ = '|';
            *o++ = '\n';
            return o;
        }

        inline constexpr size_t DumpLineChars = 16 + 2 + 16 * 3 + 1 + 1 + 1 + 16 + 2;
    }

    // Hexadecimal: a byte as two digits. The digits of encode are lower
    // case (encode_upper for the other), and decode takes either case and
    // fails on an odd length or anything that is not a digit. dump is the
    // view hexdump -C gives, a line of sixteen bytes and their characters,
    // for a log or a test that shows what came over a wire.
    class hex {
    public:
        using error = encoding::error;

        class encoder;
        class decoder;
        class dumper;

        static string encode(const slice<const byte>& data) {
            return detail::encode_text(detail::HexLower, reinterpret_cast<const uint8_t*>(data.data()), data.size());
        }

        // The bytes of a text
        static string encode(const string& text) {
            return detail::encode_text(detail::HexLower, reinterpret_cast<const uint8_t*>(text.data()), text.size());
        }

        static string encode_upper(const slice<const byte>& data) {
            return detail::encode_text(detail::HexUpper, reinterpret_cast<const uint8_t*>(data.data()), data.size());
        }

        static string encode_upper(const string& text) {
            return detail::encode_text(detail::HexUpper, reinterpret_cast<const uint8_t*>(text.data()), text.size());
        }

        // The first character that is not a digit is invalid_character at
        // its offset; an odd length is unexpected_end at the end
        static expected<vector<byte>, error> decode(const string& text) {
            return detail::decode_text(detail::HexLower, text);
        }

        // The characters n bytes take, and the most bytes n characters
        // decode to, as base64's
        static constexpr size_t encoded_size(size_t n) noexcept {
            return detail::HexLower.encoded_size(n);
        }

        static constexpr size_t max_decoded_size(size_t n) noexcept {
            return detail::HexLower.max_decoded_size(n);
        }

        // Into the caller's buffer, nothing allocated: out holds at least
        // encoded_size(data.size()) characters, or max_decoded_size of the
        // text's size bytes — a smaller one is length_error. The
        // characters or the bytes written.
        static size_t encode_to(const slice<char>& out, const slice<const byte>& data) {
            return detail::encode_into(detail::HexLower, out, reinterpret_cast<const uint8_t*>(data.data()), data.size());
        }

        static expected<size_t, error> decode_to(const slice<byte>& out, const string& text) {
            return detail::decode_to(detail::HexLower, out, text);
        }

        // A writer that encodes what is written to it into out, in lower
        // case (close() leaves out open), and a reader of the bytes the
        // digits of in decode to, as base64's
        static tracked_ptr<encoder> encoder_to(const io::writer& out);
        static tracked_ptr<decoder> decoder_from(const io::reader& in);

        // Go's hex.Dump and hexdump -C without its closing line:
        //
        //     00000000  48 65 6c 6c 6f 2c 20 57  6f 72 6c 64 21 0a        |Hello, World!.|
        static string dump(const slice<const byte>& data) {
            auto p = reinterpret_cast<const uint8_t*>(data.data());
            size_t n = data.size();
            // A line of 16 bytes is at most 87 characters: past what a
            // string holds, length_error, before the sum can wrap
            if (n / 16 >= string::max_size() / (detail::DumpLineChars + 8)) {
                throw length_error("sgcl::hex::dump: a dump longer than a string can hold");
            }
            std::string s;
            s.resize((n + 15) / 16 * (detail::DumpLineChars + 8));
            char* o = s.data();
            for (size_t i = 0; i < n; i += 16) {
                o = detail::dump_line(o, i, p + i, n - i < 16 ? n - i : 16);
            }
            return string(s.data(), size_t(o - s.data()));
        }

        // The bytes of a text
        static string dump(const string& text) {
            return dump(as_bytes(text.as_slice()));
        }

        // A writer that writes the dump of what is written to it into out,
        // a line as soon as its sixteen bytes are there; close() writes the
        // line that is short and leaves out open. A failure of out is kept
        // for good: every later write and close reports it
        static tracked_ptr<dumper> dumper_to(const io::writer& out);
    };

    class hex::encoder final
    : public detail::CodecWriter<detail::Radix<4>> {
    public:
        using CodecWriter::CodecWriter;
    };

    class hex::decoder final
    : public detail::CodecReader<detail::Radix<4>> {
    public:
        using CodecReader::CodecReader;
    };

    inline tracked_ptr<hex::encoder> hex::encoder_to(const io::writer& out) {
        return make_tracked<encoder>(detail::HexLower, out);
    }

    inline tracked_ptr<hex::decoder> hex::decoder_from(const io::reader& in) {
        return make_tracked<decoder>(detail::HexLower, in);
    }

    class hex::dumper final
    : public io::mixin::writer<hex::dumper> {
    public:
        using io::mixin::writer<hex::dumper>::write;
        using io::mixin::writer<hex::dumper>::async_write;

        explicit dumper(const io::writer& out)
        : _out(out), _block(make_tracked<detail::CodecBlock>()) {
        }

        expected<size_t, io::error> write(const slice<const byte>& data) {
            if (_error) {
                return io::detail::fail(*_error);
            }
            if (_closed) {
                return io::detail::fail(io::error(io::errc::closed, "write", "hex dump"));
            }
            auto p = reinterpret_cast<const uint8_t*>(data.data());
            auto end = p + data.size();
            for (;;) {
                size_t n = _fill(p, end);
                if (n == 0) {
                    return data.size();
                }
                auto w = _out.write(_chunk(n));
                if (!w) {
                    _error = w.error();
                    return io::detail::fail(w);
                }
            }
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            if (_error) {
                co_return io::detail::fail(*_error);
            }
            if (_closed) {
                co_return io::detail::fail(io::error(io::errc::closed, "write", "hex dump"));
            }
            auto p = reinterpret_cast<const uint8_t*>(data.data());
            auto end = p + data.size();
            for (;;) {
                size_t n = _fill(p, end);
                if (n == 0) {
                    co_return data.size();
                }
                auto w = co_await _out.async_write(_chunk(n));
                if (!w) {
                    _error = w.error();
                    co_return io::detail::fail(w);
                }
            }
        }

        expected<void, io::error> close() {
            if (_closed || _error) {
                _closed = true;
                return _error ? expected<void, io::error>(io::detail::fail(*_error)) : expected<void, io::error>();
            }
            _closed = true;
            if (size_t n = _final()) {
                auto w = _out.write(_chunk(n));
                if (!w) {
                    _error = w.error();
                    return io::detail::fail(w);
                }
            }
            return {};
        }

        async::task<expected<void, io::error>> async_close() {
            if (_closed || _error) {
                _closed = true;
                co_return _error ? expected<void, io::error>(io::detail::fail(*_error)) : expected<void, io::error>();
            }
            _closed = true;
            if (size_t n = _final()) {
                auto w = co_await _out.async_write(_chunk(n));
                if (!w) {
                    _error = w.error();
                    co_return io::detail::fail(w);
                }
            }
            co_return expected<void, io::error>();
        }

        bool is_closed() const noexcept {
            return _closed;
        }

    private:
        char* _chars() const noexcept {
            return reinterpret_cast<char*>(_block->data());
        }

        slice<const byte> _chunk(size_t n) const noexcept {
            return slice<const byte>(_block, _block->data(), n);
        }

        // The whole lines the input from p makes, into the block while it
        // has room for one; bytes short of a line wait for the next write
        size_t _fill(const uint8_t*& p, const uint8_t* end) noexcept {
            char* o = _chars();
            char* o_end = o + _block->size();
            while (p != end && size_t(o_end - o) >= detail::DumpLineChars + 8) {
                size_t k = std::min(size_t(16 - _n), size_t(end - p));
                detail::copy_bytes(_line + _n, p, k);
                _n += uint8_t(k);
                p += k;
                if (_n == 16) {
                    o = detail::dump_line(o, _offset, _line, 16);
                    _offset += 16;
                    _n = 0;
                }
            }
            return size_t(o - _chars());
        }

        size_t _final() noexcept {
            if (_n == 0) {
                return 0;
            }
            char* o = detail::dump_line(_chars(), _offset, _line, _n);
            _offset += _n;
            _n = 0;
            return size_t(o - _chars());
        }

        io::writer _out;
        tracked_ptr<detail::CodecBlock> _block;
        uint64_t _offset = 0;
        uint8_t _line[16] {};
        uint8_t _n = 0;
        optional<io::error> _error;   // the first failure of the writer under it, for good
        bool _closed = false;
    };

    inline tracked_ptr<hex::dumper> hex::dumper_to(const io::writer& out) {
        return make_tracked<dumper>(out);
    }
}
