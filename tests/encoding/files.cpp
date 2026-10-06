//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// JSON on files: json::load and save, typed and not, and their tasks; YAML's,
// TOML's, dotenv's, INI's, iCalendar's and vCard's load and save, the document as to_string writes
// it. A
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
    placeless(encoding::yaml::load(s / "none.yaml").error());
    placeless(encoding::toml::load(s / "none.toml").error());
    placeless(encoding::yaml(1).save(s / "no/cfg.yaml").error());
    placeless(encoding::toml().save(s / "no/cfg.toml").error());
    placeless(encoding::dotenv::load(s / "none.env").error());
    placeless(encoding::ini::load(s / "none.ini").error());
    placeless(encoding::dotenv().save(s / "no/.env").error());
    placeless(encoding::ini().save(s / "no/cfg.ini").error());
    placeless(encoding::icalendar::load(s / "none.ics").error());
    placeless(encoding::vcard::load(s / "none.vcf").error());
    placeless(encoding::vcard::load_all(s / "none.vcf").error());
    placeless(encoding::icalendar().save(s / "no/a.ics").error());
    placeless(encoding::vcard().save(s / "no/a.vcf").error());

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
    ASSERT_TRUE(io::write_file(s / "bad.toml", string("a = 1\nb = ")));
    EXPECT_EQ(std::string(encoding::toml::load(s / "bad.toml").error().message().view()), "2:5: a value expected");
    ASSERT_TRUE(io::write_file(s / "bad.yaml", string("a: 1\nb: [")));
    EXPECT_EQ(encoding::yaml::load(s / "bad.yaml").error().line(), 2u);
}

TEST(EncodingFiles_Tests, YamlAndToml) {
    using encoding::toml;
    using encoding::yaml;
    Scratch s;
    yaml y = yaml::mapping({{"host", "example.com"}, {"ports", yaml::sequence({80, 443})}});
    ASSERT_TRUE(y.save(s / "cfg.yaml"));
    EXPECT_EQ(text_of(s / "cfg.yaml"), std::string(y.to_string().view()));   // as written, no line added
    auto ly = yaml::load(s / "cfg.yaml");
    ASSERT_TRUE(ly);
    EXPECT_EQ(*ly, y);
    toml t = toml::table({{"host", "example.com"}, {"server", toml::table({{"port", 8080}})}});
    ASSERT_TRUE(t.save(s / "cfg.toml"));
    EXPECT_EQ(text_of(s / "cfg.toml"), std::string(t.to_string().view()));
    auto lt = toml::load(s / "cfg.toml");
    ASSERT_TRUE(lt);
    EXPECT_EQ(*lt, t);
    EXPECT_THROW((void)toml(1).save(s / "x.toml"), invalid_argument);   // a document is a table
    ASSERT_TRUE(yaml().save(s / "empty.yaml"));
    EXPECT_TRUE(yaml::load(s / "empty.yaml")->is_null());
    ASSERT_TRUE(toml().save(s / "empty.toml"));
    EXPECT_EQ(text_of(s / "empty.toml"), "");
    EXPECT_TRUE(toml::load(s / "empty.toml")->empty());
    auto task = [](string dir) -> async::task<bool> {
        toml t = toml::table({{"a", 1}});
        bool ok = bool(co_await t.async_save(dir + "/a.toml"));
        auto back = co_await toml::async_load(dir + "/a.toml");
        ok = ok && back && *back == t;
        yaml y = yaml::sequence({1, 2});
        ok = ok && co_await y.async_save(dir + "/a.yaml");
        auto yb = co_await yaml::async_load(dir + "/a.yaml");
        co_return ok && yb && *yb == y;
    }(string(s.dir));
    EXPECT_TRUE(task.wait());
}

TEST(EncodingFiles_Tests, DotenvAndIni) {
    using encoding::dotenv;
    using encoding::ini;
    Scratch s;
    dotenv e = dotenv::from({{"HOST", "example.com"}, {"GREETING", "hello world"}});
    ASSERT_TRUE(e.save(s / ".env"));
    EXPECT_EQ(text_of(s / ".env"), std::string(e.to_string().view()));
    auto le = dotenv::load(s / ".env");
    ASSERT_TRUE(le);
    EXPECT_EQ(*le, e);
    ini c = ini().set("server", "host", "example.com").set("server", "port", "8080");
    ASSERT_TRUE(c.save(s / "cfg.ini"));
    EXPECT_EQ(text_of(s / "cfg.ini"), std::string(c.to_string().view()));
    auto lc = ini::load(s / "cfg.ini");
    ASSERT_TRUE(lc);
    EXPECT_EQ(*lc, c);
    ASSERT_TRUE(io::write_file(s / "bad.ini", string("[a]\nx = 1\n[a]\n")));
    EXPECT_EQ(ini::load(s / "bad.ini").error().line(), 3u);
    auto task = [](string dir) -> async::task<bool> {
        dotenv e = dotenv::from({{"A", "1"}});
        bool ok = bool(co_await e.async_save(dir + "/a.env"));
        auto back = co_await dotenv::async_load(dir + "/a.env");
        ok = ok && back && *back == e;
        ini c = ini().set("s", "k", "v");
        ok = ok && co_await c.async_save(dir + "/a.ini");
        auto cb = co_await ini::async_load(dir + "/a.ini");
        co_return ok && cb && *cb == c;
    }(string(s.dir));
    EXPECT_TRUE(task.wait());
}

TEST(EncodingFiles_Tests, IcalendarAndVcard) {
    using encoding::content_line;
    using encoding::icalendar;
    using encoding::vcard;
    Scratch s;
    icalendar c = icalendar().add(icalendar("VEVENT").add(content_line("UID", "1")).add(content_line::text("SUMMARY", "Meeting, weekly")));
    ASSERT_TRUE(c.save(s / "a.ics"));
    EXPECT_EQ(text_of(s / "a.ics"), std::string(c.to_string().view()));
    auto lc = icalendar::load(s / "a.ics");
    ASSERT_TRUE(lc);
    EXPECT_EQ(*lc, c);
    vcard v = vcard().add(content_line::text("FN", "Jan"));
    ASSERT_TRUE(v.save(s / "a.vcf"));
    auto lv = vcard::load(s / "a.vcf");
    ASSERT_TRUE(lv);
    EXPECT_EQ(*lv, v);
    ASSERT_TRUE(io::write_file(s / "book.vcf", string(std::string(v.to_string().view()) + std::string(v.to_string().view()))));
    EXPECT_EQ(vcard::load_all(s / "book.vcf")->size(), 2u);
    EXPECT_EQ(vcard::load(s / "book.vcf").error().code(), encoding::errc::syntax);
    auto task = [](string dir) -> async::task<bool> {
        icalendar c;
        bool ok = bool(co_await c.async_save(dir + "/b.ics"));
        auto back = co_await icalendar::async_load(dir + "/b.ics");
        ok = ok && back && *back == c;
        vcard v;
        ok = ok && co_await v.async_save(dir + "/b.vcf");
        auto vb = co_await vcard::async_load(dir + "/b.vcf");
        co_return ok && vb && *vb == v;
    }(string(s.dir));
    EXPECT_TRUE(task.wait());
}
