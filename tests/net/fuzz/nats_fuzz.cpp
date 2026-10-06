//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::nats's protocol on any bytes; the first byte picks what the rest is:
//   0  a server's bytes to the client's parser: each unit read; a message
//      with headers written back as HPUB reads the same headers
//   1  a header block: one that reads is written and reads the same
//   2  subjects: "filter\nsubject", validated and matched (a valid subject
//      matches itself, ">" every subject of a token or more)
//   3  an nkey seed's text, read; one that reads signs a nonce
// The parsers run on libFuzzer's own bytes (their end the input's) or a
// malloc'd block of exactly the bytes written, never a managed copy.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/nats_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/nats.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace nd = sgcl::net::nats::detail;
    namespace nats = sgcl::net::nats;

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

    void headers_round_trip(const nats::message& m) {
        if (!nd::nats_header_valid(m)) {
            return;
        }
        std::string block;
        nd::nats_write_headers(block, m);
        Exact b(block);
        nats::message back;
        check(nd::nats_read_headers(b.view(), back));
        check(back.headers == m.headers && back.status == m.status);
    }

    void stream(std::string_view bytes) {
        for (int i = 0; i < 64 && !bytes.empty(); ++i) {
            nd::NatsFrame f;
            const char* why = nullptr;
            long used = nd::nats_parse(bytes, 1 << 20, f, why);
            if (used <= 0) {
                check(used == 0 || why != nullptr);
                return;
            }
            check(size_t(used) <= bytes.size());
            bytes.remove_prefix(size_t(used));
            if (f.k == nd::NatsFrame::kind::msg && (!f.m.headers.empty() || f.m.status)) {
                headers_round_trip(f.m);
            }
        }
    }

    void block(std::string_view bytes) {
        nats::message m;
        if (nd::nats_read_headers(bytes, m)) {
            headers_round_trip(m);
        }
    }

    void subjects(std::string_view text) {
        size_t cut = text.find('\n');
        std::string_view filter = text.substr(0, cut);
        std::string_view subject = cut == std::string_view::npos ? std::string_view() : text.substr(cut + 1);
        bool fv = nd::nats_subject_valid(filter, true);
        bool sv = nd::nats_subject_valid(subject, false);
        if (sv) {
            check(nd::nats_match(subject, subject));
            check(nd::nats_match(">", subject));
            check(nd::nats_subject_valid(subject, true));
        }
        if (fv && sv) {
            (void)nd::nats_match(filter, subject);
        }
    }

    void seed(std::string_view text) {
        sgcl::string pub, sig;
        if (nd::nats_sign_nonce(text, "nonce", pub, sig)) {
            check(pub.size() == 56 && sig.size() == 86);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    std::string_view text(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (data[0] % 4) {
        case 0: stream(text); break;
        case 1: block(text); break;
        case 2: subjects(text); break;
        case 3: seed(text); break;
    }
    return 0;
}
