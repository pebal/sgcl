//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The whole path of the HTTP/1.1 server on any bytes: a server on a
// loopback socket, one handler that reads the body and answers
// "<method> <body size>", and the input sent as what a client wrote,
// pipelined requests and all, then the writing side closed. What must
// hold:
//   - the server answers and closes within two seconds (no hang: a
//     server waiting on bytes that never come is what the closed side
//     rules out), and never crashes;
//   - what it sends is a sequence of well-formed responses and nothing
//     after them;
//   - the requests it served are the ones the parser alone (find_head_end,
//     check_request_head, the framing, the chunked decoder) cuts the same
//     bytes into, in the same order, with the same bodies: a server whose
//     reads cut the stream elsewhere than its parser does is the ground
//     request smuggling grows on.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/http_server_fuzz.cpp)
// or replayed by the library's own driver.
#include "sgcl/net/http/http.h"
#include "sgcl/net/net.h"
#include "sgcl/txt/format.h"

#include <cstdint>
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

    struct Running {
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        Running() {
            server.route("/", [](net::http::request r, net::http::response_writer w) -> async::task<> {
                auto body = co_await r.async_bytes();
                w.write(body ? txt::format("{} {}", r.method(), body->size()) : string("E"));
            });
            server.on_error = [](const string&) {};
            listener = net::tcp::listen("127.0.0.1:0");
            serving = async::spawn(server.async_serve(listener));
        }
    };

    // A managed object held by a root for the whole process (the fuzzer
    // never ends it): what holds tracked pointers may not live in plain
    // heap memory (The rules, 1)
    Running& running() {
        static root_ptr<Running> r = make_tracked<Running>();
        return *r;
    }

    // What the parser alone cuts the bytes into: "<method> <size>" for each
    // request a server would serve, up to the first it would refuse or one
    // that ends the connection. A request whose head is accepted is served
    // even when its body then breaks or runs out (the handler runs on the
    // head and reads the body as it comes): its answer is "E", and the
    // connection ends there
    struct Expected {
        std::vector<std::string> served;
        std::vector<bool> head;   // for every answer, a refusal's included: whether its request was a HEAD (no body)
    };

    Expected cut(std::string_view in) {
        Expected e;
        size_t at = 0;
        while (at < in.size()) {
            at += leading_empty_lines(in.data() + at, in.size() - at);
            size_t end = find_head_end(in.data() + at, in.size() - at);
            if (!end) {
                break;
            }
            string head(in.substr(at, end));
            RequestLine line;
            net::http::headers h;
            BodyFraming framing;
            if (check_request_head(head, line, h, framing)) {
                // refused: the answer is the head alone when the request
                // line said HEAD (the server knows the method from there)
                e.head.push_back(line.method_size == 4 && head.view().substr(line.method_at, 4) == "HEAD");
                break;
            }
            at += end;
            std::string method(head.view().substr(line.method_at, line.method_size));
            // the server's rule past the parser's: 100-continue on HTTP/1.1
            // is served (a 100 first), any other expectation refused
            if (auto expect = HeadersAccess::find(h, "expect")) {
                if (!iequal(trim_ows(*expect), "100-continue") || line.minor != 1) {
                    e.head.push_back(method == "HEAD");   // the 417's answer
                    break;
                }
            }
            size_t size = 0;
            auto broken = [&] {
                e.served.push_back("E");
                e.head.push_back(method == "HEAD");
            };
            if (framing.kind == Framing::length) {
                if (framing.length > in.size() - at) {
                    broken();
                    break;
                }
                size = size_t(framing.length);
                at += size;
            } else if (framing.kind == Framing::chunked) {
                ChunkedDecoder d;
                auto st = d.step(in.substr(at), size_t(1) << 30);
                size_t taken = st.consumed;
                size += st.data_size;
                while (!st.done && !st.error && taken < in.size() && st.consumed) {
                    st = d.step(in.substr(at + taken), size_t(1) << 30);
                    taken += st.consumed;
                    size += st.data_size;
                }
                if (!st.done) {
                    broken();
                    break;
                }
                at += taken;
            }
            e.served.push_back(method + " " + std::to_string(size));
            e.head.push_back(method == "HEAD");
            bool close = line.minor == 1 ? HeadersAccess::has_token(h, "connection", "close")
                                         : !HeadersAccess::has_token(h, "connection", "keep-alive");
            if (close) {
                break;
            }
        }
        return e;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Running& r = running();
    std::string_view in(reinterpret_cast<const char*>(data), size);
    Expected want = cut(in);
    auto c = net::tcp::connect(r.listener.local_endpoint());
    if (!c) {
        return 0;
    }
    c->set_read_deadline(clock::now() + std::chrono::seconds(2));
    (void)c->write(string(in));
    (void)c->close_write();
    auto all = c->read_all_text();
    (void)c->close();
    check(all.has_value() || !all.error().is_timeout());   // an answer and a close, never a wait
    if (!all) {
        return 0;
    }
    std::string_view out = all->view();
    // the responses, one after another, each the answer of the next request;
    // the 200s carry what the handler saw
    size_t at = 0, served = 0;
    while (at < out.size()) {
        size_t end = find_head_end(out.data() + at, out.size() - at);
        check(end != 0);
        string head(out.substr(at, end));
        StatusLine line;
        net::http::headers h;
        check(parse_response_head(head, line, h) == 0);
        at += end;
        if (line.status >= 100 && line.status < 200) {
            continue;
        }
        // every final answer is the answer of the next request, whatever its
        // status: a redirect (307 to a cleaned path) keeps the connection
        size_t index = served++;
        bool head_request = index < want.head.size() && want.head[index];
        BodyFraming framing;
        check(response_framing(h, line.status, head_request, framing));
        check(framing.kind != Framing::chunked);   // the server's answers here are short: a length
        std::string_view body;
        if (framing.kind == Framing::length) {
            check(framing.length <= out.size() - at);
            body = out.substr(at, size_t(framing.length));
            at += size_t(framing.length);
        } else if (framing.kind == Framing::until_close) {
            body = out.substr(at);
            at = out.size();
        }
        if (line.status != 200) {
            continue;   // a redirect, a refusal the parser does not model (a limit, Expect)
        }
        check(index < want.served.size());
        if (!head_request) {
            check(body == want.served[index]);
        }
    }
    return 0;
}
