//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../secret.h"
#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../encoding/base64.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace sgcl::crypto::detail {
    // A private key in PEM (RFC 7468): its label and its DER. The text is
    // read where it lies; the base64 between the boundaries goes through
    // encoding's base64 straight into a secret_bytes (decode_to into the
    // caller's buffer, the lenient codec skipping the line endings), never
    // into a string or a managed vector. encoding::pem is the module's
    // reader of PEM in general; its bytes are a managed vector, where a
    // private key's DER must not be.
    struct KeyPem {
        std::string_view label;     // "PRIVATE KEY", "EC PRIVATE KEY", "RSA PRIVATE KEY"
        secret_bytes der;
    };

    inline constexpr std::string_view KeyLabels[] = {"PRIVATE KEY", "EC PRIVATE KEY", "RSA PRIVATE KEY"};

    inline error key_pem_error(const char* what) {
        return error(errc::malformed, string(what));
    }

    // The first block of the text whose label is a private key's (text
    // before, between and after blocks passed over, as RFC 7468 §5.2 lets
    // it be; blocks of other labels too: OpenSSL writes "EC PARAMETERS"
    // before "EC PRIVATE KEY"). A key encrypted in the old way (RFC 1421
    // headers) or as PKCS #8's EncryptedPrivateKeyInfo is errc::unsupported.
    inline expected<KeyPem, error> read_key_pem(const slice<const byte>& text) {
        const std::string_view v(reinterpret_cast<const char*>(text.data()), text.size());
        size_t from = 0;
        for (;;) {
            const size_t begin = v.find("-----BEGIN ", from);
            if (begin == std::string_view::npos) {
                return unexpected(key_pem_error("PEM: no private key block"));
            }
            const size_t label_at = begin + 11;
            const size_t label_end = v.find("-----", label_at);
            if (label_end == std::string_view::npos) {
                return unexpected(key_pem_error("PEM: a BEGIN line with no end"));
            }
            const std::string_view label = v.substr(label_at, label_end - label_at);
            const size_t body = label_end + 5;
            std::string end_line = "-----END ";   // the label is not secret
            end_line.append(label);
            end_line += "-----";
            const size_t end = v.find(end_line, body);
            if (end == std::string_view::npos) {
                return unexpected(key_pem_error("PEM: no END line for the BEGIN line"));
            }
            if (label == "ENCRYPTED PRIVATE KEY") {
                return unexpected(error(errc::unsupported, string("PEM: an encrypted private key (PKCS #8 EncryptedPrivateKeyInfo)")));
            }
            std::string_view match;
            for (auto k : KeyLabels) {
                if (label == k) {
                    match = k;
                }
            }
            if (match.empty()) {
                from = end + end_line.size();
                continue;
            }
            const std::string_view b = v.substr(body, end - body);
            if (b.find(':') != std::string_view::npos) {
                return unexpected(error(errc::unsupported, string("PEM: a key encrypted with RFC 1421 headers (Proc-Type, DEK-Info)")));
            }
            const auto& codec = encoding::base64::standard.lenient();
            KeyPem out{match, secret_bytes(codec.max_decoded_size(b.size()))};
            auto n = codec.decode_to(out.der.as_slice(), slice<const char>(b.data(), b.size()));
            if (!n) {
                return unexpected(key_pem_error("PEM: the base64 of the key does not decode"));
            }
            out.der.resize(*n);
            return out;
        }
    }

    // The block of a label over the DER: the BEGIN line, the base64 in
    // lines of 64 characters (48 bytes a line through encoding's
    // encode_to), the END line, "\n" after each; written straight into a
    // secret_bytes of the exact size
    inline secret_bytes write_key_pem(std::string_view label, const slice<const byte>& der) {
        const auto& codec = encoding::base64::standard;
        const size_t chars = codec.encoded_size(der.size());
        const size_t lines = (chars + 63) / 64;
        const size_t size = (11 + label.size() + 6) + chars + lines + (9 + label.size() + 6);
        secret_bytes out(size);
        char* o = reinterpret_cast<char*>(out.as_slice().data());
        auto put = [&](std::string_view s) {
            std::copy(s.begin(), s.end(), o);
            o += s.size();
        };
        put("-----BEGIN ");
        put(label);
        put("-----\n");
        for (size_t i = 0; i < der.size(); i += 48) {
            const size_t k = std::min<size_t>(48, der.size() - i);
            o += codec.encode_to(slice<char>(o, 64), der.subslice(i, k));
            *o++ = '\n';
        }
        put("-----END ");
        put(label);
        put("-----\n");
        return out;
    }
}
