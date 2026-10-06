//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cipher.h"
#include "keys.h"
#include "modp.h"
#include "wire.h"
#include "../../../crypto/mlkem.h"
#include "../../../crypto/p256.h"
#include "../../../crypto/p384.h"
#include "../../../crypto/p521.h"
#include "../../../crypto/random.h"
#include "../../../crypto/sha256.h"
#include "../../../crypto/sha512.h"
#include "../../../crypto/x25519.h"
#include "../../../crypto/detail/rsa_math.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The key exchange of SSH without I/O: the KEXINIT message (RFC 4253
// §7.1) made and read, the algorithms negotiated (the first of the
// client's lists the server has, per list; the pseudo-algorithms
// ext-info-c/-s of RFC 8308 and OpenSSH's strict-KEX markers set apart),
// the methods both ways, the exchange hash and the keys derived from it
// (§7.2).
//
//   mlkem768x25519-sha256          draft-ietf-sshm-mlkem-hybrid-kex: the
//                                  client's ML-KEM-768 encapsulation key and
//                                  X25519 share, the server's ciphertext and
//                                  share; K = SHA-256(ML-KEM key || X25519
//                                  secret), hashed and derived as a string
//   curve25519-sha256 (and its     RFC 8731: K the X25519 secret read as a
//   @libssh.org name)              big-endian number, an mpint
//   ecdh-sha2-nistp256, -nistp384, RFC 5656: K the x of the shared point
//   -nistp521
//   diffie-hellman-group14-sha256, RFC 8268 over RFC 3526's groups: e = 2^x
//   diffie-hellman-group16-sha512  mod p with a secret x of 1024 bits, K =
//                                  f^x mod p, both by crypto's
//                                  constant-time Montgomery exponentiation
//                                  (RSA's, detail/rsa_math.h: a window of
//                                  four bits read from a table through
//                                  masks, a count of operations fixed by the
//                                  exponent's width), never by a
//                                  variable-time power; f checked within
//                                  1 < f < p - 1
//
// Every secret (the ephemeral keys, K, the keys derived) lives in
// unmanaged memory, zeroed when done with.
namespace sgcl::net::ssh::detail {
    enum class KexKind : uint8_t {
        mlkem768_x25519,
        x25519,
        p256,
        p384,
        dh14,
        dh16,
        p521,
    };

    enum class HashKind : uint8_t {
        sha256,
        sha384,
        sha512,
    };

    struct KexInfo {
        std::string_view name;
        KexKind kind;
        HashKind hash;
    };

    // In the module's order of preference
    inline constexpr KexInfo kex_table[] = {
        {"mlkem768x25519-sha256", KexKind::mlkem768_x25519, HashKind::sha256},
        {"curve25519-sha256", KexKind::x25519, HashKind::sha256},
        {"curve25519-sha256@libssh.org", KexKind::x25519, HashKind::sha256},
        {"ecdh-sha2-nistp256", KexKind::p256, HashKind::sha256},
        {"ecdh-sha2-nistp384", KexKind::p384, HashKind::sha384},
        {"ecdh-sha2-nistp521", KexKind::p521, HashKind::sha512},
        {"diffie-hellman-group16-sha512", KexKind::dh16, HashKind::sha512},
        {"diffie-hellman-group14-sha256", KexKind::dh14, HashKind::sha256},
    };

    inline const KexInfo* find_kex(std::string_view name) noexcept {
        for (const auto& k : kex_table) {
            if (k.name == name) {
                return &k;
            }
        }
        return nullptr;
    }

    inline constexpr std::string_view ExtInfoClient = "ext-info-c";
    inline constexpr std::string_view ExtInfoServer = "ext-info-s";
    inline constexpr std::string_view StrictClient = "kex-strict-c-v00@openssh.com";
    inline constexpr std::string_view StrictServer = "kex-strict-s-v00@openssh.com";

    SGCL_INLINE_HOT size_t hash_size(HashKind h) noexcept {
        return h == HashKind::sha256 ? 32 : h == HashKind::sha384 ? 48 : 64;
    }

    // The hash of bytes into out (hash_size(h) bytes)
    inline void hash_of(HashKind h, const uint8_t* p, size_t n, uint8_t* out) noexcept {
        const auto in = bytes_of(p, n);
        if (h == HashKind::sha256) {
            auto d = crypto::sha256::of(in);
            sgcl::detail::copy_bytes(out, d.data(), d.size());
        } else if (h == HashKind::sha384) {
            auto d = crypto::sha384::of(in);
            sgcl::detail::copy_bytes(out, d.data(), d.size());
        } else {
            auto d = crypto::sha512::of(in);
            sgcl::detail::copy_bytes(out, d.data(), d.size());
        }
    }

    // --- KEXINIT --------------------------------------------------------------

    // The algorithms one side offers, in its order of preference
    struct Preferences {
        std::vector<std::string> kex;
        std::vector<std::string> host_key;
        std::vector<std::string> ciphers;
        std::vector<std::string> macs;
        std::vector<std::string> compression;
    };

    inline Preferences default_preferences() {
        Preferences p;
        for (const auto& k : kex_table) {
            p.kex.emplace_back(k.name);
        }
        for (const auto& a : alg_table) {
            p.host_key.emplace_back(a.name);
        }
        for (const auto& c : cipher_table) {
            p.ciphers.emplace_back(c.name);
        }
        for (const auto& m : mac_table) {
            p.macs.emplace_back(m.name);
        }
        p.compression = {"none"};
        return p;
    }

    // A KEXINIT read: its ten lists, whether a guessed packet follows
    struct Kexinit {
        std::vector<std::string> lists[10];
        bool follows = false;
    };

    enum : int {
        ListKex = 0,
        ListHostKey = 1,
        ListCipherC2S = 2,
        ListCipherS2C = 3,
        ListMacC2S = 4,
        ListMacS2C = 5,
        ListCompC2S = 6,
        ListCompS2C = 7,
        ListLangC2S = 8,
        ListLangS2C = 9,
    };

    // Our KEXINIT's payload: a random cookie, the lists (the kex list with
    // ext-info-c or ext-info-s and the strict-KEX marker of our side in
    // the first exchange: OpenSSH sends them in every KEXINIT and they
    // mean something only in the first), no guess
    inline Bytes make_kexinit(const Preferences& p, bool client, bool first) {
        Bytes out;
        Writer w(out);
        w.u8(MsgKexinit);
        uint8_t cookie[16];
        crypto::random::fill(mutable_bytes_of(cookie, 16));
        w.raw(cookie, 16);
        std::vector<std::string> kex = p.kex;
        if (first) {
            kex.emplace_back(client ? ExtInfoClient : ExtInfoServer);
            kex.emplace_back(client ? StrictClient : StrictServer);
        }
        w.name_list(kex);
        w.name_list(p.host_key);
        w.name_list(p.ciphers);
        w.name_list(p.ciphers);
        w.name_list(p.macs);
        w.name_list(p.macs);
        w.name_list(p.compression);
        w.name_list(p.compression);
        w.u32(0);   // the languages, empty
        w.u32(0);
        w.boolean(false);
        w.u32(0);
        return out;
    }

    // A KEXINIT's payload read (its first byte the message number)
    inline bool read_kexinit(const uint8_t* p, size_t n, Kexinit& out) {
        Reader r(p, n);
        if (r.u8() != MsgKexinit) {
            return false;
        }
        (void)r.raw(16);
        for (auto& list : out.lists) {
            for (auto name : r.name_list()) {
                list.emplace_back(name);
            }
        }
        out.follows = r.boolean();
        (void)r.u32();
        return r.ok();   // RFC 4253 lets the message grow: bytes after it ignored
    }

    // What the two KEXINITs settled; index 0 is client to server, 1
    // server to client
    struct Negotiated {
        const KexInfo* kex = nullptr;
        const AlgInfo* host_key = nullptr;
        const CipherInfo* cipher[2] = {};
        const MacInfo* mac[2] = {};
        bool zlib[2] = {};
        bool strict = false;           // both offered strict KEX (only read in the first exchange)
        bool ext_info = false;         // the peer offered ext-info (the server: the client's ext-info-c)
        bool guess_wrong = false;      // the peer's guessed packet is to be dropped
    };

    // The algorithms of the two lists; a message naming the list with
    // nothing in common, or nullptr. `host_keys` are the server's key
    // algorithms that it can sign with (the server's list); `is_client`
    // says which side reads
    inline const char* negotiate(const Kexinit& c, const Kexinit& s, bool is_client, Negotiated& out) noexcept {
        std::vector<std::string_view> ck, sk;
        for (const auto& k : c.lists[ListKex]) {
            if (k != ExtInfoClient && k != ExtInfoServer && k != StrictClient && k != StrictServer) {
                ck.push_back(k);
            }
        }
        for (const auto& k : s.lists[ListKex]) {
            if (k != ExtInfoClient && k != ExtInfoServer && k != StrictClient && k != StrictServer) {
                sk.push_back(k);
            }
        }
        out.strict = contains_name(c.lists[ListKex], StrictClient) && contains_name(s.lists[ListKex], StrictServer);
        out.ext_info = is_client ? contains_name(s.lists[ListKex], ExtInfoServer) : contains_name(c.lists[ListKex], ExtInfoClient);
        auto kex = first_common(ck, sk);
        out.kex = find_kex(kex);
        if (!out.kex) {
            return "no common key exchange algorithm";
        }
        auto hk = first_common(c.lists[ListHostKey], s.lists[ListHostKey]);
        out.host_key = find_alg(hk);
        if (!out.host_key) {
            return "no common host key algorithm";
        }
        for (int d = 0; d < 2; ++d) {
            auto cn = first_common(c.lists[ListCipherC2S + d], s.lists[ListCipherC2S + d]);
            out.cipher[d] = find_cipher(cn);
            if (!out.cipher[d]) {
                return "no common cipher";
            }
            out.mac[d] = nullptr;
            if (!out.cipher[d]->aead) {
                auto mn = first_common(c.lists[ListMacC2S + d], s.lists[ListMacC2S + d]);
                out.mac[d] = find_mac(mn);
                if (!out.mac[d]) {
                    return "no common MAC";
                }
            }
            auto zn = first_common(c.lists[ListCompC2S + d], s.lists[ListCompC2S + d]);
            if (zn == "zlib@openssh.com") {
                out.zlib[d] = true;
            } else if (zn == "none") {
                out.zlib[d] = false;
            } else {
                return "no common compression";
            }
        }
        // RFC 4253 §7: a guess is wrong when the first algorithms of the
        // two kex or host key lists differ
        const Kexinit& peer = is_client ? s : c;
        if (peer.follows) {
            bool kex_first = !ck.empty() && !sk.empty() && ck[0] == sk[0];
            bool hk_first = !c.lists[ListHostKey].empty() && !s.lists[ListHostKey].empty() && c.lists[ListHostKey][0] == s.lists[ListHostKey][0];
            out.guess_wrong = !(kex_first && hk_first);
        }
        return nullptr;
    }

    // --- the exchange -----------------------------------------------------------

    // What the exchange hash covers before the method's own fields
    struct HashInputs {
        std::string_view client_version;   // without CR LF
        std::string_view server_version;
        const Bytes* client_kexinit = nullptr;
        const Bytes* server_kexinit = nullptr;
    };

    // A finite field Diffie-Hellman group as crypto's Montgomery arithmetic
    // takes it
    class DhGroup {
    public:
        explicit DhGroup(KexKind kind)
        : _p(kind == KexKind::dh14 ? modp2048 : modp4096)
        , _bytes(kind == KexKind::dh14 ? 256 : 512)
        , _k(_bytes / 8)
        , _words(4 * _k + crypto::detail::bn::pow_scratch(_k) + 8) {
            using namespace crypto::detail::bn;
            word* m = _words.data();
            from_be(m, _k, _p, _bytes);
            _m0inv = mont_m0inv(m[0]);
            mont_constants(m + _k, m + 2 * _k, m, _k, _m0inv, _scratch());
        }

        SGCL_INLINE_HOT size_t bytes() const noexcept {
            return _bytes;
        }

        // r = base^x mod p, r in `bytes()` big-endian bytes; base below p,
        // x of xbytes big-endian bytes (a secret)
        void power(const uint8_t* base, size_t base_size, const uint8_t* x, size_t xbytes, uint8_t* r) {
            using namespace crypto::detail::bn;
            const Modulus mod = _mod();
            const size_t kx = words_for_bytes(xbytes);
            SecretWords<OperatorDelete> w(3 * _k + kx);
            word* a = w.data();
            word* am = a + _k;
            word* res = am + _k;
            word* e = res + _k;
            from_be(a, _k, base, base_size);
            from_be(e, kx, x, xbytes);
            word* t = _scratch();
            to_mont(am, a, mod, t);
            mont_pow(res, am, e, 8 * xbytes, mod, t);
            from_mont(a, res, mod, t);
            to_be(r, _bytes, a, _k);
        }

        // 1 < v < p - 1, v of n big-endian bytes (an mpint's magnitude)
        bool in_range(const uint8_t* v, size_t n) const noexcept {
            while (n && *v == 0) {
                ++v;
                --n;
            }
            if (n == 0 || (n == 1 && v[0] == 1)) {
                return false;
            }
            if (n < _bytes) {
                return true;
            }
            if (n > _bytes) {
                return false;
            }
            // v < p - 1: p - 1 differs from p in its last byte alone (p is odd)
            for (size_t i = 0; i < n; ++i) {
                uint8_t q = i + 1 == n ? uint8_t(_p[i] - 1) : _p[i];
                if (v[i] != q) {
                    return v[i] < q;
                }
            }
            return false;
        }

    private:
        crypto::detail::bn::Modulus _mod() noexcept {
            using namespace crypto::detail::bn;
            word* m = _words.data();
            return Modulus{m, m + _k, m + 2 * _k, _m0inv, _k};
        }

        crypto::detail::bn::word* _scratch() noexcept {
            return _words.data() + 3 * _k;
        }

        const uint8_t* _p;
        size_t _bytes;
        size_t _k;
        crypto::detail::bn::SecretWords<crypto::detail::bn::OperatorDelete> _words;
        crypto::detail::bn::word _m0inv = 0;
    };

    // One key exchange of a method, either side: the ephemeral secret, K
    // as the hash takes it, H
    class KexExchange {
    public:
        explicit KexExchange(const KexInfo& info) noexcept
        : _info(info) {
        }

        SGCL_INLINE_HOT const KexInfo& info() const noexcept {
            return _info;
        }

        // K encoded (an mpint, or a string for the hybrid) and H
        SGCL_INLINE_HOT const Bytes& k() const noexcept {
            return _k.b;
        }

        SGCL_INLINE_HOT const Bytes& h() const noexcept {
            return _h;
        }

        // --- the client --------------------------------------------------------

        // The client's first message: KEX_INIT with its share
        Bytes client_init() {
            Bytes out;
            Writer w(out);
            w.u8(MsgKexInit);
            switch (_info.kind) {
                case KexKind::mlkem768_x25519: {
                    _mlkem.emplace(crypto::mlkem768::decapsulation_key::generate());
                    _x25519.emplace(crypto::x25519::private_key::generate());
                    auto ek = _mlkem->encapsulation_key().bytes();
                    auto x = _x25519->public_key().bytes();
                    size_t at = w.begin_string();
                    w.raw(ek.data(), ek.size());
                    w.raw(x.data(), x.size());
                    w.end_string(at);
                    break;
                }
                case KexKind::x25519: {
                    _x25519.emplace(crypto::x25519::private_key::generate());
                    auto x = _x25519->public_key().bytes();
                    w.string(x.data(), x.size());
                    break;
                }
                case KexKind::p256: {
                    _p256.emplace(crypto::p256::ecdh_key::generate());
                    auto q = _p256->public_key().bytes();
                    w.string(q.data(), q.size());
                    break;
                }
                case KexKind::p384: {
                    _p384.emplace(crypto::p384::ecdh_key::generate());
                    auto q = _p384->public_key().bytes();
                    w.string(q.data(), q.size());
                    break;
                }
                case KexKind::p521: {
                    _p521.emplace(crypto::p521::ecdh_key::generate());
                    auto q = _p521->public_key().bytes();
                    w.string(q.data(), q.size());
                    break;
                }
                case KexKind::dh14:
                case KexKind::dh16: {
                    _dh_public(w);
                    break;
                }
            }
            _ours.assign(out.begin() + 1, out.end());   // Q_C or e as written (a string or an mpint)
            return out;
        }

        // The server's reply: K and H made; the host key blob and the
        // signature over H are views into the payload, for the caller to
        // check. A message for a reply that cannot be taken, else nullptr
        const char* client_reply(const uint8_t* p, size_t n, const HashInputs& in, Span& host_key, Span& signature) {
            Reader r(p, n);
            if (r.u8() != MsgKexReply) {
                return "a key exchange reply was expected";
            }
            host_key = r.string();
            Span theirs;
            if (_info.kind == KexKind::dh14 || _info.kind == KexKind::dh16) {
                theirs = r.mpint();
            } else {
                theirs = r.string();
            }
            signature = r.string();
            if (!r.done() || host_key.n == 0) {
                return "a malformed key exchange reply";
            }
            if (const char* e = _shared_client(theirs)) {
                return e;
            }
            Bytes q;
            Writer qw(q);
            if (_info.kind == KexKind::dh14 || _info.kind == KexKind::dh16) {
                qw.mpint(theirs.p, theirs.n);
            } else {
                qw.string(theirs);
            }
            _hash(in, host_key, _ours, q);
            return nullptr;
        }

        // --- the server --------------------------------------------------------

        // The client's KEX_INIT taken: K and H made with the host key's
        // blob; the reply's payload without its signature, which the caller
        // appends (sign(H)). A message for an init that cannot be taken
        const char* server_init(const uint8_t* p, size_t n, const HashInputs& in, const Bytes& host_key, Bytes& reply_head) {
            Reader r(p, n);
            if (r.u8() != MsgKexInit) {
                return "a key exchange init was expected";
            }
            Span theirs = (_info.kind == KexKind::dh14 || _info.kind == KexKind::dh16) ? r.mpint() : r.string();
            if (!r.done()) {
                return "a malformed key exchange init";
            }
            Bytes ours;
            if (const char* e = _shared_server(theirs, ours)) {
                return e;
            }
            Bytes q_c;
            Writer qw(q_c);
            if (_info.kind == KexKind::dh14 || _info.kind == KexKind::dh16) {
                qw.mpint(theirs.p, theirs.n);
            } else {
                qw.string(theirs);
            }
            _hash(in, Span{host_key.data(), host_key.size()}, q_c, ours);
            reply_head.clear();
            Writer w(reply_head);
            w.u8(MsgKexReply);
            w.string(host_key);
            w.raw(ours.data(), ours.size());
            return nullptr;
        }

    private:
        // e = 2^x mod p, x random of 128 bytes, written as an mpint
        void _dh_public(Writer& w) {
            _dh.emplace(_info.kind);
            _x.b.assign(128, 0);
            crypto::random::fill(mutable_bytes_of(_x.b.data(), 128));
            Bytes e(_dh->bytes());
            const uint8_t two = 2;
            _dh->power(&two, 1, _x.b.data(), 128, e.data());
            w.mpint(e.data(), e.size());
        }

        // K as an mpint of a secret's big-endian bytes
        void _set_k_mpint(const uint8_t* p, size_t n) {
            _k.b.clear();
            Writer w(_k.b);
            w.mpint(p, n);
        }

        const char* _shared_client(const Span& theirs) {
            switch (_info.kind) {
                case KexKind::mlkem768_x25519: {
                    if (theirs.n != crypto::mlkem768::ciphertext_size + 32) {
                        return "a hybrid key exchange reply of the wrong size";
                    }
                    auto pq = _mlkem->decapsulate(bytes_of(theirs.p, crypto::mlkem768::ciphertext_size));
                    auto peer = crypto::x25519::public_key::from_bytes(bytes_of(theirs.p + crypto::mlkem768::ciphertext_size, 32));
                    if (!pq || !peer) {
                        return "a hybrid key exchange reply that does not decapsulate";
                    }
                    auto cl = _x25519->shared_secret(*peer);
                    if (!cl) {
                        return "an X25519 share of small order";
                    }
                    _hybrid_k(pq->bytes(), cl->bytes());
                    return nullptr;
                }
                case KexKind::x25519: {
                    auto peer = crypto::x25519::public_key::from_bytes(theirs.bytes());
                    if (!peer) {
                        return "an X25519 share of the wrong size";
                    }
                    auto s = _x25519->shared_secret(*peer);
                    if (!s) {
                        return "an X25519 share of small order";
                    }
                    _set_k_mpint(reinterpret_cast<const uint8_t*>(s->bytes().data()), 32);
                    return nullptr;
                }
                case KexKind::p256: {
                    auto peer = crypto::p256::public_key::from_bytes(theirs.bytes());
                    if (!peer || theirs.n != 65) {
                        return "an ECDH share not on P-256";
                    }
                    auto s = _p256->shared_secret(*peer);
                    if (!s) {
                        return "an ECDH share not on P-256";
                    }
                    _set_k_mpint(reinterpret_cast<const uint8_t*>(s->bytes().data()), 32);
                    return nullptr;
                }
                case KexKind::p384: {
                    auto peer = crypto::p384::public_key::from_bytes(theirs.bytes());
                    if (!peer || theirs.n != 97) {
                        return "an ECDH share not on P-384";
                    }
                    auto s = _p384->shared_secret(*peer);
                    if (!s) {
                        return "an ECDH share not on P-384";
                    }
                    _set_k_mpint(reinterpret_cast<const uint8_t*>(s->bytes().data()), 48);
                    return nullptr;
                }
                case KexKind::p521: {
                    auto peer = crypto::p521::public_key::from_bytes(theirs.bytes());
                    if (!peer || theirs.n != 133) {
                        return "an ECDH share not on P-521";
                    }
                    auto s = _p521->shared_secret(*peer);
                    if (!s) {
                        return "an ECDH share not on P-521";
                    }
                    _set_k_mpint(reinterpret_cast<const uint8_t*>(s->bytes().data()), 66);
                    return nullptr;
                }
                case KexKind::dh14:
                case KexKind::dh16: {
                    if (!_dh->in_range(theirs.p, theirs.n)) {
                        return "a Diffie-Hellman value out of range";
                    }
                    SecretBuffer k;
                    k.b.assign(_dh->bytes(), 0);
                    _dh->power(theirs.p, theirs.n, _x.b.data(), _x.b.size(), k.b.data());
                    _set_k_mpint(k.b.data(), k.b.size());
                    wipe(_x.b);
                    return nullptr;
                }
            }
            return "an unknown key exchange";
        }

        const char* _shared_server(const Span& theirs, Bytes& ours) {
            Writer w(ours);
            switch (_info.kind) {
                case KexKind::mlkem768_x25519: {
                    if (theirs.n != crypto::mlkem768::encapsulation_key_size + 32) {
                        return "a hybrid key exchange init of the wrong size";
                    }
                    auto ek = crypto::mlkem768::encapsulation_key::from_bytes(bytes_of(theirs.p, crypto::mlkem768::encapsulation_key_size));
                    auto peer = crypto::x25519::public_key::from_bytes(bytes_of(theirs.p + crypto::mlkem768::encapsulation_key_size, 32));
                    if (!ek || !peer) {
                        return "a hybrid key exchange init whose key is not one";
                    }
                    auto enc = ek->encapsulate();
                    auto mine = crypto::x25519::private_key::generate();
                    auto cl = mine.shared_secret(*peer);
                    if (!cl) {
                        return "an X25519 share of small order";
                    }
                    auto x = mine.public_key().bytes();
                    size_t at = w.begin_string();
                    w.raw(enc.ciphertext.data(), enc.ciphertext.size());
                    w.raw(x.data(), x.size());
                    w.end_string(at);
                    _hybrid_k(enc.shared_key.bytes(), cl->bytes());
                    return nullptr;
                }
                case KexKind::x25519: {
                    auto peer = crypto::x25519::public_key::from_bytes(theirs.bytes());
                    if (!peer) {
                        return "an X25519 share of the wrong size";
                    }
                    auto mine = crypto::x25519::private_key::generate();
                    auto s = mine.shared_secret(*peer);
                    if (!s) {
                        return "an X25519 share of small order";
                    }
                    auto x = mine.public_key().bytes();
                    w.string(x.data(), x.size());
                    _set_k_mpint(reinterpret_cast<const uint8_t*>(s->bytes().data()), 32);
                    return nullptr;
                }
                case KexKind::p256: {
                    auto peer = crypto::p256::public_key::from_bytes(theirs.bytes());
                    if (!peer || theirs.n != 65) {
                        return "an ECDH share not on P-256";
                    }
                    auto mine = crypto::p256::ecdh_key::generate();
                    auto s = mine.shared_secret(*peer);
                    if (!s) {
                        return "an ECDH share not on P-256";
                    }
                    auto q = mine.public_key().bytes();
                    w.string(q.data(), q.size());
                    _set_k_mpint(reinterpret_cast<const uint8_t*>(s->bytes().data()), 32);
                    return nullptr;
                }
                case KexKind::p384: {
                    auto peer = crypto::p384::public_key::from_bytes(theirs.bytes());
                    if (!peer || theirs.n != 97) {
                        return "an ECDH share not on P-384";
                    }
                    auto mine = crypto::p384::ecdh_key::generate();
                    auto s = mine.shared_secret(*peer);
                    if (!s) {
                        return "an ECDH share not on P-384";
                    }
                    auto q = mine.public_key().bytes();
                    w.string(q.data(), q.size());
                    _set_k_mpint(reinterpret_cast<const uint8_t*>(s->bytes().data()), 48);
                    return nullptr;
                }
                case KexKind::p521: {
                    auto peer = crypto::p521::public_key::from_bytes(theirs.bytes());
                    if (!peer || theirs.n != 133) {
                        return "an ECDH share not on P-521";
                    }
                    auto mine = crypto::p521::ecdh_key::generate();
                    auto s = mine.shared_secret(*peer);
                    if (!s) {
                        return "an ECDH share not on P-521";
                    }
                    auto q = mine.public_key().bytes();
                    w.string(q.data(), q.size());
                    _set_k_mpint(reinterpret_cast<const uint8_t*>(s->bytes().data()), 66);
                    return nullptr;
                }
                case KexKind::dh14:
                case KexKind::dh16: {
                    _dh.emplace(_info.kind);
                    if (!_dh->in_range(theirs.p, theirs.n)) {
                        return "a Diffie-Hellman value out of range";
                    }
                    _dh_public(w);
                    SecretBuffer k;
                    k.b.assign(_dh->bytes(), 0);
                    _dh->power(theirs.p, theirs.n, _x.b.data(), _x.b.size(), k.b.data());
                    _set_k_mpint(k.b.data(), k.b.size());
                    wipe(_x.b);
                    return nullptr;
                }
            }
            return "an unknown key exchange";
        }

        // K = SHA-256(ML-KEM key || X25519 secret), as a string
        void _hybrid_k(const slice<const byte>& pq, const slice<const byte>& cl) {
            uint8_t both[64], d[32];
            sgcl::detail::copy_bytes(both, pq.data(), 32);
            sgcl::detail::copy_bytes(both + 32, cl.data(), 32);
            hash_of(HashKind::sha256, both, 64, d);
            _k.b.clear();
            Writer w(_k.b);
            w.string(d, 32);
            crypto::detail::secure_zero(both, sizeof both);
            crypto::detail::secure_zero(d, sizeof d);
        }

        // H = HASH(V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K), the
        // shares already encoded (strings or mpints)
        void _hash(const HashInputs& in, const Span& host_key, const Bytes& q_c, const Bytes& q_s) {
            SecretBuffer data;
            Writer w(data.b);
            w.string(in.client_version);
            w.string(in.server_version);
            w.string(*in.client_kexinit);
            w.string(*in.server_kexinit);
            w.string(host_key);
            w.raw(q_c.data(), q_c.size());
            w.raw(q_s.data(), q_s.size());
            w.raw(_k.b.data(), _k.b.size());
            _h.assign(hash_size(_info.hash), 0);
            hash_of(_info.hash, data.b.data(), data.b.size(), _h.data());
        }

        const KexInfo& _info;
        optional<crypto::mlkem768::decapsulation_key> _mlkem;
        optional<crypto::x25519::private_key> _x25519;
        optional<crypto::p256::ecdh_key> _p256;
        optional<crypto::p384::ecdh_key> _p384;
        optional<crypto::p521::ecdh_key> _p521;
        optional<DhGroup> _dh;
        SecretBuffer _x;
        Bytes _ours;
        SecretBuffer _k;
        Bytes _h;
    };

    // RFC 4253 §7.2: HASH(K || H || letter || session_id), extended by
    // HASH(K || H || K1 || K2 …) to `need` bytes
    inline void derive_key(HashKind hash, const Bytes& k, const Bytes& h, const Bytes& session_id, char letter, size_t need, SecretBuffer& out) {
        const size_t hs = hash_size(hash);
        SecretBuffer data;
        Writer w(data.b);
        w.raw(k.data(), k.size());
        w.raw(h.data(), h.size());
        w.u8(uint8_t(letter));
        w.raw(session_id.data(), session_id.size());
        out.b.assign(hs, 0);
        hash_of(hash, data.b.data(), data.b.size(), out.b.data());
        while (out.b.size() < need) {
            wipe(data.b);
            Writer w2(data.b);
            w2.raw(k.data(), k.size());
            w2.raw(h.data(), h.size());
            w2.raw(out.b.data(), out.b.size());
            size_t at = out.b.size();
            out.b.resize(at + hs);
            hash_of(hash, data.b.data(), data.b.size(), out.b.data() + at);
        }
        out.b.resize(need);
    }

    // One direction's keys from an exchange: the cipher's IV and key, the
    // MAC's key, into the packet keys (d 0: client to server, letters A,
    // C, E; 1: server to client, B, D, F)
    inline void install_keys(PacketKeys& keys, const Negotiated& n, int d, const KexExchange& x, const Bytes& session_id) {
        const CipherInfo& c = *n.cipher[d];
        SecretBuffer iv, key, mac;
        if (c.iv_size) {
            derive_key(x.info().hash, x.k(), x.h(), session_id, char('A' + d), c.iv_size, iv);
        }
        derive_key(x.info().hash, x.k(), x.h(), session_id, char('C' + d), c.key_size, key);
        MacKind mk = MacKind::none;
        if (n.mac[d]) {
            derive_key(x.info().hash, x.k(), x.h(), session_id, char('E' + d), n.mac[d]->key_size, mac);
            mk = n.mac[d]->kind;
        }
        keys.set(c.kind, key.b.data(), iv.b.empty() ? nullptr : iv.b.data(), mk, mac.b.empty() ? nullptr : mac.b.data());
    }
}
