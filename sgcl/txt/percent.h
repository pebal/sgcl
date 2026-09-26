//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/string.h"

#include <string>

// The escaping of RFC 3986: the bytes a URL may not carry as they stand,
// written as %XX, and back.
//
// This stands on its own. It has no tables, it knows nothing about
// Unicode and nothing about domain names, and it is a header of its own
// rather than a corner of idna.h because nobody looking for percent
// encoding would think to open a file called idna. It is not a URL
// parser either — taking a URL apart is the business of net, and this is
// only the escaping that the parts of a URL need once they have been
// taken apart.
namespace sgcl::txt {
    // Which ASCII characters a percent encoding is to leave alone. This
    // is a value rather than a tag because there is no one answer: RFC
    // 3986 gives a different set for the path, for the query and for the
    // user information, a slash is data inside a segment and a separator
    // between them, and a program often has a set of its own. The sets
    // the RFC names are below, and they compose with |.
    class percent_set {
    public:
        constexpr percent_set() noexcept = default;

        // From the characters themselves, which is how the RFC writes
        // them: percent_set{"!$&'()*+,;="} is its sub-delims. Anything
        // above ASCII in the string is ignored — a byte that is not
        // ASCII is always encoded, since the escaping is over bytes and
        // a reader has no way to know what encoding they were
        constexpr explicit percent_set(const char* chars) noexcept {
            for (const char* p = chars; p && *p; ++p) {
                auto b = uint8_t(*p);
                if (b < 0x80) {
                    _bits[b >> 6] |= uint64_t(1) << (b & 63);
                }
            }
        }

        constexpr bool holds(char c) const noexcept {
            auto b = uint8_t(c);
            return b < 0x80 && ((_bits[b >> 6] >> (b & 63)) & 1) != 0;
        }

        constexpr percent_set operator|(const percent_set& other) const noexcept {
            percent_set out;
            out._bits[0] = _bits[0] | other._bits[0];
            out._bits[1] = _bits[1] | other._bits[1];
            return out;
        }

        // What is in this one and not in that one. RFC 3986 builds its
        // sets up from nothing and the WHATWG URL Standard builds its
        // own down from everything — "the query set and also the
        // question mark" — so both directions are here and each family
        // is written the way its own specification writes it
        constexpr percent_set operator-(const percent_set& other) const noexcept {
            percent_set out;
            out._bits[0] = _bits[0] & ~other._bits[0];
            out._bits[1] = _bits[1] & ~other._bits[1];
            return out;
        }

        constexpr bool operator==(const percent_set&) const noexcept = default;

    private:
        uint64_t _bits[2] {};
    };

    namespace percent {
        // Section 2.3: what never has to be escaped, wherever it stands
        inline constexpr percent_set unreserved {
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"
        };
        // Section 2.2: the characters a scheme may give a meaning of its
        // own inside a component
        inline constexpr percent_set sub_delims {"!$&'()*+,;="};
        // Section 3.3: a pchar, which is what one segment of a path is
        // made of. A slash encoded here stays inside the segment rather
        // than dividing it, which is the difference that matters when a
        // file name has one in it
        inline constexpr percent_set segment = unreserved | sub_delims | percent_set{":@"};
        inline constexpr percent_set path = segment | percent_set{"/"};
        // Section 3.4 and 3.5: a query and a fragment are pchar with the
        // slash and the question mark added
        inline constexpr percent_set query = path | percent_set{"?"};
        inline constexpr percent_set fragment = query;
        // Section 3.2.1
        inline constexpr percent_set userinfo = unreserved | sub_delims | percent_set{":"};

        // The other family, and the one a browser actually uses. The
        // WHATWG URL Standard does not follow RFC 3986 here: it escapes
        // less, so that names and paths already in the wild keep working,
        // and it writes its sets the other way round — as what is to be
        // escaped, built up from "C0 controls and everything above ~".
        // So these are written as subtractions, which is how the standard
        // reads, and the two families are kept side by side rather than
        // reconciled: a program parsing a URL the way a browser does
        // wants these, and a program writing a URI to a specification
        // that says RFC 3986 wants the ones above.
        //
        // The visible difference is that the WHATWG sets leave the
        // sub-delims alone in most places, so "a+b" and "a,b" go through
        // a path untouched where RFC 3986's `path` would also let them
        // and its `unreserved` would not.
        namespace whatwg {
            // The complement of the C0 control percent-encode set:
            // everything under U+0020 and over U+007E is escaped and
            // nothing else is — not even the space
            inline constexpr percent_set c0 {
                " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                "`abcdefghijklmnopqrstuvwxyz{|}~"
            };
            inline constexpr percent_set fragment = c0 - percent_set{" \"<>`"};
            // the query set is not the fragment set less anything: the
            // standard leaves the backquote out of it, and says so
            inline constexpr percent_set query = c0 - percent_set{" \"#<>"};
            inline constexpr percent_set special_query = query - percent_set{"'"};
            inline constexpr percent_set path = query - percent_set{"?^`{}"};
            inline constexpr percent_set userinfo = path - percent_set{"/:;=@[\\]|"};
            // What JavaScript's encodeURIComponent writes, character for
            // character: everything that can go into a path, a query or a
            // fragment and come back out the same, the per cent sign
            // included
            inline constexpr percent_set component = userinfo - percent_set{"$%&+,"};
            // application/x-www-form-urlencoded, which keeps only the
            // letters, the digits and * - . _ — the standard says so in
            // as many words and the test holds it to that. One thing it
            // does not do: a form encoder writes a space as '+', and
            // encode() writes %20. The substitution is the caller's, one
            // line, and it belongs to the form encoding and not to the
            // escaping
            inline constexpr percent_set form_urlencoded = component - percent_set{"!'()~"};
        }

        // The text with everything outside the set written as %XX. The
        // digits are upper case, which section 6.2.2.1 says a producer
        // should use, and the bytes are the text's own: UTF-8 here, one
        // escape a byte, which is what a URL carries.
        //
        // This cannot fail. A byte is either left alone or written as
        // three characters, and every byte has a spelling.
        inline string encode(const string& text, percent_set keep = unreserved) {
            auto v = text.view();
            std::string out;
            out.reserve(v.size());
            for (char c : v) {
                if (keep.holds(c)) {
                    out.push_back(c);
                    continue;
                }
                static constexpr char Digits[] = "0123456789ABCDEF";
                out.push_back('%');
                out.push_back(Digits[uint8_t(c) >> 4]);
                out.push_back(Digits[uint8_t(c) & 15]);
            }
            return string(out.data(), out.size());
        }

        // And back. Nothing comes back when a '%' is not followed by two
        // hexadecimal digits: a truncated escape is not a text with a
        // stray per cent sign in it, it is a text that was cut, and
        // letting it through is how a path traversal gets past a check
        // that ran before the decoding.
        //
        // What comes back is bytes and not code points. A caller that
        // knows they are UTF-8 has them; one that percent-decodes the
        // bytes of some other encoding has those, and turns them into
        // text with txt::decode.
        inline optional<string> decode(const string& text) {
            auto v = text.view();
            std::string out;
            out.reserve(v.size());
            auto nibble = [](char c) {
                if (c >= '0' && c <= '9') {
                    return c - '0';
                }
                if (c >= 'a' && c <= 'f') {
                    return c - 'a' + 10;
                }
                if (c >= 'A' && c <= 'F') {
                    return c - 'A' + 10;
                }
                return -1;
            };
            for (size_t i = 0; i < v.size(); ++i) {
                if (v[i] != '%') {
                    out.push_back(v[i]);
                    continue;
                }
                if (i + 2 >= v.size()) {
                    return nullopt;
                }
                int hi = nibble(v[i + 1]);
                int lo = nibble(v[i + 2]);
                if (hi < 0 || lo < 0) {
                    return nullopt;
                }
                out.push_back(char(hi * 16 + lo));
                i += 2;
            }
            return string(out.data(), out.size());
        }
    }
}
