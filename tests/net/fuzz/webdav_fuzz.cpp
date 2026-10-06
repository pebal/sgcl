//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::webdav on any bytes. Requests to the server's handling over a tree in
// memory (sftp's MemoryFs, which has no outside): the input cut at '\n' into
// lines of "METHOD path", headers ("Depth: 1", "Destination: ...",
// "Overwrite: F", "If: (<...>)", "Timeout: ...", "Lock-Token: ...") and a
// body up to a line of ".", request after request on the same tree. What
// must hold: a status of HTTP's range; a 207 body that reads as XML of a
// multistatus; and the client's reader of a multistatus (the first byte's
// bit 0) gives resources whose paths start with "/". The readers run on the
// input's own bytes or copies the handler makes.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/webdav_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/webdav.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace wd = sgcl::net::webdav::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    std::string_view in(reinterpret_cast<const char*>(data + 1), size - 1);
    if (data[0] & 1) {
        auto r = wd::dav_read_multistatus(std::string(in), "/dav", "fuzz", "/");
        if (r) {
            for (auto& res : *r) {
                check(!res.path.empty() && res.path.view()[0] == '/');
            }
        }
        return 0;
    }
    wd::DavState state;
    state.fs = std::make_unique<wd::sd::MemoryFs>();
    wd::DavSettings cfg;
    cfg.prefix = "/dav";
    size_t at = 0;
    auto line = [&]() -> optional<std::string> {
        if (at >= in.size()) {
            return nullopt;
        }
        size_t nl = in.find('\n', at);
        std::string l(in.substr(at, nl == std::string_view::npos ? std::string_view::npos : nl - at));
        at = nl == std::string_view::npos ? in.size() : nl + 1;
        return l;
    };
    for (int k = 0; k < 16; ++k) {
        auto first = line();
        if (!first) {
            break;
        }
        size_t sp = first->find(' ');
        if (sp == std::string::npos) {
            continue;
        }
        std::string method = first->substr(0, sp), path = first->substr(sp + 1);
        std::string depth, destination, overwrite, if_h, timeout, token, body;
        while (auto h = line()) {
            if (h->empty()) {
                break;
            }
            size_t colon = h->find(": ");
            if (colon == std::string::npos) {
                continue;
            }
            std::string name = h->substr(0, colon), value = h->substr(colon + 2);
            if (name == "Depth") depth = value;
            else if (name == "Destination") destination = value;
            else if (name == "Overwrite") overwrite = value;
            else if (name == "If") if_h = value;
            else if (name == "Timeout") timeout = value;
            else if (name == "Lock-Token") token = value;
        }
        while (auto b = line()) {
            if (*b == ".") {
                break;
            }
            body += *b;
            body += '\n';
        }
        auto o = wd::dav_handle(state, cfg, method, path, body, depth, destination, overwrite, if_h, timeout, token, "127.0.0.1");
        check(o.status >= 100 && o.status <= 599);
        if (o.status == 207) {
            auto ms = wd::dav_read_multistatus(o.body, "/dav", "fuzz", path);
            check(bool(ms));
        }
    }
    return 0;
}
