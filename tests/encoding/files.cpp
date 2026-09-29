//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// JSON on files: json::load and save, typed and not, and their tasks. A
// file saved holds what stringify (or to_string) writes, with a new line
// after it, and loads back to the same value; a file that does not open is
// errc::io with io's error inside; a text that does not parse is parse's
// error.
#include "tests/types.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
    using namespace sgcl;
    using encoding::field_list;
    using encoding::json;

    struct server {
        string host = "localhost";
        int64_t port = 8080;
        bool tls = false;

        void describe(field_list& f) {
            f.add("host", host);
            f.add("port", port);
            f.add("tls", tls);
        }

        friend bool operator==(const server&, const server&) = default;
    };

    struct Scratch {
        std::string dir;

        Scratch() {
            static std::atomic<int> n{0};
            dir = (std::filesystem::temp_directory_path() / ("sgcl_encoding_files_" + std::to_string(::getpid()) + "_" + std::to_string(n++))).string();
            std::filesystem::create_directories(dir);
        }

        ~Scratch() {
            std::filesystem::remove_all(dir);
        }

        string operator/(const std::string& name) const {
            return string(dir + "/" + name);
        }
    };

    std::string text_of(const string& path) {
        std::ifstream in(std::string(path.view()), std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
}

TEST(EncodingFiles_Tests, Json) {
    Scratch s;
    server cfg{"example.com", 443, true};
    ASSERT_TRUE(json::save(s / "cfg.json", cfg));
    EXPECT_EQ(text_of(s / "cfg.json"), std::string(json::stringify(cfg)->view()) + "\n");
    auto back = json::load<server>(s / "cfg.json");
    ASSERT_TRUE(back);
    EXPECT_EQ(*back, cfg);
    auto tree = json::load(s / "cfg.json");
    ASSERT_TRUE(tree);
    EXPECT_EQ(std::string_view((*tree)["host"].as_string().value()), "example.com");
    ASSERT_TRUE(tree->save(s / "tree.json"));
    EXPECT_EQ(text_of(s / "tree.json"), text_of(s / "cfg.json"));

    auto missing = json::load(s / "none.json");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), encoding::errc::io);
    ASSERT_TRUE(missing.error().io_error());
    EXPECT_TRUE(missing.error().io_error()->is_not_found());
    ASSERT_TRUE(io::write_file(s / "bad.json", string("{\"host\": ")));
    auto bad = json::load<server>(s / "bad.json");
    ASSERT_FALSE(bad);
    EXPECT_NE(bad.error().code(), encoding::errc::io);
}

TEST(EncodingFiles_Tests, JsonTasks) {
    Scratch s;
    auto t = [](string dir) -> async::task<bool> {
        server cfg{"example.com", 443, true};
        bool ok = bool(co_await json::async_save(dir + "/a.json", cfg));
        auto j = co_await json::async_load<server>(dir + "/a.json");
        ok = ok && j && *j == cfg;
        auto tree = co_await json::async_load(dir + "/a.json");
        ok = ok && tree && co_await tree->async_save(dir + "/b.json");
        co_return ok;
    }(string(s.dir));
    EXPECT_TRUE(t.wait());
    EXPECT_EQ(text_of(s / "a.json"), text_of(s / "b.json"));
}
