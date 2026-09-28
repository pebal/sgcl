// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the key schedule of TLS 1.3 (sgcl/net/tls/detail/
// schedule.h): OpenSSL's own implementation of it asked the same questions,
// the answers written out as a C++ header the tests include. Only in this
// tool, never in the library. Two kinds of case:
//
//   - the chain of RFC 8446 §7.1 on random inputs, computed with OpenSSL's
//     TLS13-KDF (EVP_KDF, its "extract only" mode with the previous secret
//     as the salt — which makes Derive-Secret(., "derived", "") itself —
//     and its "expand only" mode, HKDF-Expand-Label), and verify_data with
//     OpenSSL's HMAC: for SHA-256 and SHA-384, a shared secret of 32 bytes
//     (X25519) and of 64 (X25519MLKEM768), random transcript hashes;
//   - real handshakes of OpenSSL with itself in this process (libssl over
//     memory BIOs, TLS 1.3, each cipher suite and group): every handshake
//     message as it was sent (the message callback), the handshake traffic
//     secrets from the key log, and the two Finished — what the test needs
//     to compute the transcript hashes and both verify_data and compare.
//
// From the root of the tree:
//
//     clang++ -std=c++20 -O1 -I/opt/homebrew/opt/openssl@3/include tools/tls_schedule_oracle.cpp \
//         -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -o /tmp/tls_schedule_oracle
//     /tmp/tls_schedule_oracle tests/net/tls_cert.pem tests/net/tls_key.pem > tests/net/tls_schedule_vectors.h
//
// The certificate and key are made here too, when the files are not given
// (an ECDSA P-256 self-signed certificate): the tests never verify it.
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using bytes = std::vector<unsigned char>;

namespace {
    [[noreturn]] void fail(const char* what) {
        std::fprintf(stderr, "tls_schedule_oracle: %s\n", what);
        std::exit(1);
    }

    std::string hex(const bytes& b) {
        static const char* d = "0123456789abcdef";
        std::string s;
        for (unsigned char c : b) {
            s += d[c >> 4];
            s += d[c & 15];
        }
        return s;
    }

    bytes random_bytes(size_t n) {
        bytes b(n);
        if (RAND_bytes(b.data(), int(n)) != 1) {
            fail("RAND_bytes");
        }
        return b;
    }

    // TLS13-KDF: extract (salt: the previous secret, or none) or expand-label
    bytes tls13_kdf(const char* digest, int mode, const bytes& key, const bytes* salt, const char* label, const bytes& data, size_t out) {
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "TLS13-KDF", nullptr);
        EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
        std::vector<OSSL_PARAM> p;
        p.push_back(OSSL_PARAM_construct_int(OSSL_KDF_PARAM_MODE, &mode));
        p.push_back(OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char*>(digest), 0));
        if (!key.empty()) {
            p.push_back(OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, const_cast<unsigned char*>(key.data()), key.size()));
        }
        if (salt) {
            p.push_back(OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<unsigned char*>(salt->data()), salt->size()));
        }
        static unsigned char prefix[] = "tls13 ";
        p.push_back(OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PREFIX, prefix, 6));
        p.push_back(OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_LABEL, const_cast<char*>(label), std::strlen(label)));
        if (!data.empty()) {
            p.push_back(OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_DATA, const_cast<unsigned char*>(data.data()), data.size()));
        }
        p.push_back(OSSL_PARAM_construct_end());
        bytes r(out);
        if (!ctx || EVP_KDF_derive(ctx, r.data(), r.size(), p.data()) <= 0) {
            fail("EVP_KDF_derive TLS13-KDF");
        }
        EVP_KDF_CTX_free(ctx);
        EVP_KDF_free(kdf);
        return r;
    }

    bytes expand_label(const char* digest, const bytes& secret, const char* label, const bytes& context, size_t n) {
        return tls13_kdf(digest, EVP_KDF_HKDF_MODE_EXPAND_ONLY, secret, nullptr, label, context, n);
    }

    bytes hmac(const char* digest, const bytes& key, const bytes& data) {
        unsigned char out[64];
        unsigned int n = 0;
        if (!HMAC(EVP_get_digestbyname(digest), key.data(), int(key.size()), data.data(), data.size(), out, &n)) {
            fail("HMAC");
        }
        return bytes(out, out + n);
    }

    void chain_case(const char* digest, size_t hash_len, size_t shared_len) {
        bytes shared = random_bytes(shared_len), hello = random_bytes(hash_len), finished = random_bytes(hash_len), client_finished = random_bytes(hash_len);
        bytes none;
        bytes early = tls13_kdf(digest, EVP_KDF_HKDF_MODE_EXTRACT_ONLY, none, nullptr, "derived", none, hash_len);
        bytes handshake = tls13_kdf(digest, EVP_KDF_HKDF_MODE_EXTRACT_ONLY, shared, &early, "derived", none, hash_len);
        bytes chs = expand_label(digest, handshake, "c hs traffic", hello, hash_len);
        bytes shs = expand_label(digest, handshake, "s hs traffic", hello, hash_len);
        bytes master = tls13_kdf(digest, EVP_KDF_HKDF_MODE_EXTRACT_ONLY, none, &handshake, "derived", none, hash_len);
        bytes cap = expand_label(digest, master, "c ap traffic", finished, hash_len);
        bytes sap = expand_label(digest, master, "s ap traffic", finished, hash_len);
        bytes exp = expand_label(digest, master, "exp master", finished, hash_len);
        size_t key_len = hash_len == 32 ? 16 : 32;
        bytes skey = expand_label(digest, shs, "key", none, key_len), siv = expand_label(digest, shs, "iv", none, 12);
        bytes ckey32 = expand_label(digest, cap, "key", none, 32), civ = expand_label(digest, cap, "iv", none, 12);
        bytes server_verify = hmac(digest, expand_label(digest, shs, "finished", none, hash_len), finished);
        bytes client_verify = hmac(digest, expand_label(digest, chs, "finished", none, hash_len), client_finished);
        bytes upd = expand_label(digest, sap, "traffic upd", none, hash_len);
        std::printf("    {%d, \"%s\", \"%s\", \"%s\", \"%s\",\n", hash_len == 32 ? 256 : 384, hex(shared).c_str(), hex(hello).c_str(), hex(finished).c_str(), hex(client_finished).c_str());
        std::printf("     \"%s\", \"%s\", \"%s\", \"%s\",\n", hex(early).c_str(), hex(handshake).c_str(), hex(chs).c_str(), hex(shs).c_str());
        std::printf("     \"%s\", \"%s\", \"%s\", \"%s\",\n", hex(master).c_str(), hex(cap).c_str(), hex(sap).c_str(), hex(exp).c_str());
        std::printf("     \"%s\", \"%s\", \"%s\", \"%s\",\n", hex(skey).c_str(), hex(siv).c_str(), hex(ckey32).c_str(), hex(civ).c_str());
        std::printf("     \"%s\", \"%s\", \"%s\"},\n", hex(server_verify).c_str(), hex(client_verify).c_str(), hex(upd).c_str());
    }

    // --- real handshakes -------------------------------------------------------

    struct Recording {
        std::vector<std::pair<int, bytes>> messages;   // (1: client sent, 0: server sent, the handshake message)
        std::string keylog;
    };

    Recording* recording = nullptr;

    void on_message(int write_p, int, int content_type, const void* buf, size_t len, SSL* ssl, void*) {
        if (content_type != SSL3_RT_HANDSHAKE || !recording) {
            return;
        }
        bool client = SSL_is_server(ssl) == 0;
        if (!write_p) {
            return;   // each message once: as its sender wrote it
        }
        auto p = static_cast<const unsigned char*>(buf);
        recording->messages.push_back({client ? 1 : 0, bytes(p, p + len)});
    }

    void on_keylog(const SSL*, const char* line) {
        if (recording) {
            recording->keylog += line;
            recording->keylog += "\n";
        }
    }

    std::pair<X509*, EVP_PKEY*> self_signed() {
        EVP_PKEY* key = EVP_EC_gen("P-256");
        X509* cert = X509_new();
        ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
        X509_gmtime_adj(X509_getm_notBefore(cert), 0);
        X509_gmtime_adj(X509_getm_notAfter(cert), 3600);
        X509_set_pubkey(cert, key);
        X509_NAME* name = X509_get_subject_name(cert);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("sgcl.test"), -1, -1, 0);
        X509_set_issuer_name(cert, name);
        X509_sign(cert, key, EVP_sha256());
        return {cert, key};
    }

    // One handshake of OpenSSL with itself, pumped through memory BIOs
    Recording handshake(const char* suite, const char* group, X509* cert, EVP_PKEY* key) {
        Recording rec;
        recording = &rec;
        SSL_CTX* cctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX* sctx = SSL_CTX_new(TLS_server_method());
        for (SSL_CTX* c : {cctx, sctx}) {
            SSL_CTX_set_min_proto_version(c, TLS1_3_VERSION);
            SSL_CTX_set_max_proto_version(c, TLS1_3_VERSION);
            SSL_CTX_set_ciphersuites(c, suite);
            SSL_CTX_set1_groups_list(c, group);
            SSL_CTX_set_msg_callback(c, on_message);
            SSL_CTX_set_keylog_callback(c, on_keylog);
            SSL_CTX_set_options(c, SSL_OP_NO_TICKET);
        }
        SSL_CTX_use_certificate(sctx, cert);
        SSL_CTX_use_PrivateKey(sctx, key);
        SSL_CTX_set_num_tickets(sctx, 0);
        SSL* c = SSL_new(cctx);
        SSL* s = SSL_new(sctx);
        BIO *c_in = BIO_new(BIO_s_mem()), *c_out = BIO_new(BIO_s_mem()), *s_in = BIO_new(BIO_s_mem()), *s_out = BIO_new(BIO_s_mem());
        SSL_set_bio(c, c_in, c_out);
        SSL_set_bio(s, s_in, s_out);
        SSL_set_connect_state(c);
        SSL_set_accept_state(s);
        auto pump = [](BIO* from, BIO* to) {
            char buf[16384];
            int n;
            while ((n = BIO_read(from, buf, sizeof buf)) > 0) {
                BIO_write(to, buf, n);
            }
        };
        bool cd = false, sd = false;
        for (int i = 0; i < 20 && !(cd && sd); ++i) {
            if (!cd) {
                cd = SSL_do_handshake(c) == 1;
            }
            pump(c_out, s_in);
            if (!sd) {
                sd = SSL_do_handshake(s) == 1;
            }
            pump(s_out, c_in);
        }
        if (!cd || !sd) {
            fail("the handshake did not finish");
        }
        SSL_free(c);
        SSL_free(s);
        SSL_CTX_free(cctx);
        SSL_CTX_free(sctx);
        recording = nullptr;
        return rec;
    }

    std::string keylog_value(const std::string& keylog, const char* label) {
        size_t at = keylog.find(std::string(label) + " ");
        if (at == std::string::npos) {
            fail(label);
        }
        size_t start = keylog.find(' ', keylog.find(' ', at) + 1) + 1;   // label, client random, secret
        size_t end = keylog.find('\n', start);
        return keylog.substr(start, end - start);
    }

    void handshake_case(const char* suite, const char* group, X509* cert, EVP_PKEY* key) {
        Recording r = handshake(suite, group, cert, key);
        int hash = std::strstr(suite, "SHA384") ? 384 : 256;
        std::printf("    {\"%s\", \"%s\", %d,\n", suite, group, hash);
        std::printf("     \"%s\",\n", keylog_value(r.keylog, "CLIENT_HANDSHAKE_TRAFFIC_SECRET").c_str());
        std::printf("     \"%s\",\n", keylog_value(r.keylog, "SERVER_HANDSHAKE_TRAFFIC_SECRET").c_str());
        std::printf("     \"%s\",\n", keylog_value(r.keylog, "CLIENT_TRAFFIC_SECRET_0").c_str());
        std::printf("     \"%s\",\n", keylog_value(r.keylog, "SERVER_TRAFFIC_SECRET_0").c_str());
        std::printf("     {");
        for (auto& [client, m] : r.messages) {
            std::printf("{%d, \"%s\"}, ", client, hex(m).c_str());
        }
        std::printf("}},\n");
    }
}

int main(int, char**) {
    std::printf("// Made by tools/tls_schedule_oracle.cpp (OpenSSL %s): do not edit.\n", OpenSSL_version(OPENSSL_VERSION_STRING));
    std::printf("#pragma once\n\n#include <vector>\n\nnamespace tls_vectors {\n");
    std::printf("    // The chain of RFC 8446 §7.1 on random inputs (OpenSSL's TLS13-KDF and HMAC)\n");
    std::printf("    struct Chain {\n        int hash;\n        const char *shared, *hello_hash, *server_finished_hash, *client_finished_hash;\n");
    std::printf("        const char *early, *handshake, *client_hs, *server_hs;\n        const char *master, *client_ap, *server_ap, *exporter;\n");
    std::printf("        const char *server_hs_key, *server_hs_iv, *client_ap_key32, *client_ap_iv;\n        const char *server_verify, *client_verify, *server_ap_updated;\n    };\n\n");
    std::printf("    inline const Chain chains[] = {\n");
    for (int i = 0; i < 16; ++i) {
        chain_case("SHA256", 32, i % 2 ? 64 : 32);
        chain_case("SHA384", 48, i % 2 ? 64 : 32);
    }
    std::printf("    };\n\n");
    std::printf("    // Real handshakes of OpenSSL with itself: each handshake message as its sender wrote it\n");
    std::printf("    // (client 1, server 0), and the traffic secrets of the key log\n");
    std::printf("    struct Message {\n        int client;\n        const char* bytes;\n    };\n\n");
    std::printf("    struct Handshake {\n        const char *suite, *group;\n        int hash;\n        const char *client_hs, *server_hs, *client_ap, *server_ap;\n        std::vector<Message> messages;\n    };\n\n");
    std::printf("    inline const std::vector<Handshake> handshakes = {\n");
    auto [cert, key] = self_signed();
    for (const char* suite : {"TLS_AES_128_GCM_SHA256", "TLS_AES_256_GCM_SHA384", "TLS_CHACHA20_POLY1305_SHA256"}) {
        for (const char* group : {"X25519", "X25519MLKEM768"}) {
            handshake_case(suite, group, cert, key);
        }
    }
    std::printf("    };\n}\n");
    X509_free(cert);
    EVP_PKEY_free(key);
}
