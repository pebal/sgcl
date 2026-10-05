//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::sftp's server on any requests, over a tree in memory (detail::MemoryFs,
// a few files, directories and symlinks made first): the input a sequence of
// packets, each a 2-byte length and its type, id and fields, after the INIT;
// the first byte picks a read-only server. What must hold:
//   - every request is answered (or the session ended for a packet that
//     cannot be read) with one packet whose length is its own, of an answer's
//     type, echoing the request's id;
//   - a read-only server's tree is never changed;
//   - the handles held never pass the limit.
#include "sgcl/net/sftp.h"

#include <cstdint>
#include <cstring>

namespace {
    using namespace sgcl;
    namespace d = sgcl::net::sftp::detail;

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
    net::sftp::server_options o;
    o.read_only = data[0] & 1;
    o.max_handles = 8;
    auto m = std::make_unique<d::MemoryFs>();
    m->put("home/f", "hello");
    m->put("etc/passwd", "root");
    m->mkdir("empty", 0755);
    m->symlink("../etc", "home/up");
    m->symlink("/home/f", "abs");
    m->symlink("loop", "loop");
    d::MemoryFs* tree = m.get();
    d::ServerCore core(std::move(m), o);
    d::Bytes out;
    d::Bytes init = {d::FxpInit, 0, 0, 0, 3};
    check(core.handle(init.data(), init.size(), out));
    std::vector<d::DirEntry> before;
    tree->list("", before);
    const uint8_t* p = data + 1;
    size_t n = size - 1;
    while (n >= 2) {
        size_t len = size_t(p[0]) << 8 | p[1];
        p += 2;
        n -= 2;
        if (len > n) {
            len = n;
        }
        out.clear();
        bool go = core.handle(p, len, out);
        if (!go) {
            break;
        }
        // one answer, framed, of an answer's type, with the request's id
        check(out.size() >= 9);
        check(d::load32(out.data()) == out.size() - 4);
        const uint8_t t = out[4];
        check(t == d::FxpStatus || t == d::FxpHandle || t == d::FxpData || t == d::FxpName || t == d::FxpAttrs || t == d::FxpExtendedReply);
        check(len >= 5 && std::memcmp(out.data() + 5, p + 1, 4) == 0);
        check(core.open_handles() <= 8);
        p += len;
        n -= len;
    }
    if (o.read_only) {
        std::vector<d::DirEntry> after;
        tree->list("", after);
        check(after.size() == before.size());
        d::Attrs a;
        check(tree->lstat("home/f", a) == 0 && a.size == 5);
    }
    return 0;
}
