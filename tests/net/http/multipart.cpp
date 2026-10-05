//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: multipart/form-data. The form a client sends (the body byte for
// byte, its length, files read as it goes, names that need escaping), the
// reader of a multipart body on every edge RFC 2046 §5.1 has (a boundary's
// text inside a part, LF alone, a preamble and an epilogue, transport
// padding, empty parts, no close delimiter, no boundary at all, heads past
// their limit, more parts than allowed, a part of 8 MB read 1 byte to 64 KB
// at a time, the same body cut in pieces of every size), request::multipart
// and request::form on the server, the client's post of a form through
// HTTP/1.1 and HTTP/2, retried and redirected; interop with Go's
// mime/multipart both ways and with curl -F (skipped where there is none).
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string str(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // A reader over bytes that gives at most `piece` of them a read (0: all
    // it is asked for), so that a body comes cut as a connection cuts it
    struct Pieces : io::mixin::reader<Pieces> {
        std::string data;
        size_t at = 0;
        size_t piece = 0;
        bool fail_at_end = false;

        expected<size_t, io::error> read(const slice<byte>& out) {
            if (at == data.size() && fail_at_end) {
                return unexpected(io::error(std::make_error_code(std::errc::connection_reset), "read"));
            }
            size_t n = std::min(out.size(), data.size() - at);
            if (piece) {
                n = std::min(n, piece);
            }
            std::copy(data.data() + at, data.data() + at + n, reinterpret_cast<char*>(out.data()));
            at += n;
            return n;
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
            co_return read(out);
        }
    };

    io::reader pieces(const std::string& body, size_t piece = 0) {
        tracked_ptr p = make_tracked<Pieces>();
        p->data = body;
        p->piece = piece;
        return io::reader(p);
    }

    // Every part of a body: "name|filename|type|content", or the error
    std::vector<std::string> parts_of(const io::reader& body, const char* boundary, size_t read_size = 4096,
                                      net::http::multipart_reader::limits l = {}) {
        std::vector<std::string> out;
        net::http::multipart_reader m(body, boundary, l);
        vector<byte> buf(read_size);
        for (;;) {
            auto p = m.next();
            if (!p) {
                out.push_back("<" + text(p.error().message()) + ">");
                return out;
            }
            if (!*p) {
                return out;
            }
            std::string content;
            for (;;) {
                auto n = m.read(buf.as_slice());
                if (!n) {
                    out.push_back("<" + text(n.error().message()) + ">");
                    return out;
                }
                if (*n == 0) {
                    break;
                }
                content.append(reinterpret_cast<const char*>(buf.data()), *n);
            }
            out.push_back(text((*p)->name) + "|" + text((*p)->filename) + "|" + text((*p)->content_type) + "|" + content);
        }
    }

    std::vector<std::string> parse(const std::string& body, const char* boundary = "XyZ", size_t piece = 0) {
        return parts_of(pieces(body, piece), boundary);
    }

    using V = std::vector<std::string>;

    std::string temp_file(const std::string& name, const std::string& content) {
        auto dir = std::filesystem::temp_directory_path() / ("sgcl_multipart_" + std::to_string(::getpid()));
        std::filesystem::create_directories(dir);
        auto path = (dir / name).string();
        std::ofstream(path, std::ios::binary) << content;
        return path;
    }

    std::string big(size_t n) {
        std::string s(n, '\0');
        uint32_t x = 12345;
        for (auto& c : s) {
            x = x * 1103515245 + 12345;
            c = char(x >> 23);
        }
        return s;
    }

    bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
    }

    const std::string& go_helper() {
        static std::string path = [] {
            if (!have("go")) {
                return std::string();
            }
            auto src = source_root() / "tests/net/http/go_multipart/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_go_multipart";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
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

    // An upload handler: each part's name, filename, type and size, its
    // content's first 16 bytes
    net::http::server upload_server() {
        net::http::server s;
        s.route("POST /upload", [](net::http::request req, net::http::response_writer w) -> async::task<> {
            auto m = req.multipart();
            if (!m) {
                w.error(net::http::status::bad_request);
                co_return;
            }
            std::string out;
            for (;;) {
                auto p = co_await m->async_next();
                if (!p) {
                    out += "<" + text(p.error().message()) + ">\n";
                    break;
                }
                if (!*p) {
                    break;
                }
                auto content = co_await m->async_read_all();
                if (!content) {
                    out += "<" + text(content.error().message()) + ">\n";
                    break;
                }
                out += text((*p)->name) + "|" + text((*p)->filename) + "|" + text((*p)->content_type) + "|" + std::to_string(content->size()) + "|" +
                       str(*content).substr(0, 16) + "\n";
            }
            w.write(sgcl::string(out));
        });
        s.route("POST /form", [](net::http::request req, net::http::response_writer w) -> async::task<> {
            auto f = co_await req.async_form();
            if (!f) {
                w.write("<" + f.error().message() + ">");
                co_return;
            }
            w.write(f->to_string());
        });
        s.max_body_bytes = 64 << 20;
        return s;
    }
}

TEST(HttpMultipart_Tests, TheFormsBodyByteForByte) {
    auto path = temp_file("a.txt", "file contents");
    net::http::form f{{"name", "value"}, {"q\"x\\y", "two\r\nlines"}, net::http::form::file("upload", sgcl::string(path))};
    f.add("last", "");
    f.add(net::http::form::file("typed", sgcl::string(path), "application/x-test"));
    std::string b = text(f.boundary());
    EXPECT_EQ(b.size(), 32u);
    EXPECT_EQ(text(f.content_type()), "multipart/form-data; boundary=" + b);
    std::string want = "--" + b + "\r\nContent-Disposition: form-data; name=\"name\"\r\n\r\nvalue\r\n"
                     "--" + b + "\r\nContent-Disposition: form-data; name=\"q\\\"x\\\\y\"\r\n\r\ntwo\r\nlines\r\n"
                     "--" + b + "\r\nContent-Disposition: form-data; name=\"upload\"; filename=\"a.txt\"\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nfile contents\r\n"
                     "--" + b + "\r\nContent-Disposition: form-data; name=\"last\"\r\n\r\n\r\n"
                     "--" + b + "\r\nContent-Disposition: form-data; name=\"typed\"; filename=\"a.txt\"\r\nContent-Type: application/x-test\r\n\r\nfile contents\r\n"
                     "--" + b + "--\r\n";
    auto r = f.reader();
    ASSERT_TRUE(r);
    auto all = r->read_all();
    ASSERT_TRUE(all);
    EXPECT_EQ(str(*all), want);
    EXPECT_EQ(f.content_length().value(), want.size());
    // read a byte at a time, the same
    auto again = f.reader();
    std::string slow;
    vector<byte> one(1);
    while (auto n = again->read(one.as_slice())) {
        if (*n == 0) {
            break;
        }
        slow += char(one[0]);
    }
    EXPECT_EQ(slow, want);
    // and read back
    auto back = parse(want, b.c_str());
    EXPECT_EQ(back, (V{"name|||value", "q\"x\\y|||two\r\nlines", "upload|a.txt|text/plain; charset=utf-8|file contents", "last|||",
                       "typed|a.txt|application/x-test|file contents"}));
}

TEST(HttpMultipart_Tests, TheFormsBoundaries) {
    net::http::form empty;
    std::string b = text(empty.boundary());
    EXPECT_EQ(str(*empty.reader()->read_all()), "--" + b + "--\r\n");   // the close delimiter alone
    EXPECT_EQ(empty.content_length().value(), b.size() + 6);
    EXPECT_EQ(parse("--" + b + "--\r\n", b.c_str()), V{});
    EXPECT_NE(text(net::http::form().boundary()), b);   // drawn for each

    net::http::form copy = empty;   // the same form
    copy.add("a", "1");
    EXPECT_EQ(empty.content_length().value(), copy.content_length().value());

    // CR and LF of a name never end its line
    net::http::form names{{"a\r\nInjected: yes", "v"}};
    auto bytes = str(*names.reader()->read_all());
    EXPECT_NE(bytes.find("name=\"a%0D%0AInjected: yes\""), std::string::npos);
    auto parsed = parse(bytes, text(names.boundary()).c_str());
    EXPECT_EQ(parsed, V{"a%0D%0AInjected: yes|||v"});
    // another control byte the same way (a field no parser takes else), HTAB as it is
    net::http::form controls{{sgcl::string(std::string("a\x01" "b\x7f\tc")), "v"}};
    EXPECT_EQ(parse(str(*controls.reader()->read_all()), text(controls.boundary()).c_str()), V{"a%01b%7F\tc|||v"});

    // a file that is not there, a directory: the error when the body is made
    net::http::form missing{net::http::form::file("f", "/no/such/file")};
    EXPECT_EQ(missing.content_length().error().code(), std::errc::no_such_file_or_directory);
    EXPECT_FALSE(missing.reader());
    net::http::form dir{net::http::form::file("f", sgcl::string(std::filesystem::temp_directory_path().string()))};
    EXPECT_EQ(dir.content_length().error().code(), std::errc::is_a_directory);

    // an empty file, a file whose type is not known
    auto e = temp_file("empty.bin", "");
    auto u = temp_file("data.unknownext", "xyz");
    net::http::form files{net::http::form::file("e", sgcl::string(e)), net::http::form::file("u", sgcl::string(u))};
    EXPECT_EQ(parse(str(*files.reader()->read_all()), text(files.boundary()).c_str()),
              (V{"e|empty.bin|application/octet-stream|", "u|data.unknownext|application/octet-stream|xyz"}));
}

TEST(HttpMultipart_Tests, AFileThatChangesAfterItsSizeWasTaken) {
    auto path = temp_file("shrinks.txt", "0123456789");
    net::http::form f{net::http::form::file("f", sgcl::string(path))};
    auto r = f.reader();   // the size taken now: 10
    ASSERT_TRUE(r);
    std::ofstream(path, std::ios::binary) << "01234";
    auto all = r->read_all();
    ASSERT_FALSE(all);
    EXPECT_EQ(all.error().code(), io::errc::unexpected_eof);
    std::ofstream(path, std::ios::binary) << "0123456789abcdef";
    auto grown = f.reader();
    std::ofstream(path, std::ios::binary) << "0123456789abcdefXYZ";
    auto bytes = grown->read_all();
    ASSERT_TRUE(bytes);
    EXPECT_NE(str(*bytes).find("0123456789abcdef\r\n--"), std::string::npos);   // the size it had
}

TEST(HttpMultipart_Tests, TheReaderOnTheEdgesOfRfc2046) {
    // a boundary's text inside a part, not at a line's start or with more after it
    EXPECT_EQ(parse("--XyZ\r\n\r\na--XyZ b\r\n--XyZx\r\n--XyZ \tz\r\nc\r\n--XyZ--\r\n"), (V{"|||a--XyZ b\r\n--XyZx\r\n--XyZ \tz\r\nc"}));
    // LF alone, as Go reads it
    EXPECT_EQ(parse("--XyZ\nContent-Disposition: form-data; name=\"a\"\n\none\n--XyZ\n\ntwo\n--XyZ--\n"), (V{"a|||one", "|||two"}));
    // a preamble, an epilogue
    EXPECT_EQ(parse("preamble line\r\n--XyZ not this\r\nmore\r\n--XyZ\r\n\r\nx\r\n--XyZ--\r\nepilogue --XyZ\r\n"), (V{"|||x"}));
    // transport padding after a boundary
    EXPECT_EQ(parse("--XyZ \t \r\n\r\nx\r\n--XyZ  \r\n\r\ny\r\n--XyZ--"), (V{"|||x", "|||y"}));
    // empty parts: no head, no content
    EXPECT_EQ(parse("--XyZ\r\n\r\n\r\n--XyZ\r\n\r\n\r\n--XyZ--\r\n"), (V{"|||", "|||"}));
    // no part
    EXPECT_EQ(parse("--XyZ--\r\n"), V{});
    EXPECT_EQ(parse("--XyZ--"), V{});
    // no close delimiter, a body cut in a part, cut in a head
    EXPECT_EQ(parse("--XyZ\r\n\r\nx\r\n"), (V{"<multipart: unexpected end of stream>"}));
    EXPECT_EQ(parse("--XyZ\r\n\r\nxyz"), (V{"<multipart: unexpected end of stream>"}));
    EXPECT_EQ(parse("--XyZ\r\nContent-Type: a"), (V{"<multipart: unexpected end of stream>"}));
    EXPECT_EQ(parse("--XyZ\r\n\r\nx\r\n--XyZ"), (V{"<multipart: unexpected end of stream>"}));
    // no boundary at all; an empty body
    EXPECT_EQ(parse("just text"), (V{"<multipart: malformed multipart body>"}));
    EXPECT_EQ(parse(""), (V{"<multipart: malformed multipart body>"}));
    EXPECT_EQ(parse("x--XyZ\r\n\r\na\r\n--XyZ--"), V{});   // not at a line's start: the preamble, then the close delimiter
    EXPECT_EQ(parse("x--XyZ\r\n\r\na\r\n"), (V{"<multipart: malformed multipart body>"}));
    // a boundary followed by something else than padding and a line break
    EXPECT_EQ(parse("--XyZ\r\n\r\nx\r\n--XyZ--\r\n"), (V{"|||x"}));
    EXPECT_EQ(parse("--XyZ\r\n\r\nx\r\n--XyZ\tz\r\n"), (V{"<multipart: unexpected end of stream>"}));
    // a head that is not fields
    EXPECT_EQ(parse("--XyZ\r\nno colon here\r\n\r\nx\r\n--XyZ--"), (V{"<multipart: malformed multipart body>"}));
    EXPECT_EQ(parse("--XyZ\r\nA: b\r\n folded\r\n\r\nx\r\n--XyZ--"), (V{"<multipart: malformed multipart body>"}));   // no obs-fold
}

TEST(HttpMultipart_Tests, ContentDisposition) {
    auto one = [](const std::string& disposition) {
        auto r = parse("--XyZ\r\nContent-Disposition: " + disposition + "\r\n\r\nv\r\n--XyZ--");
        return r.empty() ? std::string() : r[0];
    };
    EXPECT_EQ(one("form-data; name=\"a\""), "a|||v");
    EXPECT_EQ(one("form-data; name=token"), "token|||v");
    EXPECT_EQ(one("form-data;name=\"a\";filename=\"f.txt\""), "a|f.txt||v");
    EXPECT_EQ(one("form-data; NAME=\"a\"; FileName=\"f.txt\""), "a|f.txt||v");   // names without case
    EXPECT_EQ(one("form-data; name=\"q\\\"uote\""), "q\"uote|||v");               // a quoted-string's escapes
    EXPECT_EQ(one("form-data; name=\"a\"; filename=\"C:\\\\dir\\\\f.txt\""), "a|f.txt||v");   // the last element
    EXPECT_EQ(one("form-data; name=\"a\"; filename=\"../../etc/passwd\""), "a|passwd||v");
    EXPECT_EQ(one("form-data; name=\"a\"; filename=\"..\""), "a|||v");
    EXPECT_EQ(one("form-data; name=\"a\"; filename*=UTF-8''%e2%82%ac.txt; filename=\"e.txt\""), "a|\xe2\x82\xac.txt||v");   // filename* first
    EXPECT_EQ(one("form-data; name=\"a\"; filename*=ISO-8859-1''x.txt; filename=\"e.txt\""), "a|e.txt||v");
    EXPECT_EQ(one("form-data; name=\"a\"; name=\"b\""), "a|||v");   // the first
    EXPECT_EQ(one("form-data; name=\"unclosed"), "|||v");          // a value that cannot be read: no name
    EXPECT_EQ(one("attachment"), "|||v");
}

TEST(HttpMultipart_Tests, TheSameBodyInPiecesOfEverySize) {
    std::string body = "preamble\r\n--XyZ\r\nContent-Disposition: form-data; name=\"a\"\r\n\r\nfirst\r\n--XyZ\r\nContent-Disposition: form-data; "
                       "name=\"f\"; filename=\"x.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n" +
                       big(5000) + "\r\n--XyZ\r\n\r\n\r\n--XyZ-- \r\nepilogue";
    auto whole = parse(body);
    ASSERT_EQ(whole.size(), 3u);
    for (size_t piece : {1, 2, 3, 5, 7, 11, 64, 1000, 4097}) {
        EXPECT_EQ(parse(body, "XyZ", piece), whole) << piece;
        for (size_t reads : {1, 3, 100}) {
            EXPECT_EQ(parts_of(pieces(body, piece), "XyZ", reads), whole) << piece << " " << reads;
        }
    }
}

TEST(HttpMultipart_Tests, AHugePartReadAsItComes) {
    const std::string content = big(8 << 20);
    std::string body = "--XyZ\r\nContent-Disposition: form-data; name=\"big\"; filename=\"b\"\r\n\r\n" + content + "\r\n--XyZ--\r\n";
    for (size_t read_size : {1 << 16, 777, 32768}) {
        net::http::multipart_reader m(pieces(body, 10000), "XyZ");
        auto p = m.next();
        ASSERT_TRUE(p && *p);
        vector<byte> buf(read_size);
        size_t total = 0;
        bool same = true;
        for (;;) {
            auto n = m.read(buf.as_slice());
            ASSERT_TRUE(n);
            if (*n == 0) {
                break;
            }
            same = same && std::equal(reinterpret_cast<const char*>(buf.data()), reinterpret_cast<const char*>(buf.data()) + *n, content.data() + total);
            total += *n;
            ASSERT_LE(*n, size_t(32768));   // never more than the window
        }
        EXPECT_EQ(total, content.size());
        EXPECT_TRUE(same);
        auto end = m.next();
        ASSERT_TRUE(end);
        EXPECT_FALSE(*end);
    }
}

TEST(HttpMultipart_Tests, NextReadsPastWhatWasNotRead) {
    std::string body = "--XyZ\r\nContent-Disposition: form-data; name=\"a\"\r\n\r\n" + big(100000) + "\r\n--XyZ\r\nContent-Disposition: form-data; name=\"b\"\r\n\r\nB\r\n--XyZ--";
    net::http::multipart_reader m(pieces(body, 3000), "XyZ");
    vector<byte> buf(10);
    EXPECT_EQ(*m.read(buf.as_slice()), 0u);   // before the first part
    auto a = m.next();
    ASSERT_TRUE(a && *a);
    EXPECT_EQ((*a)->name, "a");
    EXPECT_EQ(*m.read(buf.as_slice()), 10u);   // a little of it
    auto b = m.next();
    ASSERT_TRUE(b && *b);
    EXPECT_EQ((*b)->name, "b");
    EXPECT_EQ(str(*m.read_all()), "B");
    EXPECT_EQ(*m.read(buf.as_slice()), 0u);
    auto end = m.next();
    ASSERT_TRUE(end);
    EXPECT_FALSE(*end);
    EXPECT_EQ(*m.read(buf.as_slice()), 0u);   // after the last
    auto still = m.next();
    ASSERT_TRUE(still);
    EXPECT_FALSE(*still);
    net::http::multipart_reader copy = m;   // the same reader
    EXPECT_FALSE(*copy.next());
    net::http::multipart_reader none;
    EXPECT_FALSE(none);
    EXPECT_TRUE(m);
}

TEST(HttpMultipart_Tests, Limits) {
    std::string many;
    for (int i : range(1001)) {
        many += "--XyZ\r\nContent-Disposition: form-data; name=\"f" + std::to_string(i) + "\"\r\n\r\nv\r\n";
    }
    many += "--XyZ--\r\n";
    auto all = parse(many);
    ASSERT_EQ(all.size(), 1001u);
    EXPECT_EQ(all.back(), "<multipart: too many parts>");   // 1000 by default
    net::http::multipart_reader::limits l;
    l.max_parts = 1001;
    EXPECT_EQ(parts_of(pieces(many), "XyZ", 4096, l).size(), 1001u);
    EXPECT_EQ(parts_of(pieces(many), "XyZ", 4096, l).back(), "f1000|||v");
    l.max_parts = 0;
    EXPECT_EQ(parts_of(pieces(many), "XyZ", 4096, l), V{"<multipart: too many parts>"});

    std::string long_head = "--XyZ\r\nX-Long: " + std::string(16384, 'h') + "\r\n\r\nv\r\n--XyZ--";
    EXPECT_EQ(parse(long_head), V{"<multipart: header too large>"});   // 16 KB a head by default
    net::http::multipart_reader::limits wide;
    wide.max_header_bytes = 20000;
    EXPECT_EQ(parts_of(pieces(long_head), "XyZ", 4096, wide).size(), 1u);
    wide.max_header_bytes = 1 << 20;   // at most the window
    std::string longer = "--XyZ\r\nX-Long: " + std::string(40000, 'h') + "\r\n\r\nv\r\n--XyZ--";
    EXPECT_EQ(parts_of(pieces(longer), "XyZ", 4096, wide), V{"<multipart: header too large>"});
    std::string exact = "--XyZ\r\nX: " + std::string(16384 - 7, 'h') + "\r\n\r\nv\r\n--XyZ--";   // the head at its limit exactly
    EXPECT_EQ(parse(exact).size(), 1u);
    EXPECT_EQ(parse(exact)[0], "|||v");

    // boundaries of 0 and of more than 70 characters
    EXPECT_EQ(parse("----\r\n", ""), V{"<multipart: malformed multipart body>"});
    std::string b70(70, 'b'), b71(71, 'b');
    EXPECT_EQ(parse("--" + b70 + "\r\n\r\nv\r\n--" + b70 + "--", b70.c_str()), V{"|||v"});
    EXPECT_EQ(parse("--" + b71 + "\r\n\r\nv\r\n--" + b71 + "--", b71.c_str()), V{"<multipart: malformed multipart body>"});

    // the source's error
    tracked_ptr failing = make_tracked<Pieces>();
    failing->data = "--XyZ\r\n\r\npart";
    failing->fail_at_end = true;
    EXPECT_EQ(parts_of(io::reader(failing), "XyZ"), V{"<read: Connection reset by peer>"});
}

TEST(HttpMultipart_Tests, TheServersSide) {
    auto srv = upload_server();
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    std::string base = "http://127.0.0.1:" + std::to_string(l.local_endpoint().port());
    net::http::client c;
    c.proxy = net::http::proxy();
    auto path = temp_file("photo.png", big(300000));
    auto res = c.post(sgcl::string(base + "/upload"), net::http::form{{"title", "a photo"}, net::http::form::file("photo", sgcl::string(path))});
    ASSERT_TRUE(res) << text(res.error().message());
    EXPECT_EQ(text(*res->text()), "title|||7|a photo\nphoto|photo.png|image/png|300000|" + big(16) + "\n");

    // not multipart, no boundary
    auto plain = c.post(sgcl::string(base + "/upload"), "text/plain", "x");
    EXPECT_EQ(plain->status(), 400);
    (void)plain->text();
    net::http::request no_boundary("POST", sgcl::string(base + "/upload"));
    no_boundary.set_header("Content-Type", "multipart/form-data").set_body("x");
    EXPECT_EQ(c.send(no_boundary)->status(), 400);

    // form(): urlencoded and multipart, files left out
    auto urlencoded = c.post(sgcl::string(base + "/form"), "application/x-www-form-urlencoded", "a=1&b=two+words&a=3");
    EXPECT_EQ(text(*urlencoded->text()), "a=1&b=two+words&a=3");
    auto fields = c.post(sgcl::string(base + "/form"), net::http::form{{"a", "1"}, net::http::form::file("f", sgcl::string(path)), {"b", "x y"}});
    EXPECT_EQ(text(*fields->text()), "a=1&b=x+y");
    auto other = c.post(sgcl::string(base + "/form"), "application/json", "{}");
    EXPECT_EQ(text(*other->text()), "");
    net::http::form huge{{"v", sgcl::string(std::string((10 << 20) + 1, 'v'))}};
    auto too_big = c.post(sgcl::string(base + "/form"), huge);
    EXPECT_EQ(text(*too_big->text()), "<form: body too large>");
    srv.close();
    (void)serving.wait();
}

TEST(HttpMultipart_Tests, TheServersBodyLimitBoundsIt) {
    auto srv = upload_server();
    srv.max_body_bytes = 100000;
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    std::string base = "http://127.0.0.1:" + std::to_string(l.local_endpoint().port());
    net::http::client c;
    c.proxy = net::http::proxy();
    auto path = temp_file("big.bin", big(200000));
    auto res = c.post(sgcl::string(base + "/upload"), net::http::form{net::http::form::file("f", sgcl::string(path))});
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status(), 413);   // the length declared past it
    (void)res->text();
    // chunked, the limit met as it is read
    tracked_ptr stream = make_tracked<io::buffer>(sgcl::string("--b\r\n\r\n" + big(200000) + "\r\n--b--\r\n"));
    net::http::request chunked("POST", sgcl::string(base + "/upload"));
    chunked.set_header("Content-Type", "multipart/form-data; boundary=b").set_body(io::reader(stream));
    auto cut = c.send(chunked);
    ASSERT_TRUE(cut);
    EXPECT_EQ(text(*cut->text()), "<read body: body too large>\n");
    srv.close();
    (void)serving.wait();
}

TEST(HttpMultipart_Tests, TheClientsFormRetriedRedirectedAndOverHttp2) {
    net::http::server srv = upload_server();
    srv.h2c = true;
    srv.route("POST /again", [](net::http::request, net::http::response_writer w) { w.redirect("/upload", 307); });
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    std::string base = "http://127.0.0.1:" + std::to_string(l.local_endpoint().port());
    auto path = temp_file("r.txt", "redirected file");
    net::http::form f{{"k", "v"}, net::http::form::file("f", sgcl::string(path))};
    const std::string want = "k|||1|v\nf|r.txt|text/plain; charset=utf-8|15|redirected file\n";
    net::http::client c;
    c.proxy = net::http::proxy();
    auto redirected = c.post(sgcl::string(base + "/again"), f);   // a form goes again with 307
    ASSERT_TRUE(redirected) << text(redirected.error().message());
    EXPECT_EQ(text(*redirected->text()), want);
    net::http::client h2;
    h2.proxy = net::http::proxy();
    h2.h2c = true;
    auto over_h2 = h2.post(sgcl::string(base + "/upload"), f);
    ASSERT_TRUE(over_h2);
    EXPECT_EQ(text(over_h2->proto()), "HTTP/2.0");
    EXPECT_EQ(text(*over_h2->text()), want);
    auto task_form = [](net::http::client c, sgcl::string url, net::http::form f) -> async::task<std::string> {
        auto r = co_await c.async_post(url, f);
        co_return r ? text(*co_await r->async_text()) : text(r.error().message());
    };
    EXPECT_EQ(async::spawn(task_form(c, sgcl::string(base + "/upload"), f)).wait(), want);
    // a file that is not there: the send's error, before a connection
    auto missing = c.post(sgcl::string("http://127.0.0.1:1/upload"), net::http::form{net::http::form::file("f", "/no/such/file")});
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), std::errc::no_such_file_or_directory);
    srv.close();
    (void)serving.wait();
}

// Go's mime/multipart: Go writes and this reads, this writes and Go reads
// (go_multipart/main.go), and curl -F to the server
TEST(HttpMultipartInterop_Tests, GoBothWays) {
    if (go_helper().empty()) {
        GTEST_SKIP() << "no go to build the helper with";
    }
    // Go writes: the boundary on the first line, the body after it
    std::string out = run("'" + go_helper() + "' write");
    auto nl = out.find('\n');
    ASSERT_NE(nl, std::string::npos);
    std::string boundary = out.substr(0, nl);
    auto parts = parse(out.substr(nl + 1), boundary.c_str());
    ASSERT_EQ(parts.size(), 4u);
    EXPECT_EQ(parts[0], "field|||value");
    EXPECT_EQ(parts[1], "quote\"d|||with \"quotes\"");
    EXPECT_EQ(parts[2], "file|a.txt|application/octet-stream|file content\r\n--" + boundary + "x\r\n");
    EXPECT_EQ(parts[3].substr(0, 17), "big|big.bin|image");
    EXPECT_EQ(parts[3].size(), 17 + std::string("/png|").size() + 1000000 - 4);

    // this writes, Go reads
    auto path = temp_file("for_go.bin", big(70000));
    net::http::form f{{"field", "value"}, {"q\"uote", "x"}, net::http::form::file("upload", sgcl::string(path))};
    auto body_path = temp_file("form_body.bin", str(*f.reader()->read_all()));
    auto read = run("'" + go_helper() + "' read " + text(f.boundary()) + " < '" + body_path + "'");
    EXPECT_EQ(read, "field||value\nq\"uote||x\nupload|for_go.bin|70000\n");
}

TEST(HttpMultipartInterop_Tests, CurlUploadsAForm) {
    if (!have("curl")) {
        GTEST_SKIP() << "no curl";
    }
    auto srv = upload_server();
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    std::string url = "http://127.0.0.1:" + std::to_string(l.local_endpoint().port()) + "/upload";
    auto path = temp_file("curl.txt", "sent by curl");
    auto out = run("curl -s --max-time 10 -F 'name=value' -F 'upload=@" + path + ";type=text/x-curl' " + url);
    EXPECT_EQ(out, "name|||5|value\nupload|curl.txt|text/x-curl|12|sent by curl\n");
    srv.close();
    (void)serving.wait();
}
