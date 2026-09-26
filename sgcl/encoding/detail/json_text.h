//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "json_number.h"
#include "text_scan.h"
#include "../error.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;
    // The strings of JSON (RFC 8259 section 7) both ways: a string read,
    // its escapes decoded and its UTF-8 checked, in pieces when the text
    // comes in pieces; a string written with the escapes it needs.

    inline bool is_json_space(char c) noexcept {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t';
    }

    // Past the white space: byte by byte, which measured faster on the
    // indented corpora than eight at a time (the runs are short)
    inline const char* skip_space(const char* p, const char* end) noexcept {
        while (p != end && is_json_space(*p)) {
            ++p;
        }
        return p;
    }

    inline int hex_value(char c) noexcept {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        c = char(c | 0x20);
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        return -1;
    }

    // The length of a well-formed UTF-8 sequence at p, or 0 when the
    // sequence is not one (overlong, a surrogate, past U+10FFFF, a byte
    // that cannot start or continue it); -1 when the bytes before end are
    // a valid start and the rest has not come yet
    inline int utf8_sequence(const char* p, const char* end) noexcept {
        auto b0 = uint8_t(p[0]);
        int n;
        uint8_t lo = 0x80, hi = 0xBF;   // the range of the second byte
        if (b0 < 0x80) {
            return 1;
        } else if (b0 >= 0xC2 && b0 <= 0xDF) {
            n = 2;
        } else if (b0 >= 0xE0 && b0 <= 0xEF) {
            n = 3;
            if (b0 == 0xE0) {
                lo = 0xA0;
            } else if (b0 == 0xED) {
                hi = 0x9F;
            }
        } else if (b0 >= 0xF0 && b0 <= 0xF4) {
            n = 4;
            if (b0 == 0xF0) {
                lo = 0x90;
            } else if (b0 == 0xF4) {
                hi = 0x8F;
            }
        } else {
            return 0;
        }
        for (int k = 1; k < n; ++k) {
            if (p + k == end) {
                return -1;
            }
            auto b = uint8_t(p[k]);
            if (k == 1 ? (b < lo || b > hi) : (b < 0x80 || b > 0xBF)) {
                return 0;
            }
        }
        return n;
    }

    template<class Out>
    void append_utf8(Out& out, char32_t c) {
        char b[4];
        size_t n;
        if (c < 0x80) {
            b[0] = char(c);
            n = 1;
        } else if (c < 0x800) {
            b[0] = char(0xC0 | (c >> 6));
            b[1] = char(0x80 | (c & 0x3F));
            n = 2;
        } else if (c < 0x10000) {
            b[0] = char(0xE0 | (c >> 12));
            b[1] = char(0x80 | ((c >> 6) & 0x3F));
            b[2] = char(0x80 | (c & 0x3F));
            n = 3;
        } else {
            b[0] = char(0xF0 | (c >> 18));
            b[1] = char(0x80 | ((c >> 12) & 0x3F));
            b[2] = char(0x80 | ((c >> 6) & 0x3F));
            b[3] = char(0x80 | (c & 0x3F));
            n = 4;
        }
        out.append(b, n);
    }

    inline constexpr std::string_view Replacement = "\xEF\xBF\xBD";

    // Where the scan of a string is between two pieces of the text: once a
    // backslash (or, when invalid UTF-8 is let through, a byte replaced)
    // was met, the string's characters are the scratch buffer's, decoded
    // so far; before that they are the text's own, from the start
    struct StringScan {
        bool decoded = false;
    };

    // One step of a string, from p (after the opening quote, or where the
    // last step stopped) to the closing quote. start is the first byte of
    // the string's content. done: p is past the quote, and the string is
    // [start, p - 1) or, if state.decoded, the scratch. more: the data
    // ended (last is false) and p is where to go on from — the start of
    // an escape or of a UTF-8 sequence that is not whole yet, never inside
    // one — with the scratch holding what came before it. failed: p is
    // the byte at fault and code says what it is.
    template<class Out>
    ScanStatus scan_string(const char* start, const char*& p, const char* end, bool last, StringScan& state, Out& scratch, bool allow_invalid_utf8, errc& code) {
        auto to_scratch = [&] {
            if (!state.decoded) {
                state.decoded = true;
                scratch.clear();
                scratch.append(start, size_t(p - start));
            }
        };
        for (;;) {
            const char* q = json_string_stop(p, end);
            if (state.decoded) {
                scratch.append(p, size_t(q - p));
            }
            p = q;
            if (p == end) {
                if (last) {
                    code = errc::unexpected_end;
                    return ScanStatus::failed;
                }
                return ScanStatus::more;
            }
            auto c = uint8_t(*p);
            if (c == '"') {
                ++p;
                return ScanStatus::done;
            }
            if (c == '\\') {
                if (end - p < 2) {
                    if (last) {
                        code = errc::unexpected_end;
                        return ScanStatus::failed;
                    }
                    return ScanStatus::more;
                }
                char e = p[1];
                char simple = 0;
                switch (e) {
                    case '"': simple = '"'; break;
                    case '\\': simple = '\\'; break;
                    case '/': simple = '/'; break;
                    case 'b': simple = '\b'; break;
                    case 'f': simple = '\f'; break;
                    case 'n': simple = '\n'; break;
                    case 'r': simple = '\r'; break;
                    case 't': simple = '\t'; break;
                    case 'u': break;
                    default:
                        code = errc::invalid_escape;
                        return ScanStatus::failed;
                }
                to_scratch();
                if (simple) {
                    scratch.push_back(simple);
                    p += 2;
                    continue;
                }
                // \uXXXX, and a second one when the first is a high surrogate
                auto hex4 = [&](const char* h, char32_t& v) -> int {
                    // 1: four digits; 0: not; -1: cut short
                    v = 0;
                    for (int k = 0; k < 4; ++k) {
                        if (h + k == end) {
                            return -1;
                        }
                        int d = hex_value(h[k]);
                        if (d < 0) {
                            return 0;
                        }
                        v = v * 16 + char32_t(d);
                    }
                    return 1;
                };
                char32_t u;
                int r = hex4(p + 2, u);
                if (r <= 0) {
                    if (r < 0 && !last) {
                        return ScanStatus::more;
                    }
                    code = r < 0 ? errc::unexpected_end : errc::invalid_escape;
                    return ScanStatus::failed;
                }
                if (u >= 0xD800 && u <= 0xDBFF) {
                    // a low surrogate must follow as \uXXXX
                    const char* s = p + 6;
                    if (end - s < 2) {
                        if (!last) {
                            return ScanStatus::more;
                        }
                        if (!allow_invalid_utf8) {
                            code = errc::invalid_escape;
                            return ScanStatus::failed;
                        }
                        scratch.append(Replacement.data(), Replacement.size());
                        p += 6;
                        continue;
                    }
                    char32_t low = 0;
                    int r2 = s[0] == '\\' && s[1] == 'u' ? hex4(s + 2, low) : 0;
                    if (r2 < 0) {
                        if (!last) {
                            return ScanStatus::more;
                        }
                        code = errc::unexpected_end;
                        return ScanStatus::failed;
                    }
                    if (r2 == 1 && low >= 0xDC00 && low <= 0xDFFF) {
                        append_utf8(scratch, 0x10000 + ((u - 0xD800) << 10) + (low - 0xDC00));
                        p += 12;
                        continue;
                    }
                    if (!allow_invalid_utf8) {
                        code = errc::invalid_escape;
                        return ScanStatus::failed;
                    }
                    scratch.append(Replacement.data(), Replacement.size());
                    p += 6;
                    continue;
                }
                if (u >= 0xDC00 && u <= 0xDFFF) {
                    if (!allow_invalid_utf8) {
                        code = errc::invalid_escape;
                        return ScanStatus::failed;
                    }
                    scratch.append(Replacement.data(), Replacement.size());
                    p += 6;
                    continue;
                }
                append_utf8(scratch, u);
                p += 6;
                continue;
            }
            if (c < 0x20) {
                code = errc::syntax;
                return ScanStatus::failed;
            }
            // UTF-8: whole sequences, as many as there are in a row
            for (;;) {
                int n = utf8_sequence(p, end);
                if (n < 0) {
                    if (!last) {
                        return ScanStatus::more;
                    }
                    n = 0;
                }
                if (n == 0) {
                    if (!allow_invalid_utf8) {
                        code = errc::invalid_utf8;
                        return ScanStatus::failed;
                    }
                    to_scratch();
                    scratch.append(Replacement.data(), Replacement.size());
                    ++p;
                } else {
                    if (state.decoded) {
                        scratch.append(p, size_t(n));
                    }
                    p += n;
                }
                if (p == end || uint8_t(*p) < 0x80) {
                    break;
                }
            }
        }
    }

    // One of the words true, false and null at p, whose first letter is
    // there: done with p past it, more when the text so far is its start,
    // failed with p at the first letter that is wrong
    inline ScanStatus scan_word(const char*& p, const char* end, bool last, std::string_view word) noexcept {
        size_t i = 0;
        for (; i < word.size(); ++i) {
            if (p + i == end) {
                if (!last) {
                    return ScanStatus::more;
                }
                p += i;
                return ScanStatus::failed;
            }
            if (p[i] != word[i]) {
                p += i;
                return ScanStatus::failed;
            }
        }
        p += word.size();
        return ScanStatus::done;
    }

    // --- writing ---

    inline constexpr char HexDigits[] = "0123456789abcdef";

    // The string s as a JSON string, quotes included, into out: '"' and
    // '\\' escaped, the control characters as \b \f \n \r \t or \u00XX,
    // with escape_html also '<', '>' and '&' as <... (for text that
    // lands in HTML); a byte that is not UTF-8 as �, as Go writes it,
    // since a text written must be one that can be read
    template<class Out>
    void write_string(Out& out, std::string_view s, bool escape_html) {
        out.push_back('"');
        const char* p = s.data();
        const char* end = p + s.size();
        while (p != end) {
            const char* q = json_string_stop(p, end);
            if (escape_html) {
                const char* h = p;
                while (h != q && *h != '<' && *h != '>' && *h != '&') {
                    ++h;
                }
                q = h;
            }
            out.append(p, size_t(q - p));
            p = q;
            if (p == end) {
                break;
            }
            auto c = uint8_t(*p);
            if (c >= 0x80) {
                int n = utf8_sequence(p, end);
                if (n <= 0) {
                    out.append(Replacement.data(), Replacement.size());
                    ++p;
                } else {
                    out.append(p, size_t(n));
                    p += n;
                }
                continue;
            }
            switch (c) {
                case '"': out.append("\\\"", 2); break;
                case '\\': out.append("\\\\", 2); break;
                case '\b': out.append("\\b", 2); break;
                case '\f': out.append("\\f", 2); break;
                case '\n': out.append("\\n", 2); break;
                case '\r': out.append("\\r", 2); break;
                case '\t': out.append("\\t", 2); break;
                default: {
                    char u[6] = {'\\', 'u', '0', '0', HexDigits[c >> 4], HexDigits[c & 15]};
                    out.append(u, 6);
                }
            }
            ++p;
        }
        out.push_back('"');
    }
}
