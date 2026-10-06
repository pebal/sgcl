//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// otpauth:// URIs on any bytes: read or refused without a fault, with
// malformed or unsupported alone; a key read is written by to_string and
// read again to the same type, secret, issuer, account (spaces at its start
// aside, which the format does not keep after the issuer), options and
// HOTP's counter, and its code verifies against it. The input's first byte, when
// it is not 'o', makes a key instead: its secret, issuer and account from
// the rest, written and read back the same way. A difference aborts.
//
//   sh tests/fuzz/run.sh tests/crypto/fuzz/otp_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "otp_fuzz: %s\n", what);
            std::abort();
        }
    }

    std::string_view trimmed(const string& s) {
        std::string_view v(s.data(), s.size());
        while (!v.empty() && v.front() == ' ') {
            v.remove_prefix(1);
        }
        return v;
    }

    void round_trip(const crypto::otp_key& k) {
        string uri = k.to_string();
        auto back = crypto::otp_key::parse(uri);
        check(back.has_value(), "a URI written is not read");
        check(back->type == k.type, "the type");
        check(k.type == crypto::otp_type::totp || back->counter == k.counter, "the counter");   // HOTP's alone: a TOTP URI has none
        check(back->secret == k.secret, "the secret");
        check(back->issuer == k.issuer, "the issuer");
        check(trimmed(back->account) == trimmed(k.account), "the account");
        check(back->options.algorithm == k.options.algorithm && back->options.digits == k.options.digits
                  && (k.type == crypto::otp_type::hotp || back->options.period == k.options.period),
              "the options");
        check(k.verify(k.code()).has_value(), "a code of the key does not verify");
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    if (data[0] == 'o' || data[0] == 'O') {
        auto k = crypto::otp_key::parse(string(reinterpret_cast<const char*>(data), size));
        if (!k) {
            check(k.error().code() == crypto::errc::malformed || k.error().code() == crypto::errc::unsupported, "an error of another code");
            return 0;
        }
        if (k->secret.size() == 0) {
            return 0;
        }
        round_trip(*k);
        return 0;
    }
    if (size < 5) {   // a secret of one byte at least
        return 0;
    }
    size_t a = 1 + data[1] % 40, b = data[2] % 20;
    const char* rest = reinterpret_cast<const char*>(data + 4);
    size_t left = size - 4;
    a = std::min(a, left);
    b = std::min(b, left - a);
    crypto::otp_key k;
    k.type = data[3] & 1 ? crypto::otp_type::hotp : crypto::otp_type::totp;
    k.secret = crypto::secret_bytes(a);
    std::memcpy(k.secret.as_slice().data(), rest, a);
    k.issuer = string(rest + a, b);
    k.account = string(rest + a + b, left - a - b);
    const crypto::hash_id ids[3] = {crypto::hash_id::sha1, crypto::hash_id::sha256, crypto::hash_id::sha512};
    k.options.algorithm = ids[data[3] % 3];
    k.options.digits = 6 + (data[3] >> 2) % 5;
    k.options.period = 1 + data[0];
    k.counter = uint64_t(data[1]) << 40 | data[2];
    round_trip(k);
    return 0;
}
