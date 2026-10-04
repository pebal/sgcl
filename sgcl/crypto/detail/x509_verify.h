//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "x509_cert.h"
#include "x509_hostname.h"
#include "../hash_id.h"
#include "../../core/detail/bytes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// The checks of a chain that look at one certificate or at the chain as a
// whole, over the parsed data: the signature of a child by its parent
// (with RFC 5280 §4.2.1.9's rule that only a CA's key signs certificates),
// the name constraints of every CA over the names below it (§4.2.1.10),
// the extended key usages nested down the chain, and the host name and the
// IP address of the leaf (RFC 6125). The chain building is x509.h's.
namespace sgcl::crypto::x509::detail {
    inline error reject(reason why, const std::string& what) noexcept {
        return error(why, string("sgcl::crypto::x509: " + what));
    }

    // "CN=GTS CA 1C3,O=Google Trust Services LLC,C=US", or the serial when
    // the subject is empty: how a message names a certificate
    SGCL_INLINE_HOT std::string describe(const CertData& c) noexcept {
        std::string s(c.subject.to_string().view());
        if (s.empty()) {
            s = "serial:" + hex_text(reinterpret_cast<const unsigned char*>(c.serial.data()), c.serial.size());
        }
        return "\"" + s + "\"";
    }

    // The hash a signature algorithm signs, and whether the algorithm is
    // one verified here; MD2, MD5 and SHA-1 refused as insecure
    inline expected<void, error> signature_hash(const CertData& child, hash_id& id) noexcept {
        switch (child.sig_algorithm) {
            case signature_algorithm::sha256_with_rsa:
            case signature_algorithm::sha256_with_rsa_pss:
            case signature_algorithm::ecdsa_with_sha256:
                id = hash_id::sha256;
                return {};
            case signature_algorithm::sha384_with_rsa:
            case signature_algorithm::sha384_with_rsa_pss:
            case signature_algorithm::ecdsa_with_sha384:
                id = hash_id::sha384;
                return {};
            case signature_algorithm::sha512_with_rsa:
            case signature_algorithm::sha512_with_rsa_pss:
            case signature_algorithm::ecdsa_with_sha512:
                id = hash_id::sha512;
                return {};
            case signature_algorithm::ed25519:
                return {};
            case signature_algorithm::md2_with_rsa:
            case signature_algorithm::md5_with_rsa:
            case signature_algorithm::sha1_with_rsa:
            case signature_algorithm::ecdsa_with_sha1:
                return unexpected<error>(reject(reason::insecure_algorithm, "the certificate " + describe(child) + " is signed with MD2, MD5 or SHA-1 (" + std::string(child.sig_algorithm_oid.view()) + ")"));
            case signature_algorithm::unknown:
                break;
        }
        return unexpected<error>(reject(reason::unsupported_algorithm, "the certificate " + describe(child) + " is signed with an algorithm not verified here (" + std::string(child.sig_algorithm_oid.view()) + ")"));
    }

    // Whether parent's key signed child: parent a CA (a version 3
    // certificate needs basicConstraints with cA; a keyUsage, where there
    // is one, needs keyCertSign), the algorithm one of the list, the key of
    // that algorithm, the signature valid. Never an exception: every input
    // here comes from a certificate
    inline expected<void, error> check_signature(const CertData& child, const CertData& parent) noexcept {
        if ((parent.version == 3 && !parent.basic_constraints_valid) || (parent.basic_constraints_valid && !parent.is_ca)) {
            return unexpected<error>(reject(reason::not_a_ca, "the certificate " + describe(parent) + " is not a CA (basicConstraints), and cannot sign " + describe(child)));
        }
        if (parent.has_key_usage && (parent.key_usage_bits & uint16_t(key_usage::cert_sign)) == 0) {
            return unexpected<error>(reject(reason::missing_cert_sign, "the key usage of " + describe(parent) + " lacks keyCertSign"));
        }
        hash_id id = hash_id::sha256;
        if (auto h = signature_hash(child, id); !h) {
            return h;
        }
        const public_key& key = parent.key;
        if (!key.has_value()) {
            return unexpected<error>(reject(reason::unsupported_algorithm, "the public key of " + describe(parent) + " is not one verified here (" + std::string(key.algorithm().view()) + ")"
                                                                            + (parent.key_error.empty() ? std::string() : ": " + std::string(parent.key_error.view()))));
        }
        slice<const byte> tbs = child.range(child.tbs_at, child.tbs_size);
        slice<const byte> sig = child.signature.as_slice();
        bool ok = false;
        bool matched = true;
        switch (child.sig_algorithm) {
            case signature_algorithm::sha256_with_rsa:
            case signature_algorithm::sha384_with_rsa:
            case signature_algorithm::sha512_with_rsa:
                if ((matched = key.kind() == key_kind::rsa)) {
                    ok = key.rsa().verify_digest(id, digest(id, tbs), sig);
                }
                break;
            case signature_algorithm::sha256_with_rsa_pss:
            case signature_algorithm::sha384_with_rsa_pss:
            case signature_algorithm::sha512_with_rsa_pss:
                if ((matched = key.kind() == key_kind::rsa)) {
                    // the salt as long as the digest, what the parameters say (pss_of takes no other)
                    ok = key.rsa().verify_digest_pss(id, digest(id, tbs), sig, digest_size(id));
                }
                break;
            case signature_algorithm::ecdsa_with_sha256:
            case signature_algorithm::ecdsa_with_sha384:
            case signature_algorithm::ecdsa_with_sha512:
                if (key.kind() == key_kind::p256) {
                    ok = key.p256().verify_digest(digest(id, tbs), sig);
                } else if (key.kind() == key_kind::p384) {
                    ok = key.p384().verify_digest(digest(id, tbs), sig);
                } else {
                    matched = false;
                }
                break;
            case signature_algorithm::ed25519:
                if ((matched = key.kind() == key_kind::ed25519)) {
                    ok = key.ed25519().verify(tbs, sig);
                }
                break;
            default:
                matched = false;
                break;
        }
        if (!matched) {
            return unexpected<error>(reject(reason::invalid_signature, "the signature of " + describe(child) + " is of another algorithm than the key of " + describe(parent)));
        }
        if (!ok || !child.signature_whole) {
            return unexpected<error>(reject(reason::invalid_signature, "the signature of " + describe(child) + " does not verify under the key of " + describe(parent)));
        }
        return {};
    }

    // The validity of one certificate at a time (seconds since 1970)
    SGCL_INLINE_HOT expected<void, error> check_time(const CertData& c, int64_t now) noexcept {
        if (now < c.not_before) {
            return unexpected<error>(reject(reason::not_yet_valid, "the certificate " + describe(c) + " is not valid yet"));
        }
        if (now > c.not_after) {
            return unexpected<error>(reject(reason::expired, "the certificate " + describe(c) + " has expired"));
        }
        return {};
    }

    SGCL_INLINE_HOT expected<void, error> check_critical(const CertData& c) noexcept {
        if (!c.unhandled_critical.empty()) {
            return unexpected<error>(reject(reason::unhandled_critical_extension, "the certificate " + describe(c) + " has a critical extension not handled here (" + std::string(c.unhandled_critical[0].view()) + ")"));
        }
        return {};
    }

    // The leaf against the DNS name asked. A name written as an IP address
    // is never matched against dNSNames (RFC 6125 Appendix B.2): addresses
    // are verified as bytes, against the iPAddresses
    inline expected<void, error> check_hostname(const CertData& c, std::string_view host) noexcept {
        std::string_view bare = host;
        if (bare.size() >= 2 && bare.front() == '[' && bare.back() == ']') {
            bare = bare.substr(1, bare.size() - 2);
        }
        if (x509_names::looks_like_ipv4(host) || x509_names::looks_like_ipv6(bare)) {
            return unexpected<error>(reject(reason::hostname_mismatch, "\"" + std::string(host) + "\" is an IP address, verified by its bytes (verify_ip, verify_options::ip), never against DNS names"));
        }
        for (auto& d : c.dns_names) {
            if (x509_names::host_matches(d.view(), host)) {
                return {};
            }
        }
        // the names, for the message only
        std::string h(host);
        std::string list;
        for (auto& d : c.dns_names) {
            if (!list.empty()) {
                list += ", ";
            }
            list.append(d.data(), d.size());
        }
        if (list.empty()) {
            return unexpected<error>(reject(reason::hostname_mismatch, "the certificate is not valid for any DNS name, but wanted to match " + h));
        }
        return unexpected<error>(reject(reason::hostname_mismatch, "the certificate is valid for " + list + ", not " + h));
    }

    // An address given by the program: 4 or 16 bytes, an IPv4-mapped IPv6
    // address (::ffff:a.b.c.d, what a 16-byte form of an IPv4 address is)
    // taken as its 4 bytes, since a certificate never holds that form
    inline ip_address address_of(const slice<const byte>& ip) {
        if (ip.size() != 4 && ip.size() != 16) {
            throw invalid_argument("sgcl::crypto::x509: an IP address is 4 or 16 bytes");
        }
        static constexpr unsigned char mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
        ip_address a;
        const unsigned char* p = reinterpret_cast<const unsigned char*>(ip.data());
        if (ip.size() == 16 && std::memcmp(p, mapped, 12) == 0) {
            std::memcpy(a.bytes.data(), p + 12, 4);
            a.size = 4;
        } else {
            sgcl::detail::copy_bytes(a.bytes.data(), p, ip.size());
            a.size = uint8_t(ip.size());
        }
        return a;
    }

    inline expected<void, error> check_ip(const CertData& c, const ip_address& a) noexcept {
        for (auto& x : c.ip_addresses) {
            if (x == a) {
                return {};
            }
        }
        return unexpected<error>(reject(reason::hostname_mismatch, c.ip_addresses.empty() ? "the certificate holds no IP address" : "the certificate is not valid for the IP address asked"));
    }

    // Name constraints over a chain (leaf first): the names of every
    // certificate with a SAN checked against the constraints of every
    // certificate above it, as Go checks them — a permitted list of a kind
    // lets through only what one of its entries covers, an excluded one
    // stops what any covers; a name of a kind no list names passes. Common
    // names are not names here (Go and the CA/Browser Forum's profile)
    class ConstraintChecker {
    public:
        static constexpr size_t max_comparisons = 1000000;

        expected<void, error> check(const std::vector<const CertData*>& chain) noexcept {
            for (size_t i = 0; i < chain.size(); ++i) {
                const CertData& c = *chain[i];
                if (!c.has_san) {
                    continue;
                }
                bool above = false;
                for (size_t j = i + 1; j < chain.size(); ++j) {
                    above = above || chain[j]->has_name_constraints;
                }
                if (!above) {
                    continue;
                }
                // the names, parsed once for every CA above
                std::vector<std::string> uri_hosts;
                for (auto& u : c.uris) {
                    std::string_view host;
                    std::string h;
                    if (!x509_names::uri_host(u.view(), host) || !x509_names::uri_constraint_host(host, h)) {
                        return unexpected<error>(reject(reason::name_constraints, "the URI \"" + std::string(u.view()) + "\" of " + describe(c) + " has no domain name constraints can be checked against"));
                    }
                    uri_hosts.push_back(std::move(h));
                }
                std::vector<std::pair<std::string, std::string>> mailboxes;
                for (auto& e : c.email_addresses) {
                    std::string local, domain;
                    if (!x509_names::parse_mailbox(e.view(), local, domain)) {
                        return unexpected<error>(reject(reason::name_constraints, "the rfc822Name \"" + std::string(e.view()) + "\" of " + describe(c) + " cannot be read"));
                    }
                    mailboxes.emplace_back(std::move(local), x509_names::to_lower(domain));
                }
                for (auto& d : c.dns_names) {
                    if (!x509_names::domain_valid(d.view(), false)) {
                        return unexpected<error>(reject(reason::name_constraints, "the dNSName \"" + std::string(d.view()) + "\" of " + describe(c) + " cannot be read"));
                    }
                }
                for (size_t j = i + 1; j < chain.size(); ++j) {
                    const CertData& ca = *chain[j];
                    if (!ca.has_name_constraints) {
                        continue;
                    }
                    for (auto& ip : c.ip_addresses) {
                        if (auto r = _ip(ca, c, ip); !r) {
                            return r;
                        }
                    }
                    for (auto& d : c.dns_names) {
                        if (auto r = _dns(ca, c, d.view(), ca.permitted_dns, ca.excluded_dns, "DNS name"); !r) {
                            return r;
                        }
                    }
                    for (auto& h : uri_hosts) {
                        if (auto r = _dns(ca, c, h, ca.permitted_uri, ca.excluded_uri, "URI host"); !r) {
                            return r;
                        }
                    }
                    for (auto& [local, domain] : mailboxes) {
                        if (auto r = _email(ca, c, local, domain); !r) {
                            return r;
                        }
                    }
                }
            }
            return {};
        }

    private:
        size_t _count = 0;

        SGCL_INLINE_HOT expected<void, error> _tick(const CertData& ca) noexcept {
            if (++_count > max_comparisons) {
                return unexpected<error>(reject(reason::too_many_constraints, "the name constraints of " + describe(ca) + " take more than a million comparisons"));
            }
            return {};
        }

        static error _fail(const CertData& ca, const CertData& c, const char* kind, const std::string& name, bool excluded) noexcept {
            return reject(reason::name_constraints, std::string(kind) + " \"" + name + "\" of " + describe(c) + (excluded ? " is excluded by the name constraints of " : " is not permitted by the name constraints of ") + describe(ca));
        }

        expected<void, error> _ip(const CertData& ca, const CertData& c, const ip_address& ip) noexcept {
            if (!ca.permitted_ip.empty()) {
                bool found = false;
                for (auto& r : ca.permitted_ip) {
                    if (auto t = _tick(ca); !t) {
                        return t;
                    }
                    if (r.contains(ip)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return unexpected<error>(_fail(ca, c, "the IP address", "", false));
                }
            }
            for (auto& r : ca.excluded_ip) {
                if (auto t = _tick(ca); !t) {
                    return t;
                }
                if (r.contains(ip)) {
                    return unexpected<error>(_fail(ca, c, "the IP address", "", true));
                }
            }
            return {};
        }

        expected<void, error> _dns(const CertData& ca, const CertData& c, std::string_view name, const vector<string>& permitted, const vector<string>& excluded, const char* kind) noexcept {
            if (!permitted.empty()) {
                bool found = false;
                for (auto& p : permitted) {
                    if (auto t = _tick(ca); !t) {
                        return t;
                    }
                    if (x509_names::dns_has_suffix(p.view(), name)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return unexpected<error>(_fail(ca, c, kind, std::string(name), false));
                }
            }
            for (auto& x : excluded) {
                if (auto t = _tick(ca); !t) {
                    return t;
                }
                if (x509_names::dns_has_suffix(x.view(), name) || x509_names::excluded_wildcard(x.view(), name)) {
                    return unexpected<error>(_fail(ca, c, kind, std::string(name), true));
                }
            }
            return {};
        }

        // A mailbox against the email constraints: a constraint with '@'
        // is one mailbox (the local part exact, the domain folded), any
        // other a domain under the rules of DNS names
        SGCL_INLINE_HOT bool _email_match(const string& constraint, const std::string& local, const std::string& domain) noexcept {
            std::string_view s = constraint.view();
            if (s.find('@') != std::string_view::npos) {
                std::string cl, cd;
                return x509_names::parse_mailbox(s, cl, cd) && cl == local && x509_names::to_lower(cd) == domain;
            }
            return x509_names::dns_has_suffix(s, domain);
        }

        expected<void, error> _email(const CertData& ca, const CertData& c, const std::string& local, const std::string& domain) noexcept {
            std::string mailbox = local + "@" + domain;
            if (!ca.permitted_email.empty()) {
                bool found = false;
                for (auto& p : ca.permitted_email) {
                    if (auto t = _tick(ca); !t) {
                        return t;
                    }
                    if (_email_match(p, local, domain)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return unexpected<error>(_fail(ca, c, "the email address", mailbox, false));
                }
            }
            for (auto& x : ca.excluded_email) {
                if (auto t = _tick(ca); !t) {
                    return t;
                }
                if (_email_match(x, local, domain) || (x.view().find('@') == std::string_view::npos && x509_names::excluded_wildcard(x.view(), domain))) {
                    return unexpected<error>(_fail(ca, c, "the email address", mailbox, true));
                }
            }
            return {};
        }
    };

    // The extended key usages asked (none: serverAuth; any: no check)
    // against the chain, nested as Go nests them: walking down from the
    // root, a certificate with a list of usages (and without anyExtendedKeyUsage)
    // crosses out every usage asked it does not hold; the chain passes
    // while one usage asked is left
    inline expected<void, error> check_key_usages(const std::vector<const CertData*>& chain, const vector<ext_key_usage>& asked) noexcept {
        std::vector<ext_key_usage> usages;
        for (auto u : asked) {
            if (u == ext_key_usage::any) {
                return {};
            }
            usages.push_back(u);
        }
        if (usages.empty()) {
            usages.push_back(ext_key_usage::server_auth);
        }
        std::vector<bool> left(usages.size(), true);
        size_t remaining = usages.size();
        for (size_t i = chain.size(); i-- > 0;) {
            const CertData& c = *chain[i];
            if (c.ext_key_usages.empty() && c.unknown_ext_key_usages.empty()) {
                continue;
            }
            bool any = false;
            for (auto u : c.ext_key_usages) {
                any = any || u == ext_key_usage::any;
            }
            if (any) {
                continue;
            }
            for (size_t k = 0; k < usages.size(); ++k) {
                if (!left[k]) {
                    continue;
                }
                bool has = false;
                for (auto u : c.ext_key_usages) {
                    has = has || u == usages[k];
                }
                if (!has) {
                    left[k] = false;
                    if (--remaining == 0) {
                        return unexpected<error>(reject(reason::incompatible_usage, "the extended key usage of " + describe(c) + " allows none of the usages asked"));
                    }
                }
            }
        }
        return {};
    }

    // Whether the certificate is self-issued: its issuer and subject the
    // same bytes (RFC 5280 §6.1: the same name, as the chain compares them)
    SGCL_INLINE_HOT bool self_issued(const CertData& c) noexcept {
        return c.issuer_size == c.subject_size && std::memcmp(c.bytes_at(c.issuer_at), c.bytes_at(c.subject_at), c.subject_size) == 0;
    }

    // Whether candidate is already in the chain: the same subject, key and
    // SAN (a loop of cross-signatures, not only the same bytes)
    inline bool already_in_chain(const CertData& candidate, const std::vector<const CertData*>& chain) noexcept {
        auto same = [](const CertData& a, size_t aat, size_t an, const CertData& b, size_t bat, size_t bn) {
            return an == bn && std::memcmp(a.bytes_at(aat), b.bytes_at(bat), an) == 0;
        };
        for (const CertData* c : chain) {
            if (!same(candidate, candidate.subject_at, candidate.subject_size, *c, c->subject_at, c->subject_size)
                || !same(candidate, candidate.spki_at, candidate.spki_size, *c, c->spki_at, c->spki_size)) {
                continue;
            }
            if (!candidate.has_san && !c->has_san) {
                return true;
            }
            if (candidate.has_san && c->has_san && same(candidate, candidate.san_at, candidate.san_size, *c, c->san_at, c->san_size)) {
                return true;
            }
        }
        return false;
    }
}
