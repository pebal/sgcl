//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The HTTP/2 server's judgement of a request (detail/h2/request_check.h:
// check_request, check_trailers, BodyCount) on any fields, differentially
// against the rules written here again from RFC 9113 §8.1.1, §8.2 and
// §8.3.1 in another shape (one pass per rule instead of one pass for all).
// The input: a byte of flags (END_STREAM on the HEADERS), then fields as
// <name length><name><value length><value>; after a zero byte, the body:
// pieces as <length, two bytes><end> for BodyCount against its model. What
// must hold: the same verdict; for a request that passes, its pseudo-
// fields found as they are in the list; BodyCount malformed exactly when
// the model says, from the same piece on.
#include "sgcl/net/http/detail/h2/request_check.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    using namespace sgcl::net::http;
    using namespace sgcl::net::http::detail;
    using namespace sgcl::net::http::detail::h2;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    using List = std::vector<std::pair<std::string, std::string>>;

    bool upper(const std::string& s) {
        for (char c : s) {
            if ('A' <= c && c <= 'Z') {
                return true;
            }
        }
        return false;
    }

    int count(const List& l, std::string_view name) {
        int n = 0;
        for (auto& [k, v] : l) {
            n += k == name;
        }
        return n;
    }

    const std::string* value(const List& l, std::string_view name) {
        for (auto& [k, v] : l) {
            if (k == name) {
                return &v;
            }
        }
        return nullptr;
    }

    // The rules again, one at a time
    bool model_request(const List& l, bool end_stream, const headers& h) {
        for (auto& [k, v] : l) {
            if (k.empty() || upper(k)) {
                return false;
            }
        }
        // pseudo-fields only before the first regular one
        bool seen_regular = false;
        for (auto& [k, v] : l) {
            if (k[0] != ':') {
                seen_regular = true;
            } else if (seen_regular) {
                return false;
            }
        }
        // only the four of a request, each at most once
        for (auto& [k, v] : l) {
            if (k[0] == ':' && k != ":method" && k != ":scheme" && k != ":authority" && k != ":path") {
                return false;
            }
        }
        for (const char* p : {":method", ":scheme", ":authority", ":path"}) {
            if (count(l, p) > 1) {
                return false;
            }
        }
        for (const char* c : {"connection", "keep-alive", "proxy-connection", "transfer-encoding", "upgrade"}) {
            if (count(l, c)) {
                return false;
            }
        }
        for (auto& [k, v] : l) {
            if (k == "te" && v != "trailers") {
                return false;
            }
        }
        const std::string* method = value(l, ":method");
        if (!method) {
            return false;
        }
        if (*method == "CONNECT") {
            if (count(l, ":scheme") || count(l, ":path") || !count(l, ":authority")) {
                return false;
            }
        } else {
            const std::string* path = value(l, ":path");
            if (!count(l, ":scheme") || !path || path->empty()) {
                return false;
            }
        }
        if (count(l, "content-length")) {
            optional<uint64_t> n;
            if (!content_length(h, n)) {   // the parser of HTTP/1.1's lengths, shared: not what is checked here
                return false;
            }
            if (end_stream && n && *n > 0) {
                return false;
            }
        }
        return true;
    }

    bool model_trailers(const List& l) {
        for (auto& [k, v] : l) {
            if (k.empty() || k[0] == ':' || upper(k) || k == "connection" || k == "keep-alive" || k == "proxy-connection" ||
                k == "transfer-encoding" || k == "upgrade") {
                return false;
            }
        }
        return true;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const bool end_stream = data[0] & 1;
    size_t at = 1;
    List list;
    while (at < size && data[at] != 0 && list.size() < 64) {
        size_t nl = data[at++];
        if (at + nl > size) {
            break;
        }
        std::string name(reinterpret_cast<const char*>(data + at), nl);
        at += nl;
        if (at >= size) {
            break;
        }
        size_t vl = data[at++];
        if (at + vl > size) {
            break;
        }
        std::string value(reinterpret_cast<const char*>(data + at), vl);
        at += vl;
        list.emplace_back(std::move(name), std::move(value));
    }
    headers h;
    for (auto& [k, v] : list) {
        HeadersAccess::add(h, slice<const char>(k.data(), k.size()), slice<const char>(v.data(), v.size()));
    }
    RequestHead head;
    const bool passed = check_request(h, end_stream, head) == ErrorCode::no_error;
    check(passed == model_request(list, end_stream, h));
    if (passed) {
        check(head.method && head.method->first.view() == ":method");
        check(head.regulars == list.size() - size_t(count(list, ":method") + count(list, ":scheme") + count(list, ":authority") + count(list, ":path")));
        if (head.path) {
            check(head.path->second.view() == *value(list, ":path"));
        }
        if (head.authority) {
            check(head.authority->second.view() == *value(list, ":authority"));
        }
    }
    check((check_trailers(h) == ErrorCode::no_error) == model_trailers(list));
    // the body against the length
    BodyCount count_;
    optional<uint64_t> declared;
    if (passed) {
        declared = head.content_length;
    } else if (at + 1 < size) {
        declared = uint64_t(data[at + 1]) * 97;   // a length of the input's own, to exercise the count alone
    }
    count_.declared = declared;
    uint64_t sum = 0;
    bool broken = false;
    if (at < size) {
        ++at;
    }
    while (at + 3 <= size && !broken) {
        const size_t n = size_t(data[at]) << 8 | data[at + 1];
        const bool end = data[at + 2] & 1;
        at += 3;
        sum += n;
        const bool ok = count_.data(n, end);
        const bool model = !declared || (sum <= *declared && (!end || sum == *declared));
        check(ok == model);
        broken = !ok;
        if (end) {
            break;
        }
    }
    if (!broken) {
        check(count_.ended() == (!declared || sum == *declared));
    }
    return 0;
}
