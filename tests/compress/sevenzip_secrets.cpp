//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// No 7z password in managed memory (the rule that secrets never lie there):
// an archive 7-Zip encrypts (7zz, its header too) read through
// sevenzip::options::password, bytes the test holds in plain memory:
// opened, an entry read, every entry walked, the whole extracted, in a task
// too; then the managed pages in use searched (tests/managed_scan.h) for
// the password as given (UTF-8) and as the key derivation takes it
// (UTF-16LE). A control first: the password put in managed memory on
// purpose is found.
#include "common.h"
#include "tests/managed_scan.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

using namespace compress_test;
namespace sevenzip = sgcl::compress::sevenzip;
namespace fs = std::filesystem;
using managed_scan::bytes_t;

namespace {
    const std::string Password = "Tr0ub4dor&3 correct horse battery stapl";   // 40 bytes, found nowhere else

    std::vector<bytes_t> patterns() {
        std::vector<bytes_t> p;
        p.emplace_back(Password.begin(), Password.end());
        bytes_t utf16;
        for (char c : Password) {
            utf16.push_back(uint8_t(c));
            utf16.push_back(0);
        }
        p.push_back(utf16);
        return p;
    }

    // The password where the test holds it: plain memory
    sgcl::slice<const std::byte> password() {
        return sgcl::slice<const std::byte>(std::string_view(Password));
    }

    fs::path scratch() {
        auto d = fs::temp_directory_path() / "sgcl_sevenzip_secrets";
        fs::create_directories(d);
        return d;
    }

    void put(const fs::path& p, const std::string& data) {
        std::ofstream(p, std::ios::binary) << data;
    }
}

TEST(SevenZipSecrets_Tests, TheScanFindsWhatIsThere) {
    // a pattern of its own: a freed slot keeps its bytes until it is given
    // out again, so the password itself here would be found by the next test
    const std::string control = "the control's own forty bytes, not used";
    std::vector<bytes_t> p = {bytes_t(control.begin(), control.end())};
    sgcl::string held;
    EXPECT_EQ(managed_scan::found_after(p, [&] { held = sgcl::string(control); }), 1u);
}

TEST(SevenZipSecrets_Tests, NoPasswordInManagedMemory) {
    if (seven_zip().empty()) {
        GTEST_SKIP() << "7zz not found";
    }
    const fs::path dir = scratch();
    fs::remove_all(dir / "tree");
    fs::remove_all(dir / "out");
    fs::remove_all(dir / "out2");
    fs::remove(dir / "secret.7z");
    fs::create_directories(dir / "tree/sub");
    put(dir / "tree/a.txt", std::string(5000, 'a'));
    put(dir / "tree/sub/b.txt", std::string(300, 'b'));
    // 7-Zip's process takes the password: never in this one's managed memory
    ASSERT_EQ(run_in(dir / "tree", seven_zip() + " a -mhe=on '-p" + Password + "' '" + (dir / "secret.7z").string() + "' ."), 0);
    const std::string archive = (dir / "secret.7z").string();
    const std::string out = (dir / "out").string(), out2 = (dir / "out2").string();
    auto p = patterns();
    std::vector<size_t> which;
    const size_t found = managed_scan::found_after(
        p,
        [&] {
            auto a = sevenzip::archive::open(sgcl::string(archive), sevenzip::options{.password = password()});
            ASSERT_TRUE(a) << a.error().message();
            auto d = a->read("a.txt");
            ASSERT_TRUE(d) << d.error().message();
            EXPECT_EQ(d->size(), 5000u);
            for (auto [e, r] : a->walk()) {
                (void)e;
                (void)r;
            }
            auto x = sevenzip::extract(sgcl::string(archive), sgcl::string(out), sevenzip::options{.password = password()});
            ASSERT_TRUE(x) << x.error().message();
            auto t = sgcl::async::spawn(sevenzip::async_extract(sgcl::string(archive), sgcl::string(out2), sevenzip::options{.password = password()}));
            auto y = t.wait();
            ASSERT_TRUE(y) << y.error().message();
        },
        &which);
    std::string names;
    for (size_t i : which) {
        names += i == 0 ? " as given (UTF-8)" : " as hashed (UTF-16LE)";
    }
    EXPECT_EQ(found, 0u) << "the password in managed memory:" << names;
    EXPECT_EQ(fs::file_size(dir / "out/sub/b.txt"), 300u);
    EXPECT_EQ(fs::file_size(dir / "out2/a.txt"), 5000u);
}
