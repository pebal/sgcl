//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::webdav: the server over a temporary directory and the module's
// client against it (every method, the errors of RFC 4918, locks), the
// root's confinement (".." in every spelling, a symlink out), PROPFIND and
// PROPPATCH by hand, and the clients of others: curl, and Python's
// http.client with xml.etree (python/client.py).
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http.h"
#include "sgcl/net/webdav.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace {
    namespace dav = sgcl::net::webdav;
    namespace http = sgcl::net::http;

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }

    bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
    }

    // A temporary directory with a WebDAV server of it under /dav
    struct Site {
        std::filesystem::path root;
        std::filesystem::path outside;
        dav::server dav;
        http::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        explicit Site(dav::server_options o = {}) {
            auto base = std::filesystem::temp_directory_path() / ("sgcl-webdav-" + std::to_string(::getpid()) + "-" + std::to_string(std::rand()));
            root = base / "root";
            outside = base / "outside";
            std::filesystem::create_directories(root);
            std::filesystem::create_directories(outside);
            std::ofstream(outside / "secret.txt") << "outside the root";
            std::ofstream(root / "hello.txt") << "hello, world";
            if (o.prefix.empty()) {
                o.prefix = "/dav";
            }
            dav = dav::server(sgcl::string(root.string()), o);
            srv.route("/dav/", [d = dav](http::request r, http::response_writer w) { return d.async_serve(r, w); });
            srv.route("/dav", [d = dav](http::request r, http::response_writer w) { return d.async_serve(r, w); });
            listener = net::tcp::listen("127.0.0.1:0").value();
            serving = async::spawn(srv.async_serve(listener));
        }

        ~Site() {
            srv.close();
            (void)serving.wait();
            std::error_code e;
            std::filesystem::remove_all(root.parent_path(), e);
        }

        std::string base() const {
            return "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port());
        }

        sgcl::string url() const {
            return sgcl::string(base() + "/dav/");
        }
    };

    // A request by hand: its status and body
    std::pair<int, std::string> request(const Site& s, const char* method, const std::string& path, const std::string& body = "",
                                         std::vector<std::pair<std::string, std::string>> headers = {}) {
        http::request r{sgcl::string(method), sgcl::string(s.base() + path)};
        for (auto& [k, v] : headers) {
            r.set_header(sgcl::string(k), sgcl::string(v));
        }
        if (!body.empty()) {
            r.set_body(sgcl::string(body));
        }
        auto res = http::client().send(r);
        if (!res) {
            return {0, str(res.error().message())};
        }
        return {res->status(), str(res->text().value_or(sgcl::string()))};
    }
}

TEST(WebDav, ClientAgainstServer) {
    Site s;
    dav::client c(s.url());
    // list, stat, read of what is there
    auto list = c.list("/").value();
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(str(list[0].path), "/hello.txt");
    EXPECT_EQ(list[0].size, 12u);
    EXPECT_FALSE(list[0].collection);
    EXPECT_EQ(str(list[0].content_type), "text/plain");
    EXPECT_TRUE(list[0].modified.has_value());
    EXPECT_EQ(str(c.read("/hello.txt").value()), "hello, world");
    auto root = c.stat("/").value();
    EXPECT_TRUE(root.collection);
    // collections, files, names that need escaping
    ASSERT_TRUE(c.mkdir("/docs"));
    EXPECT_EQ(c.mkdir("/docs").error().code(), std::errc::file_exists);
    EXPECT_EQ(c.mkdir("/no/such/parent").error().code(), std::errc::no_such_file_or_directory);
    ASSERT_TRUE(c.write("/docs/a report (final).txt", "first"));
    ASSERT_TRUE(c.write("/docs/a report (final).txt", "second"));   // replaced
    EXPECT_EQ(str(c.read("/docs/a report (final).txt").value()), "second");
    EXPECT_EQ(c.write("/nowhere/x.txt", "x").error().code(), std::errc::no_such_file_or_directory);
    std::string big(300000, 'b');
    ASSERT_TRUE(c.write("/docs/big.bin", sgcl::string(big)));
    EXPECT_EQ(c.stat("/docs/big.bin")->size, big.size());
    EXPECT_EQ(str(c.read("/docs/big.bin").value()), big);
    auto docs = c.list("/docs").value();
    EXPECT_EQ(docs.size(), 2u);
    EXPECT_TRUE(std::filesystem::exists(s.root / "docs" / "a report (final).txt"));
    // copy and move, files and collections, with and without overwrite
    ASSERT_TRUE(c.copy("/docs", "/backup"));
    EXPECT_EQ(str(c.read("/backup/big.bin").value()), big);
    EXPECT_EQ(c.copy("/hello.txt", "/docs/big.bin", false).error().code(), std::errc::file_exists);
    ASSERT_TRUE(c.move("/backup", "/archive"));
    EXPECT_EQ(c.stat("/backup").error().code(), std::errc::no_such_file_or_directory);
    EXPECT_TRUE(c.stat("/archive/big.bin"));
    EXPECT_FALSE(c.move("/archive", "/archive/inside"));   // into itself
    // remove, whole collections too
    ASSERT_TRUE(c.remove("/archive"));
    EXPECT_FALSE(std::filesystem::exists(s.root / "archive"));
    EXPECT_EQ(c.remove("/archive").error().code(), std::errc::no_such_file_or_directory);
    EXPECT_EQ(c.read("/missing.txt").error().code(), std::errc::no_such_file_or_directory);
    // locks: another client is refused, the lock's holder writes with its token
    auto token = c.lock("/hello.txt", std::chrono::seconds(60)).value();
    EXPECT_EQ(str(token).rfind("urn:uuid:", 0), 0u);
    dav::client other(s.url());
    EXPECT_EQ(other.write("/hello.txt", "stolen").error().code(), std::errc::device_or_resource_busy);
    EXPECT_EQ(other.remove("/hello.txt").error().code(), std::errc::device_or_resource_busy);
    EXPECT_FALSE(other.lock("/hello.txt"));
    c.if_token(token);
    ASSERT_TRUE(c.write("/hello.txt", "mine"));
    ASSERT_TRUE(c.unlock("/hello.txt", token));
    c.if_token(sgcl::string());
    ASSERT_TRUE(other.write("/hello.txt", "free again"));
    EXPECT_EQ(str(c.read("/hello.txt").value()), "free again");
    EXPECT_FALSE(c.unlock("/hello.txt", token));
}

TEST(WebDav, TheRootConfines) {
    Site s;
    std::filesystem::create_symlink(s.outside, s.root / "out");
    std::filesystem::create_symlink(s.outside / "secret.txt", s.root / "secret-link");
    for (const char* path : {"/dav/../outside/secret.txt", "/dav/%2e%2e/outside/secret.txt", "/dav/..%2f..%2foutside/secret.txt", "/dav/out/secret.txt",
                             "/dav/secret-link", "/dav/docs/../../outside/secret.txt"}) {
        auto [status, body] = request(s, "GET", path);
        EXPECT_EQ(body.find("outside the root"), std::string::npos) << path << " " << status;
    }
    // a PUT through ".." lands inside the root
    auto [st, b] = request(s, "PUT", "/dav/../escaped.txt", "x");
    EXPECT_FALSE(std::filesystem::exists(s.root.parent_path() / "escaped.txt"));
    (void)st;
    (void)b;
}

TEST(WebDav, PropfindAndProppatchByHand) {
    Site s;
    auto [st0, body0] = request(s, "PROPFIND", "/dav/", "", {{"Depth", "infinity"}});
    EXPECT_EQ(st0, 403);
    EXPECT_NE(body0.find("propfind-finite-depth"), std::string::npos);
    auto [st1, body1] = request(s, "PROPFIND", "/dav/hello.txt", R"(<?xml version="1.0"?><D:propfind xmlns:D="DAV:"><D:propname/></D:propfind>)", {{"Depth", "0"}});
    EXPECT_EQ(st1, 207);
    EXPECT_NE(body1.find("<D:getcontentlength/>"), std::string::npos);
    auto [st2, body2] = request(s, "PROPFIND", "/dav/hello.txt",
                                R"(<?xml version="1.0"?><D:propfind xmlns:D="DAV:"><D:prop><D:getetag/><x:color xmlns:x="urn:example"/></D:prop></D:propfind>)",
                                {{"Depth", "0"}});
    EXPECT_EQ(st2, 207);
    EXPECT_NE(body2.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(body2.find("HTTP/1.1 404 Not Found"), std::string::npos);   // the unknown property
    auto [st3, body3] = request(s, "PROPPATCH", "/dav/hello.txt",
                                R"(<?xml version="1.0"?><D:propertyupdate xmlns:D="DAV:" xmlns:x="urn:example"><D:set><D:prop><x:color>blue</x:color></D:prop></D:set></D:propertyupdate>)");
    EXPECT_EQ(st3, 207);
    EXPECT_NE(body3.find("200 OK"), std::string::npos);
    auto [st4, body4] = request(s, "PROPFIND", "/dav/hello.txt",
                                R"(<?xml version="1.0"?><D:propfind xmlns:D="DAV:"><D:prop><x:color xmlns:x="urn:example"/></D:prop></D:propfind>)", {{"Depth", "0"}});
    EXPECT_NE(body4.find(">blue<"), std::string::npos);
    // a live property is not set by PROPPATCH: all or none
    auto [st5, body5] = request(s, "PROPPATCH", "/dav/hello.txt",
                                R"(<?xml version="1.0"?><D:propertyupdate xmlns:D="DAV:" xmlns:x="urn:example"><D:set><D:prop><D:getetag>x</D:getetag><x:size>big</x:size></D:prop></D:set></D:propertyupdate>)");
    EXPECT_NE(body5.find("403 Forbidden"), std::string::npos);
    EXPECT_NE(body5.find("424 Failed Dependency"), std::string::npos);
    auto [st6, body6] = request(s, "OPTIONS", "/dav/");
    EXPECT_EQ(st6, 200);
    EXPECT_EQ(request(s, "MKCOL", "/dav/withbody", "<x/>").first, 415);
    EXPECT_EQ(request(s, "DELETE", "/dav/").first, 403);   // the root stays
}

// A name of bytes that are not UTF-8 (a disk that takes any, sftp's tree
// in memory here): its displayname U+FFFD where XML cannot carry them, the
// multistatus XML still; found by the fuzz harness
TEST(WebDav, NamesThatAreNotUtf8) {
    namespace wd = sgcl::net::webdav::detail;
    EXPECT_EQ(wd::dav_xml_escape("a<&\"b"), "a&lt;&amp;&quot;b");
    EXPECT_EQ(wd::dav_xml_escape("\xCA\x9D\xF5\xF5."), "\xCA\x9D\xEF\xBF\xBD\xEF\xBF\xBD.");
    EXPECT_EQ(wd::dav_xml_escape("\x01\t\xEF\xBF\xBE\xED\xA0\x80"), "\xEF\xBF\xBD\t\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
    EXPECT_EQ(wd::dav_xml_escape("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xEF\xBF\xBD"), "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xEF\xBF\xBD");
    EXPECT_EQ(wd::dav_xml_escape("\xE2\x82"), "\xEF\xBF\xBD\xEF\xBF\xBD");   // cut short
    wd::DavState state;
    state.fs = std::make_unique<wd::sd::MemoryFs>();
    wd::DavSettings cfg;
    cfg.prefix = "/dav";
    auto handle = [&](const std::string& method, const std::string& path, const std::string& depth = "") {
        return wd::dav_handle(state, cfg, method, path, "", depth, "", "", "", "", "", "127.0.0.1");
    };
    EXPECT_EQ(handle("MKCOL", "/dav/%CA%9D%F5%F5.").status, 201);
    auto o = handle("PROPFIND", "/dav/", "1");
    ASSERT_EQ(o.status, 207);
    auto ms = wd::dav_read_multistatus(o.body, "/dav", "test", "/");
    ASSERT_TRUE(ms) << str(ms.error().message());
    ASSERT_EQ(ms->size(), 2u);   // the collection and its member
    EXPECT_EQ(str((*ms)[1].path), "/\xCA\x9D\xF5\xF5./");   // the href percent-encoded, decoded back as it was
}

TEST(WebDav, ReadOnlyAndAuthorization) {
    dav::server_options o;
    o.read_only = true;
    o.authorize = [](const http::request& r) { return str(r.header("Authorization")) == "Basic YWxpY2U6c2VjcmV0"; };   // alice:secret
    Site s(o);
    dav::client anonymous(s.url());
    EXPECT_EQ(anonymous.list().error().code(), std::errc::permission_denied);
    dav::client alice(s.url(), {.user = "alice", .password = "secret"});
    EXPECT_EQ(alice.list()->size(), 1u);
    EXPECT_EQ(alice.write("/x.txt", "x").error().code(), std::errc::permission_denied);
    dav::client in_url(sgcl::string("http://alice:secret@127.0.0.1:" + std::to_string(s.listener.local_endpoint().port()) + "/dav/"));
    EXPECT_EQ(str(in_url.read("/hello.txt").value()), "hello, world");
}

TEST(WebDav, Curl) {
    if (!have("curl")) {
        GTEST_SKIP() << "no curl";
    }
    Site s;
    std::string base = s.base() + "/dav";
    EXPECT_EQ(run("curl -s -o /dev/null -w '%{http_code}' -X MKCOL " + base + "/c"), "201");
    EXPECT_EQ(run("printf 'from curl' | curl -s -o /dev/null -w '%{http_code}' -T - " + base + "/c/f.txt"), "201");
    std::string listing = run("curl -s -X PROPFIND -H 'Depth: 1' " + base + "/c/");
    EXPECT_NE(listing.find("/dav/c/f.txt"), std::string::npos);
    EXPECT_NE(listing.find("<D:getcontentlength>9</D:getcontentlength>"), std::string::npos);
    EXPECT_EQ(run("curl -s -o /dev/null -w '%{http_code}' -X MOVE -H 'Destination: " + base + "/c/g.txt' " + base + "/c/f.txt"), "201");
    EXPECT_EQ(run("curl -s " + base + "/c/g.txt"), "from curl");
    EXPECT_EQ(run("curl -s -o /dev/null -w '%{http_code}' -X DELETE " + base + "/c"), "204");
}

TEST(WebDav, PythonClient) {
    if (!have("python3")) {
        GTEST_SKIP() << "no python3";
    }
    Site s;
    std::string script = (source_root() / "tests/net/webdav/python/client.py").string();
    std::string out = run("python3 " + script + " " + std::to_string(s.listener.local_endpoint().port()) + " 2>&1");
    EXPECT_EQ(out,
              "mkcol 201\n"
              "put 201\n"
              "propfind 207 /dav/py/ /dav/py/a%20b.txt 17\n"
              "get 200 hello from python\n"
              "lock 200 True\n"
              "put-locked 423\n"
              "put-token 204\n"
              "unlock 204\n"
              "move 201\n"
              "moved 200 changed\n"
              "delete 204\n");
}
