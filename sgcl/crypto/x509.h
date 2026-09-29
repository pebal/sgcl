//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/x509_cert.h"
#include "detail/x509_verify.h"
#include "error.h"
#include "sha256.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/root_ptr.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../encoding/pem.h"
#include "../io/file.h"
#include "../io/fs.h"
#include "../io/os.h"
#include "../time/datetime.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// X.509 certificates (RFC 5280), Go's crypto/x509: a certificate read from
// DER or PEM with its names, its key and its extensions; a pool of them;
// and the verification of a chain from a leaf to a root of a pool, with
// the host name or the IP address the leaf is for.
//
//   auto cert = crypto::x509::certificate::from_pem(text);
//   auto chain = cert->verify({.dns_name = "example.com"});   // the system's roots, the time now
//   if (!chain) { switch (chain.error().reason()) { ... } }
//
// What verification is: a chain built from the leaf through the
// intermediates given to a root of the pool (at most ten intermediates,
// no certificate twice, no more than a hundred signatures tried), every
// signature checked (RSA PKCS #1 v1.5 and PSS, ECDSA on P-256 and P-384,
// Ed25519; over SHA-256, SHA-384, SHA-512; MD5 and SHA-1 never), every
// certificate valid at the time asked, every issuer a CA (basicConstraints,
// keyUsage keyCertSign where there is a keyUsage), the path lengths, the
// name constraints of every CA over every name below it, the extended key
// usages nested down the chain (serverAuth when none is asked), no
// critical extension left unknown, and the leaf for the name asked (RFC
// 6125: dNSNames only, a wildcard only as the whole leftmost label, the
// common name never). Revocation is not checked: no CRL, no OCSP. Policy
// validation is not done: a critical policyConstraints, policyMappings or
// inhibitAnyPolicy is an unhandled critical extension. The implementation
// has not been through an independent cryptographic audit.
// README: docs/sgcl/crypto/x509.md
namespace sgcl::crypto::x509 {
    class certificate;
    class certificate_pool;
    struct verify_options;

    // A chain, the leaf first and the root last
    using chain = vector<certificate>;

    namespace detail {
        struct PoolData;
        struct PoolAccess;
        class ChainBuilder;

        // The first line at or after from that starts with prefix: its
        // start, or npos
        inline size_t pem_line_at(std::string_view v, size_t from, std::string_view prefix) noexcept {
            for (size_t at = from;;) {
                at = v.find(prefix, at);
                if (at == std::string_view::npos || at == 0 || v[at - 1] == '\n') {
                    return at;
                }
                ++at;
            }
        }

        // The PEM blocks of a text one by one, as Go's pem.Decode walks
        // them: each BEGIN line with the first END line after it read as a
        // block, f given the block or the error of its reading. A block
        // that reads goes on after its END line; one that does not goes on
        // from the line after its BEGIN, so that a BEGIN without an END of
        // its own does not take the next block with it. f returns true to
        // stop
        template<class F>
        void walk_pem(const string& text, F&& f) {
            auto v = text.view();
            size_t from = 0;
            while (from < v.size()) {
                size_t begin = pem_line_at(v, from, "-----BEGIN ");
                if (begin == std::string_view::npos) {
                    return;
                }
                size_t after_begin = v.find('\n', begin);
                after_begin = after_begin == std::string_view::npos ? v.size() : after_begin + 1;
                size_t end = pem_line_at(v, after_begin, "-----END ");
                if (end == std::string_view::npos) {
                    return;   // no block from here on has an END
                }
                size_t stop = v.find('\n', end);
                stop = stop == std::string_view::npos ? v.size() : stop + 1;
                auto block = encoding::pem::parse(string(std::string(v.substr(begin, stop - begin))));
                from = block ? stop : after_begin;
                if (f(block)) {
                    return;
                }
            }
        }
    }

    // A certificate: read once, never changed, a value that costs a pointer
    // to copy (the parse is shared). Everything it holds is readable, a
    // key of an algorithm the module does not have included (key().kind()
    // is none); what it may be used for is verify()'s question
    class certificate {
    public:
        // A certificate in DER: errc::malformed with the offset for
        // anything that is not one, or is not strict DER, or passes the
        // bounds of a certificate (128 KiB, 64 extensions, 1024 names in
        // its SAN, 256 subtrees of name constraints)
        [[nodiscard]] static expected<certificate, error> parse(const slice<const byte>& der) {
            // the size before the copy, so that an input of any length costs nothing
            if (der.size() > detail::max_certificate_size) {
                return unexpected<error>(error(errc::malformed, uint64_t(0), string("sgcl::crypto::x509: a certificate larger than 128 KiB")));
            }
            auto d = make_tracked<detail::CertData>();
            const byte* p = der.data();
            d->raw = vector<byte>(p, p + der.size());
            detail::CertParser parser{*d};
            if (auto r = parser.run(); !r) {
                return unexpected<error>(r.error());
            }
            return certificate(tracked_ptr<const detail::CertData>(std::move(d)));
        }

        // The first CERTIFICATE block of a PEM text (RFC 7468), whatever
        // is around it: the blocks are read one by one, as
        // certificate_pool::append_pem reads them, and one that cannot be
        // read as PEM, or is of another type, is passed over. The
        // certificate's parse, or errc::malformed for a text with no
        // CERTIFICATE block that reads (the message names the first PEM
        // error when there was one)
        [[nodiscard]] static expected<certificate, error> from_pem(const string& text) {
            optional<encoding::pem> found;
            optional<string> first_error;
            detail::walk_pem(text, [&](expected<encoding::pem, encoding::error>& block) {
                if (!block) {
                    if (!first_error) {
                        first_error = block.error().message();
                    }
                    return false;
                }
                if (block->type() != "CERTIFICATE") {
                    return false;
                }
                found = std::move(*block);
                return true;
            });
            if (found) {
                return parse(found->bytes().as_slice());
            }
            std::string why = "sgcl::crypto::x509: no CERTIFICATE block in the PEM text";
            if (first_error) {
                why += " (PEM: " + std::string(first_error->view()) + ")";
            }
            return unexpected<error>(error(errc::malformed, string(why)));
        }

        // The whole certificate, its TBSCertificate (what the signature
        // covers), its issuer and subject names and its
        // SubjectPublicKeyInfo, as the bytes of the encoding
        slice<const byte> raw() const noexcept {
            return _d->raw.as_slice();
        }

        slice<const byte> raw_tbs() const noexcept {
            return _d->range(_d->tbs_at, _d->tbs_size);
        }

        slice<const byte> raw_issuer() const noexcept {
            return _d->range(_d->issuer_at, _d->issuer_size);
        }

        slice<const byte> raw_subject() const noexcept {
            return _d->range(_d->subject_at, _d->subject_size);
        }

        slice<const byte> raw_subject_public_key_info() const noexcept {
            return _d->range(_d->spki_at, _d->spki_size);
        }

        // 1, 2 or 3
        int version() const noexcept {
            return _d->version;
        }

        // The serial number as the certificate has it: the INTEGER's
        // bytes, big-endian two's complement in the shortest form (a 00 in
        // front of a first byte of 80 or more; negative numbers, which
        // RFC 5280 forbids and a few old roots hold, start with a set bit)
        const vector<byte>& serial_number() const noexcept {
            return _d->serial;
        }

        const name& issuer() const noexcept {
            return _d->issuer;
        }

        const name& subject() const noexcept {
            return _d->subject;
        }

        // The validity, in UTC. A time the certificate holds outside
        // datetime's years (1678 to 2261: 99991231235959Z, "no expiry")
        // is the end of that range here; verification compares the times
        // as the certificate has them
        time::datetime not_before() const noexcept {
            return time::datetime::from_unix(_d->not_before, time::zone::utc());
        }

        time::datetime not_after() const noexcept {
            return time::datetime::from_unix(_d->not_after, time::zone::utc());
        }

        const x509::public_key& public_key() const noexcept {
            return _d->key;
        }

        x509::signature_algorithm signature_algorithm() const noexcept {
            return _d->sig_algorithm;
        }

        // The OID of the signature algorithm, "1.2.840.113549.1.1.11"
        const string& signature_algorithm_oid() const noexcept {
            return _d->sig_algorithm_oid;
        }

        const vector<byte>& signature() const noexcept {
            return _d->signature;
        }

        // Every extension, in the order of the certificate
        const vector<extension>& extensions() const noexcept {
            return _d->extensions;
        }

        // basicConstraints: whether there is one, its cA, its
        // pathLenConstraint (nullopt when absent)
        bool has_basic_constraints() const noexcept {
            return _d->basic_constraints_valid;
        }

        bool is_ca() const noexcept {
            return _d->is_ca;
        }

        optional<int64_t> max_path_length() const noexcept {
            if (_d->max_path_len < 0) {
                return nullopt;
            }
            return _d->max_path_len;
        }

        // keyUsage: whether there is one, and its bits
        bool has_key_usage() const noexcept {
            return _d->has_key_usage;
        }

        x509::key_usage key_usage() const noexcept {
            return x509::key_usage(_d->key_usage_bits);
        }

        bool allows(x509::key_usage u) const noexcept {
            return (_d->key_usage_bits & uint16_t(u)) == uint16_t(u);
        }

        // extKeyUsage: the purposes known by name, and the OIDs of the rest
        const vector<ext_key_usage>& ext_key_usages() const noexcept {
            return _d->ext_key_usages;
        }

        const vector<string>& unknown_ext_key_usages() const noexcept {
            return _d->unknown_ext_key_usages;
        }

        // subjectAltName, by kind
        const vector<string>& dns_names() const noexcept {
            return _d->dns_names;
        }

        const vector<string>& email_addresses() const noexcept {
            return _d->email_addresses;
        }

        const vector<ip_address>& ip_addresses() const noexcept {
            return _d->ip_addresses;
        }

        const vector<string>& uris() const noexcept {
            return _d->uris;
        }

        // authorityKeyIdentifier's keyIdentifier, subjectKeyIdentifier
        const vector<byte>& authority_key_id() const noexcept {
            return _d->authority_key_id;
        }

        const vector<byte>& subject_key_id() const noexcept {
            return _d->subject_key_id;
        }

        // nameConstraints, by kind: the permitted and the excluded subtrees
        bool has_name_constraints() const noexcept {
            return _d->has_name_constraints;
        }

        const vector<string>& permitted_dns_domains() const noexcept {
            return _d->permitted_dns;
        }

        const vector<string>& excluded_dns_domains() const noexcept {
            return _d->excluded_dns;
        }

        const vector<ip_range>& permitted_ip_ranges() const noexcept {
            return _d->permitted_ip;
        }

        const vector<ip_range>& excluded_ip_ranges() const noexcept {
            return _d->excluded_ip;
        }

        const vector<string>& permitted_email_addresses() const noexcept {
            return _d->permitted_email;
        }

        const vector<string>& excluded_email_addresses() const noexcept {
            return _d->excluded_email;
        }

        const vector<string>& permitted_uri_domains() const noexcept {
            return _d->permitted_uri;
        }

        const vector<string>& excluded_uri_domains() const noexcept {
            return _d->excluded_uri;
        }

        // certificatePolicies, as their OIDs
        const vector<string>& policies() const noexcept {
            return _d->policies;
        }

        // The OIDs of the critical extensions not handled here: a
        // certificate with one does not verify
        const vector<string>& unhandled_critical_extensions() const noexcept {
            return _d->unhandled_critical;
        }

        // Whether parent signed this certificate: parent a CA (a version 3
        // parent needs basicConstraints cA, a keyUsage keyCertSign), the
        // signature of an algorithm verified here and valid under
        // parent's key. errc::verification with not_a_ca,
        // missing_cert_sign, insecure_algorithm, unsupported_algorithm or
        // invalid_signature. Go's CheckSignatureFrom
        [[nodiscard]] expected<void, error> check_signature_from(const certificate& parent) const {
            return detail::check_signature(*_d, *parent._d);
        }

        // Whether the certificate is for the DNS name: RFC 6125 against
        // its dNSNames, case folded in ASCII, a trailing '.' of the name
        // ignored, a wildcard only as the whole leftmost label and for
        // exactly one label; the common name never. A name written as an
        // IP address is refused: an address is verified by verify_ip.
        // errc::verification, reason hostname_mismatch
        [[nodiscard]] expected<void, error> verify_hostname(const string& host) const {
            return detail::check_hostname(*_d, host.view());
        }

        // Whether the certificate is for the IP address, 4 or 16 bytes in
        // network order, byte for byte against its iPAddresses (RFC 5280
        // §4.2.1.6); a 16-byte IPv4-mapped address (::ffff:a.b.c.d) is
        // taken as its IPv4 address. Another length is
        // std::invalid_argument: the address is the program's
        [[nodiscard]] expected<void, error> verify_ip(const slice<const byte>& ip) const {
            return detail::check_ip(*_d, detail::address_of(ip));
        }

        // A text, which bytes take as its characters, is not an address:
        // "192.0.2.10" would be ten bytes (net::ip_address gives the four)
        template<class T>
        requires std::is_convertible_v<const T&, std::string_view>
        expected<void, error> verify_ip(const T& text) const = delete;

        // The chain from this certificate to a root: the system's roots,
        // the time now, serverAuth, no name
        [[nodiscard]] expected<x509::chain, error> verify() const;

        // The chain as the options ask; errc::verification with the
        // reason when there is none (verify_options says what each field
        // does)
        [[nodiscard]] expected<x509::chain, error> verify(const verify_options& options) const;

        // The same bytes
        friend bool operator==(const certificate& a, const certificate& b) noexcept {
            return a._d == b._d || a._d->raw == b._d->raw;
        }

    private:
        friend class certificate_pool;
        friend struct detail::PoolAccess;
        friend class detail::ChainBuilder;

        tracked_ptr<const detail::CertData> _d;

        explicit certificate(tracked_ptr<const detail::CertData> d) noexcept
        : _d(std::move(d)) {
        }
    };

    namespace detail {
        // The certificates of a pool and their index by subject (the
        // issuer a child names) and by digest (a certificate added once)
        struct PoolData {
            vector<certificate> certs;
            std::unordered_map<std::string, std::vector<uint32_t>> by_subject;
            std::unordered_set<std::string> sums;
        };
    }

    // A set of certificates: the roots a chain must end in, or the
    // intermediates it may pass through. A handle, as Go's *CertPool: a
    // copy shares the certificates, and add() through one is seen through
    // every copy; clone() makes a pool of its own. Reading from many
    // threads at once is safe, adding while another thread verifies is
    // not
    class certificate_pool {
    public:
        certificate_pool()
        : _d(make_tracked<detail::PoolData>()) {
        }

        // The system's roots, loaded once per process and cloned for each
        // call (a change to the pool returned touches no other): the file
        // SSL_CERT_FILE names, else the first of the bundles Go looks for
        // (/etc/ssl/certs/ca-certificates.crt, /etc/pki/tls/certs/ca-bundle.crt,
        // /etc/ssl/ca-bundle.pem, /etc/pki/tls/cacert.pem,
        // /etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem, /etc/ssl/cert.pem
        // — the last is macOS's), else every file of SSL_CERT_DIR or of
        // /etc/ssl/certs and /etc/pki/tls/certs. errc::unsupported when
        // none of them holds a certificate
        static expected<certificate_pool, error> system();

        // The same, the files read on the blocking pool, for a task
        static async::task<expected<certificate_pool, error>> async_system();

        // Every CERTIFICATE block of a PEM text that parses, as Go's
        // AppendCertsFromPEM: blocks of other types and certificates that
        // do not parse are passed over
        static certificate_pool from_pem(const string& text) {
            certificate_pool p;
            p.append_pem(text);
            return p;
        }

        // The certificates of a PEM file, as append_pem reads them (a block
        // that does not read passed over); the file system's error as
        // io::error (is_not_found for a file that is not there)
        [[nodiscard]] static expected<certificate_pool, io::error> from_file(const string& path);

        // The same, the file read on the blocking pool, for a task
        [[nodiscard]] static async::task<expected<certificate_pool, io::error>> async_from_file(string path);

        // The certificate, unless the pool has it already
        void add(const certificate& c) {
            auto d = sha256::of(c.raw());
            std::string sum(reinterpret_cast<const char*>(d.data()), d.size());
            if (!_d->sums.insert(sum).second) {
                return;
            }
            auto s = c.raw_subject();
            _d->by_subject[std::string(reinterpret_cast<const char*>(s.data()), s.size())].push_back(uint32_t(_d->certs.size()));
            _d->certs.push_back(c);
        }

        // The certificates of a PEM text added: how many parsed (Go's
        // AppendCertsFromPEM is true when one did)
        size_t append_pem(const string& text) {
            size_t n = 0;
            // block by block, so that one broken block leaves the others
            detail::walk_pem(text, [&](expected<encoding::pem, encoding::error>& block) {
                if (!block || block->type() != "CERTIFICATE" || !block->headers().empty()) {
                    return false;
                }
                if (auto c = certificate::parse(block->bytes().as_slice())) {
                    add(*c);
                    ++n;
                }
                return false;
            });
            return n;
        }

        size_t size() const noexcept {
            return _d->certs.size();
        }

        bool empty() const noexcept {
            return _d->certs.empty();
        }

        // The certificates in the order they were added
        const vector<certificate>& certificates() const noexcept {
            return _d->certs;
        }

        // Whether the pool has this very certificate (the same bytes)
        [[nodiscard]] bool contains(const certificate& c) const {
            auto d = sha256::of(c.raw());
            return _d->sums.count(std::string(reinterpret_cast<const char*>(d.data()), d.size())) != 0;
        }

        // A pool of its own with the same certificates
        certificate_pool clone() const {
            certificate_pool p;
            p._d->certs = _d->certs;
            p._d->by_subject = _d->by_subject;
            p._d->sums = _d->sums;
            return p;
        }

    private:
        friend struct detail::PoolAccess;
        friend class detail::ChainBuilder;

        tracked_ptr<detail::PoolData> _d;

        explicit certificate_pool(tracked_ptr<detail::PoolData> d) noexcept
        : _d(std::move(d)) {
        }
    };

    // What verify() checks against, each field with its default:
    //
    //   roots          the pool a chain must end in; nullopt: the system's
    //                  (certificate_pool::system(), loaded once)
    //   intermediates  certificates a chain may pass through; none
    //   dns_name       the DNS name the leaf must be for (verify_hostname);
    //                  empty: no name checked
    //   ip             the IP address the leaf must be for, 4 or 16 bytes
    //                  (verify_ip); empty: no address checked
    //   time           the instant every certificate must be valid at;
    //                  nullopt: time::now()
    //   key_usages     the extended key usages one of which the chain must
    //                  allow; empty: server_auth; ext_key_usage::any: any
    //
    //   cert.verify({.roots = pool, .dns_name = "example.com"})
    struct verify_options {
        optional<certificate_pool> roots;
        certificate_pool intermediates;
        string dns_name;
        slice<const byte> ip;
        optional<time::datetime> time;
        vector<ext_key_usage> key_usages;
    };

    namespace detail {
        struct PoolAccess {
            static const PoolData& data(const certificate_pool& p) noexcept {
                return *p._d;
            }

            static certificate_pool make(tracked_ptr<PoolData> d) noexcept {
                return certificate_pool(std::move(d));
            }

            static const tracked_ptr<PoolData>& ptr(const certificate_pool& p) noexcept {
                return p._d;
            }

            static const CertData& data(const certificate& c) noexcept {
                return *c._d;
            }
        };

        // The system's roots, loaded once and kept for the process: the
        // pool through a root_ptr made on purpose never to be destroyed,
        // so that nothing of it runs at exit
        struct SystemRoots {
            root_ptr<PoolData> pool;
            std::string error;
        };

        inline void load_pem_file(certificate_pool& pool, const string& path, bool& found) {
            auto bytes = io::read_file(path);
            if (!bytes) {
                return;
            }
            found = true;
            const char* p = reinterpret_cast<const char*>(bytes->data());
            pool.append_pem(string(std::string(p, bytes->size())));
        }

        inline SystemRoots* load_system_roots() {
            auto* s = new SystemRoots();
            certificate_pool pool;
            bool found = false;
            if (auto f = io::getenv(string("SSL_CERT_FILE")); f && !f->empty()) {
                load_pem_file(pool, *f, found);
            } else {
                static constexpr const char* files[] = {
                    "/etc/ssl/certs/ca-certificates.crt",
                    "/etc/pki/tls/certs/ca-bundle.crt",
                    "/etc/ssl/ca-bundle.pem",
                    "/etc/pki/tls/cacert.pem",
                    "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
                    "/etc/ssl/cert.pem"};
                for (auto f : files) {
                    load_pem_file(pool, string(f), found);
                    if (found) {
                        break;
                    }
                }
            }
            if (pool.empty()) {
                std::vector<std::string> dirs;
                if (auto d = io::getenv(string("SSL_CERT_DIR")); d && !d->empty()) {
                    std::string all(d->view());
                    for (size_t i = 0; i <= all.size();) {
                        size_t j = all.find(':', i);
                        j = j == std::string::npos ? all.size() : j;
                        if (j > i) {
                            dirs.push_back(all.substr(i, j - i));
                        }
                        i = j + 1;
                    }
                } else {
                    dirs = {"/etc/ssl/certs", "/etc/pki/tls/certs"};
                }
                for (auto& dir : dirs) {
                    auto entries = io::read_dir(string(dir));
                    if (!entries) {
                        continue;
                    }
                    for (auto& e : *entries) {
                        if (!e.is_directory()) {
                            load_pem_file(pool, e.path, found);
                        }
                    }
                }
            }
            if (pool.empty()) {
                s->error = "no system root certificates: SSL_CERT_FILE, the bundles of /etc/ssl and /etc/pki, SSL_CERT_DIR hold none";
            }
            s->pool = PoolAccess::ptr(pool);
            return s;
        }

        inline const SystemRoots& system_roots() {
            static const SystemRoots* roots = load_system_roots();   // never destroyed
            return *roots;
        }

        // The depth-first search of Go's buildChains: from the leaf, the
        // parents a pool has for the issuer's name (the one whose key id
        // matches first), roots before intermediates; for each, its
        // signature over the child, its own validity and CA constraints;
        // a root ends a chain, which the name constraints and the key
        // usages of the whole chain then check. The first chain that passes
        // is the answer; when none does, the reason is the one of a chain
        // that was complete (name constraints, then key usage), or else of
        // the first parent whose signature verified, as deep as it went,
        // or else of the first signature that did not
        class ChainBuilder {
        public:
            static constexpr size_t max_intermediates = 10;
            static constexpr size_t max_signature_checks = 100;

            ChainBuilder(const verify_options& o, const PoolData& roots, const PoolData& inter, int64_t now)
            : _o(o), _roots(roots), _inter(inter), _now(now) {
            }

            expected<x509::chain, error> run(const certificate& leaf) {
                const CertData& c = PoolAccess::data(leaf);
                if (auto r = check_critical(c); !r) {
                    return unexpected<error>(r.error());
                }
                if (auto r = check_time(c, _now); !r) {
                    return unexpected<error>(r.error());
                }
                if (!_o.dns_name.empty()) {
                    if (auto r = check_hostname(c, _o.dns_name.view()); !r) {
                        return unexpected<error>(r.error());
                    }
                }
                if (_o.ip.size() != 0) {
                    if (auto r = check_ip(c, address_of(_o.ip)); !r) {
                        return unexpected<error>(r.error());
                    }
                }
                _chain.push_back(leaf);
                _data.push_back(&c);
                auto sum = sha256::of(leaf.raw());
                if (_roots.sums.count(std::string(reinterpret_cast<const char*>(sum.data()), sum.size())) != 0) {
                    if (auto r = _complete(); !r) {
                        return unexpected<error>(r.error());
                    }
                    x509::chain one;
                    one.push_back(leaf);
                    return one;
                }
                auto r = _build();
                if (_found) {
                    return _result;
                }
                if (_final) {
                    return unexpected<error>(*_final);
                }
                return unexpected<error>(r.error());
            }

        private:
            const verify_options& _o;
            const PoolData& _roots;
            const PoolData& _inter;
            int64_t _now;
            vector<certificate> _chain;   // managed: it holds tracked pointers
            std::vector<const CertData*> _data;
            size_t _checks = 0;
            bool _found = false;
            x509::chain _result;
            optional<error> _final;          // why a complete chain failed
            optional<error> _eku;            // the key usage failure, reported after a constraint one

            // The checks of a whole chain
            expected<void, error> _complete() {
                ConstraintChecker nc;
                if (auto r = nc.check(_data); !r) {
                    return r;
                }
                return check_key_usages(_data, _o.key_usages);
            }

            // The parents of the last certificate in a pool, by the key ids
            // (the AKID matching the SKID first, then one of them missing,
            // then the rest)
            static void _parents(const PoolData& pool, const CertData& child, std::vector<uint32_t>& out) {
                out.clear();
                auto it = pool.by_subject.find(std::string(reinterpret_cast<const char*>(child.bytes_at(child.issuer_at)), child.issuer_size));
                if (it == pool.by_subject.end()) {
                    return;
                }
                for (int pass = 0; pass < 3; ++pass) {
                    for (uint32_t i : it->second) {
                        const CertData& p = PoolAccess::data(pool.certs[i]);
                        bool match = p.subject_key_id == child.authority_key_id;
                        bool one = p.subject_key_id.empty() != child.authority_key_id.empty();
                        int kind = match ? 0 : one ? 1 : 2;
                        if (kind == pass) {
                            out.push_back(i);
                        }
                    }
                }
            }

            // One level of the search: true when a chain was found (in
            // _result); else the error of this level, as Go's buildChains
            // gives it: the first failure of a parent whose signature over
            // the child verified (its own validity, its constraints, or the
            // levels above it), else the first signature that did not
            // verify, else an unknown authority
            expected<void, error> _build() {
                const CertData& child = *_data.back();
                optional<error> passed;      // the first failure of a parent whose signature over child verified
                optional<error> unverified;  // the first parent whose signature did not verify (or could not be checked)
                std::vector<uint32_t> list;
                for (int kind = 0; kind < 2; ++kind) {
                    const PoolData& pool = kind == 0 ? _roots : _inter;
                    _parents(pool, child, list);
                    for (uint32_t i : list) {
                        if (++_checks > max_signature_checks) {
                            return unexpected<error>(reject(reason::too_many_intermediates, "more than a hundred signatures tried while building the chain"));
                        }
                        const certificate& cand = pool.certs[i];
                        const CertData& p = PoolAccess::data(cand);
                        if (already_in_chain(p, _data)) {
                            continue;
                        }
                        if (auto s = check_signature(child, p); !s) {
                            if (!unverified) {
                                unverified = s.error();
                            }
                            continue;
                        }
                        auto r = _consider(cand, p, kind == 0);
                        if (_found) {
                            return {};
                        }
                        if (!r && r.error().reason() == reason::too_many_intermediates && _checks > max_signature_checks) {
                            return r;
                        }
                        if (!r && !passed) {
                            passed = r.error();
                        }
                    }
                }
                // a parent that signed the child says more than one that did
                // not: under a root's old key and its new one of the same
                // name, the old one is tried first and fails its signature,
                // while the path through the new one fails for its own reason
                if (passed) {
                    return unexpected<error>(*passed);
                }
                if (unverified) {
                    return unexpected<error>(*unverified);
                }
                return unexpected<error>(reject(reason::unknown_authority, "the certificate " + describe(child) + " is signed by an unknown authority"));
            }

            // A parent whose signature over the last certificate verified
            expected<void, error> _consider(const certificate& cand, const CertData& p, bool root) {
                if (auto r = check_critical(p); !r) {
                    return r;
                }
                if (auto r = check_time(p, _now); !r) {
                    return r;
                }
                if (!root && (!p.basic_constraints_valid || !p.is_ca)) {
                    return unexpected<error>(reject(reason::not_a_ca, "the intermediate " + describe(p) + " is not a CA (basicConstraints)"));
                }
                // the intermediates below the candidate that count (RFC 5280
                // §4.2.1.9, §6.1.4 (l)): a self-issued one, as a key's
                // rollover makes, does not; Go counts it
                size_t intermediates = 0;
                for (size_t i = 1; i < _data.size(); ++i) {
                    intermediates += self_issued(*_data[i]) ? 0 : 1;
                }
                if (p.basic_constraints_valid && p.max_path_len >= 0 && int64_t(intermediates) > p.max_path_len) {
                    return unexpected<error>(reject(reason::path_length, "the path length constraint of " + describe(p) + " is exceeded"));
                }
                if (!root && _data.size() > max_intermediates) {
                    return unexpected<error>(reject(reason::too_many_intermediates, "a chain of more than ten intermediates"));
                }
                _chain.push_back(cand);
                _data.push_back(&p);
                expected<void, error> r;
                if (root) {
                    r = _complete();
                    if (r) {
                        _found = true;
                        _result = x509::chain();
                        for (auto& c : _chain) {
                            _result.push_back(c);
                        }
                    } else if (r.error().reason() == reason::name_constraints || r.error().reason() == reason::too_many_constraints) {
                        _final = r.error();
                    } else if (!_final) {
                        _final = r.error();
                    }
                } else {
                    r = _build();
                }
                _chain.pop_back();
                _data.pop_back();
                return r;
            }
        };
    }

    inline expected<certificate_pool, error> certificate_pool::system() {
        const auto& s = detail::system_roots();
        if (!s.error.empty()) {
            return unexpected<error>(error(errc::unsupported, string("sgcl::crypto::x509: " + s.error)));
        }
        return detail::PoolAccess::make(s.pool.ptr()).clone();
    }

    inline async::task<expected<certificate_pool, error>> certificate_pool::async_system() {
        co_return co_await async::spawn_blocking([] { return system(); });
    }

    inline expected<certificate_pool, io::error> certificate_pool::from_file(const string& path) {
        auto bytes = io::read_file(path);
        if (!bytes) {
            return unexpected<io::error>(bytes.error());
        }
        const char* p = reinterpret_cast<const char*>(bytes->data());
        return from_pem(string(std::string(p, bytes->size())));
    }

    inline async::task<expected<certificate_pool, io::error>> certificate_pool::async_from_file(string path) {   // by value: a task is lazy
        co_return co_await async::spawn_blocking([path] { return from_file(path); });
    }

    inline expected<x509::chain, error> certificate::verify() const {
        return verify(verify_options{});
    }

    inline expected<x509::chain, error> certificate::verify(const verify_options& o) const {
        const detail::PoolData* roots;
        if (o.roots) {
            roots = &detail::PoolAccess::data(*o.roots);
        } else {
            const auto& s = detail::system_roots();
            if (!s.error.empty()) {
                return unexpected<error>(error(errc::unsupported, string("sgcl::crypto::x509: " + s.error)));
            }
            roots = s.pool.ptr().get();
        }
        int64_t now = o.time ? o.time->unix() : time::now().unix();
        detail::ChainBuilder b(o, *roots, detail::PoolAccess::data(o.intermediates), now);
        return b.run(*this);
    }
}
