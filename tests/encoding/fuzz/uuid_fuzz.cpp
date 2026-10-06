//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::uuid::parse on any text. What must hold: a text read writes back
// as the canonical form of its sixteen bytes, which reads again to the same
// UUID, in each of the four forms; a text refused is refused at an offset
// within it (or at its end, for a wrong length), and only a text of 32, 36,
// 38 or 45 characters is ever read; the sixteen bytes of the input read
// from_bytes come back by bytes().
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/uuid_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 4096) {
        return 0;
    }
    std::string text(reinterpret_cast<const char*>(data), size);
    auto u = uuid::parse(string(text));
    if (u) {
        check(size == 32 || size == 36 || size == 38 || size == 45);
        auto canonical = u->to_string();
        check(canonical.size() == 36);
        check(uuid::parse(canonical).value() == *u);
        std::string bare;
        for (char c : canonical.view()) {
            if (c != '-') {
                bare += c;
            }
        }
        check(uuid::parse(string(bare)).value() == *u);
        check(uuid::parse(string("{") + canonical + "}").value() == *u);
        check(uuid::parse(string("urn:uuid:") + canonical).value() == *u);
        check(uuid(u->bytes()) == *u);
    } else {
        check(u.error().offset() <= size);
        check(u.error().code() == errc::syntax || u.error().code() == errc::invalid_character);
    }
    if (size >= 16) {
        auto b = uuid::from_bytes(slice<const byte>(reinterpret_cast<const byte*>(data), 16));
        check(b && std::memcmp(b->bytes().data(), data, 16) == 0);
    }
    return 0;
}
