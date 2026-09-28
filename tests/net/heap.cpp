//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A connection's read_line buffer is unmanaged memory the connection owns
// (the audit of the managed heap, 2026-09-26): every line is copied out of
// it, no slice of it is handed on, so no managed block lives for it.
#include "tests/types.h"
#include "sgcl/net/net.h"

#include <typeinfo>

using namespace sgcl;

namespace {
    size_t live_of(const std::type_info& type) {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (!s.buffers && *s.type == type) {
                n += s.live_objects;
            }
        }
        return n;
    }
}

TEST(NetHeap_Tests, ReadLineKeepsNoManagedBlock) {
    size_t before = live_of(typeid(sgcl::io::detail::IoBlock));
    auto [a, b] = net::connection::in_memory();
    auto send = [](net::connection c) -> async::task<> {
        (void)co_await c.async_write(string("first\nsecond\n"));
    };
    async::go(send(a));
    auto line = b.read_line();
    ASSERT_TRUE(line && *line);
    EXPECT_EQ(**line, "first");
    EXPECT_EQ(live_of(typeid(sgcl::io::detail::IoBlock)), before);   // the connection and its line buffer alive
    line = b.read_line();
    ASSERT_TRUE(line && *line);
    EXPECT_EQ(**line, "second");
    (void)a.close();
    line = b.read_line();
    ASSERT_TRUE(line);
    EXPECT_FALSE(*line);
    (void)b.close();
}
