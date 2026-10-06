//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The chunked body of HTTP/1.1 on any bytes, decoded whole and in pieces:
// the last two bytes of the input pick the size of the pieces the body
// arrives in (1..256) and the room a step may fill with data (1..4096),
// as reads from a socket and a caller's buffer would. What must hold: the
// two give the same data, the same verdict (done, broken, or wanting
// more), the same trailers and the same end — the byte where the body
// stops and whatever follows it (the next request on the connection)
// begins. A body whose end moves with how it was read is a smuggled
// request.
#include "sgcl/net/http/http.h"
#include "tests/fuzz/input.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::net::http::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Result {
        std::string data;
        int error = 0;
        bool done = false;
        size_t end = 0;          // bytes of the input the body took
        std::string trailers;
    };

    Result decode(std::string_view in, size_t piece, size_t room) {
        ChunkedDecoder d;
        Result r;
        size_t fed = 0;
        while (fed < in.size() && !r.done && !r.error) {
            // each piece in a buffer of its own size (tests/fuzz/input.h)
            const sgcl_fuzz::exact bytes(in.substr(fed, std::min(piece, in.size() - fed)));
            std::string_view p = bytes.view();
            size_t at = 0;
            while (at < p.size()) {
                auto st = d.step(p.substr(at), room);
                check(st.consumed <= p.size() - at);
                check(st.data_at + st.data_size <= st.consumed);
                check(st.data_size <= room);
                r.data.append(p.substr(at + st.data_at, st.data_size));
                at += st.consumed;
                if (st.error) {
                    check(st.error == 400);
                    r.error = st.error;
                    break;
                }
                if (st.done) {
                    r.done = true;
                    break;
                }
                if (st.consumed == 0) {
                    break;
                }
            }
            fed += at;
            if (!r.done && !r.error && at < p.size()) {
                break;   // a step that took nothing and is not done: it would wait forever
            }
        }
        r.end = fed;
        if (r.done) {
            check(d.done());
            for (auto& f : HeadersAccess::fields(d.trailers())) {
                r.trailers.append(f.first.view());
                r.trailers += ':';
                r.trailers.append(f.second.view());
                r.trailers += '\n';
            }
        }
        return r;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 3) {
        return 0;
    }
    size_t piece = 1 + data[size - 2];
    size_t room = 1 + size_t(data[size - 1]) * 16;
    // the input less its last two bytes, in a buffer of its own size: the
    // piece of libFuzzer's would end before them, where a read past it goes
    // unseen (tests/fuzz/input.h)
    const sgcl_fuzz::exact copy(reinterpret_cast<const char*>(data), size - 2);
    std::string_view in = copy.view();
    Result whole = decode(in, in.size() ? in.size() : 1, 1 << 20);
    Result pieces = decode(in, piece, room);
    check(whole.error == pieces.error);
    check(whole.done == pieces.done);
    if (whole.done) {
        check(whole.end == pieces.end);
        check(whole.trailers == pieces.trailers);
        check(whole.data == pieces.data);
    } else if (!whole.error) {
        // wanting more: the same data so far
        check(whole.data == pieces.data);
    }
    return 0;
}
