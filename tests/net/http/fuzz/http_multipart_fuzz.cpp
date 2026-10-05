//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::multipart_reader on any bytes, cut in pieces at random, and
// http::form written and read back. The first byte picks the path (bit 0),
// the size of the pieces the body comes in (bits 1-3), of the reads
// (bits 4-6) and a boundary of 1 or 70 characters (bit 7). What must hold:
//   - the parts, their heads and their contents, and the error a body ends
//     with, are the same whatever pieces the body came in and whatever
//     reads took the contents: the reader keeps nothing of a cut but what
//     it must;
//   - a read gives at most what was asked and at most the reader's window;
//     after next() said the end, it says it again and reads give 0;
//   - with the limits at their least (no part, a head of one byte) a body
//     never gives a part past them;
//   - a form of fields (the input split at NUL bytes: names and values of
//     any bytes) written and read back is the same fields, the names with
//     their control bytes percent-encoded as written, and its length is its bytes'.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/http_multipart_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/form.h"
#include "sgcl/net/http/multipart.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Pieces : io::mixin::reader<Pieces> {
        std::string data;
        size_t at = 0;
        size_t piece = 0;

        expected<size_t, io::error> read(const slice<byte>& out) {
            size_t n = std::min(out.size(), data.size() - at);
            if (piece) {
                n = std::min(n, piece);
            }
            std::copy(data.data() + at, data.data() + at + n, reinterpret_cast<char*>(out.data()));
            at += n;
            return n;
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
            co_return read(out);
        }
    };

    std::vector<std::string> parts(std::string_view body, const std::string& boundary, size_t piece, size_t read_size,
                                   const net::http::multipart_reader::limits& l) {
        tracked_ptr src = make_tracked<Pieces>();
        src->data = std::string(body);
        src->piece = piece;
        net::http::multipart_reader m(io::reader(src), string(std::string_view(boundary)), l);
        std::vector<std::string> out;
        vector<byte> buf(read_size);
        for (size_t guard = 0; guard < 10000; ++guard) {
            auto p = m.next();
            if (!p) {
                out.push_back("error " + std::to_string(p.error().code().value()) + " " + p.error().code().category().name());
                return out;
            }
            if (!*p) {
                auto again = m.next();
                check(again && !*again);
                auto n = m.read(buf.as_slice());
                check(n && *n == 0);
                return out;
            }
            std::string item = std::string((*p)->name.view()) + '\0' + std::string((*p)->filename.view()) + '\0' + std::string((*p)->content_type.view()) + '\0';
            for (;;) {
                auto n = m.read(buf.as_slice());
                if (!n) {
                    out.push_back(item);
                    out.push_back("error " + std::to_string(n.error().code().value()) + " " + n.error().code().category().name());
                    return out;
                }
                check(*n <= read_size && *n <= net::http::detail::MultipartWindow);
                if (*n == 0) {
                    break;
                }
                item.append(reinterpret_cast<const char*>(buf.data()), *n);
            }
            out.push_back(item);
        }
        return out;
    }

    void reader(uint8_t mode, std::string_view body) {
        static const size_t sizes[] = {1, 2, 3, 7, 64, 333, 4096, 0};
        const std::string boundary = mode & 0x80 ? std::string(70, 'B') : std::string("XyZ");
        net::http::multipart_reader::limits l;
        auto whole = parts(body, boundary, 0, 65536, l);
        auto cut = parts(body, boundary, sizes[(mode >> 1) & 7], sizes[(mode >> 4) & 7] ? sizes[(mode >> 4) & 7] : 1, l);
        check(whole == cut);
        net::http::multipart_reader::limits least;
        least.max_parts = 0;
        least.max_header_bytes = 1;
        auto none = parts(body, boundary, sizes[(mode >> 1) & 7], 512, least);
        check(none.size() <= 1 && (none.empty() || none[0].rfind("error ", 0) == 0));
    }

    void form(std::string_view input) {
        net::http::form f;
        std::vector<std::pair<std::string, std::string>> fields;
        size_t at = 0;
        while (at <= input.size() && fields.size() < 64) {
            size_t a = input.find('\0', at);
            std::string name(input.substr(at, a == std::string_view::npos ? std::string_view::npos : a - at));
            std::string value;
            if (a != std::string_view::npos) {
                size_t b = input.find('\0', a + 1);
                value = std::string(input.substr(a + 1, b == std::string_view::npos ? std::string_view::npos : b - a - 1));
                at = b == std::string_view::npos ? input.size() + 1 : b + 1;
            } else {
                at = input.size() + 1;
            }
            f.add(string(std::string_view(name)), string(std::string_view(value)));
            fields.emplace_back(name, value);
        }
        auto r = f.reader();
        check(r.has_value());
        auto bytes = r->read_all();
        check(bytes.has_value());
        check(f.content_length().value() == bytes->size());
        std::string body(reinterpret_cast<const char*>(bytes->data()), bytes->size());
        auto back = parts(body, std::string(f.boundary().view()), 5, 100, net::http::multipart_reader::limits());
        check(back.size() == fields.size());
        for (size_t i = 0; i < fields.size(); ++i) {
            std::string name;
            for (char c : fields[i].first) {
                const auto u = uint8_t(c);
                if ((u < 0x20 && c != '\t') || u == 0x7F) {
                    name += '%';
                    name += "0123456789ABCDEF"[u >> 4];
                    name += "0123456789ABCDEF"[u & 15];
                } else {
                    name += c;
                }
            }
            check(back[i] == name + '\0' + '\0' + '\0' + fields[i].second);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 200000) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    if (mode & 1) {
        form(rest);
    } else {
        reader(mode, rest);
    }
    return 0;
}
