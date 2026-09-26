//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/array.h"
#include "../core/aliases.h"
#include "../core/string.h"
#include "error.h"

#include <array>
#include <compare>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string_view>

namespace sgcl::net {
    namespace detail { using namespace sgcl::detail; }
    class ip_network;
    class endpoint;

    namespace detail {
        struct IpText;

        // The readings of parse without the error: for the library's own
        // tries, where a text that is not an address is no failure
        optional<ip_network> parse_network(const string& text);
        optional<endpoint> parse_endpoint(const string& text);
    }

    // An IP address as a value: sixteen bytes of address, the kind (none,
    // v4, v6) and the zone of a scoped IPv6 address ("fe80::1%en0") kept in
    // the object, fifteen bytes at most (IFNAMSIZ is sixteen with the
    // terminator, and every interface name and index fits). Thirty-two
    // bytes, trivially copyable, nothing allocated to make, copy or compare
    // one; Go's netip.Addr, whose zone is a pointer to an interned string.
    //
    // parse takes the forms of RFC 4291 §2.2 for IPv6 (groups of up to
    // four hex digits, one "::", a dotted IPv4 address in the last 32
    // bits) and only the dotted decimal form for IPv4, four fields with
    // no leading zeros: "010.0.0.1" is not an address, since the C
    // library reads the leading zero as octal and a program checking the
    // text would disagree with the socket about where it connects.
    // to_string writes RFC 5952: lowercase, no leading zeros in a group,
    // the longest run of two or more zero groups (the first of equal
    // ones) as "::", and an IPv4-mapped address as "::ffff:1.2.3.4".
    //
    // An IPv4 address and the IPv4-mapped IPv6 address of the same bytes
    // are different values (is_v4 and is_v6), as they are in Go; unmap()
    // turns the second into the first. The predicates look through the
    // mapping (is_loopback of ::ffff:127.0.0.1 is true), except
    // is_unspecified, which is true for 0.0.0.0 and :: only.
    //
    // The order is the kind (none, then v4, then v6), the bytes, the zone.
    class ip_address {
    public:
        ip_address() noexcept = default;   // empty: !is_valid()

        // "10.0.0.1", "2001:db8::1", "fe80::1%en0", "::ffff:1.2.3.4";
        // nullopt for anything else (no surrounding spaces, no brackets)
        static expected<ip_address, io::error> parse(const string& text);

        static ip_address v4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept {
            ip_address r;
            r._kind = Kind4;
            r._bytes[10] = r._bytes[11] = 0xff;
            r._bytes[12] = a;
            r._bytes[13] = b;
            r._bytes[14] = c;
            r._bytes[15] = d;
            return r;
        }

        static ip_address v6(const array<uint8_t, 16>& bytes) noexcept {
            ip_address r;
            r._kind = Kind6;
            for (size_t i = 0; i < 16; ++i) {
                r._bytes[i] = bytes[i];
            }
            return r;
        }

        static ip_address loopback_v4() noexcept {
            return v4(127, 0, 0, 1);
        }

        static ip_address loopback_v6() noexcept {
            ip_address r;
            r._kind = Kind6;
            r._bytes[15] = 1;
            return r;
        }

        static ip_address any_v4() noexcept {
            return v4(0, 0, 0, 0);
        }

        static ip_address any_v6() noexcept {
            ip_address r;
            r._kind = Kind6;
            return r;
        }

        bool is_valid() const noexcept {
            return _kind != KindNone;
        }

        bool is_v4() const noexcept {
            return _kind == Kind4;
        }

        bool is_v6() const noexcept {
            return _kind == Kind6;
        }

        // ::ffff:a.b.c.d
        bool is_v4_mapped() const noexcept {
            return _kind == Kind6 && _mapped_prefix();
        }

        // The IPv4 address of an IPv4-mapped one; any other unchanged
        ip_address unmap() const noexcept {
            if (!is_v4_mapped()) {
                return *this;
            }
            return v4(_bytes[12], _bytes[13], _bytes[14], _bytes[15]);
        }

        // 127.0.0.0/8, ::1
        bool is_loopback() const noexcept {
            auto a = unmap();
            return a.is_v4() ? a._bytes[12] == 127 : a.is_v6() && a._is_v6_value(0, 1);
        }

        // RFC 1918 (10/8, 172.16/12, 192.168/16) and RFC 4193 (fc00::/7)
        bool is_private() const noexcept {
            auto a = unmap();
            if (a.is_v4()) {
                uint8_t x = a._bytes[12], y = a._bytes[13];
                return x == 10 || (x == 172 && (y & 0xf0) == 16) || (x == 192 && y == 168);
            }
            return a.is_v6() && (a._bytes[0] & 0xfe) == 0xfc;
        }

        // 0.0.0.0, ::
        bool is_unspecified() const noexcept {
            return is_v4() ? _v4_word() == 0 : is_v6() && _is_v6_value(0, 0);
        }

        // 224.0.0.0/4, ff00::/8
        bool is_multicast() const noexcept {
            auto a = unmap();
            return a.is_v4() ? (a._bytes[12] & 0xf0) == 0xe0 : a.is_v6() && a._bytes[0] == 0xff;
        }

        // Link-local unicast: 169.254.0.0/16, fe80::/10
        bool is_link_local() const noexcept {
            auto a = unmap();
            if (a.is_v4()) {
                return a._bytes[12] == 169 && a._bytes[13] == 254;
            }
            return a.is_v6() && a._bytes[0] == 0xfe && (a._bytes[1] & 0xc0) == 0x80;
        }

        // Neither unspecified, nor loopback, multicast, link-local unicast,
        // nor the IPv4 broadcast address (a private address is global
        // unicast: the name is the RFC's, not a statement about routing)
        bool is_global_unicast() const noexcept {
            auto a = unmap();
            if (!a.is_valid()) {
                return false;
            }
            if (a.is_v4() && (a._v4_word() == 0 || a._v4_word() == 0xffffffffu)) {
                return false;
            }
            if (a.is_v6() && a._is_v6_value(0, 0)) {
                return false;
            }
            return !a.is_loopback() && !a.is_multicast() && !a.is_link_local() && !a._is_link_local_multicast();
        }

        // The sixteen bytes in network order; an IPv4 address as the
        // IPv4-mapped one, ::ffff:a.b.c.d (the form a dual-stack socket
        // sees), its four bytes the last four
        array<uint8_t, 16> bytes() const noexcept {
            array<uint8_t, 16> out = {};
            for (size_t i = 0; i < 16; ++i) {
                out[i] = _bytes[i];
            }
            return out;
        }

        // "en0" of "fe80::1%en0"; empty for an address without one
        string zone() const {
            return string(std::string_view(_zone.data(), _zone_size()));
        }

        bool has_zone() const noexcept {
            return _zone[0] != 0;
        }

        // The address with the zone given ("" removes it); an IPv4 address
        // has no zone and is returned unchanged. A zone longer than fifteen
        // bytes, or with a NUL in it, is invalid_argument
        ip_address with_zone(const string& zone) const {
            if (!is_v6()) {
                return *this;
            }
            if (zone.size() > ZoneCapacity || std::string_view(zone.data(), zone.size()).find('\0') != std::string_view::npos) {
                throw invalid_argument("sgcl::net::ip_address::with_zone: a zone is at most 15 bytes, without NUL");
            }
            ip_address r = *this;
            r._zone.fill(0);
            std::memcpy(r._zone.data(), zone.data(), zone.size());
            return r;
        }

        // The address one above (below), the zone kept; the empty address
        // past the last one (below the first) of the kind
        ip_address next() const noexcept {
            if (!is_valid()) {
                return ip_address();
            }
            ip_address r = *this;
            size_t first = is_v4() ? 12 : 0;
            for (size_t i = 16; i-- > first;) {
                if (++r._bytes[i] != 0) {
                    return r;
                }
            }
            return ip_address();
        }

        ip_address prev() const noexcept {
            if (!is_valid()) {
                return ip_address();
            }
            ip_address r = *this;
            size_t first = is_v4() ? 12 : 0;
            for (size_t i = 16; i-- > first;) {
                if (r._bytes[i]-- != 0) {
                    return r;
                }
            }
            return ip_address();
        }

        // RFC 5952 for IPv6, dotted decimal for IPv4, "invalid IP" for the
        // empty address (Go's text)
        string to_string() const;

        auto operator<=>(const ip_address&) const noexcept = default;
        bool operator==(const ip_address&) const noexcept = default;

    private:
        static constexpr uint8_t KindNone = 0, Kind4 = 4, Kind6 = 6;
        static constexpr size_t ZoneCapacity = 15;

        // The longest text to_string writes: eight groups, seven colons,
        // the zone and its '%'
        static constexpr size_t MaxText = 39 + 1 + 15;

        // What to_string writes, into out (MaxText bytes): its length; for
        // the texts of a network and an endpoint, made without a string between
        size_t write_text(char* out) const noexcept;

        friend struct std::hash<ip_address>;
        friend struct detail::IpText;
        friend class ip_network;
        friend class endpoint;

        bool _mapped_prefix() const noexcept {
            for (size_t i = 0; i < 10; ++i) {
                if (_bytes[i]) {
                    return false;
                }
            }
            return _bytes[10] == 0xff && _bytes[11] == 0xff;
        }

        // Whether the sixteen bytes are all `fill` but the last, which is `last`
        bool _is_v6_value(uint8_t fill, uint8_t last) const noexcept {
            for (size_t i = 0; i < 15; ++i) {
                if (_bytes[i] != fill) {
                    return false;
                }
            }
            return _bytes[15] == last;
        }

        uint32_t _v4_word() const noexcept {
            return (uint32_t(_bytes[12]) << 24) | (uint32_t(_bytes[13]) << 16) | (uint32_t(_bytes[14]) << 8) | _bytes[15];
        }

        // 224.0.0.0/24, ff02::/16
        bool _is_link_local_multicast() const noexcept {
            if (is_v4()) {
                return _bytes[12] == 224 && _bytes[13] == 0 && _bytes[14] == 0;
            }
            return is_v6() && _bytes[0] == 0xff && (_bytes[1] & 0x0f) == 0x02;
        }

        size_t _zone_size() const noexcept {
            size_t n = 0;
            while (n < ZoneCapacity && _zone[n]) {
                ++n;
            }
            return n;
        }

        uint8_t _kind = KindNone;
        std::array<uint8_t, 16> _bytes = {};
        std::array<char, ZoneCapacity> _zone = {};
    };

    namespace detail {
        // The parsers over a view, for the module's own use (an endpoint, a
        // network, a "host:port" to dial): an address or nothing
        struct IpText {
            static bool parse_v4(std::string_view s, uint8_t* out) noexcept {
                size_t i = 0;
                for (int field = 0; field < 4; ++field) {
                    if (field > 0) {
                        if (i >= s.size() || s[i] != '.') {
                            return false;
                        }
                        ++i;
                    }
                    if (i >= s.size() || s[i] < '0' || s[i] > '9') {
                        return false;
                    }
                    if (s[i] == '0' && i + 1 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '9') {
                        return false;   // a leading zero: octal to the C library, refused
                    }
                    unsigned v = 0;
                    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                        v = v * 10 + unsigned(s[i] - '0');
                        if (v > 255) {
                            return false;
                        }
                        ++i;
                    }
                    out[field] = uint8_t(v);
                }
                return i == s.size();
            }

            static int hex_value(char c) noexcept {
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
            }

            // RFC 4291 §2.2: groups of one to four hex digits separated by
            // ':', one "::" standing for one or more zero groups, and the last
            // two groups optionally written as a dotted IPv4 address
            static bool parse_v6(std::string_view s, uint8_t* out) noexcept {
                std::memset(out, 0, 16);
                size_t i = 0, n = 0;   // n: the bytes filled
                int ellipsis = -1;     // where "::" stands, in bytes
                if (s.size() >= 2 && s[0] == ':' && s[1] == ':') {
                    ellipsis = 0;
                    i = 2;
                } else if (!s.empty() && s[0] == ':') {
                    return false;
                }
                while (i < s.size()) {
                    size_t j = i;
                    unsigned v = 0;
                    while (j < s.size() && j - i < 5) {
                        int h = hex_value(s[j]);
                        if (h < 0) {
                            break;
                        }
                        v = (v << 4) | unsigned(h);
                        ++j;
                    }
                    if (j == i) {
                        return false;   // an empty group, or a character that is no digit
                    }
                    if (j < s.size() && s[j] == '.') {   // the embedded IPv4 address: the last 32 bits
                        if (n > 12 || (ellipsis < 0 && n != 12)) {
                            return false;
                        }
                        if (!parse_v4(s.substr(i), out + n)) {
                            return false;
                        }
                        n += 4;
                        i = s.size();
                        break;
                    }
                    if (j - i > 4 || n == 16) {
                        return false;   // five digits in a group, or a ninth group
                    }
                    out[n] = uint8_t(v >> 8);
                    out[n + 1] = uint8_t(v);
                    n += 2;
                    i = j;
                    if (i == s.size()) {
                        break;
                    }
                    if (s[i] != ':') {
                        return false;
                    }
                    ++i;
                    if (i < s.size() && s[i] == ':') {
                        if (ellipsis >= 0) {
                            return false;   // a second "::"
                        }
                        ellipsis = int(n);
                        ++i;
                    } else if (i == s.size()) {
                        return false;   // a single trailing ':'
                    }
                }
                if (n < 16) {
                    if (ellipsis < 0) {
                        return false;   // too few groups
                    }
                    size_t tail = n - size_t(ellipsis);
                    std::memmove(out + 16 - tail, out + ellipsis, tail);
                    std::memset(out + ellipsis, 0, 16 - n);
                } else if (ellipsis >= 0) {
                    return false;   // "::" standing for no group at all
                }
                return true;
            }

            static optional<ip_address> parse(std::string_view s) noexcept {
                ip_address r;
                for (size_t i = 0; i < s.size(); ++i) {
                    char c = s[i];
                    if (c == '.') {
                        if (!parse_v4(s, r._bytes.data() + 12)) {
                            return nullopt;
                        }
                        r._kind = ip_address::Kind4;
                        r._bytes[10] = r._bytes[11] = 0xff;
                        return r;
                    }
                    if (c == ':') {
                        auto percent = s.find('%');
                        std::string_view zone;
                        if (percent != std::string_view::npos) {
                            zone = s.substr(percent + 1);
                            s = s.substr(0, percent);
                            if (zone.empty() || zone.size() > ip_address::ZoneCapacity || zone.find('\0') != std::string_view::npos) {
                                return nullopt;
                            }
                        }
                        if (!parse_v6(s, r._bytes.data())) {
                            return nullopt;
                        }
                        r._kind = ip_address::Kind6;
                        std::memcpy(r._zone.data(), zone.data(), zone.size());
                        return r;
                    }
                    if (hex_value(c) < 0) {
                        return nullopt;   // a '%' before the address, a space, anything else
                    }
                }
                return nullopt;
            }

            // RFC 5952 §4: into out, its length
            static size_t write_v6(const ip_address& a, char* out) noexcept {
                static constexpr char digits[] = "0123456789abcdef";
                char* p = out;
                if (a._mapped_prefix()) {
                    std::memcpy(p, "::ffff:", 7);
                    p += 7;
                    p += write_v4(a._bytes.data() + 12, p);
                    return size_t(p - out);
                }
                unsigned groups[8];
                for (int g = 0; g < 8; ++g) {
                    groups[g] = (unsigned(a._bytes[2 * g]) << 8) | a._bytes[2 * g + 1];
                }
                int best = -1, best_len = 1;   // a run of one zero group is written as "0" (§4.2.2)
                for (int g = 0; g < 8;) {
                    if (groups[g] != 0) {
                        ++g;
                        continue;
                    }
                    int start = g;
                    while (g < 8 && groups[g] == 0) {
                        ++g;
                    }
                    if (g - start > best_len) {   // the first of equal runs (§4.2.3)
                        best = start;
                        best_len = g - start;
                    }
                }
                for (int g = 0; g < 8; ++g) {
                    if (g == best) {
                        *p++ = ':';
                        *p++ = ':';
                        g += best_len - 1;
                        continue;
                    }
                    if (g > 0 && g != best + best_len) {
                        *p++ = ':';
                    }
                    unsigned v = groups[g];
                    bool started = false;
                    for (int shift = 12; shift >= 0; shift -= 4) {
                        unsigned d = (v >> shift) & 0xf;
                        if (d || started || shift == 0) {
                            *p++ = digits[d];
                            started = true;
                        }
                    }
                }
                return size_t(p - out);
            }

            static size_t write_v4(const uint8_t* b, char* out) noexcept {
                char* p = out;
                for (int i = 0; i < 4; ++i) {
                    if (i) {
                        *p++ = '.';
                    }
                    unsigned v = b[i];
                    if (v >= 100) {
                        *p++ = char('0' + v / 100);
                    }
                    if (v >= 10) {
                        *p++ = char('0' + v / 10 % 10);
                    }
                    *p++ = char('0' + v % 10);
                }
                return size_t(p - out);
            }

            static std::string_view view(const string& s) noexcept {
                return std::string_view(s.data(), s.size());
            }
        };
    }

    inline expected<ip_address, io::error> ip_address::parse(const string& text) {
        if (auto a = detail::IpText::parse(detail::IpText::view(text))) {
            return *a;
        }
        return unexpected(detail::net_error(errc::invalid_address, "parse IP address", text));
    }

    inline size_t ip_address::write_text(char* out) const noexcept {
        if (is_v4()) {
            return detail::IpText::write_v4(_bytes.data() + 12, out);
        }
        if (!is_v6()) {
            std::memcpy(out, "invalid IP", 10);
            return 10;
        }
        size_t n = detail::IpText::write_v6(*this, out);
        size_t z = _zone_size();
        if (z) {
            out[n++] = '%';
            std::memcpy(out + n, _zone.data(), z);
            n += z;
        }
        return n;
    }

    inline string ip_address::to_string() const {
        char buf[MaxText];
        return string(std::string_view(buf, write_text(buf)));
    }

    // A network in CIDR notation, an address and the length of its prefix
    // in bits ("10.0.0.0/8", "2001:db8::/32"): Go's netip.Prefix. The
    // address is kept as given ("10.1.2.3/8" is a value of its own);
    // masked() clears the bits past the prefix. No zone: a network is
    // not scoped to an interface.
    class ip_network {
    public:
        ip_network() noexcept = default;   // empty: !is_valid()

        // bits in 0..32 for IPv4, 0..128 for IPv6, else
        // invalid_argument; the zone of the address dropped; the
        // empty address gives the empty network
        ip_network(ip_address address, int bits)
        : _address(address.is_v6() ? address.with_zone(string()) : address)
        , _bits(address.is_valid() ? int16_t(bits) : int16_t(-1)) {
            if (address.is_valid() && (bits < 0 || bits > (address.is_v4() ? 32 : 128))) {
                throw invalid_argument("sgcl::net::ip_network: the prefix length is out of range for the address");
            }
        }

        // "address/bits": the address as ip_address::parse takes it, but
        // without a zone; bits in decimal without a sign or a leading zero
        static expected<ip_network, io::error> parse(const string& text);

        bool is_valid() const noexcept {
            return _address.is_valid() && _bits >= 0;
        }

        ip_address address() const noexcept {
            return _address;
        }

        // The length of the prefix, -1 for the empty network
        int bits() const noexcept {
            return _bits;
        }

        // The same network with the bits past the prefix cleared
        ip_network masked() const noexcept {
            if (!is_valid()) {
                return ip_network();
            }
            ip_network r = *this;
            r._address = _masked_address();
            return r;
        }

        // Whether the address lies in the network: the same kind (an
        // IPv4-mapped address is not in an IPv4 network), no zone, the
        // prefix equal
        bool contains(const ip_address& a) const noexcept {
            if (!is_valid() || !a.is_valid() || a.is_v4() != _address.is_v4() || a.has_zone()) {
                return false;
            }
            return _prefix_equal(_address, a, _bits);
        }

        // Whether the two networks have an address in common
        bool overlaps(const ip_network& o) const noexcept {
            if (!is_valid() || !o.is_valid() || _address.is_v4() != o._address.is_v4()) {
                return false;
            }
            return _prefix_equal(_address, o._address, std::min(_bits, o._bits));
        }

        // "10.0.0.0/8"; "invalid Prefix" for the empty network (Go's text)
        string to_string() const {
            if (!is_valid()) {
                return string("invalid Prefix");
            }
            char buf[ip_address::MaxText + 4];
            size_t n = _address.write_text(buf);
            buf[n++] = '/';
            int b = _bits;
            if (b >= 100) {
                buf[n++] = char('0' + b / 100);
            }
            if (b >= 10) {
                buf[n++] = char('0' + b / 10 % 10);
            }
            buf[n++] = char('0' + b % 10);
            return string(std::string_view(buf, n));
        }

        auto operator<=>(const ip_network&) const noexcept = default;
        bool operator==(const ip_network&) const noexcept = default;

    private:
        friend struct std::hash<ip_network>;

        ip_address _masked_address() const noexcept {
            auto b = _address.bytes();
            int first = _address.is_v4() ? 96 : 0;
            int keep = first + _bits;
            for (int i = 0; i < 16; ++i) {
                int bit = i * 8;
                if (bit >= keep) {
                    b[i] = 0;
                } else if (bit + 8 > keep) {
                    b[i] = uint8_t(b[i] & (0xff << (8 - (keep - bit))));
                }
            }
            return _address.is_v4() ? ip_address::v4(b[12], b[13], b[14], b[15]) : ip_address::v6(b);
        }

        // The first `bits` bits of the two addresses of one kind equal
        static bool _prefix_equal(const ip_address& x, const ip_address& y, int bits) noexcept {
            auto a = x.bytes(), b = y.bytes();
            int i = x.is_v4() ? 12 : 0;
            for (; bits >= 8; bits -= 8, ++i) {
                if (a[i] != b[i]) {
                    return false;
                }
            }
            if (bits > 0) {
                uint8_t mask = uint8_t(0xff << (8 - bits));
                return (a[i] & mask) == (b[i] & mask);
            }
            return true;
        }

        ip_address _address;
        int16_t _bits = -1;
    };
}

namespace sgcl::net {
    // An IP address and a port: where a socket is bound or connected
    // (Go's netip.AddrPort). "1.2.3.4:80", "[::1]:443", "[fe80::1%en0]:80":
    // an IPv6 address always in brackets, an IPv4 address never.
    class endpoint {
    public:
        endpoint() noexcept = default;   // empty: !is_valid()

        endpoint(ip_address address, uint16_t port) noexcept
        : _address(address)
        , _port(port) {
        }

        // "address:port" with the port in decimal (0..65535, leading
        // zeros allowed, no sign); nullopt for anything else
        static expected<endpoint, io::error> parse(const string& text);

        ip_address address() const noexcept {
            return _address;
        }

        uint16_t port() const noexcept {
            return _port;
        }

        bool is_valid() const noexcept {
            return _address.is_valid();
        }

        // "1.2.3.4:80", "[::1]:80"; "invalid AddrPort" for the empty one
        string to_string() const {
            if (!is_valid()) {
                return string("invalid AddrPort");
            }
            char buf[ip_address::MaxText + 8];
            size_t n = 0;
            if (_address.is_v6()) {
                buf[n++] = '[';
            }
            n += _address.write_text(buf + n);
            if (_address.is_v6()) {
                buf[n++] = ']';
            }
            buf[n++] = ':';
            char digits[5];
            int d = 0;
            unsigned p = _port;
            do {
                digits[d++] = char('0' + p % 10);
                p /= 10;
            } while (p);
            while (d) {
                buf[n++] = digits[--d];
            }
            return string(std::string_view(buf, n));
        }

        auto operator<=>(const endpoint&) const noexcept = default;
        bool operator==(const endpoint&) const noexcept = default;

    private:
        ip_address _address;
        uint16_t _port = 0;
    };

    namespace detail {
        // A decimal port: digits only, 0..65535
        inline optional<uint16_t> parse_port(std::string_view s) noexcept {
            if (s.empty()) {
                return nullopt;
            }
            uint32_t v = 0;
            for (char c : s) {
                if (c < '0' || c > '9') {
                    return nullopt;
                }
                v = v * 10 + uint32_t(c - '0');
                if (v > 65535) {
                    return nullopt;
                }
            }
            return uint16_t(v);
        }

        // "host:port" taken apart at the last ':', the brackets of an IPv6
        // host removed (bracketed: whether they were there); the host may
        // be empty (":80"), the port may not
        struct HostPort {
            std::string_view host;
            std::string_view port;
            bool bracketed = false;
        };

        inline optional<HostPort> split_host_port(std::string_view s) noexcept {
            auto colon = s.rfind(':');
            if (colon == std::string_view::npos) {
                return nullopt;
            }
            HostPort r;
            r.host = s.substr(0, colon);
            r.port = s.substr(colon + 1);
            if (!r.host.empty() && r.host.front() == '[') {
                if (r.host.size() < 2 || r.host.back() != ']') {
                    return nullopt;
                }
                r.host = r.host.substr(1, r.host.size() - 2);
                r.bracketed = true;
            } else if (r.host.find(':') != std::string_view::npos) {
                return nullopt;   // an IPv6 host without its brackets
            }
            return r;   // brackets left inside the host are the address parser's to refuse, or a zone's to keep (as in Go)
        }
    }

    inline expected<endpoint, io::error> endpoint::parse(const string& text) {
        if (auto e = detail::parse_endpoint(text)) {
            return *e;
        }
        return unexpected(detail::net_error(errc::invalid_address, "parse endpoint", text));
    }

    inline optional<endpoint> detail::parse_endpoint(const string& text) {
        auto hp = detail::split_host_port(sgcl::net::detail::IpText::view(text));
        if (!hp || hp->host.empty()) {
            return nullopt;
        }
        auto port = detail::parse_port(hp->port);
        if (!port) {
            return nullopt;
        }
        auto a = sgcl::net::detail::IpText::parse(hp->host);
        if (!a || a->is_v6() != hp->bracketed) {
            return nullopt;   // brackets exactly around an IPv6 address
        }
        return endpoint(*a, *port);
    }
}

namespace sgcl::net {
    namespace detail { using namespace sgcl::detail; }
    inline expected<ip_network, io::error> ip_network::parse(const string& text) {
        if (auto n = detail::parse_network(text)) {
            return *n;
        }
        return unexpected(detail::net_error(errc::invalid_address, "parse IP network", text));
    }

    inline optional<ip_network> detail::parse_network(const string& text) {
        std::string_view s = sgcl::net::detail::IpText::view(text);
        auto slash = s.rfind('/');
        if (slash == std::string_view::npos) {
            return nullopt;
        }
        auto a = detail::IpText::parse(s.substr(0, slash));
        if (!a || a->has_zone()) {
            return nullopt;
        }
        std::string_view b = s.substr(slash + 1);
        if (b.empty() || b.size() > 3 || (b.size() > 1 && b[0] == '0')) {
            return nullopt;
        }
        int bits = 0;
        for (char c : b) {
            if (c < '0' || c > '9') {
                return nullopt;
            }
            bits = bits * 10 + (c - '0');
        }
        if (bits > (a->is_v4() ? 32 : 128)) {
            return nullopt;
        }
        return ip_network(*a, bits);
    }
}

template<>
struct std::hash<sgcl::net::ip_address> {
    size_t operator()(const sgcl::net::ip_address& a) const noexcept {
        uint64_t w[4];
        static_assert(sizeof(sgcl::net::ip_address) == 32);
        std::memcpy(w, &a, sizeof(w));
        uint64_t h = 0x9e3779b97f4a7c15ull;
        for (uint64_t x : w) {
            h = (h ^ x) * 0xff51afd7ed558ccdull;
            h ^= h >> 32;
        }
        return size_t(h);
    }
};

template<>
struct std::hash<sgcl::net::ip_network> {
    size_t operator()(const sgcl::net::ip_network& n) const noexcept {
        return std::hash<sgcl::net::ip_address>()(n._address) ^ (size_t(uint16_t(n._bits)) * 0x9e3779b97f4a7c15ull);
    }
};

template<>
struct std::hash<sgcl::net::endpoint> {
    size_t operator()(const sgcl::net::endpoint& e) const noexcept {
        return std::hash<sgcl::net::ip_address>()(e.address()) ^ (size_t(e.port()) * 0xc2b2ae3d27d4eb4full);
    }
};
