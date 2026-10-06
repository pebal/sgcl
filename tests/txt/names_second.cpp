//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A second translation unit including sgcl/txt/names/pl.h: Names_Tests.Registry
// finds its table registered once. And the headers of tags whose script is
// their language's default (zh-Hans, sr-Cyrl): one key with their parent's,
// so no table of their own to shadow the parent's.
#include "tests/types.h"
#include "sgcl/time.h"
#include "sgcl/txt/names/pl.h"
#include "sgcl/txt/names/sr-Cyrl.h"
#include "sgcl/txt/names/zh-Hans.h"

TEST(Names_Tests, SecondUnit) {
    EXPECT_EQ(txt::region(string("PL")).display_name(txt::locale(string("pl"))), string("Polska"));
}

TEST(Names_Tests, DefaultScriptVariants) {
    auto zh = txt::locale(string("zh"));
    EXPECT_EQ(txt::region(string("DE")).display_name(zh), string("\xE5\xBE\xB7\xE5\x9B\xBD"));            // 德国
    EXPECT_EQ(txt::region(string("DE")).display_name(txt::locale(string("zh-Hans"))),
              string("\xE5\xBE\xB7\xE5\x9B\xBD"));
    auto t = time::datetime::from_unix_milli(0, time::zone::utc());
    EXPECT_EQ(time::date_format::from_pattern(zh, string("zzzz")).format(t),
              string("\xE5\x8D\x8F\xE8\xB0\x83\xE4\xB8\x96\xE7\x95\x8C\xE6\x97\xB6"));               // 协调世界时
    EXPECT_EQ(txt::region(string("DE")).display_name(txt::locale(string("sr"))),
              string("\xD0\x9D\xD0\xB5\xD0\xBC\xD0\xB0\xD1\x87\xD0\xBA\xD0\xB0"));                     // Немачка
    size_t zh_tables = 0;
    uint64_t key = txt::detail::cldr::normalized_key(zh);
    for (auto* n = txt::detail::names::head().load(); n; n = n->next) {
        zh_tables += n->table->locale == key;
    }
    EXPECT_EQ(zh_tables, 1u);
}
