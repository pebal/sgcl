//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cms.h"
#include "random.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/vector.h"
#include "../encoding/base64.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// S/MIME 4.0 (RFC 8551) over CMS: a MIME entity — its header fields, an
// empty line and its body, as bytes, what encoding::email writes and an
// IMAP server gives — signed into a multipart/signed (RFC 1847) with a
// detached signature, or encrypted into an application/pkcs7-mime of
// AuthEnvelopedData (or EnvelopedData), and back. Bytes rather than a
// parsed part: a signature covers the entity's bytes exactly as they were
// written, which a parse and a write again would not keep. Line ends are
// made CRLF (the canonical form of RFC 8551 3.1.1) before signing,
// encrypting and verifying.
namespace sgcl::crypto::detail::smime {
    // Bare LF to CRLF; a CR alone kept
    inline std::string canonical(std::string_view in) {
        std::string out;
        out.reserve(in.size() + in.size() / 32);
        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] == '\n' && (i == 0 || in[i - 1] != '\r')) {
                out += '\r';
            }
            out += in[i];
        }
        return out;
    }

    inline std::string lower(std::string_view s) {
        std::string out(s);
        for (auto& c : out) {
            c = char(c >= 'A' && c <= 'Z' ? c + 32 : c);
        }
        return out;
    }

    // An entity's header fields (unfolded) and where its body starts
    struct Entity {
        std::string content_type;           // the type/subtype, lower case
        std::string content_type_field;     // the whole value, for its parameters
        std::string transfer_encoding;      // lower case
        size_t body = 0;
        bool ok = false;
    };

    inline Entity read_entity(std::string_view m) {
        Entity e;
        size_t at = 0;
        std::string name, value;
        auto flush = [&] {
            if (name.empty()) {
                return;
            }
            const std::string n = lower(name);
            size_t a = value.find_first_not_of(" \t");
            size_t b = value.find_last_not_of(" \t\r\n");
            const std::string v = a == std::string::npos ? std::string() : value.substr(a, b - a + 1);
            if (n == "content-type") {
                e.content_type_field = v;
                e.content_type = lower(v.substr(0, v.find(';')));
                size_t ta = e.content_type.find_first_not_of(" \t"), tb = e.content_type.find_last_not_of(" \t");
                e.content_type = ta == std::string::npos ? std::string() : e.content_type.substr(ta, tb - ta + 1);
            } else if (n == "content-transfer-encoding") {
                e.transfer_encoding = lower(v);
            }
            name.clear();
            value.clear();
        };
        while (at < m.size()) {
            size_t end = m.find('\n', at);
            if (end == std::string_view::npos) {
                return e;   // no empty line: no body
            }
            std::string_view line = m.substr(at, end - at);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            at = end + 1;
            if (line.empty()) {
                flush();
                e.body = at;
                e.ok = true;
                return e;
            }
            if (line[0] == ' ' || line[0] == '\t') {
                value += std::string(line);   // folded
                continue;
            }
            flush();
            size_t colon = line.find(':');
            if (colon == std::string_view::npos) {
                return e;
            }
            name = std::string(line.substr(0, colon));
            value = std::string(line.substr(colon + 1));
        }
        return e;
    }

    // A parameter of a Content-Type value, quoted or not
    inline std::string param(const std::string& field, std::string_view key) {
        const std::string low = lower(field);
        size_t at = 0;
        while ((at = low.find(';', at)) != std::string::npos) {
            ++at;
            size_t k = low.find_first_not_of(" \t", at);
            if (k == std::string::npos) {
                break;
            }
            size_t eq = low.find('=', k);
            if (eq == std::string::npos) {
                break;
            }
            std::string name = low.substr(k, eq - k);
            while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
                name.pop_back();
            }
            size_t v = field.find_first_not_of(" \t", eq + 1);
            if (v == std::string::npos) {
                break;
            }
            std::string value;
            if (field[v] == '"') {
                size_t q = field.find('"', v + 1);
                value = field.substr(v + 1, q == std::string::npos ? std::string::npos : q - v - 1);
            } else {
                size_t e2 = field.find(';', v);
                value = field.substr(v, e2 == std::string::npos ? std::string::npos : e2 - v);
                while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
                    value.pop_back();
                }
            }
            if (name == key) {
                return value;
            }
        }
        return std::string();
    }

    inline expected<vector<byte>, error> base64_body(std::string_view body) {
        std::string clean;
        clean.reserve(body.size());
        for (char c : body) {
            if (c != '\r' && c != '\n' && c != ' ' && c != '\t') {
                clean += c;
            }
        }
        auto d = encoding::base64::standard.decode(string(std::string_view(clean)));
        if (!d) {
            return unexpected(error(errc::malformed, string("sgcl::crypto::smime: a body that is not base64")));
        }
        return std::move(*d);
    }

    inline std::string base64_lines(const vector<byte>& der) {
        const string b = encoding::base64::standard.encode(der);
        std::string out;
        const std::string_view v = b.view();
        for (size_t i = 0; i < v.size(); i += 64) {
            out += std::string(v.substr(i, 64));
            out += "\r\n";
        }
        return out;
    }

    inline vector<byte> bytes_of(const std::string& s) {
        return vector<byte>(reinterpret_cast<const byte*>(s.data()), reinterpret_cast<const byte*>(s.data()) + s.size());
    }

    inline const char* micalg(x509::key_kind k) noexcept {
        switch (k) {
            case x509::key_kind::p384: return "sha-384";
            case x509::key_kind::p521:
            case x509::key_kind::ed25519: return "sha-512";
            default: return "sha-256";
        }
    }
}

namespace sgcl::crypto::smime {
    // A MIME entity signed: the bytes of a multipart/signed (RFC 1847) of
    // the entity, made canonical (CRLF), and its detached SignedData
    // (application/pkcs7-signature), with MIME-Version: what goes below a
    // message's other header fields, or is the whole of a part
    inline vector<byte> sign(const slice<const byte>& entity, const x509::certificate& signer, const x509::signing_key& key, const cms::sign_options& o) {
        using namespace crypto::detail::smime;
        const std::string body = canonical(std::string_view(reinterpret_cast<const char*>(entity.data()), entity.size()));
        cms::sign_options so = o;
        so.detached = true;
        auto sd = cms::sign(slice<const byte>(reinterpret_cast<const byte*>(body.data()), body.size()), signer, key, so);
        auto r = random::bytes(16);
        std::string boundary = "----sgcl-";
        static constexpr char hex[] = "0123456789ABCDEF";
        for (auto b : r) {
            boundary += hex[uint8_t(b) >> 4];
            boundary += hex[uint8_t(b) & 15];
        }
        std::string out = "MIME-Version: 1.0\r\nContent-Type: multipart/signed; protocol=\"application/pkcs7-signature\"; micalg=" + std::string(micalg(key.kind())) +
                          "; boundary=\"" + boundary + "\"\r\n\r\nThis is an S/MIME signed message\r\n\r\n--" + boundary + "\r\n" + body + "\r\n--" + boundary +
                          "\r\nContent-Type: application/pkcs7-signature; name=\"smime.p7s\"\r\nContent-Transfer-Encoding: base64\r\nContent-Disposition: attachment; "
                          "filename=\"smime.p7s\"\r\n\r\n" + base64_lines(sd) + "\r\n--" + boundary + "--\r\n";
        return bytes_of(out);
    }

    inline vector<byte> sign(const slice<const byte>& entity, const x509::certificate& signer, const x509::signing_key& key) {
        return smime::sign(entity, signer, key, cms::sign_options());
    }

    // A signed entity verified: a multipart/signed (the first part's bytes
    // verified against its detached signature) or an application/pkcs7-mime
    // of signed-data. The content of the result is the signed entity
    // (canonical); errc::malformed for what is neither, the errors of
    // cms::verify otherwise
    inline expected<cms::verified, error> verify(const slice<const byte>& message, const cms::verify_options& o) noexcept {
        using namespace crypto::detail::smime;
        try {
            const std::string m = canonical(std::string_view(reinterpret_cast<const char*>(message.data()), message.size()));
            const Entity e = read_entity(m);
            if (!e.ok) {
                return unexpected(error(errc::malformed, string("sgcl::crypto::smime: no header fields and body")));
            }
            if (e.content_type == "application/pkcs7-mime" || e.content_type == "application/x-pkcs7-mime") {
                auto der = base64_body(std::string_view(m).substr(e.body));
                if (!der) {
                    return unexpected(der.error());
                }
                return cms::verify(*der, o);
            }
            if (e.content_type != "multipart/signed") {
                return unexpected(error(errc::malformed, string("sgcl::crypto::smime: neither multipart/signed nor application/pkcs7-mime")));
            }
            const std::string boundary = param(e.content_type_field, "boundary");
            if (boundary.empty()) {
                return unexpected(error(errc::malformed, string("sgcl::crypto::smime: a multipart/signed without its boundary")));
            }
            const std::string delim = "--" + boundary;
            std::string_view body = std::string_view(m).substr(e.body);
            // the delimiter lines: at the start of the body or after a CRLF
            auto find_delim = [&](size_t from) -> size_t {
                for (size_t at = from;;) {
                    at = body.find(delim, at);
                    if (at == std::string_view::npos) {
                        return at;
                    }
                    if (at == 0 || (at >= 2 && body[at - 2] == '\r' && body[at - 1] == '\n')) {
                        return at;
                    }
                    ++at;
                }
            };
            size_t d1 = find_delim(0);
            if (d1 == std::string_view::npos) {
                return unexpected(error(errc::malformed, string("sgcl::crypto::smime: no first part")));
            }
            size_t p1 = body.find("\r\n", d1);
            if (p1 == std::string_view::npos) {
                return unexpected(error(errc::malformed, string("sgcl::crypto::smime: no first part")));
            }
            p1 += 2;
            size_t d2 = find_delim(p1);
            if (d2 == std::string_view::npos || d2 < p1 + 2) {
                return unexpected(error(errc::malformed, string("sgcl::crypto::smime: no second part")));
            }
            const std::string_view signed_part = body.substr(p1, d2 - 2 - p1);   // the CRLF before a delimiter is the delimiter's
            size_t p2 = body.find("\r\n", d2);
            size_t d3 = p2 == std::string_view::npos ? p2 : find_delim(p2 + 2);
            if (d3 == std::string_view::npos || d3 < p2 + 2) {
                return unexpected(error(errc::malformed, string("sgcl::crypto::smime: no signature part")));
            }
            const std::string_view sig_part = body.substr(p2 + 2, d3 - p2 - 2);
            const Entity se = read_entity(sig_part);
            if (!se.ok || (se.content_type != "application/pkcs7-signature" && se.content_type != "application/x-pkcs7-signature")) {
                return unexpected(error(errc::malformed, string("sgcl::crypto::smime: the second part is not application/pkcs7-signature")));
            }
            auto der = se.transfer_encoding == "base64" ? base64_body(sig_part.substr(se.body))
                                                        : expected<vector<byte>, error>(bytes_of(std::string(sig_part.substr(se.body))));
            if (!der) {
                return unexpected(der.error());
            }
            return cms::verify_detached(*der, slice<const byte>(reinterpret_cast<const byte*>(signed_part.data()), signed_part.size()), o);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (...) {
            return unexpected(error(errc::malformed, string("sgcl::crypto::smime: bytes that do not read")));
        }
    }

    // A MIME entity encrypted to the certificates' holders: the bytes of an
    // application/pkcs7-mime (authEnveloped-data, or enveloped-data with
    // content_cipher::aes256_cbc), with MIME-Version
    inline vector<byte> encrypt(const slice<const byte>& entity, const x509::chain& recipients, const cms::encrypt_options& o) {
        using namespace crypto::detail::smime;
        const std::string body = canonical(std::string_view(reinterpret_cast<const char*>(entity.data()), entity.size()));
        auto env = cms::encrypt(slice<const byte>(reinterpret_cast<const byte*>(body.data()), body.size()), recipients, o);
        const char* type = o.cipher == cms::content_cipher::aes256_gcm ? "authEnveloped-data" : "enveloped-data";
        std::string out = std::string("MIME-Version: 1.0\r\nContent-Type: application/pkcs7-mime; smime-type=") + type +
                          "; name=\"smime.p7m\"\r\nContent-Transfer-Encoding: base64\r\nContent-Disposition: attachment; filename=\"smime.p7m\"\r\n\r\n" +
                          base64_lines(env);
        return bytes_of(out);
    }

    inline vector<byte> encrypt(const slice<const byte>& entity, const x509::chain& recipients) {
        return smime::encrypt(entity, recipients, cms::encrypt_options());
    }

    // The entity of an application/pkcs7-mime (enveloped-data or
    // authEnveloped-data) for the recipient's certificate and key:
    // errc::malformed for another entity, the errors of cms::decrypt otherwise
    inline expected<vector<byte>, error> decrypt(const slice<const byte>& message, const x509::certificate& recipient, const x509::signing_key& key) noexcept {
        using namespace crypto::detail::smime;
        try {
            const std::string m(reinterpret_cast<const char*>(message.data()), message.size());
            const Entity e = read_entity(m);
            if (!e.ok || (e.content_type != "application/pkcs7-mime" && e.content_type != "application/x-pkcs7-mime")) {
                return unexpected(error(errc::malformed, string("sgcl::crypto::smime: not application/pkcs7-mime")));
            }
            auto der = base64_body(std::string_view(m).substr(e.body));
            if (!der) {
                return unexpected(der.error());
            }
            return cms::decrypt(*der, recipient, key);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (...) {
            return unexpected(error(errc::malformed, string("sgcl::crypto::smime: bytes that do not read")));
        }
    }
}
