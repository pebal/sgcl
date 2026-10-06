//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Content-Encoding both ways: the client's transparent decoding (its
// Accept-Encoding of its own, gzip and deflate decoded as read, the fields
// that go with the identity removed, uncompressed(); none for a request of
// its own Accept-Encoding, a Range, decompress off, HEAD) against Go's
// server and the module's; the server's compression middleware (q-values
// and the server's order, the types, the least size, Vary, a flushed body
// compressed as it goes, ETag made weak, Accept-Ranges and Content-Length
// gone, 206 and HEAD left alone) against curl --compressed, Python's zlib
// and Go's client.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

using namespace sgcl;

namespace {
    namespace http = sgcl::net::http;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string shell(const std::string& cmd) {
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

    struct Running {
        http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        std::string base;

        explicit Running(http::server s)
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

    std::string json_of(size_t n) {
        std::string s = "[";
        for (size_t i = 0; s.size() < n; ++i) {
            s += "{\"id\":" + std::to_string(i) + ",\"name\":\"item " + std::to_string(i) + "\"},";
        }
        s.back() = ']';
        return s;
    }

    // A server that answers with the coding a query asks for, the bytes made
    // by compress (the module's) or by python3's zlib
    http::server coded_server(const std::string& body) {
        http::server s;
        s.route("GET /gzip", [body](http::request r, http::response_writer w) {
            w.set_header("Content-Encoding", "gzip");
            w.set_header("X-Asked", r.header("Accept-Encoding"));
            w.write(slice<const byte>(compress::gzip::compress(sgcl::string(body))));
        });
        s.route("GET /deflate", [body](http::request, http::response_writer w) {
            w.set_header("Content-Encoding", "deflate");
            w.write(slice<const byte>(compress::zlib::compress(sgcl::string(body))));
        });
        s.route("GET /br", [body](http::request, http::response_writer w) {
            w.set_header("Content-Encoding", "br");
            w.write(slice<const byte>(compress::brotli::compress(sgcl::string(body))));
        });
        s.route("GET /zstd", [body](http::request, http::response_writer w) {
            w.set_header("Content-Encoding", "zstd");
            w.write(slice<const byte>(compress::zstd::compress(sgcl::string(body))));
        });
        s.route("GET /zstd_wide", [body](http::request, http::response_writer w) {
            // a window of 128 MB, past what RFC 9659 lets a decoder take
            compress::zstd::options o;
            o.window_log = 27;
            o.content_size = false;
            w.set_header("Content-Encoding", "zstd");
            w.write(slice<const byte>(compress::zstd::compress(sgcl::string(body), o)));
        });
        s.route("GET /unknown", [](http::request, http::response_writer w) {
            w.set_header("Content-Encoding", "compress");
            w.write("as it came");
        });
        s.route("GET /broken", [](http::request, http::response_writer w) {
            w.set_header("Content-Encoding", "gzip");
            w.write("\x1f\x8b\x08\x00garbage");
        });
        s.route("GET /plain", [body](http::request r, http::response_writer w) {
            w.set_header("X-Asked", r.header("Accept-Encoding"));
            w.write(sgcl::string(body));
        });
        return s;
    }
}

// --- the client ------------------------------------------------------------------------------

TEST(HttpDecompress_Tests, TransparentByDefault) {
    const std::string body = json_of(20000);
    Running run(coded_server(body));
    http::client c;
    auto g = c.get(run.url("/gzip"));
    ASSERT_TRUE(g);
    EXPECT_TRUE(g->uncompressed());
    EXPECT_EQ(g->header("X-Asked"), "gzip, deflate, br, zstd");
    EXPECT_TRUE(g->header("Content-Encoding").empty());
    EXPECT_TRUE(g->header("Content-Length").empty());
    EXPECT_FALSE(g->content_length());
    EXPECT_EQ(text(*g->text()), body);
    auto d = c.get(run.url("/deflate"));
    ASSERT_TRUE(d);
    EXPECT_TRUE(d->uncompressed());
    auto bytes = d->bytes();
    ASSERT_TRUE(bytes);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size()), body);
    // the body as a stream, decoded
    auto s = c.get(run.url("/gzip"));
    ASSERT_TRUE(s);
    auto all = io::read_all_text(s->body());
    ASSERT_TRUE(all);
    EXPECT_EQ(text(*all), body);
    // br and zstd
    for (const char* coding : {"/br", "/zstd"}) {
        auto r = c.get(run.url(coding));
        ASSERT_TRUE(r);
        EXPECT_TRUE(r->uncompressed());
        EXPECT_TRUE(r->header("Content-Encoding").empty());
        EXPECT_EQ(text(*r->text()), body) << coding;
    }
    // zstd's window past 8 MB (RFC 9659 §3): refused as the body is read
    auto wide = c.get(run.url("/zstd_wide"));
    ASSERT_TRUE(wide);
    EXPECT_FALSE(wide->text());
    // a coding it did not ask for: as it came
    auto unknown = c.get(run.url("/unknown"));
    ASSERT_TRUE(unknown);
    EXPECT_FALSE(unknown->uncompressed());
    EXPECT_EQ(unknown->header("Content-Encoding"), "compress");
    EXPECT_EQ(*unknown->text(), "as it came");
    // a body that does not decode: the read's error
    auto broken = c.get(run.url("/broken"));
    ASSERT_TRUE(broken);
    EXPECT_FALSE(broken->text());
    // the connection is given back after a decoded body: the next request goes
    auto again = c.get(run.url("/plain"));
    ASSERT_TRUE(again);
    EXPECT_FALSE(again->uncompressed());
    EXPECT_EQ(text(*again->text()), body);
}

TEST(HttpDecompress_Tests, NotWhenTheProgramAsks) {
    const std::string body = json_of(5000);
    Running run(coded_server(body));
    http::client c;
    // its own Accept-Encoding: the bytes as they came
    http::request own("GET", run.url("/gzip"));
    own.set_header("Accept-Encoding", "gzip");
    auto r = c.send(own);
    ASSERT_TRUE(r);
    EXPECT_FALSE(r->uncompressed());
    EXPECT_EQ(r->header("Content-Encoding"), "gzip");
    auto raw = r->bytes();
    ASSERT_TRUE(raw);
    auto decoded = compress::gzip::decompress(slice<const byte>(*raw));
    ASSERT_TRUE(decoded);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(decoded->data()), decoded->size()), body);
    // a Range: no coding asked for
    http::request ranged("GET", run.url("/plain"));
    ranged.set_header("Range", "bytes=0-9");
    auto rr = c.send(ranged);
    ASSERT_TRUE(rr);
    EXPECT_EQ(rr->header("X-Asked"), "");
    (void)rr->text();
    // decompress off
    http::client off;
    off.decompress = false;
    auto o = off.get(run.url("/plain"));
    ASSERT_TRUE(o);
    EXPECT_EQ(o->header("X-Asked"), "");
    (void)o->text();
    // HEAD: no body to decode
    auto h = c.head(run.url("/gzip"));
    ASSERT_TRUE(h);
    EXPECT_FALSE(h->uncompressed());
    EXPECT_EQ(*h->text(), "");
}

TEST(HttpDecompress_Tests, AgainstGosServer) {
    if (std::system("command -v go > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no go";
    }
    // Go's net/http answers a gzip of its own handler's (compress/gzip)
    const auto dir = std::filesystem::temp_directory_path() / ("sgcl_gzip_peer_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    {
        FILE* f = std::fopen((dir / "main.go").c_str(), "w");
        std::fputs(R"(package main
import ("compress/gzip"; "fmt"; "net"; "net/http"; "os"; "strings")
func main() {
	body := strings.Repeat("go gzip body ", 5000)
	http.HandleFunc("/g", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Encoding", "gzip")
		z := gzip.NewWriter(w); z.Write([]byte(body)); z.Close()
	})
	http.HandleFunc("/quit", func(w http.ResponseWriter, r *http.Request) { os.Exit(0) })
	l, _ := net.Listen("tcp", "127.0.0.1:0")
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	http.Serve(l, nil)
}
)", f);
        std::fclose(f);
    }
    const std::string bin = (dir / "peer").string();
    ASSERT_EQ(std::system(("cd '" + dir.string() + "' && go build -o '" + bin + "' main.go").c_str()), 0);
    FILE* p = popen(("'" + bin + "'").c_str(), "r");
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof line, p));
    const std::string base = "http://127.0.0.1:" + std::to_string(std::atoi(line + 5));
    http::client c;
    auto r = c.get(sgcl::string(base + "/g"));
    ASSERT_TRUE(r);
    EXPECT_TRUE(r->uncompressed());
    std::string expected;
    for (int i : range(5000)) {
        (void)i;
        expected += "go gzip body ";
    }
    EXPECT_EQ(text(*r->text()), expected);
    (void)c.get(sgcl::string(base + "/quit"));
    pclose(p);
    std::filesystem::remove_all(dir);
}

// --- the server ------------------------------------------------------------------------------

TEST(HttpCompression_Tests, NegotiatedAndApplied) {
    const std::string body = json_of(50000);
    http::server s;
    s.use(http::compression());
    s.route("GET /json", [body](http::request, http::response_writer w) {
        w.set_header("Content-Type", "application/json");
        w.set_header("ETag", "\"v1\"");
        w.set_header("Accept-Ranges", "bytes");
        w.write(sgcl::string(body));
    });
    s.route("GET /small", [](http::request, http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("tiny");
    });
    s.route("GET /png", [body](http::request, http::response_writer w) {
        w.set_header("Content-Type", "image/png");
        w.write(sgcl::string(body));
    });
    s.route("GET /stream", [](http::request, http::response_writer w) -> async::task<> {
        w.set_header("Content-Type", "text/event-stream");
        for (int i : range(5)) {
            w.write("data: event " + to_string(i) + "\n\n");
            (void)co_await w.async_flush();
        }
    });
    s.route("GET /coded", [body](http::request, http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.set_header("Content-Encoding", "identity");
        w.write(sgcl::string(body));
    });
    Running run(s);
    auto ask = [&](const std::string& path, const std::string& accept) {
        http::request req("GET", run.url(path));
        if (!accept.empty()) {
            req.set_header("Accept-Encoding", sgcl::string(accept));
        }
        http::client c;
        c.decompress = false;
        return *c.send(req);
    };
    auto g = ask("/json", "gzip");
    EXPECT_EQ(g.header("Content-Encoding"), "gzip");
    EXPECT_EQ(g.header("Vary"), "Accept-Encoding");
    EXPECT_EQ(g.header("ETag"), "W/\"v1\"");
    EXPECT_TRUE(g.header("Accept-Ranges").empty());
    auto gb = g.bytes();
    ASSERT_TRUE(gb);
    EXPECT_LT(gb->size(), body.size() / 3);
    auto plain = compress::gzip::decompress(slice<const byte>(*gb));
    ASSERT_TRUE(plain);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(plain->data()), plain->size()), body);
    // the server's order among what the client takes; q=0 refuses
    EXPECT_EQ(ask("/json", "deflate, gzip").header("Content-Encoding"), "gzip");
    EXPECT_EQ(ask("/json", "deflate").header("Content-Encoding"), "deflate");
    EXPECT_EQ(ask("/json", "gzip;q=0, deflate;q=0.5").header("Content-Encoding"), "deflate");
    EXPECT_EQ(ask("/json", "*").header("Content-Encoding"), "zstd");
    EXPECT_EQ(ask("/json", "*, zstd;q=0").header("Content-Encoding"), "br");
    EXPECT_EQ(ask("/json", "*, zstd;q=0, br;q=0").header("Content-Encoding"), "gzip");
    EXPECT_EQ(ask("/json", "*, zstd;q=0, br;q=0, gzip;q=0").header("Content-Encoding"), "deflate");
    EXPECT_EQ(ask("/json", "gzip, deflate, br, zstd").header("Content-Encoding"), "zstd");   // a browser's field
    EXPECT_EQ(ask("/json", "compress").header("Content-Encoding"), "");
    for (const char* coding : {"br", "zstd"}) {
        auto r = ask("/json", coding);
        EXPECT_EQ(r.header("Content-Encoding"), coding);
        auto coded = r.bytes();
        ASSERT_TRUE(coded);
        EXPECT_LT(coded->size(), body.size() / 3);
        auto back = std::string_view(coding) == "br" ? compress::brotli::decompress(slice<const byte>(*coded))
                                                     : compress::zstd::decompress(slice<const byte>(*coded));
        ASSERT_TRUE(back) << coding;
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(back->data()), back->size()), body);
    }
    // a stream in zstd and in br: each flush decodable at once, the whole the events
    for (const char* coding : {"br", "zstd"}) {
        http::request req("GET", run.url("/stream"));
        req.set_header("Accept-Encoding", coding);
        http::client c;
        c.decompress = false;
        auto r = c.send(req);
        ASSERT_TRUE(r);
        auto coded = r->bytes();
        ASSERT_TRUE(coded);
        auto back = std::string_view(coding) == "br" ? compress::brotli::decompress(slice<const byte>(*coded))
                                                     : compress::zstd::decompress(slice<const byte>(*coded));
        ASSERT_TRUE(back) << coding;
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(back->data()), back->size()).substr(0, 15), "data: event 0\n\n");
    }
    auto none = ask("/json", "");
    EXPECT_EQ(none.header("Content-Encoding"), "");
    EXPECT_EQ(none.header("Vary"), "Accept-Encoding");   // the answer depends on the field anyway
    EXPECT_EQ(none.header("ETag"), "\"v1\"");
    EXPECT_EQ(text(*none.text()), body);
    auto d = ask("/json", "deflate");
    auto db = d.bytes();
    ASSERT_TRUE(db);
    auto dd = compress::zlib::decompress(slice<const byte>(*db));
    ASSERT_TRUE(dd);
    EXPECT_EQ(dd->size(), body.size());
    // too small, not a type of the list, a coding of the handler's: as they are
    EXPECT_EQ(ask("/small", "gzip").header("Content-Encoding"), "");
    auto png = ask("/png", "gzip");
    EXPECT_EQ(png.header("Content-Encoding"), "");
    EXPECT_EQ(png.header("Vary"), "");
    (void)png.text();
    EXPECT_EQ(ask("/coded", "gzip").header("Content-Encoding"), "identity");
    // a flushed stream compressed as it goes, every event decodable on its own flush
    auto st = ask("/stream", "gzip");
    EXPECT_EQ(st.header("Content-Encoding"), "gzip");
    EXPECT_EQ(st.header("Transfer-Encoding"), "chunked");
    auto sb = st.bytes();
    ASSERT_TRUE(sb);
    auto sd = compress::gzip::decompress(slice<const byte>(*sb));
    ASSERT_TRUE(sd);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(sd->data()), sd->size()), "data: event 0\n\ndata: event 1\n\ndata: event 2\n\ndata: event 3\n\ndata: event 4\n\n");
    // HEAD and a 206: left alone
    http::request head("HEAD", run.url("/json"));
    head.set_header("Accept-Encoding", "gzip");
    http::client c;
    auto hr = c.send(head);
    ASSERT_TRUE(hr);
    EXPECT_EQ(hr->header("Content-Encoding"), "");
    // the module's client both ways: decoded as read
    auto both = c.get(run.url("/json"));
    ASSERT_TRUE(both);
    EXPECT_TRUE(both->uncompressed());
    EXPECT_EQ(text(*both->text()), body);
}

TEST(HttpCompression_Tests, AgainstCurlAndPython) {
    const std::string body = json_of(40000);
    http::server s;
    s.use(http::compression());
    s.route("GET /json", [body](http::request, http::response_writer w) {
        w.set_header("Content-Type", "application/json");
        w.write(sgcl::string(body));
    });
    s.route("GET /stream", [body](http::request, http::response_writer w) -> async::task<> {
        w.set_header("Content-Type", "text/plain");
        for (int i : range(4)) {
            (void)i;
            w.write(sgcl::string(body.substr(0, 10000)));
            (void)co_await w.async_flush();
        }
    });
    Running run(s);
    if (std::system("command -v curl > /dev/null 2>&1") == 0) {
        EXPECT_EQ(shell("curl -s --compressed " + text(run.url("/json"))), body);
        std::string four;
        for (int i : range(4)) {
            (void)i;
            four += body.substr(0, 10000);
        }
        EXPECT_EQ(shell("curl -s --compressed " + text(run.url("/stream"))), four);
        EXPECT_EQ(shell("curl -s -H 'Accept-Encoding: deflate' --compressed " + text(run.url("/json"))), body);
    }
    if (std::system("command -v python3 > /dev/null 2>&1") == 0) {
        const std::string py = "python3 -c \"import urllib.request, zlib; r=urllib.request.urlopen(urllib.request.Request('" + run.base +
                               "/json', headers={'Accept-Encoding':'gzip'})); print(r.headers['Content-Encoding'], len(zlib.decompress(r.read(), 31)))\"";
        EXPECT_EQ(shell(py), "gzip " + std::to_string(body.size()) + "\n");
        // zstd by Python's compression.zstd (3.14), when it has it
        if (std::system("python3 -c 'import compression.zstd' > /dev/null 2>&1") == 0) {
            const std::string pz = "python3 -c \"import urllib.request; from compression import zstd; r=urllib.request.urlopen(urllib.request.Request('" +
                                   run.base + "/json', headers={'Accept-Encoding':'zstd'})); print(r.headers['Content-Encoding'], len(zstd.decompress(r.read())))\"";
            EXPECT_EQ(shell(pz), "zstd " + std::to_string(body.size()) + "\n");
        }
    }
}

// DESIGN 408: the q-values at their ends, the types, a level out of range
TEST(HttpCompression_Tests, Boundaries) {
    using http::detail::choose_coding;
    vector<sgcl::string> ours = {sgcl::string("gzip"), sgcl::string("deflate")};
    EXPECT_EQ(choose_coding("", ours), "");
    EXPECT_EQ(choose_coding("identity", ours), "");
    EXPECT_EQ(choose_coding("GZIP", ours), "gzip");
    EXPECT_EQ(choose_coding("gzip;q=0.000", ours), "");
    EXPECT_EQ(choose_coding("gzip;q=0.001", ours), "gzip");
    EXPECT_EQ(choose_coding("gzip; q=1.0", ours), "gzip");
    EXPECT_EQ(choose_coding("gzip;q=x", ours), "");          // not a qvalue: 0
    EXPECT_EQ(choose_coding(" , ,gzip , ", ours), "gzip");
    EXPECT_EQ(choose_coding("*;q=0", ours), "");
    EXPECT_EQ(choose_coding("deflate;q=0, *", ours), "gzip");
    using http::detail::compressible_type;
    vector<sgcl::string> types = {sgcl::string("text/"), sgcl::string("application/json")};
    EXPECT_TRUE(compressible_type("text/html; charset=utf-8", types));
    EXPECT_TRUE(compressible_type("Application/JSON", types));
    EXPECT_FALSE(compressible_type("text/", types));
    EXPECT_FALSE(compressible_type("application/json-seq", types));
    EXPECT_FALSE(compressible_type("", types));
    EXPECT_THROW(http::compression({.gzip_level = compress::level(10)}), std::invalid_argument);   // compress's own check
    http::compression fastest({.gzip_level = compress::level::fastest});
    http::compression copy = http::compression({.min_size = 0});
    http::server s;
    s.use(copy);
    s.route("GET /e", [](http::request, http::response_writer w) { w.set_header("Content-Type", "text/plain"); });
    auto req = http::test_request("GET", "/e");
    req.set_header("Accept-Encoding", "gzip");
    http::response_recorder rec;
    rec.serve(s, req);
    EXPECT_EQ(rec.header("Content-Encoding"), "gzip");   // min_size 0: even an empty body
    auto out = compress::gzip::decompress(slice<const byte>(io::detail::bytes_of(rec.body())));
    ASSERT_TRUE(out);
    EXPECT_TRUE(out->empty());
}

// Over HTTP/2 both ways, x-gzip, two codings left as they are, a file of the
// handler's compressed through memory, a 304 untouched
TEST(HttpCompression_Tests, H2AndTheRest) {
    const std::string body = json_of(30000);
    const auto dir = std::filesystem::temp_directory_path() / ("sgcl_comp_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    {
        FILE* f = std::fopen((dir / "page.html").c_str(), "w");
        std::fputs(body.c_str(), f);
        std::fclose(f);
    }
    http::server s;
    s.h2c = true;
    s.use(http::compression());
    s.route("GET /json", [body](http::request, http::response_writer w) {
        w.set_header("Content-Type", "application/json");
        w.write(sgcl::string(body));
    });
    s.route("GET /x-gzip", [body](http::request, http::response_writer w) {
        w.set_header("Content-Encoding", "x-gzip");
        w.write(slice<const byte>(compress::gzip::compress(sgcl::string(body))));
    });
    s.route("GET /two", [](http::request, http::response_writer w) {
        w.set_header("Content-Encoding", "gzip, br");
        w.write("opaque");
    });
    s.route("GET /static/{path...}", http::file_server(sgcl::string(dir.string())));
    s.route("GET /same", [](http::request, http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.set_status(304);
    });
    Running run(s);
    http::client c;
    c.h2c = true;
    auto j = c.get(run.url("/json"));
    ASSERT_TRUE(j) << text(j.error().message());
    EXPECT_EQ(j->proto(), "HTTP/2.0");
    EXPECT_TRUE(j->uncompressed());
    EXPECT_EQ(text(*j->text()), body);
    auto x = c.get(run.url("/x-gzip"));
    ASSERT_TRUE(x);
    EXPECT_TRUE(x->uncompressed());
    EXPECT_EQ(text(*x->text()), body);
    auto two = c.get(run.url("/two"));
    ASSERT_TRUE(two);
    EXPECT_FALSE(two->uncompressed());
    EXPECT_EQ(*two->text(), "opaque");
    http::client h1;
    auto file = h1.get(run.url("/static/page.html"));
    ASSERT_TRUE(file);
    EXPECT_TRUE(file->uncompressed());   // read through memory and compressed, not by sendfile
    EXPECT_EQ(text(*file->text()), body);
    http::request same("GET", run.url("/same"));
    same.set_header("Accept-Encoding", "gzip");
    auto nm = h1.send(same);
    ASSERT_TRUE(nm);
    EXPECT_EQ(nm->status(), 304);
    EXPECT_TRUE(nm->header("Content-Encoding").empty());
    std::filesystem::remove_all(dir);
}
