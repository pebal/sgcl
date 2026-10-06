//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::http::serve_content's answers and the readers under them
// (detail/content.h) on any bytes. The input: a byte for the method (GET,
// HEAD, PUT) and the options, two for the content's size (up to 4095 bytes
// of a known pattern), then lines "name: value" — the request's fields
// (Range, If-Range, If-Match, If-None-Match, If-Modified-Since,
// If-Unmodified-Since, or any) and, for "ETag" and "Last-Modified", the
// handler's own validators. What must hold, through a response recorder:
//   - never a crash, and a status of 200, 206, 304, 412 or 416;
//   - 200: the whole content (nothing for HEAD, its length in the field);
//   - 206 of one range: a Content-Range of the content's size whose bytes
//     are the body; of several: a multipart/byteranges body whose every
//     part is the content's bytes its Content-Range names, at most 100,
//     together at most the size;
//   - 304 and 412: no body; 416: no body but the error's, and a
//     Content-Range, when there is one, "bytes */size".
// And parse_ranges of the Range alone over any size: every range inside
// the representation, never empty; parse_content_range of the bytes:
// first <= last < size. Built with libFuzzer (tests/fuzz/run.sh
// tests/net/http/fuzz/serve_content_fuzz.cpp) or the library's own driver.
#include "sgcl/net/http/http.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
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

    std::string pattern(size_t n) {
        std::string s(n, '\0');
        for (size_t i = 0; i < n; ++i) {
            s[i] = char('a' + (i * 7 + i / 26) % 26);
        }
        return s;
    }

    struct Part {
        std::string range;
        std::string data;
    };

    // The parts of a multipart/byteranges body; false when it is not one
    bool parts_of(const std::string& ctype, const std::string& body, std::vector<Part>& out) {
        auto b = ctype.find("boundary=");
        if (b == std::string::npos) {
            return false;
        }
        const std::string delim = "--" + ctype.substr(b + 9);
        size_t at = 0;
        if (body.compare(0, delim.size(), delim) != 0) {
            return false;
        }
        for (;;) {
            at += delim.size();
            if (body.compare(at, 4, "--\r\n") == 0) {
                return at + 4 == body.size();
            }
            if (body.compare(at, 2, "\r\n") != 0) {
                return false;
            }
            at += 2;
            auto end = body.find("\r\n\r\n", at);
            if (end == std::string::npos) {
                return false;
            }
            Part p;
            auto cr = body.find("Content-Range: ", at);
            if (cr == std::string::npos || cr > end) {
                return false;
            }
            auto eol = body.find("\r\n", cr);
            p.range = body.substr(cr + 15, eol - cr - 15);
            auto next = body.find("\r\n" + delim, end + 4);
            if (next == std::string::npos) {
                return false;
            }
            p.data = body.substr(end + 4, next - end - 4);
            out.push_back(p);
            at = next + 2;
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 3) {
        return 0;
    }
    const uint8_t mode = data[0];
    const char* method = (mode & 3) == 0 ? "GET" : (mode & 3) == 1 ? "HEAD" : (mode & 3) == 2 ? "PUT" : "GET";
    net::http::serve_options o;
    o.etag = (mode >> 2) % 3 == 0 ? net::http::etag_kind::weak : (mode >> 2) % 3 == 1 ? net::http::etag_kind::strong : net::http::etag_kind::none;
    o.ranges = !(mode & 0x80);
    const size_t n = (size_t(data[1]) << 8 | data[2]) & 0xFFF;
    const std::string content = pattern(n);
    std::string_view rest(reinterpret_cast<const char*>(data + 3), size - 3);

    auto req = net::http::test_request(string(method), "/c.txt");
    net::http::response_recorder rec;
    auto w = rec.writer();
    std::string range_field;
    while (!rest.empty()) {
        auto nl = rest.find('\n');
        std::string_view line = rest.substr(0, nl);
        rest = nl == std::string_view::npos ? std::string_view() : rest.substr(nl + 1);
        auto colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            continue;
        }
        std::string_view name = line.substr(0, colon);
        std::string_view value = line.substr(colon + 1);
        while (!value.empty() && value.front() == ' ') {
            value.remove_prefix(1);
        }
        if (net::http::detail::iequal(name, "etag") || net::http::detail::iequal(name, "last-modified")) {
            w.headers().add(string(name), string(value));
        } else {
            req.headers().add(string(name), string(value));
            if (net::http::detail::iequal(name, "range") && range_field.empty()) {
                range_field.assign(value);
            }
        }
        // the readers alone, over the same bytes copied to exactly their size
        Exact exact(value);
        value = exact.view();
        const uint64_t any_size = uint64_t(value.size()) * 977 % 100000;
        auto rs = net::http::detail::parse_ranges(value, any_size);
        uint64_t total = 0;
        for (auto& r : rs.ranges) {
            check(r.length > 0 && r.start < any_size && r.start + r.length <= any_size);
            total += r.length;
        }
        check(rs.ranges.size() <= net::http::detail::MaxRanges);
        check(rs.verdict == net::http::detail::RangeVerdict::ranges ? !rs.ranges.empty() && total <= any_size : rs.ranges.empty());
        if (auto cr = net::http::detail::parse_content_range(value)) {
            if (!cr->unsatisfied) {
                check(cr->first <= cr->last && (!cr->size || cr->last < *cr->size));
            } else {
                check(bool(cr->size));
            }
        }
        (void)net::http::detail::etag_list_matches(value, "\"x\"", (mode & 0x40) != 0);
        (void)net::http::detail::valid_etag(value);
    }
    net::http::serve_content(req, w, "c.txt", string(content), o);

    const int status = rec.status();
    const std::string body = text(rec.body());
    auto fields = rec.headers();
    const bool head = std::string_view(method) == "HEAD";
    switch (status) {
        case 200:
            if (head) {
                check(body.empty());
                check(text(fields.get("Content-Length")) == std::to_string(content.size()));
            } else {
                check(body == content);
            }
            break;
        case 206: {
            check(std::string_view(method) != "PUT" && o.ranges);
            const std::string ctype = text(fields.get("Content-Type"));
            if (ctype.rfind("multipart/byteranges", 0) == 0) {
                if (head) {
                    check(body.empty());
                    break;
                }
                std::vector<Part> parts;
                check(parts_of(ctype, body, parts));
                check(parts.size() >= 2 && parts.size() <= net::http::detail::MaxRanges);
                uint64_t total = 0;
                for (auto& p : parts) {
                    auto cr = net::http::detail::parse_content_range(p.range);
                    check(cr && !cr->unsatisfied && cr->size == content.size());
                    check(p.data == content.substr(size_t(cr->first), size_t(cr->last - cr->first + 1)));
                    total += cr->last - cr->first + 1;
                }
                check(total <= content.size());
            } else {
                auto cr = net::http::detail::parse_content_range(text(fields.get("Content-Range")));
                check(cr && !cr->unsatisfied && cr->size == content.size());
                const std::string expected = content.substr(size_t(cr->first), size_t(cr->last - cr->first + 1));
                if (head) {
                    check(body.empty());
                    check(text(fields.get("Content-Length")) == std::to_string(expected.size()));
                } else {
                    check(body == expected);
                }
            }
            break;
        }
        case 304:
        case 412:
            check(body.empty());
            break;
        case 416: {
            check(o.ranges && !range_field.empty());
            auto cr = fields.get("Content-Range");
            check(cr.empty() || text(cr) == "bytes */" + std::to_string(content.size()));
            break;
        }
        default:
            check(false);
    }
    return 0;
}
