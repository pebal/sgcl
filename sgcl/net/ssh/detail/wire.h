//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../../core/aliases.h"
#include "../../../core/detail/bytes.h"
#include "../../../core/detail/os.h"
#include "../../../core/slice.h"
#include "../../../crypto/secure_zero.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The data types of SSH (RFC 4251 §5) and the message numbers of the
// protocol's layers (RFC 4250 §4.1, RFC 8308, RFC 4256, OpenSSH's
// PROTOCOL): a writer that appends them to a buffer and a reader that
// takes them from one, each check of a length made once per field, a
// reader past its end failing softly (ok() false, every later field
// empty) so that a message is checked once, at its end.
//
// The buffers of the protocol are unmanaged (std::vector): a packet is
// built, sealed and written, or read, opened and taken apart, within one
// call; what leaves them for the program is copied into its own types.
namespace sgcl::net::ssh::detail {
    using Bytes = std::vector<uint8_t>;

    // The message numbers
    enum : uint8_t {
        MsgDisconnect = 1,
        MsgIgnore = 2,
        MsgUnimplemented = 3,
        MsgDebug = 4,
        MsgServiceRequest = 5,
        MsgServiceAccept = 6,
        MsgExtInfo = 7,
        MsgKexinit = 20,
        MsgNewkeys = 21,
        MsgKexInit = 30,         // KEXDH_INIT, KEX_ECDH_INIT, KEX_HYBRID_INIT
        MsgKexReply = 31,        // KEXDH_REPLY, KEX_ECDH_REPLY, KEX_HYBRID_REPLY
        MsgUserauthRequest = 50,
        MsgUserauthFailure = 51,
        MsgUserauthSuccess = 52,
        MsgUserauthBanner = 53,
        MsgUserauth60 = 60,      // PK_OK, PASSWD_CHANGEREQ, INFO_REQUEST: by the method in progress
        MsgUserauthInfoResponse = 61,
        MsgGlobalRequest = 80,
        MsgRequestSuccess = 81,
        MsgRequestFailure = 82,
        MsgChannelOpen = 90,
        MsgChannelOpenConfirmation = 91,
        MsgChannelOpenFailure = 92,
        MsgChannelWindowAdjust = 93,
        MsgChannelData = 94,
        MsgChannelExtendedData = 95,
        MsgChannelEof = 96,
        MsgChannelClose = 97,
        MsgChannelRequest = 98,
        MsgChannelSuccess = 99,
        MsgChannelFailure = 100,
    };

    // The reason codes of a disconnect (RFC 4250 §4.2.2)
    enum : uint32_t {
        DisconnectHostNotAllowed = 1,
        DisconnectProtocolError = 2,
        DisconnectKeyExchangeFailed = 3,
        DisconnectReserved = 4,
        DisconnectMacError = 5,
        DisconnectCompressionError = 6,
        DisconnectServiceNotAvailable = 7,
        DisconnectProtocolVersionNotSupported = 8,
        DisconnectHostKeyNotVerifiable = 9,
        DisconnectConnectionLost = 10,
        DisconnectByApplication = 11,
        DisconnectTooManyConnections = 12,
        DisconnectAuthCancelledByUser = 13,
        DisconnectNoMoreAuthMethods = 14,
        DisconnectIllegalUserName = 15,
    };

    // The reason codes of a channel's open failure (RFC 4254 §5.1)
    enum : uint32_t {
        OpenAdministrativelyProhibited = 1,
        OpenConnectFailed = 2,
        OpenUnknownChannelType = 3,
        OpenResourceShortage = 4,
    };

    // The reason of a disconnect in words, for an error's text
    inline const char* disconnect_reason(uint32_t code) noexcept {
        switch (code) {
            case DisconnectHostNotAllowed: return "host not allowed to connect";
            case DisconnectProtocolError: return "protocol error";
            case DisconnectKeyExchangeFailed: return "key exchange failed";
            case DisconnectReserved: return "reserved";
            case DisconnectMacError: return "MAC error";
            case DisconnectCompressionError: return "compression error";
            case DisconnectServiceNotAvailable: return "service not available";
            case DisconnectProtocolVersionNotSupported: return "protocol version not supported";
            case DisconnectHostKeyNotVerifiable: return "host key not verifiable";
            case DisconnectConnectionLost: return "connection lost";
            case DisconnectByApplication: return "disconnected by application";
            case DisconnectTooManyConnections: return "too many connections";
            case DisconnectAuthCancelledByUser: return "authentication cancelled by user";
            case DisconnectNoMoreAuthMethods: return "no more authentication methods available";
            case DisconnectIllegalUserName: return "illegal user name";
        }
        return "unknown reason";
    }

    SGCL_INLINE_HOT uint32_t load32(const uint8_t* p) noexcept {
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | uint32_t(p[3]);
    }

    SGCL_INLINE_HOT void store32(uint8_t* p, uint32_t v) noexcept {
        p[0] = uint8_t(v >> 24);
        p[1] = uint8_t(v >> 16);
        p[2] = uint8_t(v >> 8);
        p[3] = uint8_t(v);
    }

    SGCL_INLINE_HOT uint64_t load64(const uint8_t* p) noexcept {
        return uint64_t(load32(p)) << 32 | load32(p + 4);
    }

    SGCL_INLINE_HOT void store64(uint8_t* p, uint64_t v) noexcept {
        store32(p, uint32_t(v >> 32));
        store32(p + 4, uint32_t(v));
    }

    SGCL_INLINE_HOT slice<const byte> bytes_of(const void* p, size_t n) noexcept {
        return slice<const byte>(static_cast<const byte*>(p), n);
    }

    SGCL_INLINE_HOT slice<const byte> bytes_of(const Bytes& b) noexcept {
        return slice<const byte>(reinterpret_cast<const byte*>(b.data()), b.size());
    }

    SGCL_INLINE_HOT slice<byte> mutable_bytes_of(void* p, size_t n) noexcept {
        return slice<byte>(static_cast<byte*>(p), n);
    }

    // A buffer of secrets zeroed before it is let go
    SGCL_INLINE_HOT void wipe(Bytes& b) noexcept {
        if (!b.empty()) {
            crypto::detail::secure_zero(b.data(), b.size());
        }
        b.clear();
    }

    // A buffer holding a secret, zeroed when it goes
    struct SecretBuffer {
        Bytes b;

        SecretBuffer() noexcept = default;
        SecretBuffer(const SecretBuffer&) = delete;
        SecretBuffer& operator=(const SecretBuffer&) = delete;

        SGCL_INLINE_HOT SecretBuffer(SecretBuffer&& o) noexcept
        : b(std::move(o.b)) {
            o.b.clear();
        }

        SGCL_INLINE_HOT SecretBuffer& operator=(SecretBuffer&& o) noexcept {
            if (this != &o) {
                wipe(b);
                b = std::move(o.b);
                o.b.clear();
            }
            return *this;
        }

        SGCL_INLINE_HOT ~SecretBuffer() {
            wipe(b);
        }
    };

    // A field read: a view into the reader's buffer
    struct Span {
        const uint8_t* p = nullptr;
        size_t n = 0;

        SGCL_INLINE_HOT std::string_view view() const noexcept {
            return std::string_view(reinterpret_cast<const char*>(p), n);
        }

        SGCL_INLINE_HOT slice<const byte> bytes() const noexcept {
            return bytes_of(p, n);
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return n == 0;
        }

        SGCL_INLINE_HOT bool operator==(std::string_view s) const noexcept {
            return view() == s;
        }
    };

    // Appends SSH's types to a buffer
    class Writer {
    public:
        SGCL_INLINE_HOT explicit Writer(Bytes& out) noexcept
        : _out(out) {
        }

        SGCL_INLINE_HOT Writer& u8(uint8_t v) {
            _out.push_back(v);
            return *this;
        }

        SGCL_INLINE_HOT Writer& boolean(bool v) {
            _out.push_back(v ? 1 : 0);
            return *this;
        }

        SGCL_INLINE_HOT Writer& u32(uint32_t v) {
            size_t at = _out.size();
            _out.resize(at + 4);
            store32(_out.data() + at, v);
            return *this;
        }

        SGCL_INLINE_HOT Writer& u64(uint64_t v) {
            size_t at = _out.size();
            _out.resize(at + 8);
            store64(_out.data() + at, v);
            return *this;
        }

        // Bytes as they are, without a length
        SGCL_INLINE_HOT Writer& raw(const void* p, size_t n) {
            if (n) {
                size_t at = _out.size();
                _out.resize(at + n);
                sgcl::detail::copy_bytes(_out.data() + at, p, n);
            }
            return *this;
        }

        SGCL_INLINE_HOT Writer& raw(std::string_view s) {
            return raw(s.data(), s.size());
        }

        // A string: its length and its bytes
        SGCL_INLINE_HOT Writer& string(const void* p, size_t n) {
            u32(uint32_t(n));
            return raw(p, n);
        }

        SGCL_INLINE_HOT Writer& string(std::string_view s) {
            return string(s.data(), s.size());
        }

        SGCL_INLINE_HOT Writer& string(const Bytes& b) {
            return string(b.data(), b.size());
        }

        SGCL_INLINE_HOT Writer& string(const Span& s) {
            return string(s.p, s.n);
        }

        SGCL_INLINE_HOT Writer& string_of(const slice<const byte>& s) {
            return string(s.data(), s.size());
        }

        // An mpint of an unsigned number in big-endian bytes: its leading
        // zeros dropped, one put back when the top bit would read as a sign
        Writer& mpint(const uint8_t* p, size_t n) {
            while (n && *p == 0) {
                ++p;
                --n;
            }
            if (n && (*p & 0x80)) {
                u32(uint32_t(n + 1));
                u8(0);
            } else {
                u32(uint32_t(n));
            }
            return raw(p, n);
        }

        // A name-list: the names separated by commas
        template<class Names>
        Writer& name_list(const Names& names) {
            size_t at = _out.size();
            u32(0);
            bool first = true;
            for (const auto& n : names) {
                if (!first) {
                    _out.push_back(',');
                }
                first = false;
                raw(std::string_view(n));
            }
            store32(_out.data() + at, uint32_t(_out.size() - at - 4));
            return *this;
        }

        // The place of a length to fill in once what follows is written
        SGCL_INLINE_HOT size_t begin_string() {
            size_t at = _out.size();
            u32(0);
            return at;
        }

        SGCL_INLINE_HOT void end_string(size_t at) noexcept {
            store32(_out.data() + at, uint32_t(_out.size() - at - 4));
        }

        SGCL_INLINE_HOT Bytes& buffer() noexcept {
            return _out;
        }

    private:
        Bytes& _out;
    };

    // Takes SSH's types from bytes; past the end, or on a field that
    // breaks its type, ok() turns false and every later field is empty
    class Reader {
    public:
        SGCL_INLINE_HOT Reader(const uint8_t* p, size_t n) noexcept
        : _p(p)
        , _n(n) {
        }

        SGCL_INLINE_HOT explicit Reader(const Span& s) noexcept
        : _p(s.p)
        , _n(s.n) {
        }

        SGCL_INLINE_HOT bool ok() const noexcept {
            return _ok;
        }

        // Every byte read and nothing broken
        SGCL_INLINE_HOT bool done() const noexcept {
            return _ok && _n == 0;
        }

        SGCL_INLINE_HOT size_t left() const noexcept {
            return _n;
        }

        SGCL_INLINE_HOT const uint8_t* at() const noexcept {
            return _p;
        }

        SGCL_INLINE_HOT void fail() noexcept {
            _ok = false;
            _n = 0;
        }

        SGCL_INLINE_HOT uint8_t u8() noexcept {
            if (_n < 1) {
                fail();
                return 0;
            }
            uint8_t v = *_p++;
            --_n;
            return v;
        }

        // RFC 4251 §5: any non-zero byte is true
        SGCL_INLINE_HOT bool boolean() noexcept {
            return u8() != 0;
        }

        SGCL_INLINE_HOT uint32_t u32() noexcept {
            if (_n < 4) {
                fail();
                return 0;
            }
            uint32_t v = load32(_p);
            _p += 4;
            _n -= 4;
            return v;
        }

        SGCL_INLINE_HOT uint64_t u64() noexcept {
            if (_n < 8) {
                fail();
                return 0;
            }
            uint64_t v = load64(_p);
            _p += 8;
            _n -= 8;
            return v;
        }

        SGCL_INLINE_HOT Span raw(size_t n) noexcept {
            if (_n < n) {
                fail();
                return Span();
            }
            Span s{_p, n};
            _p += n;
            _n -= n;
            return s;
        }

        SGCL_INLINE_HOT Span rest() noexcept {
            return raw(_n);
        }

        SGCL_INLINE_HOT Span string() noexcept {
            uint32_t n = u32();
            return raw(n);
        }

        // An mpint that is not negative, in its minimal form (RFC 4251:
        // no needless leading byte); the magnitude's bytes, without the
        // zero that keeps the sign
        Span mpint() noexcept {
            Span s = string();
            if (!_ok || s.n == 0) {
                return s;
            }
            if (s.p[0] & 0x80) {   // negative
                fail();
                return Span();
            }
            if (s.p[0] == 0) {
                if (s.n == 1 || !(s.p[1] & 0x80)) {   // a zero that is not needed
                    fail();
                    return Span();
                }
                return Span{s.p + 1, s.n - 1};
            }
            return s;
        }

        // A name-list's names: not empty ones, ASCII without commas and
        // controls (RFC 4251 §5)
        std::vector<std::string_view> name_list() noexcept {
            std::vector<std::string_view> out;
            Span s = string();
            if (!_ok || s.n == 0) {
                return out;
            }
            std::string_view v = s.view();
            size_t start = 0;
            for (size_t i = 0; i <= v.size(); ++i) {
                if (i == v.size() || v[i] == ',') {
                    if (i == start) {
                        fail();
                        out.clear();
                        return out;
                    }
                    out.push_back(v.substr(start, i - start));
                    start = i + 1;
                } else if (uint8_t(v[i]) <= 0x20 || uint8_t(v[i]) >= 0x7F) {
                    fail();
                    out.clear();
                    return out;
                }
            }
            return out;
        }

    private:
        const uint8_t* _p;
        size_t _n;
        bool _ok = true;
    };

    // Whether a name is in a list of names
    template<class Names>
    SGCL_INLINE_HOT bool contains_name(const Names& names, std::string_view n) noexcept {
        for (const auto& x : names) {
            if (std::string_view(x) == n) {
                return true;
            }
        }
        return false;
    }

    // The first of the client's names the server also has (RFC 4253
    // §7.1); empty when none
    template<class A, class B>
    std::string_view first_common(const A& client, const B& server) noexcept {
        for (const auto& c : client) {
            if (contains_name(server, std::string_view(c))) {
                return std::string_view(c);
            }
        }
        return std::string_view();
    }

    // Text from the peer, for an error's message: what is printable, the
    // rest as '?', at most 256 bytes (a peer's text is not trusted to be
    // short or clean)
    inline std::string printable(std::string_view s) {
        std::string out;
        size_t n = s.size() < 256 ? s.size() : 256;
        out.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            char c = s[i];
            out.push_back((uint8_t(c) >= 0x20 && uint8_t(c) < 0x7F) ? c : '?');
        }
        return out;
    }
}
