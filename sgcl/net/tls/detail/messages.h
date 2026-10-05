//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"
#include "../../../core/aliases.h"
#include "../../../core/expected.h"
#include "../../../core/slice.h"

#include <array>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

// The handshake messages of TLS 1.3 and their extensions (RFC 8446 §4,
// RFC 6066 §3, RFC 7301 §3), read and written. Reading makes no copy and
// allocates nothing: a message is read into views of its own bytes (the
// handshake assembler's buffer, unmanaged), valid while that buffer holds
// the message, and every length on the wire is checked against what is
// left and against the bounds the RFC gives the vector before a byte of it
// is read. A view holds a slice, a tracked word, so it lives in a frame
// (a stack, a task's frame), never in the unmanaged block of a connection
// (Rule 1); what outlives the message is copied by the handshake machine.
// Writing goes into a std::vector the connection keeps, the lengths filled
// in when a block closes; a write cannot fail (a length past its vector's
// bound is a mistake of the program, asserted). The alerts a reader gives
// are the RFC's: decode_error for what cannot be read (a length, a
// truncation, bytes past the end), illegal_parameter for a value that is
// read and forbidden; what depends on the handshake's state (an extension
// the other side was not offered, one that is missing) is the machine's
// to decide, with validate_extensions.
namespace sgcl::net::tls::detail {
    // The bytes of a message, or of a piece of one: a view without an owner
    // into unmanaged memory
    // (bytes_of, schedule.h, makes one of a pointer and a length)
    using Bytes = slice<const byte>;

    // --- reading ------------------------------------------------------------

    // A cursor over bytes: every read checks what is left first, and the
    // first failure is kept (`failure()`, with the offset it happened at,
    // counted from the start of the message). A vector's length is checked
    // against the vector's bounds and against what is left before anything
    // of it is taken
    class Reader {
    public:
        SGCL_INLINE_HOT explicit Reader(const Bytes& in, uint32_t base = 0) noexcept
        : _begin(in.data()), _p(in.data()), _end(in.data() + in.size()), _base(base) {
        }

        SGCL_INLINE_HOT size_t remaining() const noexcept {
            return size_t(_end - _p);
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _p == _end;
        }

        // The offset of the next byte, from the start of the message
        SGCL_INLINE_HOT uint32_t offset() const noexcept {
            return _base + uint32_t(_p - _begin);
        }

        SGCL_INLINE_HOT bool u8(uint8_t& v) noexcept {
            if (!_need(1)) {
                return false;
            }
            v = uint8_t(_p[0]);
            _p += 1;
            return true;
        }

        SGCL_INLINE_HOT bool u16(uint16_t& v) noexcept {
            if (!_need(2)) {
                return false;
            }
            v = uint16_t(uint16_t(_p[0]) << 8 | uint16_t(_p[1]));
            _p += 2;
            return true;
        }

        SGCL_INLINE_HOT bool u24(uint32_t& v) noexcept {
            if (!_need(3)) {
                return false;
            }
            v = uint32_t(_p[0]) << 16 | uint32_t(_p[1]) << 8 | uint32_t(_p[2]);
            _p += 3;
            return true;
        }

        SGCL_INLINE_HOT bool u32(uint32_t& v) noexcept {
            if (!_need(4)) {
                return false;
            }
            v = uint32_t(_p[0]) << 24 | uint32_t(_p[1]) << 16 | uint32_t(_p[2]) << 8 | uint32_t(_p[3]);
            _p += 4;
            return true;
        }

        SGCL_INLINE_HOT bool bytes(size_t n, Bytes& out) noexcept {
            if (!_need(n)) {
                return false;
            }
            out = Bytes(_p, n);
            _p += n;
            return true;
        }

        // A vector with a length of 1, 2 or 3 bytes in front (§3.4), its
        // length within [min, max]
        SGCL_INLINE_HOT bool vec8(Bytes& out, size_t min, size_t max) noexcept {
            uint8_t n;
            return u8(n) && _vector(n, min, max, out);
        }

        SGCL_INLINE_HOT bool vec16(Bytes& out, size_t min, size_t max) noexcept {
            uint16_t n;
            return u16(n) && _vector(n, min, max, out);
        }

        SGCL_INLINE_HOT bool vec24(Bytes& out, size_t min, size_t max) noexcept {
            uint32_t n;
            return u24(n) && _vector(n, min, max, out);
        }

        // The same as a reader of its own, whose offsets go on from this one's
        SGCL_INLINE_HOT bool sub8(Reader& out, size_t min, size_t max) noexcept {
            return _sub(&Reader::vec8, out, min, max);
        }

        SGCL_INLINE_HOT bool sub16(Reader& out, size_t min, size_t max) noexcept {
            return _sub(&Reader::vec16, out, min, max);
        }

        SGCL_INLINE_HOT bool sub24(Reader& out, size_t min, size_t max) noexcept {
            return _sub(&Reader::vec24, out, min, max);
        }

        // Everything read: bytes after the last field are decode_error
        SGCL_INLINE_HOT bool end(const char* what = "bytes past the end of the structure") noexcept {
            return empty() || fail(AlertDescription::decode_error, what);
        }

        // The failure at the current offset, the first one kept; false
        bool fail(AlertDescription d, const char* what) noexcept {
            if (!_failed) {
                _failed = true;
                _failure = Alert{d, offset(), what};
            }
            return false;
        }

        // A failure of a nested reader taken as this one's
        bool fail(const Alert& a) noexcept {
            if (!_failed) {
                _failed = true;
                _failure = a;
            }
            return false;
        }

        SGCL_INLINE_HOT const Alert& failure() const noexcept {
            return _failure;
        }

    private:
        SGCL_INLINE_HOT bool _need(size_t n) noexcept {
            return remaining() >= n || fail(AlertDescription::decode_error, "the data ends inside a field");
        }

        SGCL_INLINE_HOT bool _vector(size_t n, size_t min, size_t max, Bytes& out) noexcept {
            if (n < min || n > max) {
                return fail(AlertDescription::decode_error, "a vector's length outside its bounds");
            }
            if (n > remaining()) {
                return fail(AlertDescription::decode_error, "a vector longer than the data");
            }
            out = Bytes(_p, n);
            _p += n;
            return true;
        }

        SGCL_INLINE_HOT bool _sub(bool (Reader::*vec)(Bytes&, size_t, size_t), Reader& out, size_t min, size_t max) noexcept {
            uint32_t at = offset();
            Bytes b;
            if (!(this->*vec)(b, min, max)) {
                return false;
            }
            out = Reader(b, at + uint32_t(offset() - at - b.size()));
            return true;
        }

        const byte* _begin;
        const byte* _p;
        const byte* _end;
        uint32_t _base;
        bool _failed = false;
        Alert _failure;
    };

    // The failure of a reader as the result of a parse
    inline unexpected<Alert> failed(const Reader& r) noexcept {
        return unexpected<Alert>(r.failure());
    }

    inline unexpected<Alert> failed(AlertDescription d, uint32_t offset, const char* what) noexcept {
        return unexpected<Alert>(Alert{d, offset, what});
    }

    // --- writing ------------------------------------------------------------

    // Bytes appended to a vector the connection keeps (reused from message
    // to message); a block reserves its length and fills it in when it
    // closes, at the end of its scope or by close()
    class Builder {
    public:
        SGCL_INLINE_HOT explicit Builder(std::vector<byte>& out) noexcept
        : _out(&out) {
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _out->size();
        }

        SGCL_INLINE_HOT void u8(uint8_t v) noexcept {
            _out->push_back(byte(v));
        }

        SGCL_INLINE_HOT void u16(uint16_t v) noexcept {
            u8(uint8_t(v >> 8));
            u8(uint8_t(v));
        }

        SGCL_INLINE_HOT void u24(uint32_t v) noexcept {
            assert(v < (1u << 24));
            u8(uint8_t(v >> 16));
            u8(uint8_t(v >> 8));
            u8(uint8_t(v));
        }

        SGCL_INLINE_HOT void u32(uint32_t v) noexcept {
            u16(uint16_t(v >> 16));
            u16(uint16_t(v));
        }

        SGCL_INLINE_HOT void bytes(const Bytes& b) noexcept {
            _out->insert(_out->end(), b.begin(), b.end());
        }

        SGCL_INLINE_HOT void bytes(const void* p, size_t n) noexcept {
            auto b = static_cast<const byte*>(p);
            _out->insert(_out->end(), b, b + n);
        }

        // A length prefixed vector, filled in when the block closes
        class Block {
        public:
            Block(Builder& w, int width, size_t max) noexcept
            : _w(&w), _at(w.size()), _width(width), _max(max) {
                for (int i = 0; i < width; ++i) {
                    w.u8(0);
                }
            }

            SGCL_INLINE_HOT Block(Block&& o) noexcept
            : _w(std::exchange(o._w, nullptr)), _at(o._at), _width(o._width), _max(o._max) {
            }

            Block(const Block&) = delete;
            Block& operator=(const Block&) = delete;
            Block& operator=(Block&&) = delete;

            SGCL_INLINE_HOT ~Block() {
                close();
            }

            void close() noexcept {
                if (!_w) {
                    return;
                }
                auto& out = *_w->_out;
                size_t n = out.size() - _at - size_t(_width);
                assert(n <= _max && "a vector past its bound");
                for (int i = 0; i < _width; ++i) {
                    out[_at + size_t(i)] = byte(uint8_t(n >> (8 * (_width - 1 - i))));
                }
                _w = nullptr;
            }

        private:
            Builder* _w;
            size_t _at;
            int _width;
            size_t _max;
        };

        [[nodiscard]] SGCL_INLINE_HOT Block block8(size_t max = 0xFF) noexcept {
            return Block(*this, 1, max);
        }

        [[nodiscard]] SGCL_INLINE_HOT Block block16(size_t max = 0xFFFF) noexcept {
            return Block(*this, 2, max);
        }

        [[nodiscard]] SGCL_INLINE_HOT Block block24(size_t max = 0xFFFFFF) noexcept {
            return Block(*this, 3, max);
        }

        // A handshake message: the type, then the body in a block of 24 bits (§4)
        [[nodiscard]] SGCL_INLINE_HOT Block message(HandshakeType t) noexcept {
            u8(uint8_t(t));
            return block24();
        }

        // An extension: the type, then the body in a block of 16 bits (§4.2)
        [[nodiscard]] SGCL_INLINE_HOT Block extension(uint16_t type) noexcept {
            u16(type);
            return block16();
        }

        [[nodiscard]] SGCL_INLINE_HOT Block extension(ExtensionType type) noexcept {
            return extension(uint16_t(type));
        }

    private:
        std::vector<byte>* _out;
    };

    // --- lists --------------------------------------------------------------

    // A list of 16-bit values on the wire (cipher suites, groups, signature
    // schemes, versions), its length checked even
    struct U16List {
        Bytes raw;

        SGCL_INLINE_HOT size_t size() const noexcept {
            return raw.size() / 2;
        }

        SGCL_INLINE_HOT uint16_t operator[](size_t i) const noexcept {
            return uint16_t(uint16_t(raw[2 * i]) << 8 | uint16_t(raw[2 * i + 1]));
        }

        bool contains(uint16_t v) const noexcept {
            for (size_t i = 0; i < size(); ++i) {
                if ((*this)[i] == v) {
                    return true;
                }
            }
            return false;
        }

        struct iterator {
            using iterator_category = std::input_iterator_tag;
            using value_type = uint16_t;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = uint16_t;
            const U16List* list;
            size_t i;
            SGCL_INLINE_HOT uint16_t operator*() const noexcept {
                return (*list)[i];
            }
            SGCL_INLINE_HOT iterator& operator++() noexcept {
                ++i;
                return *this;
            }
            SGCL_INLINE_HOT iterator operator++(int) noexcept {
                iterator t = *this;
                ++*this;
                return t;
            }
            SGCL_INLINE_HOT bool operator==(const iterator& o) const noexcept {
                return i == o.i;
            }
        };

        SGCL_INLINE_HOT iterator begin() const noexcept {
            return {this, 0};
        }

        SGCL_INLINE_HOT iterator end() const noexcept {
            return {this, size()};
        }
    };

    SGCL_INLINE_HOT expected<U16List, Alert> read_u16_list(Reader& r, int width, size_t min, size_t max) noexcept {
        Bytes b;
        if (!(width == 1 ? r.vec8(b, min, max) : r.vec16(b, min, max))) {
            return failed(r);
        }
        if (b.size() % 2) {
            return failed(AlertDescription::decode_error, r.offset(), "a list of 16-bit values of an odd length");
        }
        return U16List{b};
    }

    template<class R>
    void write_u16_list(Builder& w, int width, const R& values) noexcept {
        auto b = width == 1 ? w.block8() : w.block16();
        for (uint16_t v : values) {
            w.u16(v);
        }
    }

    // --- extensions ---------------------------------------------------------

    struct Extension {
        uint16_t type;
        Bytes body;
    };

    SGCL_INLINE_HOT constexpr uint64_t bit_of(uint16_t type) noexcept {
        return type < 64 ? uint64_t(1) << type : 0;
    }

    SGCL_INLINE_HOT constexpr uint64_t bit_of(ExtensionType type) noexcept {
        return bit_of(uint16_t(type));
    }

    // The extensions of a message, in their order, read once (duplicates
    // refused) and walked as often as needed; `mask` has a bit for every
    // type below 64 that is there
    struct Extensions {
        Bytes raw;              // the entries, without the vector's length
        size_t count = 0;
        uint64_t mask = 0;

        struct iterator {
            using iterator_category = std::input_iterator_tag;
            using value_type = Extension;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = Extension;
            const byte* p;
            SGCL_INLINE_HOT Extension operator*() const noexcept {
                uint16_t type = uint16_t(uint16_t(p[0]) << 8 | uint16_t(p[1]));
                size_t n = size_t(uint16_t(p[2]) << 8 | uint16_t(p[3]));
                return Extension{type, Bytes(p + 4, n)};
            }
            SGCL_INLINE_HOT iterator& operator++() noexcept {
                size_t n = size_t(uint16_t(p[2]) << 8 | uint16_t(p[3]));
                p += 4 + n;
                return *this;
            }
            SGCL_INLINE_HOT iterator operator++(int) noexcept {
                iterator t = *this;
                ++*this;
                return t;
            }
            SGCL_INLINE_HOT bool operator==(const iterator& o) const noexcept {
                return p == o.p;
            }
        };

        SGCL_INLINE_HOT iterator begin() const noexcept {
            return {raw.data()};
        }

        SGCL_INLINE_HOT iterator end() const noexcept {
            return {raw.data() + raw.size()};
        }

        SGCL_INLINE_HOT bool has(ExtensionType t) const noexcept {
            return (mask & bit_of(t)) != 0;
        }

        // The body of the extension of this type, if it is there
        optional<Bytes> find(uint16_t type) const noexcept {
            if (type < 64 && !(mask & bit_of(type))) {
                return nullopt;
            }
            for (auto e : *this) {
                if (e.type == type) {
                    return e.body;
                }
            }
            return nullopt;
        }

        SGCL_INLINE_HOT optional<Bytes> find(ExtensionType t) const noexcept {
            return find(uint16_t(t));
        }
    };

    // The extensions vector of a message, its length within [min, max];
    // a type twice is illegal_parameter (§4.2), and so is pre_shared_key
    // anywhere but last where `psk_last` (a ClientHello, §4.2.11)
    inline expected<Extensions, Alert> read_extensions(Reader& r, size_t min, size_t max, bool psk_last = false) noexcept {
        Reader list(Bytes(), 0);
        if (!r.sub16(list, min, max)) {
            return failed(r);
        }
        Extensions x;
        const byte* first = nullptr;
        size_t bytes = list.remaining();
        std::array<uint16_t, 64> few;
        std::vector<uint64_t> many;   // a bit per type once there are more than 64 (never in practice)
        bool psk_seen = false;
        while (!list.empty()) {
            uint32_t at = list.offset();
            uint16_t type;
            Bytes body;
            if (!list.u16(type) || !list.vec16(body, 0, 0xFFFF)) {
                return failed(list);
            }
            if (!first) {
                first = body.data() - 4;
            }
            if (psk_seen) {
                return failed(AlertDescription::illegal_parameter, at, "pre_shared_key is not the last extension");
            }
            bool dup = false;
            if (x.count < few.size()) {
                for (size_t i = 0; i < x.count; ++i) {
                    dup |= few[i] == type;
                }
                few[x.count] = type;
            } else {
                if (many.empty()) {
                    many.assign(1024, 0);
                    for (uint16_t t : few) {
                        many[t / 64] |= uint64_t(1) << (t % 64);
                    }
                }
                dup = (many[type / 64] >> (type % 64)) & 1;
                many[type / 64] |= uint64_t(1) << (type % 64);
            }
            if (dup) {
                return failed(AlertDescription::illegal_parameter, at, "an extension twice in one message");
            }
            if (psk_last && type == uint16_t(ExtensionType::pre_shared_key)) {
                psk_seen = true;
            }
            x.mask |= bit_of(type);
            ++x.count;
        }
        x.raw = first ? Bytes(first, bytes) : Bytes();
        return x;
    }

    template<class F>
    SGCL_INLINE_HOT void write_extensions(Builder& w, F&& entries) noexcept(std::is_nothrow_invocable_v<F&, Builder&>) {
        auto b = w.block16();
        entries(w);
    }

    // The entries of an extensions vector copied as they are (a message
    // written back from what was read)
    SGCL_INLINE_HOT void write_raw(Builder& w, const Extensions& x) noexcept {
        w.bytes(x.raw);
    }

    // What the machines check once the other side's message is read
    // (§4.2): an extension RFC 8446 knows in a message it is not allowed in
    // is illegal_parameter; one in a reply (ServerHello, HelloRetryRequest,
    // EncryptedExtensions, a certificate entry) that the ClientHello did
    // not offer (`offered`: its mask), or one this implementation does not
    // know there, is unsupported_extension. In a ClientHello, a
    // CertificateRequest and a NewSessionTicket an extension nobody knows
    // is passed over (§4.1.2, §4.3.2, §4.6.1). `cookie` in a
    // HelloRetryRequest is the server's own and needs no offer.
    namespace ext_allowed {
        inline constexpr uint64_t bits(std::initializer_list<int> types) noexcept {
            uint64_t m = 0;
            for (int t : types) {
                m |= uint64_t(1) << t;
            }
            return m;
        }
        inline constexpr uint64_t client_hello = bits({0, 1, 5, 10, 13, 14, 15, 16, 18, 19, 20, 21, 28, 41, 42, 43, 44, 45, 47, 49, 50, 51});
        inline constexpr uint64_t server_hello = bits({41, 43, 51});
        inline constexpr uint64_t retry = bits({43, 44, 51});
        inline constexpr uint64_t encrypted_extensions = bits({0, 1, 10, 14, 15, 16, 19, 20, 28, 42});
        inline constexpr uint64_t certificate = bits({5, 18});
        inline constexpr uint64_t certificate_request = bits({5, 13, 18, 47, 48, 50});
        inline constexpr uint64_t new_session_ticket = bits({42});
        inline constexpr uint64_t recognized = client_hello | encrypted_extensions | certificate_request | bits({48});
    }

    inline expected<void, Alert> validate_extensions(HandshakeType type, bool retry, const Extensions& x, uint64_t offered) noexcept {
        using namespace ext_allowed;
        uint64_t allowed = 0;
        bool reply = false;
        switch (type) {
            case HandshakeType::client_hello: allowed = client_hello; break;
            case HandshakeType::server_hello: allowed = retry ? ext_allowed::retry : server_hello; reply = true; break;
            case HandshakeType::encrypted_extensions: allowed = encrypted_extensions; reply = true; break;
            case HandshakeType::certificate: allowed = certificate; reply = true; break;
            case HandshakeType::certificate_request: allowed = certificate_request; break;
            case HandshakeType::new_session_ticket: allowed = new_session_ticket; break;
            default: return failed(AlertDescription::internal_error, 0, "no extensions in this message");
        }
        for (auto e : x) {
            uint64_t bit = bit_of(e.type);
            bool is_known = (bit & ext_allowed::recognized) != 0;
            if (!is_known) {
                if (reply) {
                    return failed(AlertDescription::unsupported_extension, 0, "an extension that was not offered");
                }
                continue;   // passed over
            }
            if (!(bit & allowed)) {
                return failed(AlertDescription::illegal_parameter, 0, "an extension not allowed in this message");
            }
            bool own = retry && e.type == uint16_t(ExtensionType::cookie);
            if (reply && !own && !(bit & offered)) {
                return failed(AlertDescription::unsupported_extension, 0, "an extension that was not offered");
            }
        }
        return {};
    }

    // --- the bodies of the extensions ----------------------------------------

    // An extension whose body is empty (server_name and early_data in a
    // reply, early_data in a ClientHello)
    SGCL_INLINE_HOT expected<void, Alert> read_empty(const Bytes& body) noexcept {
        if (!body.empty()) {
            return failed(AlertDescription::decode_error, 0, "an extension that must be empty is not");
        }
        return {};
    }

    // server_name (RFC 6066 §3): the host_name of the list; other name types
    // passed over, a second host_name illegal_parameter; a name of 1 to 255
    // bytes
    inline expected<Bytes, Alert> read_server_name(const Bytes& body) noexcept {
        Reader r(body);
        Reader list(Bytes(), 0);
        if (!r.sub16(list, 1, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        optional<Bytes> host;
        while (!list.empty()) {
            uint32_t at = list.offset();
            uint8_t type;
            Bytes name;
            if (!list.u8(type) || !list.vec16(name, 1, 0xFFFF)) {
                return failed(list);
            }
            if (type != 0) {
                continue;
            }
            if (host) {
                return failed(AlertDescription::illegal_parameter, at, "two host names in server_name");
            }
            if (name.size() > 255) {
                return failed(AlertDescription::decode_error, at, "a host name longer than 255 bytes");
            }
            host = name;
        }
        if (!host) {
            return failed(AlertDescription::illegal_parameter, 0, "server_name without a host name");
        }
        return *host;
    }

    SGCL_INLINE_HOT void write_server_name(Builder& w, const Bytes& host) noexcept {
        assert(!host.empty() && host.size() <= 255);
        auto list = w.block16();
        w.u8(0);
        auto name = w.block16();
        w.bytes(host);
    }

    // supported_groups (§4.2.7), signature_algorithms and
    // signature_algorithms_cert (§4.2.3)
    SGCL_INLINE_HOT expected<U16List, Alert> read_groups(const Bytes& body) noexcept {
        Reader r(body);
        auto l = read_u16_list(r, 2, 2, 0xFFFF);
        if (l && !r.end()) {
            return failed(r);
        }
        return l;
    }

    SGCL_INLINE_HOT expected<U16List, Alert> read_signature_schemes(const Bytes& body) noexcept {
        Reader r(body);
        auto l = read_u16_list(r, 2, 2, 0xFFFE);
        if (l && !r.end()) {
            return failed(r);
        }
        return l;
    }

    template<class R>
    SGCL_INLINE_HOT void write_groups(Builder& w, const R& groups) noexcept {
        write_u16_list(w, 2, groups);
    }

    template<class R>
    SGCL_INLINE_HOT void write_signature_schemes(Builder& w, const R& schemes) noexcept {
        write_u16_list(w, 2, schemes);
    }

    // supported_versions (§4.2.1): a list in a ClientHello, one version in
    // a ServerHello or a HelloRetryRequest
    SGCL_INLINE_HOT expected<U16List, Alert> read_versions_offered(const Bytes& body) noexcept {
        Reader r(body);
        auto l = read_u16_list(r, 1, 2, 254);
        if (l && !r.end()) {
            return failed(r);
        }
        return l;
    }

    SGCL_INLINE_HOT expected<uint16_t, Alert> read_version_selected(const Bytes& body) noexcept {
        Reader r(body);
        uint16_t v;
        if (!r.u16(v) || !r.end()) {
            return failed(r);
        }
        return v;
    }

    template<class R>
    SGCL_INLINE_HOT void write_versions_offered(Builder& w, const R& versions) noexcept {
        write_u16_list(w, 1, versions);
    }

    SGCL_INLINE_HOT void write_version_selected(Builder& w, uint16_t v) noexcept {
        w.u16(v);
    }

    // key_share (§4.2.8): the length of a share of a group v1 knows, from
    // the client and from the server; 0 for a group it does not
    SGCL_INLINE_HOT constexpr size_t share_size(uint16_t group, bool server) noexcept {
        switch (Group(group)) {
            case Group::x25519: return 32;
            case Group::secp256r1: return 65;
            case Group::secp384r1: return 97;
            case Group::x25519_mlkem768: return server ? 1088 + 32 : 1184 + 32;
        }
        return 0;
    }

    struct KeyShare {
        uint16_t group;
        Bytes key;
    };

    // A share checked: of the size its group has, and a point of a curve
    // uncompressed (§4.2.8.2)
    inline bool share_ok(Reader& r, uint32_t at, uint16_t group, const Bytes& key, bool server) noexcept {
        size_t n = share_size(group, server);
        if (n && key.size() != n) {
            r.fail(AlertDescription::illegal_parameter, "a key share of the wrong size for its group");
            (void)at;
            return false;
        }
        if ((Group(group) == Group::secp256r1 || Group(group) == Group::secp384r1) && uint8_t(key[0]) != 4) {
            r.fail(AlertDescription::illegal_parameter, "a key share that is not an uncompressed point");
            return false;
        }
        return true;
    }

    // The client's shares, walked in their order
    struct KeyShareList {
        Bytes raw;

        struct iterator {
            using iterator_category = std::input_iterator_tag;
            using value_type = KeyShare;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = KeyShare;
            const byte* p;
            SGCL_INLINE_HOT KeyShare operator*() const noexcept {
                uint16_t g = uint16_t(uint16_t(p[0]) << 8 | uint16_t(p[1]));
                size_t n = size_t(uint16_t(p[2]) << 8 | uint16_t(p[3]));
                return KeyShare{g, Bytes(p + 4, n)};
            }
            SGCL_INLINE_HOT iterator& operator++() noexcept {
                p += 4 + (size_t(uint16_t(p[2]) << 8 | uint16_t(p[3])));
                return *this;
            }
            SGCL_INLINE_HOT iterator operator++(int) noexcept {
                iterator t = *this;
                ++*this;
                return t;
            }
            SGCL_INLINE_HOT bool operator==(const iterator& o) const noexcept {
                return p == o.p;
            }
        };

        SGCL_INLINE_HOT iterator begin() const noexcept {
            return {raw.data()};
        }

        SGCL_INLINE_HOT iterator end() const noexcept {
            return {raw.data() + raw.size()};
        }

        optional<Bytes> find(uint16_t group) const noexcept {
            for (auto s : *this) {
                if (s.group == group) {
                    return s.key;
                }
            }
            return nullopt;
        }
    };

    inline expected<KeyShareList, Alert> read_key_shares(const Bytes& body) noexcept {
        Reader r(body);
        Bytes raw;
        if (!r.vec16(raw, 0, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        Reader list(raw, 2);
        unsigned seen = 0;   // the groups v1 knows, one bit each: one of them twice is illegal_parameter (§4.2.8)
        while (!list.empty()) {
            uint32_t at = list.offset();
            uint16_t group;
            Bytes key;
            if (!list.u16(group) || !list.vec16(key, 1, 0xFFFF)) {
                return failed(list);
            }
            if (!share_ok(list, at, group, key, false)) {
                return failed(list);
            }
            unsigned bit = Group(group) == Group::x25519 ? 1 : Group(group) == Group::secp256r1 ? 2 : Group(group) == Group::secp384r1 ? 4 : Group(group) == Group::x25519_mlkem768 ? 8 : 0;
            if (bit & seen) {
                return failed(AlertDescription::illegal_parameter, at, "two key shares of one group");
            }
            seen |= bit;
        }
        return KeyShareList{raw};
    }

    inline expected<KeyShare, Alert> read_key_share_selected(const Bytes& body) noexcept {
        Reader r(body);
        uint16_t group;
        Bytes key;
        if (!r.u16(group) || !r.vec16(key, 1, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        if (!share_ok(r, 0, group, key, true)) {
            return failed(r);
        }
        return KeyShare{group, key};
    }

    // A HelloRetryRequest's: the group alone
    SGCL_INLINE_HOT expected<uint16_t, Alert> read_key_share_retry(const Bytes& body) noexcept {
        Reader r(body);
        uint16_t group;
        if (!r.u16(group) || !r.end()) {
            return failed(r);
        }
        return group;
    }

    template<class R>
    void write_key_shares(Builder& w, const R& shares) noexcept {
        auto list = w.block16();
        for (const KeyShare& s : shares) {
            w.u16(s.group);
            auto k = w.block16();
            w.bytes(s.key);
        }
    }

    SGCL_INLINE_HOT void write_key_share_selected(Builder& w, const KeyShare& s) noexcept {
        w.u16(s.group);
        auto k = w.block16();
        w.bytes(s.key);
    }

    SGCL_INLINE_HOT void write_key_share_retry(Builder& w, uint16_t group) noexcept {
        w.u16(group);
    }

    // application_layer_protocol_negotiation (RFC 7301 §3.1): a list of
    // names of 1 to 255 bytes; exactly one in a reply
    struct NameList {
        Bytes raw;

        struct iterator {
            using iterator_category = std::input_iterator_tag;
            using value_type = Bytes;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = Bytes;
            const byte* p;
            SGCL_INLINE_HOT Bytes operator*() const noexcept {
                return Bytes(p + 1, size_t(uint8_t(p[0])));
            }
            SGCL_INLINE_HOT iterator& operator++() noexcept {
                p += 1 + size_t(uint8_t(p[0]));
                return *this;
            }
            SGCL_INLINE_HOT iterator operator++(int) noexcept {
                iterator t = *this;
                ++*this;
                return t;
            }
            SGCL_INLINE_HOT bool operator==(const iterator& o) const noexcept {
                return p == o.p;
            }
        };

        SGCL_INLINE_HOT iterator begin() const noexcept {
            return {raw.data()};
        }

        SGCL_INLINE_HOT iterator end() const noexcept {
            return {raw.data() + raw.size()};
        }
    };

    inline expected<NameList, Alert> read_protocols(const Bytes& body) noexcept {
        Reader r(body);
        Bytes raw;
        if (!r.vec16(raw, 2, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        Reader list(raw, 2);
        while (!list.empty()) {
            Bytes name;
            if (!list.vec8(name, 1, 0xFF)) {
                return failed(list);
            }
        }
        return NameList{raw};
    }

    inline expected<Bytes, Alert> read_protocol_selected(const Bytes& body) noexcept {
        auto l = read_protocols(body);
        if (!l) {
            return unexpected<Alert>(l.error());
        }
        auto it = l->begin();
        Bytes one = *it;
        if (++it != l->end()) {
            return failed(AlertDescription::illegal_parameter, 0, "more than one protocol in a reply");
        }
        return one;
    }

    template<class R>
    void write_protocols(Builder& w, const R& names) noexcept {
        auto list = w.block16();
        for (const auto& n : names) {
            Bytes b = bytes_of(n.data(), n.size());
            assert(!b.empty() && b.size() <= 255);
            auto one = w.block8();
            w.bytes(b);
        }
    }

    // cookie (§4.2.2)
    SGCL_INLINE_HOT expected<Bytes, Alert> read_cookie(const Bytes& body) noexcept {
        Reader r(body);
        Bytes c;
        if (!r.vec16(c, 1, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        return c;
    }

    SGCL_INLINE_HOT void write_cookie(Builder& w, const Bytes& c) noexcept {
        auto b = w.block16();
        w.bytes(c);
    }

    // psk_key_exchange_modes (§4.2.9): read for its syntax
    SGCL_INLINE_HOT expected<Bytes, Alert> read_psk_modes(const Bytes& body) noexcept {
        Reader r(body);
        Bytes m;
        if (!r.vec8(m, 1, 0xFF) || !r.end()) {
            return failed(r);
        }
        return m;
    }

    SGCL_INLINE_HOT void write_psk_modes(Builder& w, const Bytes& modes) noexcept {
        auto b = w.block8();
        w.bytes(modes);
    }

    // early_data in a NewSessionTicket (§4.2.10): max_early_data_size
    SGCL_INLINE_HOT expected<uint32_t, Alert> read_early_data_limit(const Bytes& body) noexcept {
        Reader r(body);
        uint32_t n;
        if (!r.u32(n) || !r.end()) {
            return failed(r);
        }
        return n;
    }

    // pre_shared_key in a ClientHello (§4.2.11): its syntax, as many binders
    // as identities; the entries walked with identity() and binder()
    struct PskIdentity {
        Bytes identity;
        uint32_t obfuscated_age = 0;
    };

    struct PreSharedKeys {
        Bytes identities;   // the entries, each identity<1..2^16-1> and a 32-bit age
        Bytes binders;      // the entries, each binder<32..255>
        size_t count = 0;

        // The i-th entry of each list (i < count; read once already: cannot fail)
        PskIdentity identity(size_t i) const noexcept {
            Reader r(identities);
            PskIdentity id;
            for (size_t k = 0; k <= i; ++k) {
                (void)r.vec16(id.identity, 1, 0xFFFF);
                (void)r.u32(id.obfuscated_age);
            }
            return id;
        }

        Bytes binder(size_t i) const noexcept {
            Reader r(binders);
            Bytes b;
            for (size_t k = 0; k <= i; ++k) {
                (void)r.vec8(b, 32, 255);
            }
            return b;
        }

        // The ClientHello's bytes the binders are computed over (§4.2.11.2):
        // the message from its header up to the binders' list, its length
        // included (`message` the whole ClientHello the extension is of)
        SGCL_INLINE_HOT size_t truncated_size(const Bytes& message) const noexcept {
            return size_t(binders.data() - message.data()) - 2;
        }
    };

    inline expected<PreSharedKeys, Alert> read_pre_shared_keys(const Bytes& body) noexcept {
        Reader r(body);
        PreSharedKeys k;
        if (!r.vec16(k.identities, 7, 0xFFFF) || !r.vec16(k.binders, 33, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        Reader ids(k.identities, 2);
        while (!ids.empty()) {
            Bytes id;
            uint32_t age;
            if (!ids.vec16(id, 1, 0xFFFF) || !ids.u32(age)) {
                return failed(ids);
            }
            ++k.count;
        }
        Reader bs(k.binders, uint32_t(4 + k.identities.size()));
        size_t binders = 0;
        while (!bs.empty()) {
            Bytes b;
            if (!bs.vec8(b, 32, 255)) {
                return failed(bs);
            }
            ++binders;
        }
        if (binders != k.count) {
            return failed(AlertDescription::illegal_parameter, 0, "pre_shared_key with as many binders as identities it has not");
        }
        return k;
    }

    // pre_shared_key in a ServerHello: the identity chosen
    SGCL_INLINE_HOT expected<uint16_t, Alert> read_pre_shared_key_selected(const Bytes& body) noexcept {
        return read_version_selected(body);
    }

    SGCL_INLINE_HOT void write_pre_shared_key_selected(Builder& w, uint16_t identity) noexcept {
        w.u16(identity);
    }

    // certificate_authorities (§4.2.4): the distinguished names (each the
    // DER of an X.501 Name) of the authorities whose certificates the
    // sender takes, walked in their order
    struct NameDers {
        Bytes raw;   // the entries, each DistinguishedName<1..2^16-1>
        size_t count = 0;

        template<class F>
        void each(F&& f) const noexcept(std::is_nothrow_invocable_v<F&, const Bytes&>) {
            Reader r(raw);
            Bytes n;
            while (r.vec16(n, 1, 0xFFFF)) {
                f(n);
            }
        }
    };

    inline expected<NameDers, Alert> read_certificate_authorities(const Bytes& body) noexcept {
        Reader r(body);
        NameDers names;
        if (!r.vec16(names.raw, 3, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        Reader list(names.raw, 2);
        while (!list.empty()) {
            Bytes n;
            if (!list.vec16(n, 1, 0xFFFF)) {
                return failed(list);
            }
            ++names.count;
        }
        return names;
    }

    // The names given (a range of Bytes, each 1 to 2^16 - 1 bytes, the
    // whole at most 2^16 - 1: the caller's to keep it so)
    template<class R>
    void write_certificate_authorities(Builder& w, const R& names) noexcept {
        auto list = w.block16();
        for (const Bytes& n : names) {
            auto one = w.block16();
            w.bytes(n);
        }
    }

    // record_size_limit (RFC 8449 §4): read for its syntax, not acted on
    SGCL_INLINE_HOT expected<uint16_t, Alert> read_record_size_limit(const Bytes& body) noexcept {
        auto v = read_version_selected(body);
        if (v && *v < 64) {
            return failed(AlertDescription::illegal_parameter, 0, "record_size_limit below 64");
        }
        return v;
    }

    // --- the messages -------------------------------------------------------

    // A whole message as the assembler gives it: the type and the body
    struct Handshake {
        uint8_t type;
        Bytes body;
    };

    // The header of a message, its length the rest exactly
    inline expected<Handshake, Alert> read_handshake(const Bytes& message) noexcept {
        Reader r(message);
        uint8_t type;
        uint32_t n;
        Bytes body;
        if (!r.u8(type) || !r.u24(n) || !r.bytes(n, body) || !r.end("bytes past the message")) {
            return failed(r);
        }
        return Handshake{type, body};
    }

    // ClientHello (§4.1.2)
    struct ClientHello {
        uint16_t legacy_version = Tls12;
        Bytes random;           // 32 bytes
        Bytes session_id;       // 0 to 32
        U16List cipher_suites;
        Extensions extensions;
    };

    inline expected<ClientHello, Alert> read_client_hello(const Bytes& body) noexcept {
        Reader r(body, 4);
        ClientHello m;
        Bytes compression;
        if (!r.u16(m.legacy_version) || !r.bytes(32, m.random) || !r.vec8(m.session_id, 0, 32)) {
            return failed(r);
        }
        auto suites = read_u16_list(r, 2, 2, 0xFFFE);
        if (!suites) {
            return unexpected<Alert>(suites.error());
        }
        m.cipher_suites = *suites;
        uint32_t at = r.offset();
        if (!r.vec8(compression, 1, 0xFF)) {
            return failed(r);
        }
        if (m.legacy_version != Tls12) {
            return failed(AlertDescription::illegal_parameter, 4, "ClientHello.legacy_version is not 0x0303");
        }
        if (compression.size() != 1 || uint8_t(compression[0]) != 0) {
            return failed(AlertDescription::illegal_parameter, at, "compression methods other than null alone");
        }
        auto x = read_extensions(r, 8, 0xFFFF, true);
        if (!x) {
            return unexpected<Alert>(x.error());
        }
        m.extensions = *x;
        if (!r.end()) {
            return failed(r);
        }
        return m;
    }

    // A ClientHello written: legacy_version 0x0303, the null compression
    // alone; `extensions(w)` writes the entries (Builder::extension)
    template<class R, class F>
    void write_client_hello(Builder& w, const Bytes& random, const Bytes& session_id, const R& cipher_suites, F&& extensions) noexcept(std::is_nothrow_invocable_v<F&, Builder&>) {
        assert(random.size() == 32 && session_id.size() <= 32);
        auto m = w.message(HandshakeType::client_hello);
        w.u16(Tls12);
        w.bytes(random);
        {
            auto s = w.block8(32);
            w.bytes(session_id);
        }
        write_u16_list(w, 2, cipher_suites);
        w.u8(1);
        w.u8(0);
        write_extensions(w, std::forward<F>(extensions));
    }

    // ServerHello and HelloRetryRequest (§4.1.3, §4.1.4)
    struct ServerHello {
        uint16_t legacy_version = Tls12;
        Bytes random;
        Bytes session_id;
        uint16_t cipher_suite = 0;
        Extensions extensions;

        SGCL_INLINE_HOT bool is_retry() const noexcept {
            return random.size() == 32 && std::memcmp(random.data(), HelloRetryRandom, 32) == 0;
        }
    };

    inline expected<ServerHello, Alert> read_server_hello(const Bytes& body) noexcept {
        Reader r(body, 4);
        ServerHello m;
        uint8_t compression;
        if (!r.u16(m.legacy_version) || !r.bytes(32, m.random) || !r.vec8(m.session_id, 0, 32) || !r.u16(m.cipher_suite)) {
            return failed(r);
        }
        uint32_t at = r.offset();
        if (!r.u8(compression)) {
            return failed(r);
        }
        if (m.legacy_version < Tls12) {
            return failed(AlertDescription::protocol_version, 4, "a ServerHello of a version before TLS 1.2");
        }
        if (m.legacy_version != Tls12) {
            return failed(AlertDescription::illegal_parameter, 4, "ServerHello.legacy_version is not 0x0303");
        }
        if (compression != 0) {
            return failed(AlertDescription::illegal_parameter, at, "a compression method other than null");
        }
        if (r.empty()) {
            return m;   // TLS 1.2: no extensions at all (RFC 5246 §7.4.1.3); 1.3's lack supported_versions then
        }
        auto x = read_extensions(r, 0, 0xFFFF);
        if (!x) {
            return unexpected<Alert>(x.error());
        }
        m.extensions = *x;
        if (!r.end()) {
            return failed(r);
        }
        return m;
    }

    // A ServerHello written; a HelloRetryRequest is one with
    // HelloRetryRandom for its random
    template<class F>
    void write_server_hello(Builder& w, const Bytes& random, const Bytes& session_id, uint16_t cipher_suite, F&& extensions) noexcept(std::is_nothrow_invocable_v<F&, Builder&>) {
        assert(random.size() == 32 && session_id.size() <= 32);
        auto m = w.message(HandshakeType::server_hello);
        w.u16(Tls12);
        w.bytes(random);
        {
            auto s = w.block8(32);
            w.bytes(session_id);
        }
        w.u16(cipher_suite);
        w.u8(0);
        write_extensions(w, std::forward<F>(extensions));
    }

    // EncryptedExtensions (§4.3.1)
    SGCL_INLINE_HOT expected<Extensions, Alert> read_encrypted_extensions(const Bytes& body) noexcept {
        Reader r(body, 4);
        auto x = read_extensions(r, 0, 0xFFFF);
        if (x && !r.end()) {
            return failed(r);
        }
        return x;
    }

    template<class F>
    SGCL_INLINE_HOT void write_encrypted_extensions(Builder& w, F&& extensions) noexcept(std::is_nothrow_invocable_v<F&, Builder&>) {
        auto m = w.message(HandshakeType::encrypted_extensions);
        write_extensions(w, std::forward<F>(extensions));
    }

    // CertificateRequest (§4.3.2): signature_algorithms is required
    struct CertificateRequest {
        Bytes context;
        Extensions extensions;
    };

    inline expected<CertificateRequest, Alert> read_certificate_request(const Bytes& body) noexcept {
        Reader r(body, 4);
        CertificateRequest m;
        if (!r.vec8(m.context, 0, 0xFF)) {
            return failed(r);
        }
        auto x = read_extensions(r, 2, 0xFFFF);
        if (!x) {
            return unexpected<Alert>(x.error());
        }
        m.extensions = *x;
        if (!r.end()) {
            return failed(r);
        }
        if (!m.extensions.has(ExtensionType::signature_algorithms)) {
            return failed(AlertDescription::missing_extension, 0, "CertificateRequest without signature_algorithms");
        }
        return m;
    }

    template<class F>
    SGCL_INLINE_HOT void write_certificate_request(Builder& w, const Bytes& context, F&& extensions) noexcept(std::is_nothrow_invocable_v<F&, Builder&>) {
        auto m = w.message(HandshakeType::certificate_request);
        {
            auto c = w.block8();
            w.bytes(context);
        }
        write_extensions(w, std::forward<F>(extensions));
    }

    // Certificate (§4.4.2): the entries, each a certificate's DER and its
    // extensions, walked in their order (the end-entity first)
    struct CertificateEntry {
        Bytes der;
        Extensions extensions;
    };

    struct Certificate {
        Bytes context;
        Bytes raw;          // the entries, without the list's length
        size_t count = 0;

        struct iterator {
            using iterator_category = std::input_iterator_tag;
            using value_type = CertificateEntry;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = CertificateEntry;
            const byte* p;
            SGCL_INLINE_HOT CertificateEntry operator*() const noexcept {
                size_t n = size_t(uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | uint32_t(p[2]));
                Reader r(Bytes(p + 3 + n, 2 + (size_t(uint16_t(p[3 + n]) << 8 | uint16_t(p[4 + n])))));
                auto x = read_extensions(r, 0, 0xFFFF);   // read once already: cannot fail
                return CertificateEntry{Bytes(p + 3, n), x ? *x : Extensions()};
            }
            SGCL_INLINE_HOT iterator& operator++() noexcept {
                size_t n = size_t(uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | uint32_t(p[2]));
                size_t e = size_t(uint16_t(p[3 + n]) << 8 | uint16_t(p[4 + n]));
                p += 3 + n + 2 + e;
                return *this;
            }
            SGCL_INLINE_HOT iterator operator++(int) noexcept {
                iterator t = *this;
                ++*this;
                return t;
            }
            SGCL_INLINE_HOT bool operator==(const iterator& o) const noexcept {
                return p == o.p;
            }
        };

        SGCL_INLINE_HOT iterator begin() const noexcept {
            return {raw.data()};
        }

        SGCL_INLINE_HOT iterator end() const noexcept {
            return {raw.data() + raw.size()};
        }
    };

    inline expected<Certificate, Alert> read_certificate(const Bytes& body) noexcept {
        Reader r(body, 4);
        Certificate m;
        if (!r.vec8(m.context, 0, 0xFF)) {
            return failed(r);
        }
        Reader list(Bytes(), 0);
        if (!r.sub24(list, 0, 0xFFFFFF) || !r.end()) {
            return failed(r);
        }
        size_t n = list.remaining();
        const byte* first = nullptr;
        while (!list.empty()) {
            Bytes der;
            if (!list.vec24(der, 1, 0xFFFFFF)) {
                return failed(list);
            }
            if (!first) {
                first = der.data() - 3;
            }
            auto x = read_extensions(list, 0, 0xFFFF);
            if (!x) {
                return unexpected<Alert>(x.error());
            }
            ++m.count;
        }
        m.raw = first ? Bytes(first, n) : Bytes();
        return m;
    }

    // CertificateStatus (RFC 6066 §8): status_type ocsp (1) and the
    // OCSPResponse's DER, the body of TLS 1.2's message and of TLS 1.3's
    // status_request in a CertificateEntry (RFC 8446 §4.4.2.1)
    SGCL_INLINE_HOT void write_certificate_status(Builder& w, const Bytes& ocsp) noexcept {
        w.u8(1);
        auto r = w.block24();
        w.bytes(ocsp);
    }

    // The OCSPResponse of a CertificateStatus; a status_type other than
    // ocsp, or an empty response, decode_error
    inline expected<Bytes, Alert> read_certificate_status(const Bytes& body) noexcept {
        Reader r(body);
        uint8_t type;
        Bytes response;
        if (!r.u8(type) || !r.vec24(response, 1, 0xFFFFFF) || !r.end()) {
            return failed(r);
        }
        if (type != 1) {
            return failed(AlertDescription::decode_error, 0, "a CertificateStatus of a status_type other than ocsp");
        }
        return response;
    }

    // A ClientHello's status_request (RFC 6066 §8): CertificateStatusRequest
    // of status_type ocsp, no responder ids, no extensions
    SGCL_INLINE_HOT void write_status_request(Builder& w) noexcept {
        w.u8(1);
        w.u16(0);
        w.u16(0);
    }

    // Whether a ClientHello's status_request asks for an OCSP response
    // (status_type ocsp; its lists read for their syntax); another
    // status_type is passed over (false), a broken one decode_error
    inline expected<bool, Alert> read_status_request(const Bytes& body) noexcept {
        Reader r(body);
        uint8_t type;
        if (!r.u8(type)) {
            return failed(r);
        }
        if (type != 1) {
            return false;
        }
        Bytes ids, extensions;
        if (!r.vec16(ids, 0, 0xFFFF) || !r.vec16(extensions, 0, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        return true;
    }

    // The entries: a range of the DER of each certificate (no extensions),
    // or of CertificateEntry (their extensions copied as they are). A
    // leaf_ocsp given goes into the first entry's status_request (TLS 1.3's
    // stapling, RFC 8446 §4.4.2.1)
    template<class R>
    void write_certificate(Builder& w, const Bytes& context, const R& entries, const Bytes& leaf_ocsp = Bytes()) noexcept {
        auto m = w.message(HandshakeType::certificate);
        {
            auto c = w.block8();
            w.bytes(context);
        }
        auto list = w.block24();
        bool first = true;
        for (const auto& e : entries) {
            if constexpr (std::is_same_v<std::decay_t<decltype(e)>, CertificateEntry>) {
                {
                    auto d = w.block24();
                    w.bytes(e.der);
                }
                auto x = w.block16();
                write_raw(w, e.extensions);
            } else {
                {
                    auto d = w.block24();
                    w.bytes(e);
                }
                if (first && !leaf_ocsp.empty()) {
                    auto x = w.block16();
                    auto ext = w.extension(ExtensionType::status_request);
                    write_certificate_status(w, leaf_ocsp);
                } else {
                    w.u16(0);
                }
            }
            first = false;
        }
    }

    // CertificateVerify (§4.4.3)
    struct CertificateVerify {
        uint16_t scheme;
        Bytes signature;
    };

    SGCL_INLINE_HOT expected<CertificateVerify, Alert> read_certificate_verify(const Bytes& body) noexcept {
        Reader r(body, 4);
        CertificateVerify m;
        if (!r.u16(m.scheme) || !r.vec16(m.signature, 0, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        return m;
    }

    SGCL_INLINE_HOT void write_certificate_verify(Builder& w, uint16_t scheme, const Bytes& signature) noexcept {
        auto m = w.message(HandshakeType::certificate_verify);
        w.u16(scheme);
        auto s = w.block16();
        w.bytes(signature);
    }

    // Finished (§4.4.4): verify_data of the length of the hash
    SGCL_INLINE_HOT expected<Bytes, Alert> read_finished(const Bytes& body, size_t hash_size) noexcept {
        if (body.size() != hash_size) {
            return failed(AlertDescription::decode_error, 4, "Finished of another length than the hash's");
        }
        return body;
    }

    SGCL_INLINE_HOT void write_finished(Builder& w, const Bytes& verify_data) noexcept {
        auto m = w.message(HandshakeType::finished);
        w.bytes(verify_data);
    }

    // KeyUpdate (§4.6.3): whether the other side is asked to update too
    SGCL_INLINE_HOT expected<bool, Alert> read_key_update(const Bytes& body) noexcept {
        Reader r(body, 4);
        uint8_t request;
        if (!r.u8(request) || !r.end()) {
            return failed(r);
        }
        if (request > 1) {
            return failed(AlertDescription::illegal_parameter, 4, "KeyUpdate asking neither 0 nor 1");
        }
        return request == 1;
    }

    SGCL_INLINE_HOT void write_key_update(Builder& w, bool request_update) noexcept {
        auto m = w.message(HandshakeType::key_update);
        w.u8(request_update ? 1 : 0);
    }

    // NewSessionTicket (§4.6.1): a lifetime past seven days is
    // illegal_parameter
    struct NewSessionTicket {
        uint32_t lifetime = 0;
        uint32_t age_add = 0;
        Bytes nonce;
        Bytes ticket;
        Extensions extensions;
    };

    inline expected<NewSessionTicket, Alert> read_new_session_ticket(const Bytes& body) noexcept {
        Reader r(body, 4);
        NewSessionTicket m;
        if (!r.u32(m.lifetime) || !r.u32(m.age_add) || !r.vec8(m.nonce, 0, 0xFF) || !r.vec16(m.ticket, 1, 0xFFFF)) {
            return failed(r);
        }
        auto x = read_extensions(r, 0, 0xFFFE);
        if (!x) {
            return unexpected<Alert>(x.error());
        }
        m.extensions = *x;
        if (!r.end()) {
            return failed(r);
        }
        if (m.lifetime > 604800) {
            return failed(AlertDescription::illegal_parameter, 4, "a ticket's lifetime past seven days");
        }
        return m;
    }

    inline void write_new_session_ticket(Builder& w, const NewSessionTicket& t) noexcept {
        auto m = w.message(HandshakeType::new_session_ticket);
        w.u32(t.lifetime);
        w.u32(t.age_add);
        {
            auto n = w.block8();
            w.bytes(t.nonce);
        }
        {
            auto k = w.block16();
            w.bytes(t.ticket);
        }
        auto x = w.block16(0xFFFE);
        write_raw(w, t.extensions);
    }

    // EndOfEarlyData (§4.5): empty
    SGCL_INLINE_HOT expected<void, Alert> read_end_of_early_data(const Bytes& body) noexcept {
        if (!body.empty()) {
            return failed(AlertDescription::decode_error, 4, "EndOfEarlyData with a body");
        }
        return {};
    }

    SGCL_INLINE_HOT void write_end_of_early_data(Builder& w) noexcept {
        auto m = w.message(HandshakeType::end_of_early_data);
    }

    // message_hash (§4.4.1): what the transcript holds in place of the
    // first ClientHello after a HelloRetryRequest
    SGCL_INLINE_HOT void write_message_hash(Builder& w, const Bytes& hash) noexcept {
        auto m = w.message(HandshakeType::message_hash);
        w.bytes(hash);
    }

    // An alert record's two bytes (§6): the level is not read, the kind
    // says what the alert is (every one fatal but close_notify and
    // user_canceled), as Go reads it
    SGCL_INLINE_HOT expected<Alert, Alert> read_alert(const Bytes& fragment) noexcept {
        Reader r(fragment);
        uint8_t level, description;
        if (!r.u8(level) || !r.u8(description) || !r.end("an alert of more than two bytes")) {
            return failed(r);
        }
        return Alert{AlertDescription(description), 0, nullptr};
    }

    SGCL_INLINE_HOT void write_alert(Builder& w, AlertDescription d) noexcept {
        Alert a{d};
        w.u8(a.fatal() ? 2 : 1);
        w.u8(uint8_t(d));
    }

    // --- the messages of the client's TLS 1.2 (RFC 5246 §7.4, RFC 8422 §5) -----

    // Certificate: the certificates' DER, the end-entity first, no context
    // and no extensions per entry (§7.4.2)
    struct Certificate12 {
        Bytes raw;          // the entries, each ASN.1Cert<1..2^24-1>
        size_t count = 0;

        template<class F>
        void each(F&& f) const noexcept(std::is_nothrow_invocable_v<F&, const Bytes&>) {
            Reader r(raw);
            Bytes der;
            while (r.vec24(der, 1, 0xFFFFFF)) {
                f(der);
            }
        }
    };

    inline expected<Certificate12, Alert> read_certificate12(const Bytes& body) noexcept {
        Reader r(body, 4);
        Certificate12 m;
        if (!r.vec24(m.raw, 0, 0xFFFFFF) || !r.end()) {
            return failed(r);
        }
        Reader list(m.raw, 7);
        while (!list.empty()) {
            Bytes der;
            if (!list.vec24(der, 1, 0xFFFFFF)) {
                return failed(list);
            }
            ++m.count;
        }
        return m;
    }

    template<class R>
    void write_certificate12(Builder& w, const R& ders) noexcept {
        auto m = w.message(HandshakeType::certificate);
        auto list = w.block24();
        for (const Bytes& d : ders) {
            auto one = w.block24();
            w.bytes(d);
        }
    }

    // ServerKeyExchange of ECDHE (RFC 8422 §5.4): ECParameters of a named
    // curve, the server's point, the signature over the randoms and them
    struct ServerKeyExchange12 {
        Bytes params;       // curve_type, the curve and the point: what is signed after the randoms
        uint16_t group = 0;
        Bytes point;
        uint16_t scheme = 0;
        Bytes signature;
    };

    inline expected<ServerKeyExchange12, Alert> read_server_key_exchange12(const Bytes& body) noexcept {
        Reader r(body, 4);
        ServerKeyExchange12 m;
        uint8_t curve_type;
        if (!r.u8(curve_type)) {
            return failed(r);
        }
        if (curve_type != 3) {
            return failed(AlertDescription::illegal_parameter, 4, "ServerKeyExchange of a curve that is not named");
        }
        if (!r.u16(m.group) || !r.vec8(m.point, 1, 0xFF)) {
            return failed(r);
        }
        m.params = Bytes(body.data(), size_t(m.point.data() + m.point.size() - body.data()));
        if (!r.u16(m.scheme) || !r.vec16(m.signature, 0, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        return m;
    }

    // CertificateRequest (§7.4.4): the certificate types, the schemes, the
    // authorities (DistinguishedName<1..2^16-1> each, the list possibly empty)
    struct CertificateRequest12 {
        Bytes types;
        U16List schemes;
        NameDers authorities;
    };

    inline expected<CertificateRequest12, Alert> read_certificate_request12(const Bytes& body) noexcept {
        Reader r(body, 4);
        CertificateRequest12 m;
        if (!r.vec8(m.types, 1, 0xFF)) {
            return failed(r);
        }
        auto schemes = read_u16_list(r, 2, 2, 0xFFFE);
        if (!schemes) {
            return unexpected<Alert>(schemes.error());
        }
        m.schemes = *schemes;
        if (!r.vec16(m.authorities.raw, 0, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        Reader list(m.authorities.raw);
        while (!list.empty()) {
            Bytes n;
            if (!list.vec16(n, 1, 0xFFFF)) {
                return failed(list);
            }
            ++m.authorities.count;
        }
        return m;
    }

    // ServerHelloDone (§7.4.5): empty
    SGCL_INLINE_HOT expected<void, Alert> read_server_hello_done(const Bytes& body) noexcept {
        if (!body.empty()) {
            return failed(AlertDescription::decode_error, 4, "ServerHelloDone with a body");
        }
        return {};
    }

    // ClientKeyExchange of ECDHE (RFC 8422 §5.7): the client's point
    SGCL_INLINE_HOT void write_client_key_exchange12(Builder& w, const Bytes& point) noexcept {
        auto m = w.message(HandshakeType::client_key_exchange);
        auto p = w.block8();
        w.bytes(point);
    }

    // NewSessionTicket of TLS 1.2 (RFC 5077 §3.3): the lifetime hint in
    // seconds (0: not given), the ticket (empty: none issued)
    struct NewSessionTicket12 {
        uint32_t lifetime_hint = 0;
        Bytes ticket;
    };

    inline expected<NewSessionTicket12, Alert> read_new_session_ticket12(const Bytes& body) noexcept {
        Reader r(body, 4);
        NewSessionTicket12 m;
        if (!r.u32(m.lifetime_hint) || !r.vec16(m.ticket, 0, 0xFFFF) || !r.end()) {
            return failed(r);
        }
        return m;
    }

    SGCL_INLINE_HOT void write_new_session_ticket12(Builder& w, uint32_t lifetime_hint, const Bytes& ticket) noexcept {
        auto m = w.message(HandshakeType::new_session_ticket);
        w.u32(lifetime_hint);
        auto t = w.block16();
        w.bytes(ticket);
    }

    // ec_point_formats (RFC 8422 §5.1.2): the formats, uncompressed (0)
    // among them
    SGCL_INLINE_HOT expected<Bytes, Alert> read_point_formats(const Bytes& body) noexcept {
        Reader r(body);
        Bytes f;
        if (!r.vec8(f, 1, 0xFF) || !r.end()) {
            return failed(r);
        }
        return f;
    }
}
