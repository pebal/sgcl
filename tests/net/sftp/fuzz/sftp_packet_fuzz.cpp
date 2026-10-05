//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::sftp's reading of what a server sends, on any bytes: the attributes
// (read, written back and read again the same), an answer as the client
// takes it apart (a STATUS as its error, a NAME's entries as read_dir reads
// them, a HANDLE, DATA), the VERSION's extensions. What must hold:
//   - attributes read write back to bytes that read to the same fields;
//   - an answer is an error or its fields, never read past;
//   - a STATUS's error is one of the codes the module documents.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/sftp/fuzz/sftp_packet_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/sftp.h"

#include <cstdint>

namespace {
    using namespace sgcl;
    namespace d = sgcl::net::sftp::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool documented(error_code e) {
        return e == std::errc::no_such_file_or_directory || e == std::errc::permission_denied || e == std::errc::not_supported || e == io::errc::unexpected_eof ||
               e == io::errc::closed || e == net::errc::sftp_failure || e == net::errc::sftp_protocol;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // attributes
    {
        d::Reader r(data, size);
        d::Attrs a;
        if (d::read_attrs(r, a)) {
            d::Bytes b;
            d::Writer w(b);
            d::write_attrs(w, a);
            d::Reader r2(b.data(), b.size());
            d::Attrs back;
            check(d::read_attrs(r2, back) && r2.done());
            check(back.flags == (a.flags & ~d::AttrExtended));
            check(!(a.flags & d::AttrSize) || back.size == a.size);
            check(!(a.flags & d::AttrPermissions) || back.permissions == a.permissions);
            auto info = d::info_of(a, "n");
            check(info.name == "n");
        }
    }
    // an answer: type, id, fields
    if (size >= 5) {
        d::Bytes reply(data, data + size);
        expected<d::Bytes, io::error> got = reply;
        if (auto e = d::status_of(reply, "op", "path")) {
            check(documented(e->code()));
        }
        auto ok = d::expect_ok(got, "op", "path");
        if (!ok) {
            check(documented(ok.error().code()));
        }
        auto name = d::one_name(got, "op", "path");
        if (!name) {
            check(documented(name.error().code()));
        }
        auto nr = d::expect(got, d::FxpName, "op", "path");
        if (nr) {
            uint32_t count = nr->u32();
            for (uint32_t i = 0; i < count && nr->ok() && i < 100000; ++i) {
                (void)nr->string();
                (void)nr->string();
                d::Attrs a;
                if (!d::read_attrs(*nr, a)) {
                    break;
                }
            }
        } else {
            check(documented(nr.error().code()));
        }
    }
    return 0;
}
