// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the record layer of TLS 1.3 (sgcl/net/tls/detail/
// record.h): real sessions of OpenSSL with itself in this process (libssl
// over memory BIOs, TLS 1.3, each cipher suite, with and without record
// padding, middlebox compatibility on as OpenSSL has it by default), the
// bytes on the wire in each direction written out with the traffic secrets
// of the key log and every handshake message as its sender wrote it (the
// message callback). Only in this tool, never in the library. Each session:
//
//   - the handshake, one session ticket;
//   - the client writes "hello";
//   - the server writes 17000 bytes (i * 7 + 3 mod 256: two records), then
//     a KeyUpdate with update_requested, then "after update";
//   - the client reads everything (it answers with its own KeyUpdate),
//     writes "client after update" and closes (close_notify).
//
// From the root of the tree:
//
//     clang++ -std=c++20 -O1 -I/opt/homebrew/opt/openssl@3/include tools/tls_record_oracle.cpp \
//         -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -o /tmp/tls_record_oracle
//     /tmp/tls_record_oracle > tests/net/tls_record_vectors.h
#include <openssl/evp.h>
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
        std::fprintf(stderr, "tls_record_oracle: %s\n", what);
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

    // A long hex string as pieces of a literal, one per line
    void print_hex(const bytes& b) {
        std::string h = hex(b);
        if (h.empty()) {
            std::printf("\"\"");
        }
        for (size_t i = 0; i < h.size(); i += 2000) {
            std::printf("%s\"%s\"", i ? "\n        " : "", h.substr(i, 2000).c_str());
        }
    }

    struct Recording {
        std::vector<std::pair<int, bytes>> messages;   // (1: client sent, 0: server sent, the handshake message)
        std::string keylog;
        bytes client_wire, server_wire;
    };

    Recording* recording = nullptr;

    void on_message(int write_p, int, int content_type, const void* buf, size_t len, SSL* ssl, void*) {
        if (content_type != SSL3_RT_HANDSHAKE || !recording || !write_p) {
            return;
        }
        auto p = static_cast<const unsigned char*>(buf);
        recording->messages.push_back({SSL_is_server(ssl) == 0 ? 1 : 0, bytes(p, p + len)});
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

    bytes pattern(size_t n) {
        bytes b(n);
        for (size_t i = 0; i < n; ++i) {
            b[i] = (unsigned char)(i * 7 + 3);
        }
        return b;
    }

    void pump(BIO* from, BIO* to, bytes& wire) {
        char buf[16384];
        int n;
        while ((n = BIO_read(from, buf, sizeof buf)) > 0) {
            BIO_write(to, buf, n);
            wire.insert(wire.end(), buf, buf + n);
        }
    }

    void read_exactly(SSL* ssl, size_t n) {
        std::vector<char> buf(n);
        size_t got = 0;
        while (got < n) {
            int r = SSL_read(ssl, buf.data() + got, int(n - got));
            if (r <= 0) {
                fail("SSL_read");
            }
            got += size_t(r);
        }
    }

    Recording session(const char* suite, size_t padding, X509* cert, EVP_PKEY* key) {
        Recording rec;
        recording = &rec;
        SSL_CTX* cctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX* sctx = SSL_CTX_new(TLS_server_method());
        for (SSL_CTX* c : {cctx, sctx}) {
            SSL_CTX_set_min_proto_version(c, TLS1_3_VERSION);
            SSL_CTX_set_max_proto_version(c, TLS1_3_VERSION);
            SSL_CTX_set_ciphersuites(c, suite);
            SSL_CTX_set1_groups_list(c, "X25519");
            SSL_CTX_set_msg_callback(c, on_message);
            SSL_CTX_set_keylog_callback(c, on_keylog);
            if (padding) {
                SSL_CTX_set_block_padding(c, padding);
            }
        }
        SSL_CTX_use_certificate(sctx, cert);
        SSL_CTX_use_PrivateKey(sctx, key);
        SSL_CTX_set_num_tickets(sctx, 1);
        SSL* c = SSL_new(cctx);
        SSL* s = SSL_new(sctx);
        BIO *c_in = BIO_new(BIO_s_mem()), *c_out = BIO_new(BIO_s_mem()), *s_in = BIO_new(BIO_s_mem()), *s_out = BIO_new(BIO_s_mem());
        SSL_set_bio(c, c_in, c_out);
        SSL_set_bio(s, s_in, s_out);
        SSL_set_connect_state(c);
        SSL_set_accept_state(s);
        bool cd = false, sd = false;
        for (int i = 0; i < 20 && !(cd && sd); ++i) {
            if (!cd) {
                cd = SSL_do_handshake(c) == 1;
            }
            pump(c_out, s_in, rec.client_wire);
            if (!sd) {
                sd = SSL_do_handshake(s) == 1;
            }
            pump(s_out, c_in, rec.server_wire);
        }
        if (!cd || !sd) {
            fail("the handshake did not finish");
        }
        if (SSL_write(c, "hello", 5) != 5) {
            fail("SSL_write hello");
        }
        pump(c_out, s_in, rec.client_wire);
        read_exactly(s, 5);
        pump(s_out, c_in, rec.server_wire);
        bytes big = pattern(17000);
        if (SSL_write(s, big.data(), int(big.size())) != int(big.size())) {
            fail("SSL_write big");
        }
        if (SSL_key_update(s, SSL_KEY_UPDATE_REQUESTED) != 1 || SSL_do_handshake(s) != 1) {
            fail("SSL_key_update");
        }
        if (SSL_write(s, "after update", 12) != 12) {
            fail("SSL_write after update");
        }
        pump(s_out, c_in, rec.server_wire);
        read_exactly(c, 17000 + 12);
        if (SSL_write(c, "client after update", 19) != 19) {
            fail("SSL_write client after update");
        }
        SSL_shutdown(c);
        pump(c_out, s_in, rec.client_wire);
        read_exactly(s, 19);
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

    void session_case(const char* suite, size_t padding, X509* cert, EVP_PKEY* key) {
        Recording r = session(suite, padding, cert, key);
        std::printf("    {\"%s\", %zu,\n", suite, padding);
        std::printf("     \"%s\",\n", keylog_value(r.keylog, "CLIENT_HANDSHAKE_TRAFFIC_SECRET").c_str());
        std::printf("     \"%s\",\n", keylog_value(r.keylog, "SERVER_HANDSHAKE_TRAFFIC_SECRET").c_str());
        std::printf("     \"%s\",\n", keylog_value(r.keylog, "CLIENT_TRAFFIC_SECRET_0").c_str());
        std::printf("     \"%s\",\n", keylog_value(r.keylog, "SERVER_TRAFFIC_SECRET_0").c_str());
        std::printf("     // the client's bytes on the wire\n        ");
        print_hex(r.client_wire);
        std::printf(",\n     // the server's\n        ");
        print_hex(r.server_wire);
        std::printf(",\n     {");
        for (auto& [client, m] : r.messages) {
            std::printf("{%d, \"%s\"}, ", client, hex(m).c_str());
        }
        std::printf("}},\n");
    }
}

int main(int, char**) {
    std::printf("// Made by tools/tls_record_oracle.cpp (OpenSSL %s): do not edit.\n", OpenSSL_version(OPENSSL_VERSION_STRING));
    std::printf("#pragma once\n\n#include <cstddef>\n#include <vector>\n\nnamespace tls_record_vectors {\n");
    std::printf("    struct Message {\n        int client;\n        const char* bytes;\n    };\n\n");
    std::printf("    // A session of OpenSSL with itself (see the tool): the traffic secrets of the key log,\n");
    std::printf("    // the bytes each side wrote to the wire, each handshake message as its sender wrote it\n");
    std::printf("    struct Session {\n        const char* suite;\n        size_t padding;\n");
    std::printf("        const char *client_hs, *server_hs, *client_ap, *server_ap;\n        const char *client_wire, *server_wire;\n");
    std::printf("        std::vector<Message> messages;\n    };\n\n");
    std::printf("    inline const std::vector<Session> sessions = {\n");
    auto [cert, key] = self_signed();
    for (const char* suite : {"TLS_AES_128_GCM_SHA256", "TLS_AES_256_GCM_SHA384", "TLS_CHACHA20_POLY1305_SHA256"}) {
        session_case(suite, 0, cert, key);
    }
    session_case("TLS_AES_128_GCM_SHA256", 256, cert, key);
    session_case("TLS_CHACHA20_POLY1305_SHA256", 64, cert, key);
    std::printf("    };\n}\n");
    X509_free(cert);
    EVP_PKEY_free(key);
}
