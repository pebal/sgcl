//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/utf8.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;

    // The characters of XML 1.0 (fifth edition), section 2.2 and 2.3: what
    // a document may hold at all (Char), what a name may start with and
    // go on with. Ranges straight from the productions — a dozen of them,
    // no table of Unicode properties: the fifth edition made names a
    // matter of blocks, not of letters.

    // [2] Char: #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF]
    constexpr bool xml_char(char32_t c) noexcept {
        return c >= 0x20 ? (c < 0xD800 || (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF))
                         : (c == 0x9 || c == 0xA || c == 0xD);
    }

    // [4] NameStartChar
    constexpr bool xml_name_start(char32_t c) noexcept {
        if (c < 0x80) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':';
        }
        return (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) || (c >= 0xF8 && c <= 0x2FF)
            || (c >= 0x370 && c <= 0x37D) || (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D)
            || (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) || (c >= 0x3001 && c <= 0xD7FF)
            || (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
    }

    // [4a] NameChar
    constexpr bool xml_name_char(char32_t c) noexcept {
        if (c < 0x80) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                || c == '_' || c == ':' || c == '-' || c == '.';
        }
        return xml_name_start(c) || c == 0xB7 || (c >= 0x300 && c <= 0x36F) || (c >= 0x203F && c <= 0x2040);
    }

    // What a byte is, for the loops that go over the bytes of a document:
    // one load of a table and one test, the byte of ASCII that needs no
    // more thought taken without a second look
    enum XmlByte : uint8_t {
        XmlText = 1,        // copied as it is in character data: not < & ] \r, not a control, not past 127
        XmlValue = 2,       // copied as it is in an attribute's value: not < & " ' and no white space but ' '
        XmlNameStart = 4,   // an ASCII NameStartChar
        XmlNameChar = 8,    // an ASCII NameChar
        XmlSpace = 16,      // [3] S: #x20 | #x9 | #xD | #xA
        XmlPlain = 32       // copied as it is in a comment, an instruction or a CDATA section: not \r, - ? ], no control, not past 127
    };

    inline constexpr std::array<uint8_t, 256> XmlBytes = [] {
        std::array<uint8_t, 256> t {};
        for (int c = 0; c < 128; ++c) {
            uint8_t f = 0;
            bool printable = c >= 0x20 || c == '\t' || c == '\n';
            if (printable && c != '<' && c != '&' && c != ']') {
                f |= XmlText;
            }
            if (c >= 0x20 && c != '<' && c != '&' && c != '"' && c != '\'') {
                f |= XmlValue;
            }
            if (xml_name_start(char32_t(c))) {
                f |= XmlNameStart;
            }
            if (xml_name_char(char32_t(c))) {
                f |= XmlNameChar;
            }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                f |= XmlSpace;
            }
            if (printable && c != '-' && c != '?' && c != ']') {
                f |= XmlPlain;
            }
            t[size_t(c)] = f;
        }
        return t;
    }();

    constexpr bool xml_space(char c) noexcept {
        return XmlBytes[uint8_t(c)] & XmlSpace;
    }

    // A strict step of UTF-8: the code point at p and the bytes it takes,
    // or a width of 0 when the bytes there are not a valid sequence (an
    // overlong form, a surrogate, a byte that starts nothing, a sequence
    // cut by the end)
    struct XmlRune {
        char32_t c;
        uint32_t width;
    };

    inline XmlRune xml_rune(const char* p, const char* end) noexcept {
        auto [c, n] = utf8::decode(std::string_view(p, size_t(end - p)), 0);
        if (c == utf8::replacement && n == 1) {
            return {c, 0};
        }
        return {c, uint32_t(n)};
    }

    // Whether the whole of s is a Name ([5]); with namespaces, whether it
    // is an NCName (no colon) or a QName (one colon, not at either end)
    inline bool xml_name(std::string_view s) noexcept {
        if (s.empty()) {
            return false;
        }
        const char* p = s.data();
        const char* e = p + s.size();
        bool first = true;
        while (p < e) {
            char32_t c;
            uint32_t w;
            if (uint8_t(*p) < 0x80) {
                c = char32_t(uint8_t(*p));
                w = 1;
            } else {
                auto r = xml_rune(p, e);
                if (!r.width) {
                    return false;
                }
                c = r.c;
                w = r.width;
            }
            if (first ? !xml_name_start(c) : !xml_name_char(c)) {
                return false;
            }
            first = false;
            p += w;
        }
        return true;
    }

    inline bool xml_ncname(std::string_view s) noexcept {
        return xml_name(s) && s.find(':') == std::string_view::npos;
    }

    inline bool xml_qname(std::string_view s) noexcept {
        if (!xml_name(s)) {
            return false;
        }
        auto colon = s.find(':');
        if (colon == std::string_view::npos) {
            return true;
        }
        return colon != 0 && colon + 1 < s.size() && s.find(':', colon + 1) == std::string_view::npos
            && xml_name_start(utf8::decode(s, colon + 1).first);
    }

    // Whether a string already known to be a Name is a QName: one colon at
    // the most, not at either end, a NameStartChar after it
    inline bool xml_qname_of_name(std::string_view s) noexcept {
        auto colon = s.find(':');
        if (colon == std::string_view::npos) {
            return true;
        }
        return colon != 0 && colon + 1 < s.size() && s.find(':', colon + 1) == std::string_view::npos
            && xml_name_start(utf8::decode(s, colon + 1).first);
    }

    // The two namespaces a document never declares (Namespaces in XML
    // 1.0, section 3)
    inline constexpr std::string_view XmlNamespace = "http://www.w3.org/XML/1998/namespace";
    inline constexpr std::string_view XmlnsNamespace = "http://www.w3.org/2000/xmlns/";
}
