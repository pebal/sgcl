//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// JSON Web Keys and key sets on any bytes: jwk::parse, jwk_set::parse and
// the scanner under them (detail::JsonPlaces), with encoding::json as the
// oracle of what a JSON object is.
//
//   - text encoding::json reads as an object with no key given twice and
//     nesting under the scanner's bound is text the scanner reads, with the
//     same keys in the same order;
//   - a key read writes its private JSON, which reads to the same key and
//     writes the same text again; its public JSON reads to its public half,
//     of the same thumbprint;
//   - a key set read writes its private JSON, which reads to as many keys.
//
// A disagreement aborts; ASan and UBSan catch the rest.
//
//   tests/fuzz/run.sh tests/crypto/fuzz/jose_jwk_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    namespace jose = sgcl::crypto::jose;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "jose_jwk_fuzz: %s\n", what);
            std::abort();
        }
    }

    std::string text(const crypto::secret_bytes& b) {
        return std::string(reinterpret_cast<const char*>(b.as_slice().data()), b.size());
    }

    slice<const byte> bytes(const std::string& s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    size_t depth(const encoding::json& j) {
        size_t d = 0;
        for (const auto& e : j.elements()) {
            d = std::max(d, depth(e));
        }
        for (const auto& m : j.members()) {
            d = std::max(d, depth(m.value));
        }
        return j.is_array() || j.is_object() ? d + 1 : 0;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    // the scanner against encoding::json
    std::vector<crypto::detail::JsonPlace> places;
    const bool scanned = crypto::detail::JsonPlaces::read(input, places);
    auto j = encoding::json::parse(string(input));
    if (j && j->is_object() && depth(*j) <= crypto::detail::JsonPlaces::max_depth) {
        check(scanned, "a JSON object the scanner does not read");
        check(places.size() == j->size(), "the scanner finds another count of members");
        for (size_t i = 0; i < places.size(); ++i) {
            check(places[i].key == std::string_view(j->members()[i].key.view()), "the scanner reads another key");
        }
    }
    // a key
    // parsed from libFuzzer's buffer itself (ASan sees no read past a managed copy)
    auto k = crypto::detail::JoseTextAccess::jwk_of(input);
    if (k) {
        check(scanned, "a key of text the scanner does not read");
        auto priv = k->to_private_json();
        auto again = jose::jwk::parse(priv);
        check(bool(again), "a key's private JSON does not read");
        check(*again == *k, "a key's private JSON reads to another key");
        check(text(again->to_private_json()) == text(priv), "a key's private JSON is not written the same again");
        check(again->thumbprint() == k->thumbprint(), "the thumbprint changes through the private JSON");
        if (k->type() != jose::key_type::oct) {
            auto pub = jose::jwk::parse(k->to_json());
            check(bool(pub), "a key's public JSON does not read");
            check(*pub == k->public_key(), "a key's public JSON reads to another key than its public half");
            check(pub->thumbprint() == k->thumbprint(), "the thumbprint changes through the public JSON");
        }
    }
    // a key set
    auto s = crypto::detail::JoseTextAccess::jwk_set_of(input);
    if (s) {
        auto again = jose::jwk_set::parse(s->to_private_json());
        check(bool(again) && again->size() == s->size(), "a key set's private JSON does not read to as many keys");
    }
    return 0;
}
