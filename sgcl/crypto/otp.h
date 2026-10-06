//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "error.h"
#include "hash_id.h"
#include "hmac.h"
#include "random.h"
#include "secret.h"
#include "secure_zero.h"
#include "sha1.h"
#include "sha256.h"
#include "sha512.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../encoding/base32.h"
#include "../time/datetime.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

// One-time passwords: HOTP (RFC 4226), a code of 6 to 10 digits from an
// HMAC of a counter under a shared secret, and TOTP (RFC 6238), the same
// with the counter the number of periods (30 seconds by default) since the
// Unix epoch: the codes of authenticator apps and of two-factor logins.
// otp_key is such a key as the apps take it, the otpauth:// URI of Google
// Authenticator's Key Uri Format that a QR code carries.
//
// A code is checked by computing every code of the window the options
// allow and comparing each with the one given in constant time, with no
// early exit; verify gives the counter or the time step that matched, which
// the server keeps: a TOTP step it has seen once is refused after (RFC 6238
// §5.2), an HOTP counter moves past the match (RFC 4226 §7.2).
namespace sgcl::crypto {
    // The algorithm, the length of a code and, for TOTP, its period; how
    // far verify looks. sha1 is RFC 4226's and the apps' default; sha256
    // and sha512 are RFC 6238's others; any other id is std::invalid_argument
    struct otp_options {
        hash_id algorithm = hash_id::sha1;
        uint32_t digits = 6;    // 6 to 10
        uint32_t period = 30;   // TOTP: the seconds a code lives, 1 or more
        uint32_t skew = 1;      // verify: TOTP periods accepted either side of now; HOTP counters accepted ahead
    };

    namespace detail {
        inline void otp_check(const otp_options& o) {
            if (o.algorithm != hash_id::sha1 && o.algorithm != hash_id::sha256 && o.algorithm != hash_id::sha512) {
                throw invalid_argument("sgcl::crypto::otp: an algorithm other than SHA-1, SHA-256 and SHA-512");
            }
            if (o.digits < 6 || o.digits > 10) {
                throw invalid_argument("sgcl::crypto::otp: digits outside 6 to 10");
            }
            if (o.period == 0) {
                throw invalid_argument("sgcl::crypto::otp: a period of 0 seconds");
            }
        }

        // HOTP(K, C) of RFC 4226 §5.3 as a number below 10^digits: the HMAC of
        // the counter as 8 bytes big-endian, dynamically truncated to 31 bits
        inline uint32_t otp_value(const slice<const byte>& secret, uint64_t counter, const otp_options& o) noexcept {
            unsigned char c[8];
            store_be64(c, counter);
            auto mac = [&]<class H>(std::type_identity<H>) -> uint32_t {
                hmac<H> m(secret);
                m.update(slice<const byte>(reinterpret_cast<const byte*>(c), 8));
                auto d = m.value();
                const unsigned char* p = bytes(d.data());
                const size_t off = p[d.size() - 1] & 0x0f;
                uint32_t bin = uint32_t(p[off] & 0x7f) << 24 | uint32_t(p[off + 1]) << 16 | uint32_t(p[off + 2]) << 8 | uint32_t(p[off + 3]);
                secure_zero(d.data(), d.size());
                return bin;
            };
            uint32_t bin = o.algorithm == hash_id::sha256   ? mac(std::type_identity<sha256>())
                         : o.algorithm == hash_id::sha512 ? mac(std::type_identity<sha512>())
                                                          : mac(std::type_identity<sha1>());
            static constexpr uint64_t powers[11] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000, 10000000000ull};
            return uint32_t(bin % powers[o.digits]);
        }

        inline string otp_text(uint32_t value, uint32_t digits) {
            char buf[10];
            for (uint32_t i = digits; i-- > 0;) {
                buf[i] = char('0' + value % 10);
                value /= 10;
            }
            return string(buf, digits);
        }

        // A code given as text: its value, or -1 when it is not `digits`
        // decimal digits (the format is public: no need for constant time)
        inline int64_t otp_parse_code(const string& code, uint32_t digits) noexcept {
            if (code.size() != digits) {
                return -1;
            }
            int64_t v = 0;
            for (char ch : code) {
                if (ch < '0' || ch > '9') {
                    return -1;
                }
                v = v * 10 + (ch - '0');
            }
            return v;
        }

        // Every counter of [first, first + count) computed, the one whose
        // code is `given` kept, with no branch on the comparisons
        inline optional<uint64_t> otp_find(const slice<const byte>& secret, uint64_t first, uint64_t count, int64_t given,
                                           const otp_options& o) noexcept {
            if (given < 0) {
                return nullopt;
            }
            uint64_t found = 0, match = 0;
            for (uint64_t i = 0; i < count; ++i) {
                const uint64_t diff = uint64_t(otp_value(secret, first + i, o)) ^ uint64_t(given);
                const uint64_t same = ((diff | (0 - diff)) >> 63) ^ 1;   // 1 when equal
                const uint64_t mask = 0 - same;
                match = (match & ~mask) | ((first + i) & mask);
                found |= same;
            }
            if (!found) {
                return nullopt;
            }
            return match;
        }

        inline uint64_t totp_step(int64_t unix, uint32_t period) {
            if (unix < 0) {
                throw invalid_argument("sgcl::crypto::totp: a time before the Unix epoch");
            }
            return uint64_t(unix) / period;
        }
    }

    // HOTP (RFC 4226): a code of a counter
    class hotp {
    public:
        // The code of counter under secret: `digits` decimal digits, leading
        // zeros kept. Options out of their ranges are std::invalid_argument
        static string generate(const slice<const byte>& secret, uint64_t counter, const otp_options& o = {}) {
            detail::otp_check(o);
            return detail::otp_text(detail::otp_value(secret, counter, o), o.digits);
        }

        // The counter of counter .. counter + skew whose code is `code`
        // (RFC 4226 §7.4's look-ahead), or nullopt; the server keeps the
        // match + 1 as its next counter. A code of another length or with
        // other characters than digits is nullopt
        [[nodiscard]] static optional<uint64_t> verify(const slice<const byte>& secret, uint64_t counter, const string& code,
                                                       const otp_options& o = {}) {
            detail::otp_check(o);
            const uint64_t window = uint64_t(o.skew) + 1;
            const uint64_t count = counter > UINT64_MAX - (window - 1) ? UINT64_MAX - counter + 1 : window;
            return detail::otp_find(secret, counter, count, detail::otp_parse_code(code, o.digits), o);
        }
    };

    // TOTP (RFC 6238): a code of the time, the counter the periods since
    // the Unix epoch
    class totp {
    public:
        // The code of now
        static string generate(const slice<const byte>& secret, const otp_options& o = {}) {
            return generate(secret, time::now(), o);
        }

        // The code of the time at; a time before the Unix epoch is
        // std::invalid_argument
        static string generate(const slice<const byte>& secret, const time::datetime& at, const otp_options& o = {}) {
            detail::otp_check(o);
            return detail::otp_text(detail::otp_value(secret, detail::totp_step(at.unix(), o.period), o), o.digits);
        }

        // The time step of now - skew .. now + skew periods whose code is
        // `code`, or nullopt: the server refuses a step it has accepted
        // before (a code is good once). A code of another length or with
        // other characters than digits is nullopt
        [[nodiscard]] static optional<uint64_t> verify(const slice<const byte>& secret, const string& code, const otp_options& o = {}) {
            return verify(secret, code, time::now(), o);
        }

        [[nodiscard]] static optional<uint64_t> verify(const slice<const byte>& secret, const string& code,
                                                       const time::datetime& at, const otp_options& o = {}) {
            detail::otp_check(o);
            if (at.unix() < 0) {
                return nullopt;
            }
            const uint64_t now = detail::totp_step(at.unix(), o.period);
            const uint64_t first = now >= o.skew ? now - o.skew : 0;
            return detail::otp_find(secret, first, now - first + o.skew + 1, detail::otp_parse_code(code, o.digits), o);
        }
    };

    enum class otp_type : uint8_t {
        hotp,
        totp
    };

    namespace detail {
        inline bool otp_unreserved(unsigned char c) noexcept {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~' || c == '@';
        }

        inline void otp_percent_encode(std::string& out, const string& s) {
            static const char hexdigits[] = "0123456789ABCDEF";
            for (char ch : s) {
                unsigned char c = static_cast<unsigned char>(ch);
                if (otp_unreserved(c)) {
                    out += ch;
                } else {
                    out += '%';
                    out += hexdigits[c >> 4];
                    out += hexdigits[c & 15];
                }
            }
        }

        inline int otp_hex(char c) noexcept {
            return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        }

        // Percent-decoded, '+' as a space where plus_space (a query's value);
        // false for a '%' not followed by two hex digits
        inline bool otp_percent_decode(std::string_view in, std::string& out, bool plus_space) {
            out.clear();
            for (size_t i = 0; i < in.size(); ++i) {
                char c = in[i];
                if (c == '%') {
                    if (i + 2 >= in.size()) {
                        return false;
                    }
                    int h = otp_hex(in[i + 1]), l = otp_hex(in[i + 2]);
                    if (h < 0 || l < 0) {
                        return false;
                    }
                    out += char(h << 4 | l);
                    i += 2;
                } else if (c == '+' && plus_space) {
                    out += ' ';
                } else {
                    out += c;
                }
            }
            return true;
        }

        inline bool otp_iequal(std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) {
                return false;
            }
            for (size_t i = 0; i < a.size(); ++i) {
                char x = a[i], y = b[i];
                if (x >= 'A' && x <= 'Z') {
                    x = char(x - 'A' + 'a');
                }
                if (y >= 'A' && y <= 'Z') {
                    y = char(y - 'A' + 'a');
                }
                if (x != y) {
                    return false;
                }
            }
            return true;
        }

        inline bool otp_number(std::string_view s, uint64_t& out) noexcept {
            if (s.empty() || s.size() > 20) {
                return false;
            }
            uint64_t v = 0;
            for (char c : s) {
                if (c < '0' || c > '9') {
                    return false;
                }
                uint64_t d = uint64_t(c - '0');
                if (v > (UINT64_MAX - d) / 10) {
                    return false;
                }
                v = v * 10 + d;
            }
            out = v;
            return true;
        }
    }

    // A one-time-password key as an authenticator app takes it: Google
    // Authenticator's Key Uri Format,
    // otpauth://totp/Example:alice@example.com?secret=JBSWY3DPEHPK3PXP&issuer=Example,
    // the URI a QR code carries. Its fields are public; the secret is a
    // secret_bytes, so the key is move-only and clone() is its copy
    struct otp_key {
        otp_type type = otp_type::totp;
        secret_bytes secret;
        string issuer;
        string account;
        otp_options options;   // the skew is the server's, not in the URI
        uint64_t counter = 0;  // HOTP's next counter

        otp_key() = default;

        // The key of a URI: parse(uri).value(), a URI parse() refuses
        // throwing bad_expected_access<crypto::error> with its error
        explicit otp_key(const string& uri)
        : otp_key(parse(uri).value()) {
        }

        otp_key(otp_key&&) noexcept = default;
        otp_key& operator=(otp_key&&) noexcept = default;
        otp_key(const otp_key&) = delete;
        otp_key& operator=(const otp_key&) = delete;

        otp_key clone() const {
            otp_key k;
            k.type = type;
            k.secret = secret.clone();
            k.issuer = issuer;
            k.account = account;
            k.options = options;
            k.counter = counter;
            return k;
        }

        // A new TOTP key: 20 random bytes of secret (RFC 4226's 160 bits),
        // for the issuer and the account the app shows
        static otp_key generate(const string& issuer, const string& account, const otp_options& o = {}) {
            detail::otp_check(o);
            otp_key k;
            k.secret = random::secret(20);
            k.issuer = issuer;
            k.account = account;
            k.options = o;
            return k;
        }

        // The key of an otpauth:// URI: errc::malformed for a text that is not
        // one (the scheme, the label, a parameter, the base32 of the secret,
        // a number), errc::unsupported for another type than totp and hotp, an
        // algorithm other than SHA1, SHA256 and SHA512, digits outside 6 to 10
        static expected<otp_key, error> parse(const string& uri) noexcept {
            auto malformed = [](const char* why) {
                return unexpected(error(errc::malformed, string(std::string("sgcl::crypto::otp_key: ") + why)));
            };
            auto unsupported = [](const char* why) {
                return unexpected(error(errc::unsupported, string(std::string("sgcl::crypto::otp_key: ") + why)));
            };
            std::string_view s(uri.data(), uri.size());
            if (s.size() < 10 || !detail::otp_iequal(s.substr(0, 10), "otpauth://")) {
                return malformed("not an otpauth:// URI");
            }
            s.remove_prefix(10);
            const size_t slash = s.find('/');
            if (slash == std::string_view::npos) {
                return malformed("no label");
            }
            otp_key k;
            std::string_view type = s.substr(0, slash);
            if (detail::otp_iequal(type, "totp")) {
                k.type = otp_type::totp;
            } else if (detail::otp_iequal(type, "hotp")) {
                k.type = otp_type::hotp;
            } else {
                return unsupported("a type other than totp and hotp");
            }
            s.remove_prefix(slash + 1);
            const size_t q = s.find('?');
            const std::string_view raw_label = s.substr(0, q);
            std::string label;
            if (!detail::otp_percent_decode(raw_label, label, false)) {
                return malformed("a bad percent-encoding in the label");
            }
            // the issuer and the account are split where the issuer
            // parameter says, when there is one, and at the first colon
            // otherwise; spaces after the colon are not the account's
            std::string_view query = q == std::string_view::npos ? std::string_view() : s.substr(q + 1);
            bool has_secret = false, has_counter = false, has_issuer = false;
            std::string value;
            while (!query.empty()) {
                size_t amp = query.find('&');
                std::string_view pair = query.substr(0, amp);
                query = amp == std::string_view::npos ? std::string_view() : query.substr(amp + 1);
                if (pair.empty()) {
                    continue;
                }
                size_t eq = pair.find('=');
                std::string_view name = pair.substr(0, eq);
                if (!detail::otp_percent_decode(eq == std::string_view::npos ? std::string_view() : pair.substr(eq + 1), value, true)) {
                    return malformed("a bad percent-encoding in a parameter");
                }
                uint64_t n = 0;
                if (detail::otp_iequal(name, "secret")) {
                    // base32 of any case, the padding optional, spaces skipped
                    std::string clean;
                    for (char c : value) {
                        if (c == ' ' || c == '=') {
                            continue;
                        }
                        clean += (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
                    }
                    if (clean.empty()) {
                        return malformed("an empty secret");
                    }
                    constexpr auto b32 = encoding::base32::standard.without_padding().lenient();
                    secret_bytes sec(b32.max_decoded_size(clean.size()));
                    auto got = b32.decode_to(sec, slice<const char>(clean.data(), clean.size()));
                    secure_zero(slice<byte>(reinterpret_cast<byte*>(clean.data()), clean.size()));
                    if (!got || *got == 0) {
                        return malformed("a secret that is not base32");
                    }
                    sec.resize(*got);
                    k.secret = std::move(sec);
                    has_secret = true;
                } else if (detail::otp_iequal(name, "issuer")) {
                    k.issuer = string(value.data(), value.size());
                    has_issuer = true;
                } else if (detail::otp_iequal(name, "algorithm")) {
                    if (detail::otp_iequal(value, "SHA1")) {
                        k.options.algorithm = hash_id::sha1;
                    } else if (detail::otp_iequal(value, "SHA256")) {
                        k.options.algorithm = hash_id::sha256;
                    } else if (detail::otp_iequal(value, "SHA512")) {
                        k.options.algorithm = hash_id::sha512;
                    } else {
                        return unsupported("an algorithm other than SHA1, SHA256 and SHA512");
                    }
                } else if (detail::otp_iequal(name, "digits")) {
                    if (!detail::otp_number(value, n)) {
                        return malformed("digits that are not a number");
                    }
                    if (n < 6 || n > 10) {
                        return unsupported("digits outside 6 to 10");
                    }
                    k.options.digits = uint32_t(n);
                } else if (detail::otp_iequal(name, "period")) {
                    if (!detail::otp_number(value, n) || n == 0 || n > 0xffffffffu) {
                        return malformed("a period that is not a number of seconds");
                    }
                    k.options.period = uint32_t(n);
                } else if (detail::otp_iequal(name, "counter")) {
                    if (!detail::otp_number(value, n)) {
                        return malformed("a counter that is not a number");
                    }
                    k.counter = n;
                    has_counter = true;
                }
            }
            if (!has_secret) {
                return malformed("no secret");
            }
            if (k.type == otp_type::hotp && !has_counter) {
                return malformed("an HOTP key without its counter");
            }
            size_t account_at = 0;
            if (has_issuer) {
                std::string_view prefix(k.issuer.data(), k.issuer.size());
                if (!prefix.empty() && label.size() > prefix.size() && std::string_view(label).substr(0, prefix.size()) == prefix
                    && label[prefix.size()] == ':') {
                    account_at = prefix.size() + 1;
                }
            } else {
                // a literal colon first, an encoded one (%3A) after it
                size_t colon = raw_label.find(':');
                size_t width = 1;
                if (colon == std::string_view::npos) {
                    for (size_t i = 0; i + 2 < raw_label.size(); ++i) {
                        if (raw_label[i] == '%' && raw_label[i + 1] == '3' && (raw_label[i + 2] == 'A' || raw_label[i + 2] == 'a')) {
                            colon = i;
                            width = 3;
                            break;
                        }
                    }
                }
                if (colon != std::string_view::npos) {
                    std::string issuer;
                    detail::otp_percent_decode(raw_label.substr(0, colon), issuer, false);
                    k.issuer = string(issuer.data(), issuer.size());
                    std::string account;
                    detail::otp_percent_decode(raw_label.substr(colon + width), account, false);
                    size_t a = 0;
                    while (a < account.size() && account[a] == ' ') {
                        ++a;
                    }
                    k.account = string(account.data() + a, account.size() - a);
                    return k;
                }
            }
            if (account_at != 0) {
                while (account_at < label.size() && label[account_at] == ' ') {
                    ++account_at;
                }
            }
            k.account = string(label.data() + account_at, label.size() - account_at);
            return k;
        }

        // The otpauth:// URI: the label issuer:account (the account alone
        // when there is no issuer), the secret in base32 without padding,
        // the issuer again, the algorithm, the digits and the period when
        // they are not the defaults, the counter of an HOTP key. The text
        // holds the secret, as the URI a QR code carries does
        string to_string() const {
            detail::otp_check(options);
            std::string out = type == otp_type::totp ? "otpauth://totp/" : "otpauth://hotp/";
            if (!issuer.empty()) {
                detail::otp_percent_encode(out, issuer);
                out += ':';
            }
            detail::otp_percent_encode(out, account);
            out += "?secret=";
            constexpr auto b32 = encoding::base32::standard.without_padding();
            string text = b32.encode(secret);
            out += std::string_view(text.data(), text.size());
            if (!issuer.empty()) {
                out += "&issuer=";
                detail::otp_percent_encode(out, issuer);
            } else if (std::string_view(account.data(), account.size()).find(':') != std::string_view::npos) {
                out += "&issuer=";   // an account with a colon is not split into an issuer when read back
            }
            if (options.algorithm != hash_id::sha1) {
                out += options.algorithm == hash_id::sha256 ? "&algorithm=SHA256" : "&algorithm=SHA512";
            }
            if (options.digits != 6) {
                out += "&digits=" + std::to_string(options.digits);
            }
            if (type == otp_type::totp && options.period != 30) {
                out += "&period=" + std::to_string(options.period);
            }
            if (type == otp_type::hotp) {
                out += "&counter=" + std::to_string(counter);
            }
            string uri(out.data(), out.size());
            secure_zero(slice<byte>(reinterpret_cast<byte*>(out.data()), out.size()));
            return uri;
        }

        // The code now (TOTP) or at the counter (HOTP)
        string code() const {
            return type == otp_type::totp ? totp::generate(secret, options) : hotp::generate(secret, counter, options);
        }

        // verify of totp or hotp with the key's secret, options and counter
        [[nodiscard]] optional<uint64_t> verify(const string& code) const {
            return type == otp_type::totp ? totp::verify(secret, code, options) : hotp::verify(secret, counter, code, options);
        }
    };
}
