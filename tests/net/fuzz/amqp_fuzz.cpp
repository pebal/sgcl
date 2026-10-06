//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::amqp's codec on any bytes; the first byte picks what the rest is:
//   0  a field table: one that reads is written back and reads the same
//   1  the basic class's properties: the same round trip
//   2  frames: each head read, its payload as a method's arguments (the
//      domains in turn), a content header's properties
// The readers run on libFuzzer's own bytes (their end the input's) or a
// malloc'd block of exactly the bytes written, never a managed copy: a
// read past the end is ASan's to see.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/amqp_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/amqp.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace ad = sgcl::net::amqp::detail;
    namespace amqp = sgcl::net::amqp;

    // The bytes in a malloc'd block of exactly their size
    class Exact {
    public:
        explicit Exact(std::string_view s)
        : _p(static_cast<char*>(std::malloc(s.size()))), _n(s.size()) {
            std::copy_n(s.data(), s.size(), _p);
        }

        Exact(const Exact&) = delete;
        Exact& operator=(const Exact&) = delete;

        ~Exact() {
            std::free(_p);
        }

        std::string_view view() const noexcept {
            return std::string_view(_p, _n);
        }

    private:
        char* _p;
        size_t _n;
    };

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void table(std::string_view bytes) {
        ad::AmqpReader r(bytes);
        auto t = r.table();
        if (!r.ok) {
            return;
        }
        std::string out;
        ad::AmqpWriter w(out);
        w.table(t);
        Exact again(out);
        ad::AmqpReader r2(again.view());
        auto t2 = r2.table();
        check(r2.ok && r2.done());
        std::string out2;   // compared as bytes: a NaN is not equal to itself
        ad::AmqpWriter w2(out2);
        w2.table(t2);
        check(out2 == out);
    }

    void properties(std::string_view bytes) {
        ad::AmqpReader r(bytes);
        auto p = r.properties();
        if (!r.ok) {
            return;
        }
        std::string out;
        ad::AmqpWriter w(out);
        w.properties(p);
        Exact again(out);
        ad::AmqpReader r2(again.view());
        auto p2 = r2.properties();
        check(r2.ok && r2.done());
        std::string out2;
        ad::AmqpWriter w2(out2);
        w2.properties(p2);
        check(out2 == out);
    }

    void frames(std::string_view bytes) {
        for (int i = 0; i < 64; ++i) {
            uint8_t type = 0;
            uint16_t channel = 0;
            uint32_t size = 0;
            if (!ad::amqp_frame_head(bytes, type, channel, size) || bytes.size() < size_t(size) + 8) {
                return;
            }
            std::string_view payload = bytes.substr(7, size);
            bytes.remove_prefix(size_t(size) + 8);
            ad::AmqpReader r(payload);
            if (type == ad::FrameHeader) {
                (void)r.u16();
                (void)r.u16();
                (void)r.u64();
                (void)r.properties();
                continue;
            }
            (void)r.u32();
            // the domains in the order the payload's bytes pick
            for (int k = 0; k < 16 && r.ok && !r.done(); ++k) {
                switch (k % 6) {
                    case 0: (void)r.shortstr(); break;
                    case 1: (void)r.u16(); break;
                    case 2: (void)r.bit(); break;
                    case 3: (void)r.longstr(); break;
                    case 4: (void)r.table(); break;
                    case 5: (void)r.u64(); break;
                }
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    std::string_view text(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (data[0] % 3) {
        case 0: table(text); break;
        case 1: properties(text); break;
        case 2: frames(text); break;
    }
    return 0;
}
