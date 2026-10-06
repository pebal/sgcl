//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// bcrypt on any bytes. A string starting with '$': read or refused without
// a fault, with malformed or unsupported alone; when read, written back by
// the module's own format and read again to the same cost, salt and hash,
// and verified when its cost is small. Anything else: a password, hashed at
// the cost 4 and verified, refused past 72 bytes, and verified as a password
// one byte longer (equal when it is 72 bytes or more, different under).
// No other library of bcrypt links into C++ here (Go's is the oracle of the
// tests' vectors), so the harness holds the module to itself and to the
// rule. A difference aborts.
//
//   sh tests/fuzz/run.sh tests/crypto/fuzz/bcrypt_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
    using namespace sgcl;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "bcrypt_fuzz: %s\n", what);
            std::abort();
        }
    }

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size != 0 && data[0] == '$') {
        slice<const char> text(reinterpret_cast<const char*>(data), size);
        crypto::detail::BcryptHash parsed;
        auto r = crypto::detail::bcrypt_parse(text, parsed);
        if (!r) {
            check(r.error().code() == crypto::errc::malformed || r.error().code() == crypto::errc::unsupported, "an error of another code");
            return 0;
        }
        string again = crypto::detail::bcrypt_format(parsed.cost, parsed.salt, parsed.hash);
        crypto::detail::BcryptHash second;
        check(bool(crypto::detail::bcrypt_parse(slice<const char>(again.data(), again.size()), second)), "a hash written is not read");
        check(second.cost == parsed.cost && std::memcmp(second.salt, parsed.salt, 16) == 0 && std::memcmp(second.hash, parsed.hash, 23) == 0,
              "a hash written is read otherwise");
        if (parsed.cost <= 5) {
            auto v = crypto::bcrypt::verify("password", string(text.data(), text.size()));
            check(v || v.error().code() == crypto::errc::authentication, "verify of a hash read");
        }
        return 0;
    }
    auto h = crypto::bcrypt::generate(view(data, size), 4);
    if (size > 72) {
        check(!h && h.error().code() == crypto::errc::invalid_key, "a password past 72 bytes generated");
        return 0;
    }
    check(h.has_value(), "generate");
    check(bool(crypto::bcrypt::verify(view(data, size), *h)), "verify of its own hash");
    std::string longer(reinterpret_cast<const char*>(data), size);
    longer += 'x';
    bool same = bool(crypto::bcrypt::verify(slice<const byte>(reinterpret_cast<const byte*>(longer.data()), longer.size()), *h));
    check(same == (size >= 72), "the 72-byte rule");
    return 0;
}
