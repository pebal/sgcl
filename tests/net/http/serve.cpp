//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::http::serve and serve_tls (serve.h): the files of a directory — the
// type by the extension, the length, Last-Modified and 304, a directory by
// its index.html and its URL without the slash redirected, 404 for a file
// that is not there and for a name out of the directory ("..%2f" among
// them, which the URL's normalization does not see) — and https with the
// certificate and the key read from their files.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

using namespace sgcl;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string testdata(const std::string& name) {
        return (std::filesystem::path(__FILE__).parent_path().parent_path() / "tls_testdata" / name).string();
    }

    void write(const std::filesystem::path& p, const std::string& data) {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << data;
    }

    // A tree: public/ with files and directories, a secret beside it
    struct Tree {
        std::filesystem::path root;
        std::string dir;

        Tree() {
            root = std::filesystem::temp_directory_path() / ("sgcl_serve_" + std::to_string(::getpid()));
            std::filesystem::remove_all(root);
            write(root / "public/hello.txt", "hello\n");
            write(root / "public/pic.PNG", "not really a png");
            write(root / "public/data.bin2", "?");
            write(root / "public/site/index.html", "<h1>site</h1>\n");
            std::filesystem::create_directories(root / "public/empty");
            write(root / "secret.txt", "the secret\n");
            dir = (root / "public").string();
        }

        ~Tree() {
            std::filesystem::remove_all(root);
        }
    };

    // The file server serve() makes, on a port of the loopback
    struct Running {
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        std::string base;

        explicit Running(const std::string& dir)
        : server(net::http::detail::file_server(sgcl::string(dir))) {
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
}

TEST(HttpServe_Tests, FilesByTheirType) {
    Tree t;
    Running r(t.dir);
    net::http::client web;
    auto hello = web.get(r.url("/hello.txt"));
    ASSERT_TRUE(hello) << text(hello.error().message());
    EXPECT_EQ(hello->status(), 200);
    EXPECT_EQ(text(hello->header("Content-Type")), "text/plain; charset=utf-8");
    EXPECT_EQ(hello->content_length().value_or(0), 6u);
    EXPECT_FALSE(hello->header("Last-Modified").empty());
    EXPECT_EQ(*hello->text(), "hello\n");
    auto pic = web.get(r.url("/pic.PNG"));   // the extension without case
    ASSERT_TRUE(pic);
    EXPECT_EQ(text(pic->header("Content-Type")), "image/png");
    pic->close();
    auto other = web.get(r.url("/data.bin2"));
    ASSERT_TRUE(other);
    EXPECT_EQ(text(other->header("Content-Type")), "application/octet-stream");
    other->close();
    // HEAD: the head alone
    auto head = web.head(r.url("/hello.txt"));
    ASSERT_TRUE(head);
    EXPECT_EQ(head->status(), 200);
    EXPECT_EQ(*head->text(), "");
}

TEST(HttpServe_Tests, NothingOutOfTheDirectory) {
    Tree t;
    Running r(t.dir);
    net::http::client web;
    for (const char* path : {"/none.txt", "/..%2fsecret.txt", "/site%2f..%2f..%2fsecret.txt", "/../secret.txt", "/%2e%2e/secret.txt"}) {
        auto res = web.get(r.url(path));
        ASSERT_TRUE(res) << path;
        EXPECT_EQ(res->status(), 404) << path;
        auto body = res->text();
        EXPECT_EQ(std::string(body->view()).find("secret"), std::string::npos) << path;
    }
}

TEST(HttpServe_Tests, Directories) {
    Tree t;
    Running r(t.dir);
    net::http::client web;
    auto site = web.get(r.url("/site"));   // 301 to /site/, followed
    ASSERT_TRUE(site);
    EXPECT_EQ(site->status(), 200);
    EXPECT_EQ(text(site->url().path()), "/site/");
    EXPECT_EQ(text(site->header("Content-Type")), "text/html; charset=utf-8");
    EXPECT_EQ(*site->text(), "<h1>site</h1>\n");
    auto empty = web.get(r.url("/empty/"));   // no index.html, no listing
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty->status(), 404);
    empty->close();
    auto root = web.get(r.url("/"));
    ASSERT_TRUE(root);
    EXPECT_EQ(root->status(), 404);
    root->close();
}

TEST(HttpServe_Tests, NotModified) {
    Tree t;
    Running r(t.dir);
    net::http::client web;
    auto first = web.get(r.url("/hello.txt"));
    ASSERT_TRUE(first);
    const sgcl::string modified = first->header("Last-Modified");
    first->close();
    net::http::request again("GET", r.url("/hello.txt"));
    again.set_header("If-Modified-Since", modified);
    auto same = web.send(again);
    ASSERT_TRUE(same);
    EXPECT_EQ(same->status(), 304);
    EXPECT_EQ(*same->text(), "");
    net::http::request older("GET", r.url("/hello.txt"));
    older.set_header("If-Modified-Since", "Sun, 06 Nov 1994 08:49:37 GMT");
    auto changed = web.send(older);
    ASSERT_TRUE(changed);
    EXPECT_EQ(changed->status(), 200);
    EXPECT_EQ(*changed->text(), "hello\n");
}

TEST(HttpServe_Tests, TheContentTypes) {
    using net::http::detail::content_type_of;
    EXPECT_STREQ(content_type_of("a/index.html"), "text/html; charset=utf-8");
    EXPECT_STREQ(content_type_of("app.JS"), "text/javascript; charset=utf-8");
    EXPECT_STREQ(content_type_of("x.svg"), "image/svg+xml");
    EXPECT_STREQ(content_type_of("x.wasm"), "application/wasm");
    EXPECT_STREQ(content_type_of("dir.d/noext"), "application/octet-stream");
    EXPECT_STREQ(content_type_of("x.verylongextension"), "application/octet-stream");
    EXPECT_STREQ(content_type_of("Makefile"), "application/octet-stream");
}

TEST(HttpServe_Tests, TlsFromFiles) {
    auto cfg = net::http::detail::tls_from_files(sgcl::string(testdata("ecdsa.pem")), sgcl::string(testdata("ecdsa.key")));
    ASSERT_TRUE(cfg) << text(cfg.error().message());
    ASSERT_EQ(cfg->identities.size(), 1u);
    auto missing = net::http::detail::tls_from_files(sgcl::string(testdata("none.pem")), sgcl::string(testdata("ecdsa.key")));
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is_not_found());
    auto mismatched = net::http::detail::tls_from_files(sgcl::string(testdata("ecdsa.pem")), sgcl::string(testdata("rsa.key")));
    EXPECT_FALSE(mismatched);
    // the error of serve_tls, before anything listens
    auto refused = net::http::serve_tls("127.0.0.1:0", sgcl::string(testdata("none.pem")), sgcl::string(testdata("ecdsa.key")),
                                        [](net::http::request, net::http::response_writer w) { w.write("x"); });
    ASSERT_FALSE(refused);
    EXPECT_TRUE(refused.error().is_not_found());
    // the config over TLS: a request answered
    net::http::server srv;
    srv.route("/", [](net::http::request req, net::http::response_writer w) { w.write("hello over " + req.proto() + "\n"); });
    auto l = net::tls::listen("127.0.0.1:0", *cfg);
    ASSERT_TRUE(l);
    auto serving = async::spawn(srv.async_serve(*l));
    net::http::client web;
    web.tls.roots = crypto::x509::certificate_pool::from_pem(io::read_text(sgcl::string(testdata("ca.pem"))).value());
    auto res = web.get(sgcl::string("https://localhost:" + std::to_string(l->local_endpoint().port()) + "/any/path"));
    ASSERT_TRUE(res) << text(res.error().message());
    EXPECT_EQ(*res->text(), "hello over HTTP/1.1\n");
    srv.close();
    (void)serving.wait();
}

// DESIGN 408: the file server at its ends: an empty file, HEAD of one, a
// name that is not a regular file (a FIFO, which an open would wait on
// forever) 404 at once, a NUL in the name, If-Modified-Since a second
// before the file's time, a directory that is not there
TEST(HttpServe_Tests, Boundaries) {
    Tree t;
    write(t.root / "public/empty.txt", "");
    ASSERT_EQ(::mkfifo((t.root / "public/fifo").c_str(), 0600), 0);
    Running r(t.dir);
    net::http::client web;
    web.timeout = std::chrono::seconds(5);
    auto empty = web.get(r.url("/empty.txt"));
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty->status(), 200);
    EXPECT_EQ(empty->content_length(), 0u);
    EXPECT_EQ(*empty->text(), "");
    auto head = web.head(r.url("/hello.txt"));
    ASSERT_TRUE(head);
    EXPECT_EQ(head->content_length(), 6u);
    EXPECT_EQ(*head->text(), "");
    auto fifo = web.get(r.url("/fifo"));
    ASSERT_TRUE(fifo) << text(fifo.error().message());
    EXPECT_EQ(fifo->status(), 404);
    auto nul = web.get(r.url("/hello.txt%00.png"));
    ASSERT_TRUE(nul);
    EXPECT_EQ(nul->status(), 404);
    auto first = web.get(r.url("/hello.txt"));
    ASSERT_TRUE(first);
    auto modified = first->headers().date("Last-Modified");
    first->close();
    ASSERT_TRUE(modified);
    net::http::request before("GET", r.url("/hello.txt"));
    before.headers().set_date("If-Modified-Since", time::datetime::from_unix(modified->unix() - 1, time::zone::utc()));
    auto changed = web.send(before);
    ASSERT_TRUE(changed);
    EXPECT_EQ(changed->status(), 200);
    EXPECT_EQ(*changed->text(), "hello\n");
    Running gone((t.root / "not_there").string());
    auto none = web.get(gone.url("/"));
    ASSERT_TRUE(none);
    EXPECT_EQ(none->status(), 404);
}
