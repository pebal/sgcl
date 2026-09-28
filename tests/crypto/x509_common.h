//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of X.509 share: certificates made by OpenSSL's libcrypto in
// the test itself (a builder over X509_new with OpenSSL's own extension
// syntax, signed with RSA PKCS #1 v1.5 or PSS, ECDSA on P-256 and P-384,
// Ed25519, over any digest), X509_verify_cert as the oracle of a chain with
// its error mapped to x509::reason, a small DER writer for certificates
// OpenSSL will not make (broken times, bad encodings, limits), and the data
// files of tests/crypto/data/x509 written from Go's own test tables.
#pragma once

#include "ecc_common.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace x509_test {
    using namespace ecc_test;
    namespace x509 = sgcl::crypto::x509;
    using sgcl::string;
    using x509::reason;

    // A fixed "now" for every chain made here: 2027-01-15T08:00:00Z
    inline constexpr int64_t now = 1800000000;

    // Where the data files are: next to this header
    inline std::string data_dir() {
        std::string f = __FILE__;
        return f.substr(0, f.rfind('/')) + "/data/x509";
    }

    inline std::string read_file(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }

    //--------------------------------------------------------------------
    // DER by hand
    //--------------------------------------------------------------------
    inline bytes_t der(unsigned char tag, const bytes_t& content) {
        bytes_t out{tag};
        size_t n = content.size();
        if (n < 0x80) {
            out.push_back((unsigned char)n);
        } else {
            bytes_t len;
            for (size_t l = n; l; l >>= 8) {
                len.insert(len.begin(), (unsigned char)l);
            }
            out.push_back((unsigned char)(0x80 | len.size()));
            out.insert(out.end(), len.begin(), len.end());
        }
        out.insert(out.end(), content.begin(), content.end());
        return out;
    }

    inline bytes_t cat(std::initializer_list<bytes_t> parts) {
        bytes_t out;
        for (auto& p : parts) {
            out.insert(out.end(), p.begin(), p.end());
        }
        return out;
    }

    inline bytes_t seq(std::initializer_list<bytes_t> parts) {
        return der(0x30, cat(parts));
    }

    inline bytes_t oid(const std::string& dotted) {
        std::vector<uint64_t> arcs;
        std::stringstream ss(dotted);
        std::string part;
        while (std::getline(ss, part, '.')) {
            arcs.push_back(std::stoull(part));
        }
        bytes_t c;
        auto put = [&](uint64_t v) {
            bytes_t b{(unsigned char)(v & 0x7f)};
            for (v >>= 7; v; v >>= 7) {
                b.insert(b.begin(), (unsigned char)(0x80 | (v & 0x7f)));
            }
            c.insert(c.end(), b.begin(), b.end());
        };
        put(arcs[0] * 40 + arcs[1]);
        for (size_t i = 2; i < arcs.size(); ++i) {
            put(arcs[i]);
        }
        return der(0x06, c);
    }

    inline bytes_t utf8_der(const std::string& s) {
        return der(0x0c, bytes_t(s.begin(), s.end()));
    }

    // A Name of one attribute per RDN, each (oid, DER value)
    inline bytes_t name_of(std::initializer_list<std::pair<std::string, bytes_t>> attrs) {
        bytes_t rdns;
        for (auto& [o, v] : attrs) {
            auto r = der(0x31, seq({oid(o), v}));
            rdns.insert(rdns.end(), r.begin(), r.end());
        }
        return der(0x30, rdns);
    }

    // An extension of any OID with the DER value given
    inline void raw_ext(X509* x, const std::string& o, bool critical, const bytes_t& value) {
        ASN1_OBJECT* obj = OBJ_txt2obj(o.c_str(), 1);
        ASN1_OCTET_STRING* v = ASN1_OCTET_STRING_new();
        ASN1_OCTET_STRING_set(v, value.data(), int(value.size()));
        X509_EXTENSION* e = X509_EXTENSION_create_by_OBJ(nullptr, obj, critical ? 1 : 0, v);
        X509_add_ext(x, e, -1);
        X509_EXTENSION_free(e);
        ASN1_OCTET_STRING_free(v);
        ASN1_OBJECT_free(obj);
    }

    //--------------------------------------------------------------------
    // Keys
    //--------------------------------------------------------------------
    enum class key_type { rsa2048, rsa1024, p256, p384, ed25519 };

    inline const char* key_name(key_type k) {
        switch (k) {
            case key_type::rsa2048: return "RSA-2048";
            case key_type::rsa1024: return "RSA-1024";
            case key_type::p256: return "P-256";
            case key_type::p384: return "P-384";
            case key_type::ed25519: return "Ed25519";
        }
        return "?";
    }

    struct pkey_free {
        void operator()(EVP_PKEY* k) const { EVP_PKEY_free(k); }
    };
    using pkey_ptr = std::shared_ptr<EVP_PKEY>;

    // A new key; RSA keys come from a small pool made once per run (a
    // 2048-bit key costs tens of milliseconds)
    inline pkey_ptr new_key(key_type t) {
        EVP_PKEY* k = nullptr;
        switch (t) {
            case key_type::rsa2048:
            case key_type::rsa1024: {
                static std::mutex m;
                static std::map<int, std::vector<pkey_ptr>> pool;
                static std::map<int, size_t> next;
                int bits = t == key_type::rsa2048 ? 2048 : 1024;
                std::lock_guard<std::mutex> g(m);
                auto& v = pool[bits];
                if (v.size() < 8) {
                    v.push_back(pkey_ptr(EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", size_t(bits)), EVP_PKEY_free));
                    return v.back();
                }
                return v[next[bits]++ % v.size()];
            }
            case key_type::p256:
                k = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
                break;
            case key_type::p384:
                k = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-384");
                break;
            case key_type::ed25519:
                k = EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519");
                break;
        }
        return pkey_ptr(k, EVP_PKEY_free);
    }

    //--------------------------------------------------------------------
    // Certificates made by OpenSSL
    //--------------------------------------------------------------------
    struct x509_free {
        void operator()(X509* x) const { X509_free(x); }
    };
    using x509_ptr = std::shared_ptr<X509>;

    // A certificate made and signed: its OpenSSL object, its DER and its key
    struct made {
        x509_ptr cert;
        bytes_t der;
        pkey_ptr key;
    };

    enum class sig { sha256, sha384, sha512, sha1, md5, pss256, pss384, pss512 };

    // What a certificate is to be; the extensions in OpenSSL's syntax
    // (X509V3_EXT_conf_nid), empty for none
    struct spec {
        std::string cn = "Test Leaf";
        std::string o;
        key_type key = key_type::p256;
        pkey_ptr use_key;                 // a key of its own, else a new one
        int64_t not_before = now - 86400;
        int64_t not_after = now + 86400 * 365;
        long version = 3;                 // 1 writes no extensions
        long serial = 1;
        std::string basic_constraints;    // "critical,CA:TRUE,pathlen:0"
        std::string key_usage;            // "critical,keyCertSign,cRLSign"
        std::string ext_key_usage;        // "serverAuth,clientAuth"
        std::string san;                  // "DNS:a.example.com,IP:10.0.0.1"
        std::string name_constraints;     // "critical,permitted;DNS:.example.com"
        std::vector<std::string> policies; // policy OIDs, written by hand
        bool key_ids = true;              // subjectKeyIdentifier and authorityKeyIdentifier
        bool unknown_critical = false;    // an extension of an unknown OID, critical
        sig algorithm = sig::sha256;
        bool break_signature = false;     // one bit of the signature flipped
        int pss_salt = -1;                // PSS: the TBS signed again with a salt of this length, the parameters left naming the digest's
    };

    inline const EVP_MD* md_of(sig s) {
        switch (s) {
            case sig::sha256: case sig::pss256: return EVP_sha256();
            case sig::sha384: case sig::pss384: return EVP_sha384();
            case sig::sha512: case sig::pss512: return EVP_sha512();
            case sig::sha1: return EVP_sha1();
            case sig::md5: return EVP_md5();
        }
        return EVP_sha256();
    }

    inline bool is_pss(sig s) {
        return s == sig::pss256 || s == sig::pss384 || s == sig::pss512;
    }

    inline void add_ext(X509* x, X509V3_CTX* ctx, int nid, const std::string& value) {
        if (value.empty()) {
            return;
        }
        X509_EXTENSION* e = X509V3_EXT_conf_nid(nullptr, ctx, nid, value.c_str());
        if (!e) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("OpenSSL refused the extension " + value);
        }
        X509_add_ext(x, e, -1);
        X509_EXTENSION_free(e);
    }

    // A certificate of the spec signed by issuer (nullptr: by itself)
    inline made make_cert(const spec& s, const made* issuer = nullptr) {
        made m;
        m.key = s.use_key ? s.use_key : new_key(s.key);
        X509* x = X509_new();
        m.cert = x509_ptr(x, X509_free);
        X509_set_version(x, s.version - 1);
        ASN1_INTEGER_set_int64(X509_get_serialNumber(x), s.serial);
        X509_NAME* name = X509_get_subject_name(x);
        if (!s.o.empty()) {
            X509_NAME_add_entry_by_txt(name, "O", MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(s.o.c_str()), -1, -1, 0);
        }
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(s.cn.c_str()), -1, -1, 0);
        X509_set_issuer_name(x, issuer ? X509_get_subject_name(issuer->cert.get()) : name);
        ASN1_TIME_set(X509_getm_notBefore(x), time_t(s.not_before));
        ASN1_TIME_set(X509_getm_notAfter(x), time_t(s.not_after));
        X509_set_pubkey(x, m.key.get());
        if (s.version == 3) {
            X509V3_CTX ctx;
            X509V3_set_ctx_nodb(&ctx);
            X509V3_set_ctx(&ctx, issuer ? issuer->cert.get() : x, x, nullptr, nullptr, 0);
            add_ext(x, &ctx, NID_basic_constraints, s.basic_constraints);
            add_ext(x, &ctx, NID_key_usage, s.key_usage);
            add_ext(x, &ctx, NID_ext_key_usage, s.ext_key_usage);
            add_ext(x, &ctx, NID_subject_alt_name, s.san);
            add_ext(x, &ctx, NID_name_constraints, s.name_constraints);
            if (!s.policies.empty()) {
                bytes_t list;
                for (auto& p : s.policies) {
                    auto one = seq({oid(p)});
                    list.insert(list.end(), one.begin(), one.end());
                }
                raw_ext(x, "2.5.29.32", false, der(0x30, list));
            }
            if (s.key_ids) {
                add_ext(x, &ctx, NID_subject_key_identifier, "hash");
                add_ext(x, &ctx, NID_authority_key_identifier, "keyid");
            }
            if (s.unknown_critical) {
                raw_ext(x, "1.3.6.1.4.1.55555.1.2", true, {0x05, 0x00});
            }
        }
        EVP_PKEY* signer = issuer ? issuer->key.get() : m.key.get();
        EVP_MD_CTX* md = EVP_MD_CTX_new();
        EVP_PKEY_CTX* pctx = nullptr;
        bool ed = EVP_PKEY_get_id(signer) == EVP_PKEY_ED25519;
        if (EVP_DigestSignInit(md, &pctx, ed ? nullptr : md_of(s.algorithm), nullptr, signer) != 1) {
            throw std::runtime_error("EVP_DigestSignInit");
        }
        if (!ed && is_pss(s.algorithm)) {
            EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING);
            EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, RSA_PSS_SALTLEN_DIGEST);
            EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, md_of(s.algorithm));
        }
        if (X509_sign_ctx(x, md) <= 0) {
            ERR_print_errors_fp(stderr);
            throw std::runtime_error("X509_sign_ctx");
        }
        EVP_MD_CTX_free(md);
        unsigned char* p = nullptr;
        int n = i2d_X509(x, &p);
        m.der.assign(p, p + n);
        OPENSSL_free(p);
        if (s.pss_salt >= 0) {
            unsigned char* t = nullptr;
            int tn = i2d_re_X509_tbs(x, &t);
            bytes_t tbs(t, t + tn);
            OPENSSL_free(t);
            EVP_MD_CTX* again = EVP_MD_CTX_new();
            EVP_PKEY_CTX* actx = nullptr;
            if (EVP_DigestSignInit(again, &actx, md_of(s.algorithm), nullptr, signer) != 1) {
                throw std::runtime_error("EVP_DigestSignInit");
            }
            EVP_PKEY_CTX_set_rsa_padding(actx, RSA_PKCS1_PSS_PADDING);
            EVP_PKEY_CTX_set_rsa_pss_saltlen(actx, s.pss_salt);
            EVP_PKEY_CTX_set_rsa_mgf1_md(actx, md_of(s.algorithm));
            size_t sn = 0;
            EVP_DigestSign(again, nullptr, &sn, tbs.data(), tbs.size());
            bytes_t sig(sn);
            if (EVP_DigestSign(again, sig.data(), &sn, tbs.data(), tbs.size()) != 1) {
                throw std::runtime_error("EVP_DigestSign");
            }
            EVP_MD_CTX_free(again);
            // the signature is the last field, as long as the key's modulus
            std::memcpy(m.der.data() + m.der.size() - sn, sig.data(), sn);
            const unsigned char* q = m.der.data();
            m.cert = x509_ptr(d2i_X509(nullptr, &q, long(m.der.size())), X509_free);
        }
        if (s.break_signature) {
            m.der[m.der.size() - 5] ^= 0x01;   // inside the signature, the last element
            const unsigned char* q = m.der.data();
            m.cert = x509_ptr(d2i_X509(nullptr, &q, long(m.der.size())), X509_free);
        }
        return m;
    }

    // The usual shapes
    inline spec root_spec(key_type k, const std::string& cn = "Test Root") {
        spec s;
        s.cn = cn;
        s.o = "SGCL Tests";
        s.key = k;
        s.not_before = now - 86400 * 3650;
        s.not_after = now + 86400 * 3650;
        s.basic_constraints = "critical,CA:TRUE";
        s.key_usage = "critical,keyCertSign,cRLSign";
        return s;
    }

    inline spec intermediate_spec(key_type k, const std::string& cn = "Test Intermediate") {
        spec s = root_spec(k, cn);
        s.not_before = now - 86400 * 365;
        s.not_after = now + 86400 * 365 * 3;
        return s;
    }

    inline spec leaf_spec(key_type k, const std::string& san = "DNS:www.example.com,DNS:*.example.org,IP:192.0.2.7") {
        spec s;
        s.key = k;
        s.san = san;
        s.basic_constraints = "critical,CA:FALSE";
        s.key_usage = "critical,digitalSignature";
        s.ext_key_usage = "serverAuth";
        return s;
    }

    // The signature algorithm a key signs with in the tests: the digest of
    // its strength, PKCS #1 v1.5 for RSA
    inline sig default_sig(key_type k) {
        return k == key_type::p384 ? sig::sha384 : sig::sha256;
    }

    //--------------------------------------------------------------------
    // Verification by both
    //--------------------------------------------------------------------
    struct chain_case {
        std::vector<made> roots;
        std::vector<made> intermediates;
        made leaf;
        std::string dns;
        bytes_t ip;
        int64_t time = now;
        std::vector<x509::ext_key_usage> ekus;   // empty: serverAuth
        int max_intermediates = 10;
    };

    // OpenSSL's verdict as a reason: none for a chain it takes
    inline reason openssl_reason(int e) {
        switch (e) {
            case X509_V_OK: return reason::none;
            case X509_V_ERR_CERT_HAS_EXPIRED: return reason::expired;
            case X509_V_ERR_CERT_NOT_YET_VALID: return reason::not_yet_valid;
            case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
            case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
            case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
            case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
            case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                return reason::unknown_authority;
            case X509_V_ERR_HOSTNAME_MISMATCH:
            case X509_V_ERR_IP_ADDRESS_MISMATCH:
                return reason::hostname_mismatch;
            case X509_V_ERR_PERMITTED_VIOLATION:
            case X509_V_ERR_EXCLUDED_VIOLATION:
            case X509_V_ERR_SUBTREE_MINMAX:
            case X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE:
            case X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX:
            case X509_V_ERR_UNSUPPORTED_NAME_SYNTAX:
                return reason::name_constraints;
            case X509_V_ERR_CERT_SIGNATURE_FAILURE:
            case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
                return reason::invalid_signature;
            case X509_V_ERR_INVALID_CA:
                return reason::not_a_ca;
            case X509_V_ERR_PATH_LENGTH_EXCEEDED:
                return reason::path_length;
            case X509_V_ERR_INVALID_PURPOSE:
                return reason::incompatible_usage;
            case X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION:
                return reason::unhandled_critical_extension;
            case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
                return reason::missing_cert_sign;
            case X509_V_ERR_CA_MD_TOO_WEAK:
                return reason::insecure_algorithm;
            case X509_V_ERR_CERT_CHAIN_TOO_LONG:
                return reason::too_many_intermediates;
            default:
                return reason(255);
        }
    }

    inline int openssl_verify(const chain_case& c) {
        X509_STORE* store = X509_STORE_new();
        for (auto& r : c.roots) {
            X509_STORE_add_cert(store, r.cert.get());
        }
        STACK_OF(X509)* untrusted = sk_X509_new_null();
        for (auto& i : c.intermediates) {
            sk_X509_push(untrusted, i.cert.get());
        }
        X509_STORE_CTX* ctx = X509_STORE_CTX_new();
        X509_STORE_CTX_init(ctx, store, c.leaf.cert.get(), untrusted);
        X509_VERIFY_PARAM* p = X509_STORE_CTX_get0_param(ctx);
        X509_VERIFY_PARAM_set_time(p, time_t(c.time));
        X509_VERIFY_PARAM_set_depth(p, c.max_intermediates);
        X509_VERIFY_PARAM_set_auth_level(p, 1);
        if (c.ekus.empty()) {
            X509_VERIFY_PARAM_set_purpose(p, X509_PURPOSE_SSL_SERVER);
        } else if (c.ekus[0] == x509::ext_key_usage::client_auth) {
            X509_VERIFY_PARAM_set_purpose(p, X509_PURPOSE_SSL_CLIENT);
        } else if (c.ekus[0] == x509::ext_key_usage::email_protection) {
            X509_VERIFY_PARAM_set_purpose(p, X509_PURPOSE_SMIME_SIGN);
        }
        if (!c.dns.empty()) {
            X509_VERIFY_PARAM_set1_host(p, c.dns.c_str(), c.dns.size());
            X509_VERIFY_PARAM_set_hostflags(p, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS | X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
        }
        if (!c.ip.empty()) {
            X509_VERIFY_PARAM_set1_ip(p, c.ip.data(), c.ip.size());
        }
        int ok = X509_verify_cert(ctx);
        int err = ok == 1 ? X509_V_OK : X509_STORE_CTX_get_error(ctx);
        X509_STORE_CTX_free(ctx);
        sk_X509_free(untrusted);
        X509_STORE_free(store);
        return err;
    }

    inline x509::certificate parse_or_fail(const bytes_t& der) {
        auto c = x509::certificate::parse(view(der));
        if (!c) {
            throw std::runtime_error("the certificate did not parse: " + std::string(c.error().message().view()));
        }
        return *c;
    }

    // Ours: none for a chain found, else the reason
    inline reason our_reason(const chain_case& c, std::string* message = nullptr, size_t* length = nullptr) {
        x509::verify_options o;
        x509::certificate_pool roots;
        for (auto& r : c.roots) {
            roots.add(parse_or_fail(r.der));
        }
        o.roots = roots;
        for (auto& i : c.intermediates) {
            o.intermediates.add(parse_or_fail(i.der));
        }
        o.dns_name = string(c.dns);
        o.ip = view(c.ip);
        o.time = sgcl::time::datetime::from_unix(c.time, sgcl::time::zone::utc());
        for (auto u : c.ekus) {
            o.key_usages.push_back(u);
        }
        auto leaf = parse_or_fail(c.leaf.der);
        auto r = leaf.verify(o);
        if (r) {
            if (length) {
                *length = r->size();
            }
            return reason::none;
        }
        if (message) {
            *message = std::string(r.error().message().view());
        }
        EXPECT_EQ(r.error().code(), sgcl::crypto::errc::verification);
        return r.error().reason();
    }

    inline const char* reason_name(reason r) {
        switch (r) {
            case reason::none: return "ok";
            case reason::expired: return "expired";
            case reason::not_yet_valid: return "not_yet_valid";
            case reason::unknown_authority: return "unknown_authority";
            case reason::hostname_mismatch: return "hostname_mismatch";
            case reason::name_constraints: return "name_constraints";
            case reason::unsupported_algorithm: return "unsupported_algorithm";
            case reason::insecure_algorithm: return "insecure_algorithm";
            case reason::invalid_signature: return "invalid_signature";
            case reason::too_many_intermediates: return "too_many_intermediates";
            case reason::path_length: return "path_length";
            case reason::not_a_ca: return "not_a_ca";
            case reason::missing_cert_sign: return "missing_cert_sign";
            case reason::incompatible_usage: return "incompatible_usage";
            case reason::unhandled_critical_extension: return "unhandled_critical_extension";
            case reason::too_many_constraints: return "too_many_constraints";
        }
        return "openssl_other";
    }

    // The parts of a certificate written by hand, each replaceable; the
    // signature is not a valid one (parse does not check it)
    struct hand {
        bytes_t version = der(0xa0, der(0x02, {0x02}));
        bytes_t serial = der(0x02, {0x01});
        bytes_t algorithm = seq({oid("1.2.840.10045.4.3.2")});
        bytes_t issuer = name_of({{"2.5.4.3", utf8_der("Hand Root")}});
        bytes_t validity = seq({der(0x17, text("250101000000Z")), der(0x17, text("300101000000Z"))});
        bytes_t subject = name_of({{"2.5.4.3", utf8_der("Hand Leaf")}});
        bytes_t spki;                       // set by the constructor: a P-256 key
        bytes_t extensions;                 // the [3] element, or empty
        bytes_t trailing_tbs;               // after the extensions, inside the TBSCertificate
        bytes_t signature = der(0x03, {0x00, 0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01});

        hand() {
            static bytes_t key = [] {
                auto k = new_key(key_type::p256);
                unsigned char* p = nullptr;
                int n = i2d_PUBKEY(k.get(), &p);
                bytes_t v(p, p + n);
                OPENSSL_free(p);
                return v;
            }();
            spki = key;
        }

        // extensions from Extension SEQUENCEs
        void set_extensions(const std::vector<bytes_t>& exts) {
            bytes_t all;
            for (auto& e : exts) {
                all.insert(all.end(), e.begin(), e.end());
            }
            extensions = der(0xa3, der(0x30, all));
        }

        bytes_t tbs() const {
            return seq({version, serial, algorithm, issuer, validity, subject, spki, extensions, trailing_tbs});
        }

        bytes_t build() const {
            return seq({tbs(), algorithm, signature});
        }
    };

    inline bytes_t extension(const std::string& o, bool critical, const bytes_t& value) {
        return critical ? seq({oid(o), der(0x01, {0xff}), der(0x04, value)}) : seq({oid(o), der(0x04, value)});
    }

    // PEM blocks of a text: (type, DER)
    inline std::vector<std::pair<std::string, bytes_t>> pem_blocks(const std::string& text) {
        std::vector<std::pair<std::string, bytes_t>> out;
        auto blocks = sgcl::encoding::pem::parse_all(string(text));
        if (!blocks) {
            return out;
        }
        for (auto& b : *blocks) {
            out.emplace_back(std::string(b.type().view()), to_bytes(b.bytes()));
        }
        return out;
    }

    // Go's tree, for its testdata: GOROOT from the environment or the usual places
    inline std::string go_root() {
        std::vector<std::string> roots;
        if (const char* g = std::getenv("GOROOT")) {
            roots.push_back(g);
        }
        roots.push_back("/opt/homebrew/opt/go/libexec");
        roots.push_back("/usr/local/go");
        for (auto& r : roots) {
            if (std::ifstream(r + "/src/crypto/x509/verify.go")) {
                return r;
            }
        }
        return "";
    }
}
