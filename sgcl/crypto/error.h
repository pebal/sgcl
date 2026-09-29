//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/string.h"

#include <cstdint>
#include <string>
#include <system_error>

namespace sgcl::crypto {
    namespace detail { using namespace sgcl::detail; }
    // What went wrong in data the module was given: a tag that does not
    // match, a key or a signature that cannot be one, an encoding that
    // cannot be read. One list for the whole module, as compress and
    // encoding have one each. A broken contract — a key of the wrong
    // length given by the program, a nonce of the wrong size, more output
    // than an algorithm can give — is not here: it is std::invalid_argument,
    // thrown. The values start at 1: an error_code of 0 is success.
    enum class errc : uint8_t {
        authentication = 1,   // an AEAD's tag does not match: the data, the aad, the nonce or the key is not the sender's
        invalid_key,          // a key from data that cannot be one: a point off the curve, a zero shared secret, a wrong size
        invalid_signature,    // a signature that cannot be one: out of range, badly encoded
        malformed,            // DER, ASN.1 or PEM that cannot be read
        unsupported,          // an algorithm, a curve or a parameter the module does not do
        verification          // a certificate chain that does not verify
    };

    namespace detail {
        class CryptoCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "crypto";
            }

            std::string message(int c) const override {
                switch (static_cast<errc>(c)) {
                    case errc::authentication: return "message authentication failed";
                    case errc::invalid_key: return "invalid key";
                    case errc::invalid_signature: return "invalid signature";
                    case errc::malformed: return "malformed data";
                    case errc::unsupported: return "unsupported algorithm or parameter";
                    case errc::verification: return "verification failed";
                }
                return "unknown crypto error";
            }
        };
    }

    inline const std::error_category& crypto_category() noexcept {
        static const detail::CryptoCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), crypto_category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::crypto::errc> : std::true_type {};

namespace sgcl::crypto::x509 {
    // Why a certificate chain does not verify (§11 A7 of the design):
    // the reason() of an error whose code is errc::verification, none for
    // every other error. Go's x509 has these as the types and the
    // InvalidReason of its errors; here they are one list, read by a
    // switch. The certificate at fault is named in the error's message.
    enum class reason : uint8_t {
        none = 0,                       // not a verification error
        expired,                        // a certificate of the chain is past its not_after
        not_yet_valid,                  // a certificate of the chain is before its not_before
        unknown_authority,              // no chain leads to a root of the pool
        hostname_mismatch,              // the leaf is not for the DNS name or the IP address asked
        name_constraints,               // a name of the chain is outside a CA's name constraints
        unsupported_algorithm,          // a signature or a key of an algorithm the module does not verify
        insecure_algorithm,             // a signature over MD5 or SHA-1, never accepted
        invalid_signature,              // a signature that does not verify under its issuer's key
        too_many_intermediates,         // more than ten intermediates, or more signatures tried than a hundred
        path_length,                    // a CA's path length constraint is exceeded
        not_a_ca,                       // an intermediate without basicConstraints cA
        missing_cert_sign,              // a CA whose keyUsage lacks keyCertSign
        incompatible_usage,             // no extended key usage asked is allowed by the whole chain
        unhandled_critical_extension,   // a critical extension the module does not know
        too_many_constraints            // name constraints that would take too many comparisons
    };
}

namespace sgcl::crypto {
    // The error of the module: the code, the byte of the input where it was
    // found when the input is an encoding (DER, PEM), and what message()
    // says in place of the code's own words when there is more to say. A
    // value: copied, compared, held in an expected. A certificate chain
    // that does not verify is errc::verification with its x509::reason.
    class error {
    public:
        error() = default;

        explicit error(errc code)
        : _code(code) {
        }

        error(errc code, const string& detail)
        : _code(code), _detail(detail) {
        }

        // A chain that does not verify: errc::verification and why
        error(x509::reason why, const string& detail)
        : _code(errc::verification), _reason(why), _detail(detail) {
        }

        // The code at a byte of an encoded input
        error(errc code, uint64_t offset)
        : _code(code), _offset(offset) {
        }

        error(errc code, uint64_t offset, const string& detail)
        : _code(code), _offset(offset), _detail(detail) {
        }

        errc code() const noexcept {
            return _code;
        }

        // Bytes from the start of the encoded input; 0 for data that is not
        // an encoding (a tag, a key's bytes)
        uint64_t offset() const noexcept {
            return _offset;
        }

        // Why a certificate chain does not verify; x509::reason::none for
        // an error that is not errc::verification
        x509::reason reason() const noexcept {
            return _reason;
        }

        // "message authentication failed", "offset 17: malformed data",
        // "offset 4: DER: length past the end"
        string message() const {
            std::string m;
            if (_offset != 0) {
                m = "offset ";
                m += std::to_string(_offset);
                m += ": ";
            }
            if (!_detail.empty()) {
                m.append(_detail.data(), _detail.size());
            } else {
                m += crypto_category().message(static_cast<int>(_code));
            }
            return string(m);
        }

        friend bool operator==(const error& a, const error& b) noexcept {
            return a._code == b._code && a._reason == b._reason && a._offset == b._offset && a._detail == b._detail;
        }

    private:
        errc _code = errc::malformed;
        x509::reason _reason = x509::reason::none;
        uint64_t _offset = 0;
        string _detail;
    };
}
