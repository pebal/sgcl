//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the revocation harnesses share (ocsp_response_fuzz.cpp, crl_fuzz.cpp,
// ocsp_request_fuzz.cpp): the certificates of tests/crypto/data/revocation,
// read once from the root of the tree (the harnesses run from there, as
// tests/fuzz/run.sh is run), and a check that aborts.
#pragma once

#include "sgcl/core.h"
#include "sgcl/crypto/crypto.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace revocation_fuzz {
    using namespace sgcl;
    namespace x509 = crypto::x509;

    inline void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "revocation fuzz: %s\n", what);
            std::abort();
        }
    }

    inline slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    // A certificate of the fixtures; aborts when it is not there (run from
    // the root of the tree)
    inline x509::certificate fixture(const char* name) {
        std::ifstream in(std::string("tests/crypto/data/revocation/") + name + ".pem");
        std::stringstream s;
        s << in.rdbuf();
        auto c = x509::certificate::from_pem(string(s.str()));
        check(c.has_value(), "the fixtures of tests/crypto/data/revocation (run from the root of the tree)");
        return *c;
    }

    // A verification error is errc::verification with a reason, never an
    // exception
    template<class E>
    void verification_error(const E& e) {
        check(e.code() == crypto::errc::verification && e.reason() != x509::reason::none, "a verification failure without errc::verification and a reason");
    }
}
