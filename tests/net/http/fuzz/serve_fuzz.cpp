//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::http::serve's path and handler on any bytes: the file server serve()
// makes, over a directory beside a secret file, on a loopback socket; the
// input is the target of a GET (and, one input in two, sent raw as the
// whole request). What must hold:
//   - an answer and a close within two seconds, never a crash;
//   - the secret's bytes never in an answer, whatever the target
//     ("..%2f", "%2e%2e", backslashes, NULs, overlong segments), unless the
//     request itself held them (a redirect repeats its target);
//   - a 200 carries exactly one file of the directory, and its
//     Content-Type is the one of that file's extension.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/http/fuzz/serve_fuzz.cpp)
// or replayed by the library's own driver.
#include "sgcl/net/http/http.h"
#include "sgcl/net/net.h"

#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    constexpr std::string_view Secret = "TOP-SECRET-7f3a91";

    struct Running {
        std::filesystem::path root;
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        Running() {
            root = std::filesystem::temp_directory_path() / ("sgcl_serve_fuzz_" + std::to_string(::getpid()));
            std::filesystem::remove_all(root);
            std::filesystem::create_directories(root / "public/site");
            std::ofstream(root / "public/a.txt") << "file a\n";
            std::ofstream(root / "public/site/index.html") << "<p>index</p>\n";
            std::ofstream(root / "public/b.json") << "{\"b\":1}\n";
            std::ofstream(root / "secret.txt") << Secret;
            std::ofstream(root / "public/..secret") << "a dot-dot name inside\n";
            server = net::http::detail::file_server(string((root / "public").string()));
            listener = *net::tcp::listen("127.0.0.1:0");
            serving = async::spawn(server.async_serve(listener));
        }
    };

    // In managed memory, held by a root: its handles live where tracked
    // words may (a static Running would hold them in a global)
    Running& running() {
        static root_ptr<Running> r = make_tracked<Running>();
        return *r;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Running& r = running();
    std::string_view in(reinterpret_cast<const char*>(data), size);
    std::string request;
    if (size && (data[0] & 1)) {
        request.assign(in.substr(1));   // the whole request, as it comes
    } else {
        request = "GET /" + std::string(in.substr(size ? 1 : 0)) + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    }
    auto c = net::tcp::connect(r.listener.local_endpoint());
    if (!c) {
        return 0;
    }
    c->set_read_deadline(clock::now() + std::chrono::seconds(2));
    (void)c->write(string(request));
    (void)c->close_write();
    auto all = c->read_all_text();
    (void)c->close();
    check(all.has_value() || !all.error().is_timeout());   // an answer and a close, never a wait
    if (!all) {
        return 0;
    }
    const std::string_view out = all->view();
    // the secret's bytes only when the request carried them itself (a
    // redirect's Location repeats the target)
    check(out.find(Secret) == std::string_view::npos || in.find(Secret) != std::string_view::npos);
    // the first answer, when a 200 to a GET: its body (its Content-Length's
    // bytes; pipelined requests answered after it) is one of the files,
    // with its type. A HEAD's 200 has the length and no body
    const bool head_request = std::string_view(request).rfind("HEAD", 0) == 0;
    if (out.rfind("HTTP/1.1 200", 0) == 0 && !head_request) {
        const auto end = out.find("\r\n\r\n");
        check(end != std::string_view::npos);
        const std::string_view head = out.substr(0, end);
        const auto at = head.find("Content-Length: ");
        check(at != std::string_view::npos);
        const size_t length = std::strtoul(std::string(head.substr(at + 16, 12)).c_str(), nullptr, 10);
        const std::string_view body = out.substr(end + 4, length);
        const bool a = body == "file a\n" && head.find("text/plain; charset=utf-8") != std::string_view::npos;
        const bool index = body == "<p>index</p>\n" && head.find("text/html; charset=utf-8") != std::string_view::npos;
        const bool b = body == "{\"b\":1}\n" && head.find("application/json") != std::string_view::npos;
        const bool dots = body == "a dot-dot name inside\n";
        check(a || index || b || dots);
    }
    return 0;
}
