//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once
#include "../core/detail/bytes.h"

#include "../containers/vector.h"
#include "properties.h"
#include "detail/encoding_tables.h"

#include <cstring>

// Getting text in and out of the shapes other people keep it in: the two
// other encodings of Unicode itself (UTF-16 at the edge of a Windows
// system call, UTF-32 where a code point is an integer) and the single
// byte encodings a file or a web page may still arrive in. The library
// keeps text in UTF-8 and nothing here changes that: everything decodes
// to a string and everything encodes from one.
//
// This is the one header of the module whose clearest use is the case
// where there is no Unicode yet — bytes in iso-8859-2 out of an HTTP
// header, a file written by a program from the nineties.
namespace sgcl::txt {
    // An encoding is a value, not a tag, and this is the one place in the
    // module where that is so: the name comes out of a header at run time
    // (`charset=iso-8859-2`), so nothing can be chosen when the program is
    // built.
    //
    // The set is the single byte encodings the Encoding Standard of the
    // WHATWG lists — what a browser must understand, which is what a page
    // or a file may still arrive in — with the two other encodings of
    // Unicode itself, ASCII and ISO-8859-1 beside them. The enum, the
    // names, the aliases and the tables all come out of one list in the
    // generator, so none of them can drift from the others.
    using detail::encoding;

    // The name a header would use for it
    constexpr const char* name_of(encoding e) noexcept {
        return detail::EncodingNames[uint8_t(e)];
    }

    namespace detail {
        // A name as a header writes it, which is to say in any case, with
        // or without the dashes, and under any of the aliases people use
        constexpr bool same_name(std::string_view a, std::string_view b) noexcept {
            size_t i = 0, j = 0;
            auto skip = [](std::string_view s, size_t& k) {
                while (k < s.size() && (s[k] == '-' || s[k] == '_' || s[k] == ' ')) {
                    ++k;
                }
            };
            for (;;) {
                skip(a, i);
                skip(b, j);
                if (i == a.size() || j == b.size()) {
                    return i == a.size() && j == b.size();
                }
                char x = a[i] >= 'A' && a[i] <= 'Z' ? char(a[i] + 32) : a[i];
                char y = b[j] >= 'A' && b[j] <= 'Z' ? char(b[j] + 32) : b[j];
                if (x != y) {
                    return false;
                }
                ++i;
                ++j;
            }
        }

        constexpr bool single_byte(encoding e) noexcept {
            return uint8_t(e) >= FirstSingleByte;
        }

        constexpr const SingleByte& table_of(encoding e) noexcept {
            return SingleBytes[uint8_t(e) - FirstSingleByte];
        }

        // The byte that writes this code point in that encoding, or -1.
        // One probe and a little more on the average of the 27 encodings
        // (1.13 measured, 8 at the worst), where the sorted list this
        // replaced wanted a binary search for every character written.
        constexpr int single_byte_of(char32_t c, encoding e) noexcept {
            if (c >= 0x10000) {
                return -1;                 // no single byte encoding goes there
            }
            auto& t = table_of(e);
            for (uint32_t i = ((uint32_t(c) * 2654435761u) >> 24) & 255; ; i = (i + 1) & 255) {
                uint16_t key = t.keys[i];
                if (key == c) {
                    return t.bytes[i];
                }
                if (!key) {
                    return -1;
                }
            }
        }

        inline void put(std::string& out, char32_t c) {
            char buf[utf8::max_width];
            out.append(buf, utf8::encode(c, buf));
        }

        // The same into a buffer already sized, which is what decoding
        // does: a code point of the Basic Multilingual Plane takes three
        // bytes at the most and one outside it takes four, so three bytes
        // an input byte covers every encoding here and the rest is given
        // back when the text is made.
        inline void put(char*& at, char32_t c) noexcept {
            at += utf8::encode(c, at);
        }

        // A unit of UTF-16 from two bytes, and of UTF-32 from four
        constexpr uint32_t unit(const std::byte* p, size_t n, bool big) noexcept {
            uint32_t v = 0;
            for (size_t i = 0; i < n; ++i) {
                v |= uint32_t(uint8_t(p[big ? i : n - 1 - i])) << (8 * (n - 1 - i));
            }
            return v;
        }
    }

    // What a byte order mark at the front says, and how many bytes it
    // takes. Nothing else in this header looks at one: a caller that
    // wants it honoured skips those bytes itself, which keeps the
    // decision where it belongs.
    struct byte_order_mark {
        optional<encoding> says;
        size_t size = 0;

        explicit operator bool() const noexcept {
            return says.has_value();
        }
    };

    inline byte_order_mark detect_bom(slice<const std::byte> bytes) noexcept {
        auto at = [&](size_t i) { return i < bytes.size() ? uint8_t(bytes[i]) : 0x100u; };
        if (at(0) == 0xEF && at(1) == 0xBB && at(2) == 0xBF) {
            return {encoding::utf8, 3};
        }
        if (at(0) == 0xFF && at(1) == 0xFE) {
            if (at(2) == 0x00 && at(3) == 0x00) {
                return {encoding::utf32le, 4};
            }
            return {encoding::utf16le, 2};
        }
        if (at(0) == 0xFE && at(1) == 0xFF) {
            return {encoding::utf16be, 2};
        }
        if (at(0) == 0x00 && at(1) == 0x00 && at(2) == 0xFE && at(3) == 0xFF) {
            return {encoding::utf32be, 4};
        }
        return {};
    }

    // The encoding a name stands for, or nothing when nobody knows it.
    // Not a result: a name out of a header that means nothing is an
    // ordinary answer, not a failure of the operation
    inline optional<encoding> encoding_from_name(const string& name) noexcept {
        for (auto& e : detail::EncodingAliases) {
            if (detail::same_name(name.view(), e.name)) {
                return e.value;
            }
        }
        return nullopt;
    }

    // Bytes in some encoding as text. A byte that means nothing in it is
    // one replacement character, as an invalid byte of UTF-8 is: nothing
    // is refused and nothing is thrown.
    inline string decode(slice<const std::byte> bytes, encoding from) {
        // Three bytes an input byte covers every encoding here: a byte of
        // a single byte encoding stands for a code point of the Basic
        // Multilingual Plane, a pair of UTF-16 units for one outside it,
        // and UTF-8 passes through as it is. Written through a pointer
        // rather than appended a character at a time, which costs ten
        // times as much, and the rest is given back when the text is made.
        std::string out(bytes.size() * 3, '\0');
        char* at = out.data();
        auto* p = bytes.data();
        size_t n = bytes.size();
        switch (from) {
            case encoding::utf8: {
                std::string_view v(reinterpret_cast<const char*>(p), n);
                for (size_t i = 0; i < v.size();) {
                    auto [c, w] = utf8::decode(v, i);
                    detail::put(at, c);
                    i += w;
                }
                break;
            }
            case encoding::utf16le:
            case encoding::utf16be: {
                bool big = from == encoding::utf16be;
                for (size_t i = 0; i + 1 < n; i += 2) {
                    uint32_t u = detail::unit(p + i, 2, big);
                    if (u >= 0xD800 && u < 0xDC00 && i + 3 < n) {
                        uint32_t low = detail::unit(p + i + 2, 2, big);
                        if (low >= 0xDC00 && low < 0xE000) {
                            detail::put(at, char32_t(0x10000 + ((u - 0xD800) << 10) + (low - 0xDC00)));
                            i += 2;
                            continue;
                        }
                    }
                    detail::put(at, u >= 0xD800 && u < 0xE000 ? utf8::replacement : char32_t(u));
                }
                if (n % 2) {
                    detail::put(at, utf8::replacement);
                }
                break;
            }
            case encoding::utf32le:
            case encoding::utf32be: {
                bool big = from == encoding::utf32be;
                for (size_t i = 0; i + 3 < n; i += 4) {
                    uint32_t u = detail::unit(p + i, 4, big);
                    detail::put(at, utf8::valid(char32_t(u)) ? char32_t(u) : utf8::replacement);
                }
                if (n % 4) {
                    detail::put(at, utf8::replacement);
                }
                break;
            }
            case encoding::ascii:
                for (size_t i = 0; i < n; ++i) {
                    uint8_t b = uint8_t(p[i]);
                    detail::put(at, b < 0x80 ? char32_t(b) : utf8::replacement);
                }
                break;
            case encoding::latin1:
                for (size_t i = 0; i < n; ++i) {
                    detail::put(at, char32_t(uint8_t(p[i])));
                }
                break;
            default: {
                // Every encoding of the set writes ASCII as itself (the
                // generator asserts it), so a run of bytes under 128 is
                // already the UTF-8 it decodes to and is taken whole
                auto* table = detail::table_of(from).to_unicode;
                for (size_t i = 0; i < n;) {
                    size_t run = i;
                    while (run < n && uint8_t(p[run]) < 0x80) {
                        ++run;
                    }
                    if (run > i) {
                        sgcl::detail::copy_bytes(at, p + i, run - i);
                        at += run - i;
                        i = run;
                        continue;
                    }
                    detail::put(at, char32_t(table[uint8_t(p[i])]));
                    ++i;
                }
                break;
            }
        }
        return string(out.data(), size_t(at - out.data()));
    }

    // Text as bytes in some encoding. A character the encoding cannot
    // write is a question mark, which is what every library that does
    // this has always done and what a reader can at least see
    inline vector<std::byte> encode(const string& text, encoding to) {
        vector<std::byte> out;
        auto v = text.view();
        // Sized once and written through, as to_utf16 is. The room is
        // taken from the length of the text and not from counting its
        // code points, because counting is a pass of its own and costs
        // more than it saves: a code point is at least one byte here, so
        // it is at most one byte there in an encoding of one byte, two
        // in UTF-16 and four in UTF-32. What is not used is given back at
        // the end.
        out.resize(to == encoding::utf16le || to == encoding::utf16be ? v.size() * 2
                 : to == encoding::utf32le || to == encoding::utf32be ? v.size() * 4
                 : v.size());
        auto* at = out.data();
        auto byte = [&](uint32_t b) { *at++ = std::byte(uint8_t(b)); };
        auto unit16 = [&](uint32_t u, bool big) {
            byte(big ? u >> 8 : u & 0xFF);
            byte(big ? u & 0xFF : u >> 8);
        };
        for (size_t i = 0; i < v.size();) {
            auto [c, w] = utf8::decode(v, i);
            i += w;
            switch (to) {
                case encoding::utf8: {
                    char buf[utf8::max_width];
                    size_t k = utf8::encode(c, buf);
                    for (size_t j = 0; j < k; ++j) {
                        byte(uint8_t(buf[j]));
                    }
                    break;
                }
                case encoding::utf16le:
                case encoding::utf16be: {
                    bool big = to == encoding::utf16be;
                    if (c < 0x10000) {
                        unit16(uint32_t(c), big);
                    } else {
                        uint32_t x = uint32_t(c) - 0x10000;
                        unit16(0xD800 + (x >> 10), big);
                        unit16(0xDC00 + (x & 0x3FF), big);
                    }
                    break;
                }
                case encoding::utf32le:
                case encoding::utf32be: {
                    bool big = to == encoding::utf32be;
                    for (int k = 0; k < 4; ++k) {
                        byte((uint32_t(c) >> (8 * (big ? 3 - k : k))) & 0xFF);
                    }
                    break;
                }
                case encoding::ascii:
                    byte(c < 0x80 ? uint32_t(c) : uint32_t('?'));
                    break;
                case encoding::latin1:
                    byte(c < 0x100 ? uint32_t(c) : uint32_t('?'));
                    break;
                default: {
                    int b = c < 0x80 ? int(c) : detail::single_byte_of(c, to);
                    byte(b >= 0 ? uint32_t(b) : uint32_t('?'));
                    break;
                }
            }
        }
        out.resize(size_t(at - out.data()));
        return out;
    }

    // The two encodings of Unicode that have a shape of their own in C++,
    // for the edge of a system call and for the code that wants a code
    // point to be an integer
    inline vector<char16_t> to_utf16(const string& text) {
        auto v = text.view();
        // Sized once and written through: a unit a time through push_back
        // costs ten times what the writing does (0.77 ns an element
        // against 0.07), and no code point takes more units than its
        // bytes. What is not used is given back at the end.
        vector<char16_t> out;
        out.resize(v.size());
        auto* p = out.data();
        size_t n = 0;
        for (size_t i = 0; i < v.size();) {
            for (size_t run = utf8::ascii_run(v, i); run--; ++i) {
                p[n++] = char16_t(uint8_t(v[i]));
            }
            if (i >= v.size()) {
                break;
            }
            auto [c, w] = utf8::decode(v, i);
            i += w;
            if (c < 0x10000) {
                p[n++] = char16_t(c);
            } else {
                uint32_t x = uint32_t(c) - 0x10000;
                p[n++] = char16_t(0xD800 + (x >> 10));
                p[n++] = char16_t(0xDC00 + (x & 0x3FF));
            }
        }
        out.resize(n);
        return out;
    }

    inline string from_utf16(slice<const char16_t> units) {
        std::string out(units.size() * 3, '\0');
        char* at = out.data();
        for (size_t i = 0; i < units.size(); ++i) {
            // a unit under 128 is one byte of UTF-8 and nothing else
            if (units[i] < 0x80) {
                *at++ = char(units[i]);
                continue;
            }
            char32_t c = units[i];
            if (c >= 0xD800 && c < 0xDC00 && i + 1 < units.size()
                && units[i + 1] >= 0xDC00 && units[i + 1] < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (units[i + 1] - 0xDC00);
                ++i;
            } else if (c >= 0xD800 && c < 0xE000) {
                c = utf8::replacement;                 // a surrogate on its own
            }
            detail::put(at, c);
        }
        return string(out.data(), size_t(at - out.data()));
    }

    inline vector<char32_t> to_utf32(const string& text) {
        auto v = text.view();
        vector<char32_t> out;
        out.resize(v.size());                       // a code point a byte at the most
        auto* p = out.data();
        size_t n = 0;
        for (size_t i = 0; i < v.size();) {
            for (size_t run = utf8::ascii_run(v, i); run--; ++i) {
                p[n++] = char32_t(uint8_t(v[i]));
            }
            if (i >= v.size()) {
                break;
            }
            auto [c, w] = utf8::decode(v, i);
            p[n++] = c;
            i += w;
        }
        out.resize(n);
        return out;
    }

    inline string from_utf32(slice<const char32_t> points) {
        std::string out;
        out.reserve(points.size());
        for (auto c : points) {
            detail::put(out, utf8::valid(c) ? c : utf8::replacement);
        }
        return string(out.data(), out.size());
    }
}
