//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/keys.h"
#include "../error.h"
#include "../ip.h"
#include "../../core/detail/handle_word.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../crypto/error.h"
#include "../../crypto/read_secret.h"
#include "../../crypto/secret.h"
#include "../../io/error.h"
#include "../../io/file.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>

// The keys of SSH: public keys as authorized_keys and .pub files write
// them, with OpenSSH's certificates over them, and private keys in
// OpenSSH's own file format (encrypted with a passphrase or not) or in
// PEM's (PKCS #8, SEC 1, PKCS #1), made, read and written.
//
//   auto key = net::ssh::private_key::load("/home/me/.ssh/id_ed25519");
//   println("{}", key->public_key().fingerprint());   // SHA256:…
namespace sgcl::net::ssh {
    // The kinds of key SSH signs with here
    enum class key_type : uint8_t {
        ed25519,       // ssh-ed25519 (RFC 8709)
        ecdsa_p256,    // ecdsa-sha2-nistp256 (RFC 5656)
        ecdsa_p384,    // ecdsa-sha2-nistp384
        rsa,           // ssh-rsa keys, signing rsa-sha2-512 and rsa-sha2-256 (RFC 8332)
        ecdsa_p521,    // ecdsa-sha2-nistp521 (appended: the earlier values unchanged)
    };

    // What an OpenSSH certificate is for (PROTOCOL.certkeys)
    enum class certificate_type : uint8_t {
        user = 1,
        host = 2,
    };

    struct certificate;

    namespace detail {
        using sgcl::io::detail::fail;
        using net::detail::parse_port;
        using net::detail::split_host_port;

        inline io::error key_error(crypto::errc code, std::string_view what, const string& path = {}) noexcept {
            return io::error(crypto::make_error_code(code), "ssh key", path.empty() ? string(what) : string(std::string(path.view()) + ": " + std::string(what)));
        }

        SGCL_INLINE_HOT key_type public_type(KeyKind k) noexcept {
            return key_type(uint8_t(k));
        }

        struct PublicKeyAccess;
    }

    // A public key: its blob (the wire form of RFC 4253 §6.6, which a
    // certificate's is too) and the comment of its text form; a value,
    // copied freely. An empty one (default-constructed) is false
    class public_key {
    public:
        public_key() noexcept = default;

        // A key as authorized_keys and .pub files write it, "type base64
        // [comment]" (options in front, as authorized_keys allows them,
        // are not taken here: authorized_keys reads those).
        // errc::malformed (crypto) for anything else
        static expected<public_key, io::error> parse(const string& text) noexcept {
            detail::Bytes blob;
            std::string comment;
            std::string_view type;
            if (!detail::parse_public_line(text.view(), blob, comment, type)) {
                return unexpected(detail::key_error(crypto::errc::malformed, "not a public key line (type, base64, comment)"));
            }
            public_key k;
            k._blob = string(std::string_view(reinterpret_cast<const char*>(blob.data()), blob.size()));
            k._comment = string(comment);
            return k;
        }

        // The same, a text that is not one thrown (std::invalid_argument)
        explicit public_key(const string& text) {
            auto r = parse(text);
            if (!r) {
                throw std::invalid_argument(std::string(r.error().message().view()));
            }
            *this = std::move(*r);
        }

        // A key from its blob
        static expected<public_key, io::error> from_bytes(const slice<const byte>& blob) noexcept {
            detail::ParsedKey pk;
            if (!detail::parse_key(reinterpret_cast<const uint8_t*>(blob.data()), blob.size(), pk)) {
                return unexpected(detail::key_error(crypto::errc::malformed, "not a public key blob"));
            }
            public_key k;
            k._blob = string(std::string_view(reinterpret_cast<const char*>(blob.data()), blob.size()));
            return k;
        }

        // The kind of the key (a certificate's: of the key it certifies)
        ssh::key_type type() const noexcept {
            detail::ParsedKey pk;
            _parse(pk);
            return detail::public_type(pk.key.kind);
        }

        // The name of its type: "ssh-ed25519", "ecdsa-sha2-nistp256",
        // "ssh-rsa", "ssh-ed25519-cert-v01@openssh.com" …
        string type_name() const noexcept {
            detail::Reader r(_data(), _blob.size());
            return string(r.string().view());
        }

        // The blob, the key's wire form
        vector<byte> bytes() const noexcept {
            vector<byte> out(_blob.size());
            if (!_blob.empty()) {
                sgcl::detail::copy_bytes(out.data(), _blob.data(), _blob.size());
            }
            return out;
        }

        SGCL_INLINE_HOT const string& comment() const noexcept {
            return _comment;
        }

        // The key with another comment
        SGCL_INLINE_HOT public_key with_comment(const string& comment) const noexcept {
            public_key k = *this;
            k._comment = comment;
            return k;
        }

        // "SHA256:" and the unpadded base64 of the blob's SHA-256, as
        // ssh-keygen -l prints it
        string fingerprint() const noexcept {
            return string(detail::fingerprint_of(_data(), _blob.size()));
        }

        // "type base64 comment" (the comment left out when empty), as a .pub
        // file holds it
        string to_string() const noexcept {
            std::string s(type_name().view());
            s += ' ';
            auto b = encoding::base64::standard.encode(slice<const byte>(reinterpret_cast<const byte*>(_blob.data()), _blob.size()));
            s.append(b.view());
            if (!_comment.empty()) {
                s += ' ';
                s.append(_comment.view());
            }
            return string(s);
        }

        // Whether the key is an OpenSSH certificate
        bool is_certificate() const noexcept {
            detail::ParsedKey pk;
            return _parse(pk) && pk.cert;
        }

        // The certificate's fields, when the key is one whose signature
        // verifies under its signature key; nullopt otherwise
        optional<ssh::certificate> certificate() const noexcept;

        // Whether signature (a signature blob: its algorithm's name and
        // its bytes) signs data under the key (a certificate's: the
        // certified key's). An "ssh-rsa" (SHA-1) signature is false
        [[nodiscard]] bool verify(const slice<const byte>& data, const slice<const byte>& signature) const noexcept {
            detail::ParsedKey pk;
            if (!_parse(pk)) {
                return false;
            }
            detail::Reader r(reinterpret_cast<const uint8_t*>(signature.data()), signature.size());
            detail::Span alg = r.string();
            if (!r.ok()) {
                return false;
            }
            return detail::verify(pk.key, alg.view(), reinterpret_cast<const uint8_t*>(data.data()), data.size(), reinterpret_cast<const uint8_t*>(signature.data()),
                                  signature.size());
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return !_blob.empty();
        }

        // The same key: the same blob (the comment not compared)
        SGCL_INLINE_HOT friend bool operator==(const public_key& a, const public_key& b) noexcept {
            return a._blob == b._blob;
        }

    private:
        friend struct detail::PublicKeyAccess;

        SGCL_INLINE_HOT const uint8_t* _data() const noexcept {
            return reinterpret_cast<const uint8_t*>(_blob.data());
        }

        SGCL_INLINE_HOT bool _parse(detail::ParsedKey& pk) const noexcept {
            return !_blob.empty() && detail::parse_key(_data(), _blob.size(), pk);
        }

        string _blob;
        string _comment;
    };

    // An OpenSSH certificate's fields (PROTOCOL.certkeys): what
    // public_key::certificate() reads from a certificate whose signature
    // verifies. The times are seconds since 1970, valid_before
    // UINT64_MAX for "forever"
    struct certificate {
        ssh::public_key key;                               // the key certified (a plain key)
        certificate_type type = certificate_type::user;
        uint64_t serial = 0;
        string key_id;
        vector<string> principals;                         // the users or hosts it is for; empty: any
        uint64_t valid_after = 0;
        uint64_t valid_before = 0;
        vector<pair<string, string>> critical_options;     // name and value ("force-command", "source-address")
        vector<pair<string, string>> extensions;           // name and value ("permit-pty" …)
        ssh::public_key signature_key;                     // the authority that signed it
    };

    namespace detail {
        struct PublicKeyAccess {
            SGCL_INLINE_HOT static public_key make(const uint8_t* p, size_t n) noexcept {
                public_key k;
                k._blob = string(std::string_view(reinterpret_cast<const char*>(p), n));
                return k;
            }

            SGCL_INLINE_HOT static const string& blob(const public_key& k) noexcept {
                return k._blob;
            }

            SGCL_INLINE_HOT static const uint8_t* data(const public_key& k) noexcept {
                return k._data();
            }
        };

        // The plain key of a parsed key's material
        inline public_key plain_of(const KeyMaterial& m) {
            Bytes b;
            Writer w(b);
            write_plain(w, m);
            return PublicKeyAccess::make(b.data(), b.size());
        }

        // Whether a certificate holds at a time, of its type, for a name
        // among its principals (an empty list holds for any)
        inline bool certificate_holds(const certificate& c, certificate_type type, std::string_view name, uint64_t now) noexcept {
            if (c.type != type || now < c.valid_after || now >= c.valid_before) {
                return false;
            }
            if (c.principals.empty()) {
                return true;
            }
            for (const auto& p : c.principals) {
                if (p.view() == name) {
                    return true;
                }
            }
            return false;
        }
    }

    inline optional<certificate> public_key::certificate() const noexcept {
        detail::ParsedKey pk;
        if (!_parse(pk) || !pk.cert || !detail::cert_signature_ok(_data(), pk)) {
            return nullopt;
        }
        ssh::certificate c;
        c.key = detail::plain_of(pk.key);
        c.type = certificate_type(pk.c.type);
        c.serial = pk.c.serial;
        c.key_id = string(detail::printable(pk.c.key_id.view()));
        for (auto p : detail::strings_of(pk.c.principals)) {
            c.principals.push_back(string(p));
        }
        c.valid_after = pk.c.valid_after;
        c.valid_before = pk.c.valid_before;
        std::vector<std::pair<std::string_view, std::string_view>> opts;
        if (!detail::options_of(pk.c.critical_options, opts)) {
            return nullopt;
        }
        for (auto& [n, v] : opts) {
            c.critical_options.push_back(pair<string, string>(string(n), string(v)));
        }
        opts.clear();
        if (!detail::options_of(pk.c.extensions, opts)) {
            return nullopt;
        }
        for (auto& [n, v] : opts) {
            c.extensions.push_back(pair<string, string>(string(n), string(v)));
        }
        c.signature_key = detail::PublicKeyAccess::make(pk.c.signature_key.p, pk.c.signature_key.n);
        return c;
    }

    class private_key;

    namespace detail {
        // A private key in unmanaged memory, zeroed by crypto's keys when
        // the handle's state is collected
        struct PrivateKeyState {
            std::unique_ptr<KeyPair> key;
        };

        struct PrivateKeyAccess {
            static const KeyPair& key(const private_key& k) noexcept;
            static tracked_ptr<const void> word(const private_key& k) noexcept;
        };

        inline io::error key_file_error(KeyFileError e, const string& path) noexcept {
            switch (e) {
                case KeyFileError::unsupported: return key_error(crypto::errc::unsupported, "a key of a form, cipher or type not read here", path);
                case KeyFileError::passphrase: return key_error(crypto::errc::authentication, "encrypted: no passphrase, or a wrong one", path);
                default: return key_error(crypto::errc::malformed, "not a private key of OpenSSH's or PEM's forms", path);
            }
        }
    }

    // A private key: a handle of one word, whose key lives in unmanaged
    // memory, never copied (copies of the handle share it) and zeroed when
    // the last of them is gone
    class private_key {
    public:
        private_key() noexcept = default;

        // A key from its file's text: OpenSSH's format ("-----BEGIN OPENSSH
        // PRIVATE KEY-----", encrypted with the passphrase when it is:
        // bcrypt_pbkdf with aes256-ctr, aes192-ctr, aes128-ctr or AES-GCM),
        // or PEM's (PKCS #8, SEC 1, PKCS #1, unencrypted). The text is read
        // where it lies (a secret_bytes of crypto::read_secret)
        static expected<private_key, io::error> parse(const slice<const byte>& text, const string& passphrase = {}) noexcept {
            return _parse(text, passphrase, string());
        }

        // The key of a file, its bytes read straight into a secret_bytes
        static expected<private_key, io::error> load(const string& path, const string& passphrase = {}) noexcept {
            auto text = crypto::read_secret(path);
            if (!text) {
                return unexpected(text.error());
            }
            return _parse(text->as_slice(), passphrase, path);
        }

        // A new key from crypto::random: Ed25519, ECDSA on P-256, P-384 or
        // P-521, or RSA of rsa_bits (2048 to 16384, ssh-keygen's 3072 by
        // default)
        static private_key generate(key_type type = key_type::ed25519, size_t rsa_bits = 3072) {
            auto s = make_tracked<detail::PrivateKeyState>();
            s->key = std::make_unique<detail::KeyPair>();
            detail::KeyPair& k = *s->key;
            k.kind = detail::KeyKind(uint8_t(type));
            switch (type) {
                case key_type::ed25519: k.ed25519.emplace(crypto::ed25519::private_key::generate()); break;
                case key_type::ecdsa_p256: k.p256.emplace(crypto::p256::private_key::generate()); break;
                case key_type::ecdsa_p384: k.p384.emplace(crypto::p384::private_key::generate()); break;
                case key_type::ecdsa_p521: k.p521.emplace(crypto::p521::private_key::generate()); break;
                case key_type::rsa: k.rsa.emplace(crypto::rsa::private_key::generate(rsa_bits)); break;
            }
            k.make_public();
            return private_key(std::move(s));
        }

        // The public half, with the key's comment
        ssh::public_key public_key() const noexcept {
            const detail::KeyPair& k = *_s->key;
            return detail::PublicKeyAccess::make(k.public_blob.data(), k.public_blob.size()).with_comment(string(k.comment));
        }

        SGCL_INLINE_HOT key_type type() const noexcept {
            return detail::public_type(_s->key->kind);
        }

        SGCL_INLINE_HOT string comment() const noexcept {
            return string(_s->key->comment);
        }

        // The key with another comment (a new handle; the key's bytes copied
        // into its own unmanaged block)
        private_key with_comment(const string& comment) const {
            auto s = make_tracked<detail::PrivateKeyState>();
            s->key = std::make_unique<detail::KeyPair>();
            const detail::KeyPair& from = *_s->key;
            detail::KeyPair& k = *s->key;
            k.kind = from.kind;
            if (from.ed25519) {
                k.ed25519.emplace(from.ed25519->clone());
            }
            if (from.p256) {
                k.p256.emplace(from.p256->clone());
            }
            if (from.p384) {
                k.p384.emplace(from.p384->clone());
            }
            if (from.p521) {
                k.p521.emplace(from.p521->clone());
            }
            if (from.rsa) {
                k.rsa.emplace(from.rsa->clone());
            }
            k.public_blob = from.public_blob;
            k.comment.assign(comment.view());
            return private_key(std::move(s));
        }

        // The key in OpenSSH's file format, as ssh-keygen writes it:
        // unencrypted, or with a passphrase under bcrypt_pbkdf (16 rounds)
        // and aes256-ctr. A secret_bytes: never managed memory
        crypto::secret_bytes to_openssh(const string& passphrase = {}) const noexcept {
            detail::SecretBuffer text;
            detail::write_openssh(*_s->key, passphrase.view(), text);
            crypto::secret_bytes out(text.b.size());
            if (!text.b.empty()) {
                sgcl::detail::copy_bytes(out.as_slice().data(), text.b.data(), text.b.size());
            }
            return out;
        }

        // to_openssh() written to a file made with mode 0600 (replaced when
        // it is there)
        expected<void, io::error> save(const string& path, const string& passphrase = {}) const noexcept {
            auto text = to_openssh(passphrase);
            auto f = io::open(path, io::open_flags::write | io::open_flags::create | io::open_flags::truncate, io::permissions(0600));
            if (!f) {
                return unexpected(f.error());
            }
            (void)::fchmod(f->fd(), 0600);   // a file that was there keeps its mode otherwise
            auto w = f->write(text.as_slice());
            if (!w) {
                (void)f->close();
                return unexpected(w.error());
            }
            return f->close();
        }

        // The signature blob of data (the algorithm's name and the
        // signature) by the key's default algorithm: ssh-ed25519,
        // ecdsa-sha2-nistp256, -nistp384 or -nistp521, rsa-sha2-512
        vector<byte> sign(const slice<const byte>& data) const noexcept {
            const detail::KeyPair& k = *_s->key;
            auto b = k.sign(k.default_alg(), reinterpret_cast<const uint8_t*>(data.data()), data.size());
            vector<byte> out(b.size());
            sgcl::detail::copy_bytes(out.data(), b.data(), b.size());
            return out;
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_s;
        }

    private:
        friend struct detail::PrivateKeyAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit private_key(tracked_ptr<detail::PrivateKeyState> s) noexcept
        : _s(std::move(s)) {
        }

        SGCL_INLINE_HOT private_key(sgcl::detail::FromWord, const tracked_ptr<detail::PrivateKeyState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::PrivateKeyState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::PrivateKeyState>& _handle_word() const noexcept {
            return _s;
        }

        static expected<private_key, io::error> _parse(const slice<const byte>& text, const string& passphrase, const string& path) noexcept {
            auto s = make_tracked<detail::PrivateKeyState>();
            s->key = std::make_unique<detail::KeyPair>();
            auto e = detail::read_key_file(text, passphrase.view(), *s->key);
            if (e != detail::KeyFileError::none) {
                return unexpected(detail::key_file_error(e, path));
            }
            return private_key(std::move(s));
        }

        tracked_ptr<detail::PrivateKeyState> _s;
    };

    namespace detail {
        SGCL_INLINE_HOT const KeyPair& PrivateKeyAccess::key(const private_key& k) noexcept {
            return *k._s->key;
        }

        SGCL_INLINE_HOT tracked_ptr<const void> PrivateKeyAccess::word(const private_key& k) noexcept {
            return tracked_ptr<const void>(k._s);
        }
    }
}
