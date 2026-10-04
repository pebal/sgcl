//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "der.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// The distinguished names of X.509 (RFC 5280 §4.1.2.4, X.501): a SEQUENCE
// of RelativeDistinguishedNames, each a SET of attributes, each a type (an
// OBJECT IDENTIFIER) and a value. The values of the six string types of
// RFC 5280 are read into UTF-8 as Go's x509 reads them (PrintableString
// with '*' and '&', which real certificates hold; T61String as Latin-1;
// BMPString as UCS-2, its surrogates and noncharacters refused; UTF8String,
// IA5String and NumericString checked); a value of another type is kept as
// its DER and written as '#' and its hex, as RFC 4514 §2.4 has it.
namespace sgcl::crypto::detail {
    // "2.5.4.3" of the content of an OBJECT IDENTIFIER that oid_valid took
    inline std::string oid_text(const unsigned char* p, size_t n) noexcept {
        std::string s;
        uint64_t arc = 0;
        bool first = true;
        for (size_t i = 0; i < n; ++i) {
            arc = arc << 7 | (p[i] & 0x7f);
            if (p[i] & 0x80) {
                continue;
            }
            if (first) {
                unsigned top = arc < 40 ? 0 : arc < 80 ? 1 : 2;
                s += char('0' + top);
                s += '.';
                s += std::to_string(arc - 40 * top);
                first = false;
            } else {
                s += '.';
                s += std::to_string(arc);
            }
            arc = 0;
        }
        return s;
    }

    inline void append_utf8(std::string& s, uint32_t c) noexcept {
        if (c < 0x80) {
            s += char(c);
        } else if (c < 0x800) {
            s += char(0xc0 | c >> 6);
            s += char(0x80 | (c & 0x3f));
        } else if (c < 0x10000) {
            s += char(0xe0 | c >> 12);
            s += char(0x80 | (c >> 6 & 0x3f));
            s += char(0x80 | (c & 0x3f));
        } else {
            s += char(0xf0 | c >> 18);
            s += char(0x80 | (c >> 12 & 0x3f));
            s += char(0x80 | (c >> 6 & 0x3f));
            s += char(0x80 | (c & 0x3f));
        }
    }

    // Whether the bytes are UTF-8 as Go's utf8.Valid has it: no overlong
    // form, no surrogate, nothing above U+10FFFF
    inline bool utf8_valid(const unsigned char* p, size_t n) noexcept {
        for (size_t i = 0; i < n;) {
            unsigned char b = p[i];
            if (b < 0x80) {
                ++i;
                continue;
            }
            size_t k;
            uint32_t c;
            if (b >= 0xc2 && b <= 0xdf) {
                k = 1;
                c = b & 0x1f;
            } else if (b >= 0xe0 && b <= 0xef) {
                k = 2;
                c = b & 0x0f;
            } else if (b >= 0xf0 && b <= 0xf4) {
                k = 3;
                c = b & 0x07;
            } else {
                return false;
            }
            if (k > n - i - 1) {
                return false;
            }
            for (size_t j = 1; j <= k; ++j) {
                if ((p[i + j] & 0xc0) != 0x80) {
                    return false;
                }
                c = c << 6 | (p[i + j] & 0x3f);
            }
            if ((k == 2 && (c < 0x800 || (c >= 0xd800 && c <= 0xdfff))) || (k == 3 && (c < 0x10000 || c > 0x10ffff))) {
                return false;
            }
            i += k + 1;
        }
        return true;
    }

    inline bool ia5_valid(const unsigned char* p, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            if (p[i] >= 0x80) {
                return false;
            }
        }
        return true;
    }

    // The set of PrintableString, and the two characters outside it that
    // certificates hold in one: '*' (wildcards) and '&'
    SGCL_INLINE_HOT bool printable_char(unsigned char b) noexcept {
        return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9') || (b >= '\'' && b <= ')') || (b >= '+' && b <= '/')
            || b == ' ' || b == ':' || b == '=' || b == '?' || b == '*' || b == '&';
    }

    // The text of a value of one of the six string types, in UTF-8: true
    // and the text in out when tag is one of them and the value is valid
    // for it; false with is_string set when it is one of them and is not
    // valid (the name, and the certificate, cannot be read); false with
    // is_string clear for any other type
    inline bool asn1_string(unsigned char tag, const unsigned char* p, size_t n, std::string& out, bool& is_string) noexcept {
        is_string = true;
        out.clear();
        switch (tag) {
            case der::printable_string:
                for (size_t i = 0; i < n; ++i) {
                    if (!printable_char(p[i])) {
                        return false;
                    }
                }
                out.assign(reinterpret_cast<const char*>(p), n);
                return true;
            case der::utf8_string:
                if (!utf8_valid(p, n)) {
                    return false;
                }
                out.assign(reinterpret_cast<const char*>(p), n);
                return true;
            case der::t61_string:
                for (size_t i = 0; i < n; ++i) {
                    append_utf8(out, p[i]);   // as Latin-1, as BoringSSL and Go read it
                }
                return true;
            case der::bmp_string: {
                if (n % 2 != 0) {
                    return false;
                }
                if (n >= 2 && p[n - 1] == 0 && p[n - 2] == 0) {
                    n -= 2;   // a terminator
                }
                for (size_t i = 0; i < n; i += 2) {
                    uint32_t c = uint32_t(p[i]) << 8 | p[i + 1];
                    if (c == 0xfffe || c == 0xffff || (c >= 0xfdd0 && c <= 0xfdef) || (c >= 0xd800 && c <= 0xdfff)) {
                        return false;
                    }
                    append_utf8(out, c);
                }
                return true;
            }
            case der::ia5_string:
                if (!ia5_valid(p, n)) {
                    return false;
                }
                out.assign(reinterpret_cast<const char*>(p), n);
                return true;
            case der::numeric_string:
                for (size_t i = 0; i < n; ++i) {
                    if (!((p[i] >= '0' && p[i] <= '9') || p[i] == ' ')) {
                        return false;
                    }
                }
                out.assign(reinterpret_cast<const char*>(p), n);
                return true;
            default:
                is_string = false;
                return false;
        }
    }

    inline std::string hex_text(const unsigned char* p, size_t n) noexcept {
        static constexpr char digits[] = "0123456789abcdef";
        std::string s;
        s.reserve(2 * n);
        for (size_t i = 0; i < n; ++i) {
            s += digits[p[i] >> 4];
            s += digits[p[i] & 15];
        }
        return s;
    }
}

namespace sgcl::crypto::x509 {
    class name;

    namespace detail {
        using namespace sgcl::crypto::detail;
        bool parse_name(DerReader& in, name& out) noexcept;
    }

    // A distinguished name (RFC 5280 §4.1.2.4): the issuer or the subject
    // of a certificate. Its attributes in the order of the encoding, and the
    // common ones by name, as Go's pkix.Name has them: common_name() the
    // last CN, country() every C, and so on. A value; the bytes it was read
    // from are the certificate's raw_subject() and raw_issuer(), which is
    // what chains are built by.
    class name {
    public:
        // One attribute: its type as a dotted OID ("2.5.4.3" for CN) and
        // its value, as text when it is one of the string types (text is
        // true), else '#' and the hex of its DER
        struct attribute {
            string oid;
            string value;
            bool text = false;
        };

        name() = default;

        SGCL_INLINE_HOT const vector<attribute>& attributes() const noexcept {
            return _attributes;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _attributes.empty();
        }

        // CN, the last one where there are more, as Go's CommonName
        SGCL_INLINE_HOT string common_name() const noexcept {
            return _last("2.5.4.3");
        }

        // SERIALNUMBER (2.5.4.5), the last one
        SGCL_INLINE_HOT string serial_number() const noexcept {
            return _last("2.5.4.5");
        }

        SGCL_INLINE_HOT vector<string> country() const noexcept {
            return _all("2.5.4.6");
        }

        SGCL_INLINE_HOT vector<string> organization() const noexcept {
            return _all("2.5.4.10");
        }

        SGCL_INLINE_HOT vector<string> organizational_unit() const noexcept {
            return _all("2.5.4.11");
        }

        SGCL_INLINE_HOT vector<string> locality() const noexcept {
            return _all("2.5.4.7");
        }

        // ST, the state or province
        SGCL_INLINE_HOT vector<string> province() const noexcept {
            return _all("2.5.4.8");
        }

        SGCL_INLINE_HOT vector<string> street_address() const noexcept {
            return _all("2.5.4.9");
        }

        SGCL_INLINE_HOT vector<string> postal_code() const noexcept {
            return _all("2.5.4.17");
        }

        // The name as Go's pkix.Name.String() writes it, roughly RFC 2253:
        // "CN=www.example.com,O=Example\, Inc.,C=US". The common attributes
        // in the fixed order SERIALNUMBER, CN, OU, O, POSTALCODE, STREET,
        // L, ST, C (the values of one type joined by '+', in reverse of the
        // encoding's order), then every other attribute as its dotted OID
        // in reverse of the encoding's order; ',', '+', '"', '\', '<', '>',
        // ';', a leading '#' and a leading or trailing space escaped with
        // '\'
        string to_string() const noexcept {
            // the RDNs Go's String builds, in its order; written reversed
            struct Rdn {
                std::string text;
            };
            std::vector<Rdn> rdns;
            for (auto& a : _attributes) {
                if (!_is_common(a.oid)) {
                    Rdn r;
                    r.text.append(a.oid.data(), a.oid.size());
                    r.text += '=';
                    if (a.text) {
                        _escape(r.text, a.value);
                    } else {
                        r.text.append(a.value.data(), a.value.size());
                    }
                    rdns.push_back(std::move(r));
                }
            }
            static constexpr const char* order[][2] = {
                {"2.5.4.6", "C"}, {"2.5.4.8", "ST"}, {"2.5.4.7", "L"}, {"2.5.4.9", "STREET"}, {"2.5.4.17", "POSTALCODE"},
                {"2.5.4.10", "O"}, {"2.5.4.11", "OU"}, {"2.5.4.3", "CN"}, {"2.5.4.5", "SERIALNUMBER"}};
            for (auto& o : order) {
                bool single = std::strcmp(o[1], "CN") == 0 || std::strcmp(o[1], "SERIALNUMBER") == 0;
                Rdn r;
                bool any = false;
                if (single) {
                    string v = _last(o[0]);
                    if (!v.empty()) {
                        r.text = std::string(o[1]) + "=";
                        _escape(r.text, v);
                        any = true;
                    }
                } else {
                    for (auto& a : _attributes) {
                        if (a.text && a.oid == o[0]) {
                            if (any) {
                                r.text += '+';
                            }
                            r.text += o[1];
                            r.text += '=';
                            _escape(r.text, a.value);
                            any = true;
                        }
                    }
                }
                if (any) {
                    rdns.push_back(std::move(r));
                }
            }
            std::string s;
            for (size_t i = rdns.size(); i-- > 0;) {
                if (!s.empty() || i + 1 != rdns.size()) {
                    s += ',';
                }
                s += rdns[i].text;
            }
            return string(s);
        }

        friend bool operator==(const name& a, const name& b) noexcept {
            if (a._attributes.size() != b._attributes.size()) {
                return false;
            }
            for (size_t i = 0; i < a._attributes.size(); ++i) {
                auto& x = a._attributes[i];
                auto& y = b._attributes[i];
                if (x.oid != y.oid || x.value != y.value || x.text != y.text) {
                    return false;
                }
            }
            return true;
        }

    private:
        friend bool detail::parse_name(crypto::detail::DerReader& in, name& out) noexcept;

        vector<attribute> _attributes;

        static bool _is_common(const string& oid) noexcept {
            static constexpr const char* common[] = {"2.5.4.3", "2.5.4.5", "2.5.4.6", "2.5.4.7", "2.5.4.8", "2.5.4.9", "2.5.4.10", "2.5.4.11", "2.5.4.17"};
            for (auto c : common) {
                if (oid == c) {
                    return true;
                }
            }
            return false;
        }

        string _last(const char* oid) const noexcept {
            string v;
            for (auto& a : _attributes) {
                if (a.text && a.oid == oid) {
                    v = a.value;
                }
            }
            return v;
        }

        vector<string> _all(const char* oid) const noexcept {
            vector<string> v;
            for (auto& a : _attributes) {
                if (a.text && a.oid == oid) {
                    v.push_back(a.value);
                }
            }
            return v;
        }

        static void _escape(std::string& s, const string& value) noexcept {
            auto v = value.view();
            for (size_t k = 0; k < v.size(); ++k) {
                char c = v[k];
                bool escape = false;
                switch (c) {
                    case ',': case '+': case '"': case '\\': case '<': case '>': case ';':
                        escape = true;
                        break;
                    case ' ':
                        escape = k == 0 || k == v.size() - 1;
                        break;
                    case '#':
                        escape = k == 0;
                        break;
                    default:
                        break;
                }
                if (escape) {
                    s += '\\';
                }
                s += c;
            }
        }
    };

    namespace detail {
        // The most attributes a name may have (§11 A7): real names have
        // under a dozen
        inline constexpr size_t max_name_attributes = 64;

        // The types an attribute's value may have besides the six strings:
        // what both Go (any element) and OpenSSL (its DirectoryString of
        // B_ASN1_PRINTABLE: BIT STRING, UniversalString, SEQUENCE and the
        // universal types it has no name for) read, so that a name read
        // here is one every common parser reads. INTEGER, BOOLEAN, OCTET
        // STRING, OBJECT IDENTIFIER, the times and every tag outside the
        // universal class are not
        SGCL_INLINE_HOT bool other_value_type(unsigned char tag) noexcept {
            switch (tag) {
                case 0x03: case 0x07: case 0x08: case 0x09: case 0x0b: case 0x0d: case 0x0e: case 0x0f:
                case 0x30: case 0x1c: case 0x1d: case 0x1f:
                    return true;
                default:
                    return false;
            }
        }

        // A UniversalString's content: code points of four bytes, none a
        // surrogate or above U+10FFFF (kept as '#' and hex, as Go keeps
        // it; OpenSSL reads it as text and refuses anything else)
        inline bool ucs4_valid(const unsigned char* p, size_t n) noexcept {
            if (n % 4 != 0) {
                return false;
            }
            for (size_t i = 0; i < n; i += 4) {
                uint32_t c = uint32_t(p[i]) << 24 | uint32_t(p[i + 1]) << 16 | uint32_t(p[i + 2]) << 8 | p[i + 3];
                if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) {
                    return false;
                }
            }
            return true;
        }

        // A Name: SEQUENCE OF SET OF AttributeTypeAndValue, each SET not
        // empty and each attribute exactly a type and a value. False for
        // anything else, a string value not valid for its type included,
        // and for more than max_name_attributes attributes
        inline bool parse_name(DerReader& in, name& out) noexcept {
            DerReader seq;
            if (!in.read(der::sequence, seq)) {
                return false;
            }
            vector<name::attribute> attrs;
            while (!seq.empty()) {
                DerReader set;
                if (!seq.read(der::set, set) || set.empty()) {
                    return false;
                }
                while (!set.empty()) {
                    DerReader atav, oid, value;
                    unsigned char tag;
                    if (attrs.size() == max_name_attributes || !set.read(der::sequence, atav) || !atav.read_oid(oid)) {
                        return false;
                    }
                    const unsigned char* vstart = atav.data();
                    if (!atav.read_any(tag, value) || !atav.empty()) {
                        return false;
                    }
                    size_t vlen = size_t(atav.data() - vstart);
                    name::attribute a;
                    a.oid = string(oid_text(oid.data(), oid.size()));
                    std::string text;
                    bool is_string;
                    if (asn1_string(tag, value.data(), value.size(), text, is_string)) {
                        a.value = string(text);
                        a.text = true;
                    } else if (is_string || !other_value_type(tag) || (tag == 0x1c && !ucs4_valid(value.data(), value.size()))) {
                        return false;
                    } else {
                        a.value = string("#" + hex_text(vstart, vlen));
                        a.text = false;
                    }
                    attrs.push_back(std::move(a));
                }
            }
            out._attributes = std::move(attrs);
            return true;
        }
    }
}
