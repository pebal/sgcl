//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The event stream's interpretation (http::event_reader's parser) on any
// bytes, and an event's text read back. The first byte picks the path (bit
// 0) and how the bytes are cut (the rest). What must hold:
//   - the events a stream gives are the same whatever pieces it comes in
//     (a CR at a piece's end, a BOM cut over pieces), and so is whether
//     it passed the limit;
//   - no event has an empty type, an id with CR, LF or NUL;
//   - an event written (its type and id of one line, its data any bytes)
//     and read back is the same type, id and data, CR and CRLF in the
//     data read as LF.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/http_events_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/http/events.h"

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    using namespace sgcl::net::http::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Result {
        std::vector<std::string> events;
        bool too_large = false;
        std::string last_id;

        bool operator==(const Result&) const = default;
    };

    Result run(std::string_view bytes, size_t piece, size_t limit) {
        EventParser p(limit);
        std::deque<ParsedEvent> out;
        Result r;
        size_t step = piece ? piece : std::max<size_t>(bytes.size(), 1);
        for (size_t i = 0; i < bytes.size(); i += step) {
            if (!p.feed(bytes.data() + i, std::min(step, bytes.size() - i), out)) {
                r.too_large = true;
                break;
            }
        }
        if (!r.too_large) {
            p.finish();
        }
        for (auto& e : out) {
            check(!e.type.empty());
            check(e.id.find_first_of(std::string_view("\r\n\0", 3)) == std::string::npos);
            r.events.push_back(e.type + '\x1F' + e.id + '\x1F' + e.data + '\x1F' + (e.retry ? std::to_string(*e.retry) : std::string()));
        }
        r.last_id = p.last_id();
        return r;
    }

    void stream(uint8_t mode, std::string_view bytes) {
        const size_t limit = mode & 2 ? 64 : 1 << 20;
        Result whole = run(bytes, 0, limit);
        Result cut = run(bytes, 1 + (mode >> 2), limit);
        if (whole.too_large || cut.too_large) {
            // the limit met in pieces may be met before the events a whole read would give: only that both meet it
            check(whole.too_large == cut.too_large);
            return;
        }
        check(whole == cut);
    }

    void written(std::string_view input) {
        // type NUL id NUL data
        auto a = input.find('\0');
        auto b = a == std::string_view::npos ? a : input.find('\0', a + 1);
        if (b == std::string_view::npos) {
            return;
        }
        net::http::event e;
        e.type = string(input.substr(0, a));
        e.id = string(input.substr(a + 1, b - a - 1));
        e.data = string(input.substr(b + 1));
        auto text = event_text(e);
        auto one_line = [](std::string_view v) { return v.find_first_of(std::string_view("\r\n\0", 3)) == std::string_view::npos; };
        if (!one_line(e.type.view()) || !one_line(e.id.view())) {
            check(!text);
            return;
        }
        check(text.has_value());
        EventParser p(1 << 22);
        std::deque<ParsedEvent> out;
        check(p.feed(text->data(), text->size(), out));
        check(out.size() == 1);
        std::string data;
        std::string_view d = e.data.view();
        for (size_t i = 0; i < d.size(); ++i) {
            if (d[i] == '\r') {
                data += '\n';
                if (i + 1 < d.size() && d[i + 1] == '\n') {
                    ++i;
                }
            } else {
                data += d[i];
            }
        }
        std::string type(e.type.view());
        // a type a reader would take the text of a field for: what a field reads it as
        check(out[0].type == (type.empty() ? std::string("message") : type));
        check(out[0].id == std::string(e.id.view()));
        check(out[0].data == data);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    if (mode & 1) {
        written(rest);
    } else {
        stream(mode, rest);
    }
    return 0;
}
