//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::http::download and the response's readers of download.h: a file
// saved through path + ".part" and a rename, a body cut in the middle
// leaving no part and the file there before untouched, a status other than
// 2xx the error http_status and no file, the client's own download and the
// async form in a task, response::save, response::json and json<T>.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    struct Running {
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        std::string base;

        explicit Running(net::http::server s)
        : server(s) {
            listener = *net::tcp::listen("127.0.0.1:0");
            base = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port());
            serving = async::spawn(server.async_serve(listener));
        }

        ~Running() {
            server.close();
            (void)serving.wait();
        }

        sgcl::string url(const std::string& path) const {
            return sgcl::string(base + path);
        }
    };

    net::http::server files() {
        net::http::server s;
        s.route("GET /big", [](net::http::request, net::http::response_writer w) {
            w.write(sgcl::string(std::string(200000, 'b')));
        });
        s.route("GET /missing", [](net::http::request, net::http::response_writer w) {
            w.set_status(404);
            w.write("not here");
        });
        s.route("GET /old", [](net::http::request, net::http::response_writer w) {
            w.redirect("/big");
        });
        s.route("GET /json", [](net::http::request, net::http::response_writer w) {
            w.set_header("Content-Type", "application/json");
            w.write(R"({"name":"kot","age":3})");
        });
        s.route("GET /cut", [](net::http::request, net::http::response_writer w) -> async::task<> {
            // a promised length the body never reaches: the connection taken
            // over and closed after 5000 bytes, the client's read fails in the middle
            auto taken = w.hijack();
            if (taken) {
                (void)co_await taken->first.async_write(sgcl::string("HTTP/1.1 200 OK\r\nContent-Length: 100000\r\n\r\n" + std::string(5000, 'c')));
                (void)taken->first.close();
            }
        });
        return s;
    }

    std::string read_file(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::string temp(const std::string& name) {
        return (std::filesystem::temp_directory_path() / ("sgcl_download_" + name)).string();
    }

    struct item {
        sgcl::string name;
        int age = 0;

        void describe(encoding::field_list& f) {
            f.add("name", name);
            f.add("age", age);
        }
    };
}

TEST(HttpDownload_Tests, ThroughAPartFile) {
    Running r(files());
    const std::string path = temp("big.bin");
    std::filesystem::remove(path);
    auto res = net::http::download(r.url("/old"), sgcl::string(path));   // redirected to /big
    ASSERT_TRUE(res) << res.error().message();
    EXPECT_EQ(res->status(), 200);
    EXPECT_EQ(read_file(path), std::string(200000, 'b'));
    EXPECT_FALSE(std::filesystem::exists(path + ".part"));

    // a body cut in the middle: no part left, the file there before untouched
    {
        std::ofstream out(path, std::ios::binary);
        out << "before";
    }
    auto cut = net::http::download(r.url("/cut"), sgcl::string(path));
    EXPECT_FALSE(cut);
    EXPECT_EQ(read_file(path), "before");
    EXPECT_FALSE(std::filesystem::exists(path + ".part"));
    std::filesystem::remove(path);
}

TEST(HttpDownload_Tests, AStatusOtherThan2xxIsAnErrorAndNoFile) {
    Running r(files());
    net::http::client web;
    auto plain = web.get(r.url("/missing"));
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->status(), 404);   // get: a 4xx is a response

    const std::string path = temp("missing.bin");
    std::filesystem::remove(path);
    auto none = net::http::download(r.url("/missing"), sgcl::string(path));
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), net::errc::http_status);
    EXPECT_NE(text(none.error().message()).find("404 Not Found"), std::string::npos) << text(none.error().message());
    EXPECT_FALSE(std::filesystem::exists(path));
    EXPECT_FALSE(std::filesystem::exists(path + ".part"));
}

TEST(HttpDownload_Tests, TheClientsOwnAndTheAsyncForm) {
    Running r(files());
    net::http::client web;
    web.timeout = 5s;
    const std::string path = temp("client.bin");
    std::filesystem::remove(path);
    auto got = web.download(r.url("/big"), sgcl::string(path));
    ASSERT_TRUE(got) << got.error().message();
    EXPECT_EQ(read_file(path).size(), 200000u);
    std::filesystem::remove(path);

    auto status = async::spawn([](net::http::client c, sgcl::string u, sgcl::string p) -> async::task<int> {
        auto res = co_await c.async_download(u, p);
        co_return res ? res->status() : -1;
    }(web, r.url("/big"), sgcl::string(path))).wait();
    EXPECT_EQ(status, 200);
    EXPECT_EQ(read_file(path).size(), 200000u);
    std::filesystem::remove(path);

    auto free = async::spawn([](sgcl::string u, sgcl::string p) -> async::task<int> {
        auto res = co_await net::http::async_download(u, p);
        co_return res ? res->status() : -1;
    }(r.url("/big"), sgcl::string(path))).wait();
    EXPECT_EQ(free, 200);
    EXPECT_EQ(read_file(path).size(), 200000u);
    std::filesystem::remove(path);
}

TEST(HttpDownload_Tests, TheResponsesSave) {
    Running r(files());
    net::http::client web;
    const std::string path = temp("save.bin");
    auto res = web.get(r.url("/big"));
    ASSERT_TRUE(res);
    auto n = res->save(sgcl::string(path));
    ASSERT_TRUE(n) << n.error().message();
    EXPECT_EQ(*n, 200000u);
    EXPECT_EQ(read_file(path).size(), 200000u);
    EXPECT_FALSE(std::filesystem::exists(path + ".part"));
    std::filesystem::remove(path);
}

TEST(HttpDownload_Tests, TheJsonReaders) {
    Running r(files());
    net::http::client web;
    auto v = web.get(r.url("/json"))->json();
    ASSERT_TRUE(v) << v.error().message();
    EXPECT_EQ(text((*v)["name"].as<sgcl::string>().value()), "kot");
    auto t = web.get(r.url("/json"))->json<item>();
    ASSERT_TRUE(t) << t.error().message();
    EXPECT_EQ(text(t->name), "kot");
    EXPECT_EQ(t->age, 3);
    auto bad = web.get(r.url("/big"))->json();
    EXPECT_FALSE(bad);
}

// DESIGN 408: saving at the ends: an empty body (200 and 204) is an empty
// file; a save of a body read already writes what is left of it, nothing;
// a path in no directory fails at the start and one that is a directory at
// the end, the part removed either way and the client's next request
// served; a URL that is no URL; JSON of an empty body
TEST(HttpDownload_Tests, Boundaries) {
    auto s = files();
    s.route("GET /empty", [](net::http::request, net::http::response_writer) {});
    s.route("GET /none", [](net::http::request, net::http::response_writer w) { w.set_status(204); });
    Running r(s);
    net::http::client web;
    const std::string path = temp("boundary.bin");
    for (const char* where : {"/empty", "/none"}) {
        std::filesystem::remove(path);
        auto got = web.download(r.url(where), sgcl::string(path));
        ASSERT_TRUE(got) << where << ": " << got.error().message();
        EXPECT_TRUE(std::filesystem::exists(path)) << where;
        EXPECT_EQ(std::filesystem::file_size(path), 0u) << where;
        EXPECT_FALSE(std::filesystem::exists(path + ".part"));
    }
    auto res = web.get(r.url("/big"));
    ASSERT_TRUE(res);
    EXPECT_EQ(*res->save(sgcl::string(path)), 200000u);
    auto again = res->save(sgcl::string(path));                 // the body has gone: nothing left to save
    ASSERT_TRUE(again);
    EXPECT_EQ(*again, 0u);
    EXPECT_EQ(std::filesystem::file_size(path), 0u);
    std::filesystem::remove(path);
    const std::string nowhere = temp("no_such_dir/file.bin");
    auto lost = web.download(r.url("/big"), sgcl::string(nowhere));
    ASSERT_FALSE(lost);
    EXPECT_EQ(lost.error().code(), std::errc::no_such_file_or_directory);
    EXPECT_FALSE(std::filesystem::exists(nowhere + ".part"));
    const std::string dir = temp("a_dir");
    std::filesystem::create_directories(dir);
    { std::ofstream(dir + "/inside") << "kept"; }
    auto over = web.download(r.url("/big"), sgcl::string(dir));
    ASSERT_FALSE(over);
    EXPECT_FALSE(std::filesystem::exists(dir + ".part"));
    EXPECT_EQ(read_file(dir + "/inside"), "kept");
    std::filesystem::remove_all(dir);
    EXPECT_EQ(web.get(r.url("/json"))->status(), 200);           // the client goes on
    auto bad_url = net::http::download("not a url", sgcl::string(path));
    ASSERT_FALSE(bad_url);
    EXPECT_EQ(bad_url.error().code(), net::errc::invalid_url);
    EXPECT_FALSE(std::filesystem::exists(path));
    auto empty_json = web.get(r.url("/empty"))->json();
    EXPECT_FALSE(empty_json);
}
