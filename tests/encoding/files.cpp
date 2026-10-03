//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// JSON on files: json::load and save, typed and not, and their tasks. A
// file saved holds what stringify (or to_string) writes, with a new line
// after it, and loads back to the same value; a file that does not open is
// errc::io with io's error inside; a text that does not parse is parse's
// error. The errors of a file of json, xml and csv that does not open,
// read or write have no place; the errors of its text keep theirs.
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

// An error that did not come from the text has no place: a file that does
// not open, cannot be read or written says the stream's error alone, not
// "offset 0"; an error of the text keeps its line and column
TEST(EncodingFiles_Tests, ErrorsOutsideTheTextHaveNoPlace) {
    using encoding::csv;
    using encoding::xml;
    Scratch s;
    auto placeless = [](const encoding::error& e) {
        auto m = std::string(e.message().view());
        EXPECT_EQ(e.code(), encoding::errc::io) << m;
        EXPECT_TRUE(e.io_error()) << m;
        EXPECT_EQ(e.offset(), 0u) << m;
        EXPECT_EQ(e.line(), 0u) << m;
        EXPECT_EQ(e.column(), 0u) << m;
        EXPECT_EQ(m.rfind("input/output error: ", 0), 0u) << m;
    };

    // open
    placeless(json::load(s / "none.json").error());
    placeless(json::load<server>(s / "none.json").error());
    placeless(xml::load(s / "none.xml").error());
    placeless(xml::load<server>(s / "none.xml").error());
    placeless(csv::load<server>(s / "none.csv").error());

    // read: a directory opens and does not read
    std::filesystem::create_directories(s.dir + "/dir");
    auto dir = s / "dir";
    if (io::open(dir)) {
        placeless(json::load(dir).error());
        placeless(json::load<server>(dir).error());
        placeless(xml::load(dir).error());
        placeless(xml::load<server>(dir).error());
        placeless(csv::load<server>(dir).error());
    }

    // write: a file in a directory that is not there
    server cfg;
    placeless(json::save(s / "no/cfg.json", cfg).error());
    placeless(json(1).save(s / "no/cfg.json").error());
    placeless(xml::save(s / "no/cfg.xml", "server", cfg).error());
    placeless(xml::text_node("t").save(s / "no/cfg.xml").error());
    placeless(csv::save(s / "no/cfg.csv", vector<server>{cfg}).error());

    // the text's own errors keep their place
    ASSERT_TRUE(io::write_file(s / "bad.json", string("{\"host\": ")));
    EXPECT_EQ(std::string(json::load(s / "bad.json").error().message().view()), "1:10: unexpected end of input, expected a value");
    ASSERT_TRUE(io::write_file(s / "bad.xml", string("<a>\n<b></a>")));
    auto x = xml::load(s / "bad.xml");
    ASSERT_FALSE(x);
    EXPECT_EQ(x.error().line(), 2u);
    ASSERT_TRUE(io::write_file(s / "bad.csv", string("host,port,tls\nexample.com,x\"y,true\n")));
    auto c = csv::load<server>(s / "bad.csv");
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().line(), 2u);
}
