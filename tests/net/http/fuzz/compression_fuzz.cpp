//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Content-Encoding on any bytes, a round trip: the input is a byte of
// choices, an Accept-Encoding line, and a body. The server's compression
// middleware answers it through a recorder (whole, or flushed in pieces of
// the size the choice byte gives); what must hold:
//   - never a crash; the coding chosen is one the field takes with q > 0
//     (or identity), and the server's order decides;
//   - the body decoded by compress (gzip, zlib, brotli, zstd) is the body written,
//     byte for byte, whole or flushed;
//   - Vary: Accept-Encoding on every compressible response.
// And the readers alone: choose_coding of the line over both orders never
// names a coding the line refuses with q=0.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/compression_fuzz.cpp)
// or the library's own driver.
#include "sgcl/net/http/http.h"
#include "sgcl/compress/compress.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace http = sgcl::net::http;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
    // A copy of exactly the bytes in malloc memory, for a reader to parse:
    // ASan sees a read past its end, which in a managed copy (or a string's
    // spare capacity) it would not
    struct Exact {
        char* p;
        size_t n;

        explicit Exact(std::string_view v)
        : p(static_cast<char*>(std::malloc(v.size() ? v.size() : 1))), n(v.size()) {
            std::copy(v.begin(), v.end(), p);
        }

        Exact(const Exact&) = delete;
        Exact& operator=(const Exact&) = delete;

        ~Exact() {
            std::free(p);
        }

        std::string_view view() const noexcept {
            return std::string_view(p, n);
        }
    };


    std::string text(const string& s) {
        return std::string(s.data(), s.size());
    }

    // "gzip;q=0" (or "gzip; q=0.000"...) in the line, by the reader's own rule
    bool refused(std::string_view line, std::string_view coding) {
        vector<string> one = {string(coding)};
        Exact exact(line);
        return http::detail::choose_coding(exact.view(), one).empty();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    const uint8_t choice = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    const size_t nl = rest.find('\n');
    const std::string_view accept = rest.substr(0, nl);
    const std::string body(nl == std::string_view::npos ? std::string_view() : rest.substr(nl + 1));
    const bool flushed = choice & 1;
    const size_t piece = 1 + (choice >> 1) * 37;

    // the four codings in an order the choice byte rotates
    const string all[4] = {string("zstd"), string("br"), string("gzip"), string("deflate")};
    vector<string> order;
    for (int k = 0; k < 4; ++k) {
        order.push_back(all[(k + (choice >> 6)) % 4]);
    }
    Exact exact_accept(accept);   // the reader parses exactly the field's bytes
    const std::string_view coding = http::detail::choose_coding(exact_accept.view(), order);
    check(coding.empty() || coding == "gzip" || coding == "deflate" || coding == "br" || coding == "zstd");
    if (!coding.empty()) {
        check(!refused(accept, coding));
    }

    http::server s;
    s.use(http::compression({.encodings = order, .min_size = 0}));
    s.route("/", [&body, flushed, piece](http::request, http::response_writer w) -> async::task<> {
        w.set_header("Content-Type", "text/plain");
        if (!flushed) {
            w.write(string(body));
            co_return;
        }
        for (size_t at = 0; at < body.size(); at += piece) {
            w.write(string(std::string_view(body).substr(at, piece)));
            (void)co_await w.async_flush();
        }
    });
    auto req = http::test_request("GET", "/");
    req.headers().add("Accept-Encoding", string(accept));
    http::response_recorder rec;
    rec.serve(s, req);
    check(text(rec.header("Vary")) == "Accept-Encoding");
    const std::string got = text(rec.body());
    const std::string used = text(rec.header("Content-Encoding"));
    check(used == std::string(coding));
    const slice<const byte> coded(reinterpret_cast<const byte*>(got.data()), got.size());
    auto same = [&](const auto& plain) {
        check(plain && std::string(reinterpret_cast<const char*>(plain->data()), plain->size()) == body);
    };
    if (used == "gzip") {
        same(compress::gzip::decompress(coded));
    } else if (used == "deflate") {
        same(compress::zlib::decompress(coded));
    } else if (used == "br") {
        same(compress::brotli::decompress(coded));
    } else if (used == "zstd") {
        same(compress::zstd::decompress(coded));
    } else {
        check(got == body);
    }
    return 0;
}
