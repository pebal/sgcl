//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The validators, the conditional requests and the ranges of serve.h
// (serve_file, serve_content, file_server, serve with options) and the
// pure functions under them (detail/content.h): the Range field read as RFC
// 9110 §14.1.2 reads it, the lists of entity tags, the order of §13.2.2,
// If-Range; the answers through a recorder (bytes in memory) and through a
// server (a file by sendfile, HTTP/1.1 and h2c, HEAD); every case held
// against Go's http.ServeContent over the same bytes and validators
// (go_serve/main.go), but the few where the RFC and Go part, each named.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <cctype>
#include <mutex>
#include <tuple>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

using namespace sgcl;
using net::http::detail::ByteRange;
using net::http::detail::RangeVerdict;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // The 4000 bytes of the Go peer's /c
    std::string content4000() {
        std::string s;
        for (int i : range(200)) {
            (void)i;
            s += "0123456789abcdefghij";
        }
        return s;
    }

    void write(const std::filesystem::path& p, const std::string& data) {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << data;
    }

    std::string read(const std::filesystem::path& p) {
        std::ifstream f(p, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(f), {});
    }

    struct Dir {
        std::filesystem::path root;

        explicit Dir(const char* name) {
            root = std::filesystem::temp_directory_path() / (std::string(name) + "_" + std::to_string(::getpid()));
            std::filesystem::remove_all(root);
            std::filesystem::create_directories(root);
        }

        ~Dir() {
            std::filesystem::remove_all(root);
        }

        std::string at(const std::string& name) const {
            return (root / name).string();
        }
    };

    // A server of the program's on a port of the loopback
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

    // The parts of a multipart/byteranges body: each part's Content-Range
    // and bytes; empty when the body is not one
    struct Part {
        std::string range;
        std::string type;
        std::string data;
    };

    std::vector<Part> parts_of(const std::string& content_type, const std::string& body) {
        std::vector<Part> out;
        auto b = content_type.find("boundary=");
        if (content_type.rfind("multipart/byteranges", 0) != 0 || b == std::string::npos) {
            return out;
        }
        const std::string delim = "--" + content_type.substr(b + 9);
        size_t at = body.find(delim);
        while (at != std::string::npos) {
            at += delim.size();
            if (body.compare(at, 2, "--") == 0) {
                break;
            }
            at += 2;   // CRLF
            auto end_head = body.find("\r\n\r\n", at);
            if (end_head == std::string::npos) {
                break;
            }
            Part p;
            std::string head = body.substr(at, end_head - at);
            size_t line = 0;
            while (line < head.size()) {
                auto eol = head.find("\r\n", line);
                std::string l = head.substr(line, eol == std::string::npos ? std::string::npos : eol - line);
                if (l.rfind("Content-Range: ", 0) == 0) {
                    p.range = l.substr(15);
                } else if (l.rfind("Content-Type: ", 0) == 0) {
                    p.type = l.substr(14);
                }
                if (eol == std::string::npos) {
                    break;
                }
                line = eol + 2;
            }
            auto next = body.find("\r\n" + delim, end_head + 4);
            if (next == std::string::npos) {
                break;
            }
            p.data = body.substr(end_head + 4, next - end_head - 4);
            out.push_back(p);
            at = next + 2;
        }
        return out;
    }

    // serve_content of bytes through a recorder: the request's fields given
    struct Answer {
        int status = 0;
        net::http::headers fields;
        std::string body;
    };

    Answer answer(const std::string& method, std::initializer_list<std::pair<const char*, const char*>> in, const std::string& content,
                  const net::http::serve_options& o = {}, std::initializer_list<std::pair<const char*, const char*>> preset = {}) {
        auto req = net::http::test_request(sgcl::string(method), "/c.txt");
        for (auto& f : in) {
            req.headers().add(f.first, f.second);
        }
        net::http::response_recorder rec;
        auto w = rec.writer();
        for (auto& f : preset) {
            w.set_header(f.first, f.second);
        }
        net::http::serve_content(req, w, "c.txt", sgcl::string(content), o);
        return Answer{rec.status(), rec.headers(), text(rec.body())};
    }

    // Every byte but an unreserved one as %XX, for a query's value
    std::string escape(std::string_view s) {
        std::string out;
        for (unsigned char c : s) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                out += char(c);
            } else {
                char b[4];
                std::snprintf(b, sizeof b, "%%%02X", c);
                out += b;
            }
        }
        return out;
    }

    // The Go peer built once; "" when there is no go to build it with
    const std::string& go_peer() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto src = source_root() / "tests/net/http/go_serve/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_http_go_serve";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }
}

// --- the Range field (RFC 9110 §14.1.2), as Go answers it but where the RFC parts ----------

TEST(HttpRanges_Tests, TheField) {
    struct Case {
        const char* field;
        uint64_t size;
        RangeVerdict verdict;
        std::vector<std::pair<uint64_t, uint64_t>> ranges;
    };
    const std::vector<Case> cases = {
        {"bytes=0-4", 20, RangeVerdict::ranges, {{0, 5}}},
        {"bytes=5-", 20, RangeVerdict::ranges, {{5, 15}}},
        {"bytes=-3", 20, RangeVerdict::ranges, {{17, 3}}},
        {"bytes=-25", 20, RangeVerdict::ranges, {{0, 20}}},           // a suffix longer than the whole: all of it
        {"bytes=19-", 20, RangeVerdict::ranges, {{19, 1}}},
        {"bytes=0-0", 20, RangeVerdict::ranges, {{0, 1}}},
        {"bytes=15-99", 20, RangeVerdict::ranges, {{15, 5}}},          // the last cut to the end
        {"bytes=0-4,6-8", 20, RangeVerdict::ranges, {{0, 5}, {6, 3}}},
        {"bytes=0-4 , 6-8", 20, RangeVerdict::ranges, {{0, 5}, {6, 3}}},
        {"bytes= 0-4", 20, RangeVerdict::ranges, {{0, 5}}},
        {"bytes=0-4,", 20, RangeVerdict::ranges, {{0, 5}}},
        {"bytes=00-04", 20, RangeVerdict::ranges, {{0, 5}}},
        {"bytes=1-1,3-3,5-5", 20, RangeVerdict::ranges, {{1, 1}, {3, 1}, {5, 1}}},
        {"bytes=0-4,30-40", 20, RangeVerdict::ranges, {{0, 5}}},       // the one that does not overlap dropped
        {"Bytes=0-4", 20, RangeVerdict::ranges, {{0, 5}}},            // a unit is without case (RFC §14.1)
        {"bytes=20-", 20, RangeVerdict::unsatisfiable, {}},
        {"bytes=25-30", 20, RangeVerdict::unsatisfiable, {}},
        {"bytes=-0", 20, RangeVerdict::unsatisfiable, {}},            // Go: 206 "bytes 20-19/20"; the RFC: unsatisfiable
        {"bytes=0-", 0, RangeVerdict::unsatisfiable, {}},
        {"bytes=-5", 0, RangeVerdict::unsatisfiable, {}},
        {"bytes=4-2", 20, RangeVerdict::invalid, {}},
        {"bytes=a-b", 20, RangeVerdict::invalid, {}},
        {"bytes=-", 20, RangeVerdict::invalid, {}},
        {"bytes=5", 20, RangeVerdict::invalid, {}},
        {"bytes=+5-6", 20, RangeVerdict::invalid, {}},
        {"bytes=0-99999999999999999999", 20, RangeVerdict::invalid, {}},
        {"bytes=9223372036854775808-", 20, RangeVerdict::invalid, {}},  // 2^63
        {"bytes=0000000000000000000000004-5", 20, RangeVerdict::ranges, {{4, 2}}},   // zeros lead any number
        {"bytes=", 20, RangeVerdict::whole, {}},
        {"bytes=,", 20, RangeVerdict::whole, {}},
        {"items=0-4", 20, RangeVerdict::whole, {}},                   // Go: 416; the RFC: another unit ignored
        {"0-4", 20, RangeVerdict::whole, {}},
        {"bytes=0-,0-", 20, RangeVerdict::whole, {}},                 // more than the size together: whole
        {"bytes=0-10,5-15", 20, RangeVerdict::whole, {}},
    };
    for (auto& c : cases) {
        auto r = net::http::detail::parse_ranges(c.field, c.size);
        EXPECT_EQ(int(r.verdict), int(c.verdict)) << c.field;
        ASSERT_EQ(r.ranges.size(), c.ranges.size()) << c.field;
        for (size_t i = 0; i < c.ranges.size(); ++i) {
            EXPECT_EQ(r.ranges[i].start, c.ranges[i].first) << c.field;
            EXPECT_EQ(r.ranges[i].length, c.ranges[i].second) << c.field;
        }
    }
    // a hundred ranges answered, a hundred and one: the whole
    std::string hundred = "bytes=";
    for (int i : range(101)) {
        hundred += (i ? "," : "") + std::to_string(i) + "-" + std::to_string(i);
    }
    EXPECT_EQ(int(net::http::detail::parse_ranges(hundred, 1000).verdict), int(RangeVerdict::whole));
    hundred = hundred.substr(0, hundred.rfind(','));
    auto ok = net::http::detail::parse_ranges(hundred, 1000);
    EXPECT_EQ(int(ok.verdict), int(RangeVerdict::ranges));
    EXPECT_EQ(ok.ranges.size(), 100u);
}

TEST(HttpRanges_Tests, ContentRangeReadBack) {
    using net::http::detail::parse_content_range;
    auto a = parse_content_range("bytes 0-4/20");
    ASSERT_TRUE(a);
    EXPECT_EQ(a->first, 0u);
    EXPECT_EQ(a->last, 4u);
    EXPECT_EQ(a->size, 20u);
    auto star = parse_content_range("bytes */20");
    ASSERT_TRUE(star);
    EXPECT_TRUE(star->unsatisfied);
    EXPECT_EQ(star->size, 20u);
    auto unknown = parse_content_range("bytes 5-9/*");
    ASSERT_TRUE(unknown);
    EXPECT_FALSE(unknown->size);
    for (const char* bad : {"", "bytes", "bytes 4-2/20", "bytes 0-20/20", "items 0-4/20", "bytes */*", "bytes 0-4", "bytes a-b/c", "bytes -4/20"}) {
        EXPECT_FALSE(parse_content_range(bad)) << bad;
    }
    ByteRange r{5, 10};
    EXPECT_EQ(net::http::detail::content_range(&r, 100), "bytes 5-14/100");
    EXPECT_EQ(net::http::detail::content_range(nullptr, 100), "bytes */100");
}

TEST(HttpRanges_Tests, EntityTagLists) {
    using net::http::detail::etag_list_matches;
    // strong: both strong and equal; weak: the opaque parts equal
    EXPECT_TRUE(etag_list_matches(R"("abc")", R"("abc")", true));
    EXPECT_FALSE(etag_list_matches(R"(W/"abc")", R"("abc")", true));
    EXPECT_FALSE(etag_list_matches(R"("abc")", R"(W/"abc")", true));
    EXPECT_TRUE(etag_list_matches(R"(W/"abc")", R"("abc")", false));
    EXPECT_TRUE(etag_list_matches(R"("abc")", R"(W/"abc")", false));
    EXPECT_TRUE(etag_list_matches(R"("x", "abc")", R"("abc")", true));
    EXPECT_TRUE(etag_list_matches(R"( "x" ,, "abc" )", R"("abc")", true));
    EXPECT_TRUE(etag_list_matches("*", R"("abc")", true));
    EXPECT_TRUE(etag_list_matches("*", "", false));          // a representation is there
    EXPECT_FALSE(etag_list_matches(R"("abc")", "", false));  // no tag of ours: only * matches
    EXPECT_FALSE(etag_list_matches(R"(abc)", R"("abc")", false));     // not a tag: the list ends
    EXPECT_FALSE(etag_list_matches(R"(bad, "abc")", R"("abc")", false));
    EXPECT_FALSE(etag_list_matches(R"("ab c")", R"("ab c")", false));  // a space is no etagc
    EXPECT_FALSE(etag_list_matches(R"("abc)", R"("abc")", false));     // unterminated
    EXPECT_FALSE(etag_list_matches("", R"("abc")", false));
    EXPECT_TRUE(net::http::detail::valid_etag(R"(W/"a")"));
    EXPECT_TRUE(net::http::detail::valid_etag(R"("")"));
    EXPECT_FALSE(net::http::detail::valid_etag(R"("a" "b")"));
    EXPECT_FALSE(net::http::detail::valid_etag("a"));
}

TEST(HttpRanges_Tests, PreconditionsInTheirOrder) {
    using net::http::detail::check_preconditions;
    using net::http::detail::Precondition;
    auto h = [](std::initializer_list<std::pair<const char*, const char*>> f) {
        net::http::headers out;
        for (auto& x : f) {
            out.add(x.first, x.second);
        }
        return out;
    };
    const int64_t mod = 1700000000;
    const char* at = "Tue, 14 Nov 2023 22:13:20 GMT";
    const char* before = "Tue, 14 Nov 2023 22:13:19 GMT";
    EXPECT_EQ(check_preconditions(h({}), "GET", R"("a")", mod), Precondition::proceed);
    EXPECT_EQ(check_preconditions(h({{"If-Match", R"("b")"}}), "GET", R"("a")", mod), Precondition::failed);
    // If-Match decides alone: If-Unmodified-Since is not looked at
    EXPECT_EQ(check_preconditions(h({{"If-Match", R"("a")"}, {"If-Unmodified-Since", before}}), "GET", R"("a")", mod), Precondition::proceed);
    EXPECT_EQ(check_preconditions(h({{"If-Unmodified-Since", before}}), "GET", R"("a")", mod), Precondition::failed);
    EXPECT_EQ(check_preconditions(h({{"If-Unmodified-Since", at}}), "GET", R"("a")", mod), Precondition::proceed);
    EXPECT_EQ(check_preconditions(h({{"If-Unmodified-Since", "garbage"}}), "GET", R"("a")", mod), Precondition::proceed);
    EXPECT_EQ(check_preconditions(h({{"If-None-Match", R"(W/"a")"}}), "GET", R"("a")", mod), Precondition::not_modified);
    EXPECT_EQ(check_preconditions(h({{"If-None-Match", R"("a")"}}), "HEAD", R"("a")", mod), Precondition::not_modified);
    EXPECT_EQ(check_preconditions(h({{"If-None-Match", R"("a")"}}), "PUT", R"("a")", mod), Precondition::failed);
    // If-None-Match decides alone: If-Modified-Since is not looked at
    EXPECT_EQ(check_preconditions(h({{"If-None-Match", R"("z")"}, {"If-Modified-Since", at}}), "GET", R"("a")", mod), Precondition::proceed);
    EXPECT_EQ(check_preconditions(h({{"If-Modified-Since", at}}), "GET", R"("a")", mod), Precondition::not_modified);
    EXPECT_EQ(check_preconditions(h({{"If-Modified-Since", before}}), "GET", R"("a")", mod), Precondition::proceed);
    EXPECT_EQ(check_preconditions(h({{"If-Modified-Since", at}}), "POST", R"("a")", mod), Precondition::proceed);   // GET and HEAD only
    EXPECT_EQ(check_preconditions(h({{"If-Modified-Since", at}}), "GET", R"("a")", nullopt), Precondition::proceed);   // no time: no condition
    // the RFC's order: a failed If-Match beats a matching If-None-Match
    EXPECT_EQ(check_preconditions(h({{"If-Match", R"("b")"}, {"If-None-Match", R"("a")"}}), "GET", R"("a")", mod), Precondition::failed);

    using net::http::detail::if_range_allows;
    EXPECT_TRUE(if_range_allows(h({}), R"("a")", mod));
    EXPECT_TRUE(if_range_allows(h({{"If-Range", R"("a")"}}), R"("a")", mod));
    EXPECT_FALSE(if_range_allows(h({{"If-Range", R"(W/"a")"}}), R"(W/"a")", mod));   // the strong comparison
    EXPECT_FALSE(if_range_allows(h({{"If-Range", R"("b")"}}), R"("a")", mod));
    EXPECT_TRUE(if_range_allows(h({{"If-Range", at}}), R"("a")", mod));
    EXPECT_FALSE(if_range_allows(h({{"If-Range", before}}), R"("a")", mod));
    EXPECT_FALSE(if_range_allows(h({{"If-Range", at}}), R"("a")", nullopt));
    EXPECT_FALSE(if_range_allows(h({{"If-Range", "garbage"}}), R"("a")", mod));
}

// --- serve_content of bytes, through a recorder ---------------------------------------------

TEST(HttpServeContent_Tests, WholeWithItsValidators) {
    const std::string c = "0123456789abcdefghij";
    auto a = answer("GET", {}, c);
    EXPECT_EQ(a.status, 200);
    EXPECT_EQ(a.body, c);
    EXPECT_EQ(text(a.fields.get("Accept-Ranges")), "bytes");
    EXPECT_EQ(text(a.fields.get("Content-Type")), "text/plain; charset=utf-8");
    const std::string weak = text(a.fields.get("ETag"));
    EXPECT_EQ(weak.rfind("W/\"", 0), 0u) << weak;   // weak by default: of the bytes' 64-bit digest
    EXPECT_FALSE(a.fields.contains("Last-Modified"));   // bytes in memory have no time but the handler's
    auto strong = answer("GET", {}, c, {.etag = net::http::etag_kind::strong});
    const std::string s = text(strong.fields.get("ETag"));
    EXPECT_EQ(s.size(), 34u) << s;   // 32 hex digits in quotes
    EXPECT_EQ(s.front(), '"');
    EXPECT_EQ(text(answer("GET", {}, c, {.etag = net::http::etag_kind::strong}).fields.get("ETag")), s);   // the same bytes, the same tag
    EXPECT_NE(text(answer("GET", {}, c + "!", {.etag = net::http::etag_kind::strong}).fields.get("ETag")), s);
    auto none = answer("GET", {}, c, {.etag = net::http::etag_kind::none});
    EXPECT_FALSE(none.fields.contains("ETag"));
    // the handler's own validators and type are the ones used
    auto own = answer("GET", {{"If-None-Match", R"("v7")"}}, c, {}, {{"ETag", R"("v7")"}, {"Content-Type", "x/y"}});
    EXPECT_EQ(own.status, 304);
    EXPECT_EQ(text(own.fields.get("ETag")), R"("v7")");
    auto typed = answer("GET", {}, c, {}, {{"Content-Type", "x/y"}, {"Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT"}});
    EXPECT_EQ(text(typed.fields.get("Content-Type")), "x/y");
    auto since = answer("GET", {{"If-Modified-Since", "Tue, 14 Nov 2023 22:13:20 GMT"}}, c, {.etag = net::http::etag_kind::none},
                        {{"Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT"}});
    EXPECT_EQ(since.status, 304);
    EXPECT_TRUE(since.fields.contains("Last-Modified"));   // no ETag: Last-Modified stays on the 304
}

TEST(HttpServeContent_Tests, NotModifiedAndFailed) {
    const std::string c = "0123456789abcdefghij";
    auto tag = text(answer("GET", {}, c, {.etag = net::http::etag_kind::strong}).fields.get("ETag"));
    auto nm = answer("GET", {{"If-None-Match", tag.c_str()}}, c, {.etag = net::http::etag_kind::strong},
                     {{"Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT"}, {"Content-Encoding", "identity"}});
    EXPECT_EQ(nm.status, 304);
    EXPECT_EQ(nm.body, "");
    EXPECT_FALSE(nm.fields.contains("Content-Type"));
    EXPECT_FALSE(nm.fields.contains("Content-Encoding"));
    EXPECT_FALSE(nm.fields.contains("Last-Modified"));   // the ETag carries the validation
    EXPECT_EQ(text(nm.fields.get("ETag")), tag);
    auto failed = answer("PUT", {{"If-None-Match", "*"}}, c);
    EXPECT_EQ(failed.status, 412);
    EXPECT_EQ(failed.body, "");
    auto match = answer("GET", {{"If-Match", R"("other")"}}, c);
    EXPECT_EQ(match.status, 412);
    auto weak_match = answer("GET", {{"If-Match", "W/\"x\""}}, c, {}, {{"ETag", "W/\"x\""}});
    EXPECT_EQ(weak_match.status, 412);   // If-Match compares strongly: a weak tag never matches
    auto star = answer("GET", {{"If-Match", "*"}}, c);
    EXPECT_EQ(star.status, 200);
}

TEST(HttpServeContent_Tests, Ranges) {
    const std::string c = "0123456789abcdefghij";
    auto one = answer("GET", {{"Range", "bytes=5-9"}}, c);
    EXPECT_EQ(one.status, 206);
    EXPECT_EQ(one.body, "56789");
    EXPECT_EQ(text(one.fields.get("Content-Range")), "bytes 5-9/20");
    auto many = answer("GET", {{"Range", "bytes=0-1,-2"}}, c);
    EXPECT_EQ(many.status, 206);
    const std::string ctype = text(many.fields.get("Content-Type"));
    auto parts = parts_of(ctype, many.body);
    ASSERT_EQ(parts.size(), 2u) << ctype << "\n" << many.body;
    EXPECT_EQ(parts[0].range, "bytes 0-1/20");
    EXPECT_EQ(parts[0].data, "01");
    EXPECT_EQ(parts[0].type, "text/plain; charset=utf-8");
    EXPECT_EQ(parts[1].range, "bytes 18-19/20");
    EXPECT_EQ(parts[1].data, "ij");
    EXPECT_EQ(many.body.substr(0, 2), "--");   // no CRLF before the first boundary, as Go writes it
    // unsatisfiable: 416 with the size; invalid: 416 without; the validators dropped
    auto past = answer("GET", {{"Range", "bytes=20-"}}, c, {}, {{"Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT"}, {"Cache-Control", "max-age=60"}});
    EXPECT_EQ(past.status, 416);
    EXPECT_EQ(text(past.fields.get("Content-Range")), "bytes */20");
    EXPECT_FALSE(past.fields.contains("ETag"));
    EXPECT_FALSE(past.fields.contains("Last-Modified"));
    EXPECT_FALSE(past.fields.contains("Cache-Control"));
    EXPECT_FALSE(past.fields.contains("Accept-Ranges"));
    auto bad = answer("GET", {{"Range", "bytes=4-2"}}, c);
    EXPECT_EQ(bad.status, 416);
    EXPECT_FALSE(bad.fields.contains("Content-Range"));
    // another unit, another method, ranges off: the whole
    EXPECT_EQ(answer("GET", {{"Range", "items=0-4"}}, c).status, 200);
    auto post = answer("POST", {{"Range", "bytes=0-4"}}, c);
    EXPECT_EQ(post.status, 200);
    EXPECT_EQ(post.body, c);
    auto off = answer("GET", {{"Range", "bytes=0-4"}}, c, {.ranges = false});
    EXPECT_EQ(off.status, 200);
    EXPECT_EQ(text(off.fields.get("Accept-Ranges")), "none");
    // If-Range: the strong tag or the exact date; a weak tag never
    auto tag = text(answer("GET", {}, c, {.etag = net::http::etag_kind::strong}).fields.get("ETag"));
    EXPECT_EQ(answer("GET", {{"Range", "bytes=0-4"}, {"If-Range", tag.c_str()}}, c, {.etag = net::http::etag_kind::strong}).status, 206);
    EXPECT_EQ(answer("GET", {{"Range", "bytes=0-4"}, {"If-Range", R"("old")"}}, c, {.etag = net::http::etag_kind::strong}).status, 200);
    auto weak_tag = text(answer("GET", {}, c).fields.get("ETag"));
    EXPECT_EQ(answer("GET", {{"Range", "bytes=0-4"}, {"If-Range", weak_tag.c_str()}}, c).status, 200);
    std::initializer_list<std::pair<const char*, const char*>> lm = {{"Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT"}};
    EXPECT_EQ(answer("GET", {{"Range", "bytes=0-4"}, {"If-Range", "Tue, 14 Nov 2023 22:13:20 GMT"}}, c, {}, lm).status, 206);
    EXPECT_EQ(answer("GET", {{"Range", "bytes=0-4"}, {"If-Range", "Tue, 14 Nov 2023 22:13:21 GMT"}}, c, {}, lm).status, 200);
    // an encoded body is not ranged by its encoding's bytes: no Accept-Ranges
    auto encoded = answer("GET", {}, c, {}, {{"Content-Encoding", "gzip"}});
    EXPECT_FALSE(encoded.fields.contains("Accept-Ranges"));
}

// DESIGN 408: at the ends — empty content, a range of the last byte, the
// largest number, a field of only commas, the handler's invalid ETag kept
TEST(HttpServeContent_Tests, Boundaries) {
    auto empty = answer("GET", {}, "");
    EXPECT_EQ(empty.status, 200);
    EXPECT_EQ(empty.body, "");
    auto empty_range = answer("GET", {{"Range", "bytes=0-"}}, "");
    EXPECT_EQ(empty_range.status, 416);
    EXPECT_EQ(text(empty_range.fields.get("Content-Range")), "bytes */0");
    auto last = answer("GET", {{"Range", "bytes=-1"}}, "abc");
    EXPECT_EQ(last.body, "c");
    auto max = answer("GET", {{"Range", "bytes=0-9223372036854775807"}}, "abc");
    EXPECT_EQ(max.status, 206);
    EXPECT_EQ(max.body, "abc");
    EXPECT_EQ(answer("GET", {{"Range", ",,,"}}, "abc").status, 200);
    auto bad_tag = answer("GET", {{"If-None-Match", "x"}}, "abc", {}, {{"ETag", "x"}});
    EXPECT_EQ(bad_tag.status, 200);   // no tag compares with what is not one
    EXPECT_EQ(text(bad_tag.fields.get("ETag")), "x");
    // a default-constructed options and a copy of it
    net::http::serve_options o;
    net::http::serve_options copy = o;
    EXPECT_EQ(int(copy.etag), int(net::http::etag_kind::weak));
    EXPECT_TRUE(copy.ranges);
}

// --- files through a server ------------------------------------------------------------------

TEST(HttpServeFile_Tests, RangesOfAFile) {
    Dir d("sgcl_serve_ranges");
    const std::string c = content4000();
    write(d.root / "c.txt", c);
    net::http::server s;
    s.route("GET /static/{path...}", net::http::file_server(sgcl::string(d.root.string())));
    s.route("GET /one", [&](net::http::request req, net::http::response_writer w) { net::http::serve_file(req, w, sgcl::string(d.at("c.txt"))); });
    Running r(s);
    net::http::client web;
    for (const char* path : {"/static/c.txt", "/one"}) {
        net::http::request one("GET", r.url(path));
        one.set_header("Range", "bytes=100-199");
        auto res = web.send(one);
        ASSERT_TRUE(res) << text(res.error().message());
        EXPECT_EQ(res->status(), 206) << path;
        EXPECT_EQ(text(res->header("Content-Range")), "bytes 100-199/4000");
        EXPECT_EQ(res->content_length(), 100u);
        EXPECT_EQ(text(*res->text()), c.substr(100, 100));
        net::http::request many("GET", r.url(path));
        many.set_header("Range", "bytes=0-9,3990-");
        auto m = web.send(many);
        ASSERT_TRUE(m);
        EXPECT_EQ(m->status(), 206);
        auto parts = parts_of(text(m->header("Content-Type")), text(*m->text()));
        ASSERT_EQ(parts.size(), 2u);
        EXPECT_EQ(parts[0].data, c.substr(0, 10));
        EXPECT_EQ(parts[1].data, c.substr(3990));
        EXPECT_EQ(parts[1].range, "bytes 3990-3999/4000");
        // HEAD: the head of the range alone
        net::http::request head("HEAD", r.url(path));
        head.set_header("Range", "bytes=10-19");
        auto h = web.send(head);
        ASSERT_TRUE(h);
        EXPECT_EQ(h->status(), 206);
        EXPECT_EQ(h->content_length(), 10u);
        EXPECT_EQ(text(h->header("Content-Range")), "bytes 10-19/4000");
        EXPECT_EQ(*h->text(), "");
        net::http::request mhead("HEAD", r.url(path));
        mhead.set_header("Range", "bytes=0-9,20-29");
        auto mh = web.send(mhead);
        ASSERT_TRUE(mh);
        EXPECT_EQ(mh->status(), 206);
        EXPECT_GT(mh->content_length().value_or(0), 20u);
        EXPECT_EQ(*mh->text(), "");
        // the rest of the connection is still in step after the HEADs
        auto whole = web.get(r.url(path));
        ASSERT_TRUE(whole);
        EXPECT_EQ(whole->status(), 200);
        EXPECT_EQ(text(*whole->text()), c);
    }
}

TEST(HttpServeFile_Tests, TagsOfAFile) {
    Dir d("sgcl_serve_tags");
    write(d.root / "a.txt", "first version");
    net::http::server s;
    s.route("GET /weak/{path...}", net::http::file_server(sgcl::string(d.root.string())));
    s.route("GET /strong/{path...}", net::http::file_server(sgcl::string(d.root.string()), {.etag = net::http::etag_kind::strong}));
    s.route("GET /none/{path...}", net::http::file_server(sgcl::string(d.root.string()), {.etag = net::http::etag_kind::none}));
    s.route("GET /plain/{path...}", net::http::file_server(sgcl::string(d.root.string()), {.ranges = false}));
    Running r(s);
    net::http::client web;
    auto weak = web.get(r.url("/weak/a.txt"));
    ASSERT_TRUE(weak);
    const std::string w = text(weak->header("ETag"));
    struct stat st;
    ASSERT_EQ(::stat(d.at("a.txt").c_str(), &st), 0);
    char expect[64];
#if defined(__APPLE__)
    const int64_t ns = int64_t(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
    const int64_t ns = int64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
    std::snprintf(expect, sizeof expect, "W/\"%llx-%llx\"", (unsigned long long)st.st_size, (unsigned long long)ns);
    EXPECT_EQ(w, expect);
    (void)weak->text();
    auto strong = web.get(r.url("/strong/a.txt"));
    ASSERT_TRUE(strong);
    const std::string s1 = text(strong->header("ETag"));
    EXPECT_EQ(s1, "\"" + [] {
        auto d = hash::xxh3_128::of("first version");
        std::string out;
        char b[3];
        for (auto x : d) {
            std::snprintf(b, sizeof b, "%02x", unsigned(uint8_t(x)));
            out += b;
        }
        return out;
    }() + "\"");
    (void)strong->text();
    // a strong tag ranges with If-Range; a weak one gets the whole
    net::http::request ranged("GET", r.url("/strong/a.txt"));
    ranged.set_header("Range", "bytes=0-4").set_header("If-Range", sgcl::string(s1));
    auto rr = web.send(ranged);
    ASSERT_TRUE(rr);
    EXPECT_EQ(rr->status(), 206);
    EXPECT_EQ(*rr->text(), "first");
    net::http::request weak_ranged("GET", r.url("/weak/a.txt"));
    weak_ranged.set_header("Range", "bytes=0-4").set_header("If-Range", sgcl::string(w));
    auto wr = web.send(weak_ranged);
    ASSERT_TRUE(wr);
    EXPECT_EQ(wr->status(), 200);
    (void)wr->text();
    // 304 by either tag
    for (auto [path, tag] : {std::pair<const char*, std::string>{"/weak/a.txt", w}, {"/strong/a.txt", s1}}) {
        net::http::request again("GET", r.url(path));
        again.set_header("If-None-Match", sgcl::string(tag));
        auto res = web.send(again);
        ASSERT_TRUE(res);
        EXPECT_EQ(res->status(), 304) << path;
        EXPECT_EQ(*res->text(), "");
    }
    // the file changed: both tags change, the old ones no longer match
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    write(d.root / "a.txt", "second version!");
    auto changed = web.get(r.url("/strong/a.txt"));
    ASSERT_TRUE(changed);
    EXPECT_NE(text(changed->header("ETag")), s1);
    EXPECT_EQ(*changed->text(), "second version!");
    net::http::request old("GET", r.url("/strong/a.txt"));
    old.set_header("If-None-Match", sgcl::string(s1));
    auto fresh = web.send(old);
    ASSERT_TRUE(fresh);
    EXPECT_EQ(fresh->status(), 200);
    (void)fresh->text();
    auto weak2 = web.get(r.url("/weak/a.txt"));
    ASSERT_TRUE(weak2);
    EXPECT_NE(text(weak2->header("ETag")), w);
    (void)weak2->text();
    auto none = web.get(r.url("/none/a.txt"));
    ASSERT_TRUE(none);
    EXPECT_TRUE(none->header("ETag").empty());
    EXPECT_FALSE(none->header("Last-Modified").empty());
    (void)none->text();
    net::http::request plain("GET", r.url("/plain/a.txt"));
    plain.set_header("Range", "bytes=0-4");
    auto p = web.send(plain);
    ASSERT_TRUE(p);
    EXPECT_EQ(p->status(), 200);
    EXPECT_EQ(text(p->header("Accept-Ranges")), "none");
    (void)p->text();
}

TEST(HttpServeFile_Tests, ServeFileAndContentOfAFile) {
    Dir d("sgcl_serve_file");
    write(d.root / "doc.html", "<p>doc</p>");
    ASSERT_EQ(::mkfifo(d.at("fifo").c_str(), 0600), 0);
    std::filesystem::create_directories(d.root / "dir");
    net::http::server s;
    s.route("GET /f/{name}", [&](net::http::request req, net::http::response_writer w) {
        net::http::serve_file(req, w, sgcl::string(d.at(text(req.path_value("name")))));
    });
    s.route("GET /open", [&](net::http::request req, net::http::response_writer w) {
        auto f = io::open(sgcl::string(d.at("doc.html")));
        (void)f->seek(5);   // the content is read from its start whatever the position
        net::http::serve_content(req, w, "x.json", *f);
    });
    s.route("GET /pipe", [](net::http::request req, net::http::response_writer w) {
        int fds[2];
        if (::pipe(fds) != 0) {
            w.error(500);
            return;
        }
        ::close(fds[1]);
        io::file f = io::from_fd(fds[0]);
        net::http::serve_content(req, w, "x", f);
    });
    s.route("GET /any", net::http::file_server(sgcl::string(d.root.string())));   // no wildcard: the URL's path
    Running r(s);
    net::http::client web;
    web.timeout = std::chrono::seconds(5);
    auto doc = web.get(r.url("/f/doc.html"));
    ASSERT_TRUE(doc);
    EXPECT_EQ(doc->status(), 200);
    EXPECT_EQ(text(doc->header("Content-Type")), "text/html; charset=utf-8");
    EXPECT_EQ(*doc->text(), "<p>doc</p>");
    for (const char* missing : {"/f/none", "/f/fifo", "/f/dir"}) {
        auto res = web.get(r.url(missing));
        ASSERT_TRUE(res) << missing;
        EXPECT_EQ(res->status(), 404) << missing;
        (void)res->text();
    }
    auto opened = web.get(r.url("/open"));
    ASSERT_TRUE(opened);
    EXPECT_EQ(text(opened->header("Content-Type")), "application/json");
    EXPECT_EQ(*opened->text(), "<p>doc</p>");
    auto pipe = web.get(r.url("/pipe"));
    ASSERT_TRUE(pipe);
    EXPECT_EQ(pipe->status(), 500);
    (void)pipe->text();
    write(d.root / "any", "the any file");
    auto any = web.get(r.url("/any"));
    ASSERT_TRUE(any);
    EXPECT_EQ(*any->text(), "the any file");
    // the one-call server with options
    net::http::file_server copy = net::http::file_server(sgcl::string(d.root.string()), {.etag = net::http::etag_kind::none});
    net::http::file_server moved = std::move(copy);
    net::http::server s2;
    s2.route("GET /{path...}", moved);
    Running r2(s2);
    auto via = web.get(r2.url("/doc.html"));
    ASSERT_TRUE(via);
    EXPECT_TRUE(via->header("ETag").empty());
    EXPECT_EQ(*via->text(), "<p>doc</p>");
}

TEST(HttpServeFile_Tests, RangesOverH2c) {
    Dir d("sgcl_serve_h2");
    const std::string c = content4000();
    write(d.root / "c.txt", c);
    net::http::server s;
    s.h2c = true;
    s.route("GET /{path...}", net::http::file_server(sgcl::string(d.root.string())));
    Running r(s);
    net::http::client web;
    web.h2c = true;
    net::http::request one("GET", r.url("/c.txt"));
    one.set_header("Range", "bytes=3000-");
    auto res = web.send(one);
    ASSERT_TRUE(res) << text(res.error().message());
    EXPECT_EQ(res->proto(), "HTTP/2.0");
    EXPECT_EQ(res->status(), 206);
    EXPECT_EQ(text(*res->text()), c.substr(3000));
    net::http::request many("GET", r.url("/c.txt"));
    many.set_header("Range", "bytes=1-2,5-6");
    auto m = web.send(many);
    ASSERT_TRUE(m);
    auto parts = parts_of(text(m->header("Content-Type")), text(*m->text()));
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[1].data, c.substr(5, 2));
    net::http::request head("HEAD", r.url("/c.txt"));
    head.set_header("Range", "bytes=0-99");
    auto h = web.send(head);
    ASSERT_TRUE(h);
    EXPECT_EQ(h->status(), 206);
    EXPECT_EQ(h->content_length(), 100u);
    EXPECT_EQ(*h->text(), "");
}

// --- against Go's ServeContent ---------------------------------------------------------------

TEST(HttpServeContent_Tests, SameAnswersAsGo) {
    if (go_peer().empty()) {
        GTEST_SKIP() << "no go to build the peer with";
    }
    FILE* p = popen(("'" + go_peer() + "'").c_str(), "r");
    ASSERT_TRUE(p);
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof(line), p));
    const int port = std::atoi(line + 5);
    ASSERT_GT(port, 0);
    const std::string go = "http://127.0.0.1:" + std::to_string(port);
    const std::string c = content4000();
    net::http::server s;
    s.route("GET /c", [&](net::http::request req, net::http::response_writer w) {
        auto e = req.query("etag");
        if (!e.empty()) {
            w.set_header("ETag", e);
        }
        w.set_header("Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT");
        net::http::serve_content(req, w, "c.txt", sgcl::string(c), {.etag = net::http::etag_kind::none});
    });
    s.route("HEAD /c", [&](net::http::request req, net::http::response_writer w) {
        auto e = req.query("etag");
        if (!e.empty()) {
            w.set_header("ETag", e);
        }
        w.set_header("Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT");
        net::http::serve_content(req, w, "c.txt", sgcl::string(c), {.etag = net::http::etag_kind::none});
    });
    Running ours(s);
    struct Case {
        const char* method;
        const char* etag;   // the representation's, "" for none
        std::vector<std::pair<const char*, const char*>> fields;
    };
    const char* at = "Tue, 14 Nov 2023 22:13:20 GMT";
    const char* before = "Tue, 14 Nov 2023 22:13:19 GMT";
    const char* after = "Tue, 14 Nov 2023 22:13:21 GMT";
    std::vector<Case> cases;
    for (const char* rg : {"bytes=0-4", "bytes=5-", "bytes=-3", "bytes=-4000", "bytes=-5000", "bytes=3999-", "bytes=4000-", "bytes=5000-6000",
                           "bytes=0-0", "bytes=4-2", "bytes=0-4,6-8", "bytes=0-4,9000-9999", "bytes=0-,0-", "bytes=0-2000,1000-3000",
                           "bytes= 0-4", "bytes=0-4 , 6-8", "bytes=", "bytes=,", "bytes=0-4,", "bytes=a-b", "bytes=-", "bytes=00-04",
                           "bytes=0-99999999999999999999", "bytes=1-1,3-3,5-5", "bytes=10-20,30-40,3990-"}) {
        cases.push_back({"GET", "", {{"Range", rg}}});
    }
    cases.push_back({"HEAD", "", {{"Range", "bytes=0-4"}}});
    cases.push_back({"HEAD", "", {{"Range", "bytes=0-4,10-14"}}});
    cases.push_back({"GET", R"("abc")", {{"If-None-Match", R"("abc")"}}});
    cases.push_back({"GET", R"("abc")", {{"If-None-Match", R"(W/"abc")"}}});
    cases.push_back({"GET", R"("abc")", {{"If-None-Match", "*"}}});
    cases.push_back({"GET", R"("abc")", {{"If-None-Match", R"("x", "abc")"}}});
    cases.push_back({"GET", R"("abc")", {{"If-None-Match", R"("x")"}}});
    cases.push_back({"HEAD", R"("abc")", {{"If-None-Match", R"("abc")"}}});
    cases.push_back({"GET", R"("abc")", {{"If-Match", R"("abc")"}}});
    cases.push_back({"GET", R"("abc")", {{"If-Match", R"("x")"}}});
    cases.push_back({"GET", R"(W/"abc")", {{"If-Match", R"(W/"abc")"}}});
    cases.push_back({"GET", R"("abc")", {{"If-Match", "*"}}});
    cases.push_back({"GET", "", {{"If-Match", "*"}}});
    cases.push_back({"GET", "", {{"If-Unmodified-Since", at}}});
    cases.push_back({"GET", "", {{"If-Unmodified-Since", before}}});
    cases.push_back({"GET", "", {{"If-Modified-Since", at}}});
    cases.push_back({"GET", "", {{"If-Modified-Since", before}}});
    cases.push_back({"GET", "", {{"If-Modified-Since", after}}});
    cases.push_back({"GET", "", {{"If-Modified-Since", "garbage"}}});
    cases.push_back({"GET", R"("abc")", {{"If-Modified-Since", at}, {"If-None-Match", R"("zz")"}}});
    cases.push_back({"GET", R"("abc")", {{"If-Match", R"("x")"}, {"If-None-Match", R"("abc")"}}});
    cases.push_back({"GET", R"("abc")", {{"Range", "bytes=0-4"}, {"If-Range", R"("abc")"}}});
    cases.push_back({"GET", R"("abc")", {{"Range", "bytes=0-4"}, {"If-Range", R"("x")"}}});
    cases.push_back({"GET", R"(W/"abc")", {{"Range", "bytes=0-4"}, {"If-Range", R"(W/"abc")"}}});
    cases.push_back({"GET", "", {{"Range", "bytes=0-4"}, {"If-Range", at}}});
    cases.push_back({"GET", "", {{"Range", "bytes=0-4"}, {"If-Range", after}}});
    cases.push_back({"GET", R"("abc")", {{"Range", "bytes=0-4"}, {"If-None-Match", R"("abc")"}}});
    cases.push_back({"GET", R"("abc")", {{"Range", "bytes=0-4"}, {"If-Match", R"("abc")"}}});
    net::http::client web;
    int compared = 0;
    for (auto& k : cases) {
        auto one = [&](const std::string& base) {
            std::string url = base + "/c";
            if (*k.etag) {
                url += "?etag=" + escape(k.etag);
            }
            net::http::request req(k.method, sgcl::string(url));
            for (auto& f : k.fields) {
                req.headers().add(f.first, f.second);
            }
            auto res = web.send(req);
            EXPECT_TRUE(res) << url;
            std::map<std::string, std::string> out;
            if (!res) {
                return out;
            }
            auto body = text(*res->text());
            out["status"] = std::to_string(res->status());
            for (const char* f : {"Content-Range", "Accept-Ranges", "ETag", "Last-Modified"}) {
                out[f] = text(res->header(f));
            }
            const std::string ctype = text(res->header("Content-Type"));
            if (res->status() == 416) {
                body.clear();   // the text of the error is each side's own
            }
            if (ctype.rfind("multipart/byteranges", 0) == 0) {
                out["type"] = "multipart";
                for (auto& part : parts_of(ctype, body)) {
                    out["parts"] += part.range + "|" + part.type + "|" + part.data + ";";
                }
                // the length of the head of a HEAD: the boundaries differ in length
            } else {
                out["type"] = res->status() == 416 ? "" : ctype;
                out["length"] = res->status() == 416 ? "" : std::to_string(res->content_length().value_or(0));
                out["body"] = body;
            }
            return out;
        };
        auto theirs = one(go);
        auto mine = one(ours.base);
        std::string what = std::string(k.method) + " etag=" + k.etag;
        for (auto& f : k.fields) {
            what += std::string(" ") + f.first + ": " + f.second;
        }
        EXPECT_EQ(mine, theirs) << what;
        ++compared;
    }
    EXPECT_EQ(compared, int(cases.size()));
    // where the RFC and Go part, each named (the RFC's answer above in TheField)
    for (auto [rg, go_status, our_status] : {std::tuple<const char*, int, int>{"items=0-4", 416, 200}, {"bytes=-0", 206, 416}}) {
        net::http::request a("GET", sgcl::string(go + "/c"));
        a.set_header("Range", rg);
        net::http::request b("GET", ours.url("/c"));
        b.set_header("Range", rg);
        auto ra = web.send(a);
        auto rb = web.send(b);
        ASSERT_TRUE(ra);
        ASSERT_TRUE(rb);
        EXPECT_EQ(ra->status(), go_status) << rg;
        EXPECT_EQ(rb->status(), our_status) << rg;
        (void)ra->text();
        (void)rb->text();
    }
    net::http::request post_go("POST", sgcl::string(go + "/c"));
    post_go.set_header("Range", "bytes=0-4");
    auto pg = web.send(post_go);
    ASSERT_TRUE(pg);
    EXPECT_EQ(pg->status(), 206);   // Go ranges any method; ours: GET and HEAD (Ranges above)
    (void)pg->text();
    (void)web.get(sgcl::string(go + "/quit"));
    pclose(p);
}

// --- a download continued -------------------------------------------------------------------

namespace {
    // A server of one file whose first `breaks` answers stop half-way: the
    // head with the whole length and half the body, then the connection
    // closed. The validators are the handler's (the ETag given, "" for
    // none; Last-Modified when lm). What each request asked is kept
    struct Flaky {
        std::string content;
        std::string etag = "\"v1\"";
        bool lm = false;
        std::atomic<int> breaks = 1;
        std::atomic<int> requests = 0;
        std::mutex lock;
        std::vector<std::string> asked;   // "Range|If-Range" of each request

        net::http::server server() {
            net::http::server s;
            s.route("GET /file", [this](net::http::request req, net::http::response_writer w) {
                ++requests;
                {
                    std::lock_guard<std::mutex> g(lock);
                    asked.push_back(text(req.header("Range")) + "|" + text(req.header("If-Range")));
                }
                if (breaks.load() > 0 && req.header("Range").empty()) {
                    --breaks;
                    auto h = w.hijack();
                    if (!h) {
                        return;
                    }
                    std::string head = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(content.size()) + "\r\n";
                    if (!etag.empty()) {
                        head += "ETag: " + etag + "\r\n";
                    }
                    if (lm) {
                        head += "Last-Modified: Tue, 14 Nov 2023 22:13:20 GMT\r\n";
                    }
                    head += "\r\n" + content.substr(0, content.size() / 2);
                    (void)h->first.write(sgcl::string(head));
                    (void)h->first.close();
                    return;
                }
                if (!etag.empty()) {
                    w.set_header("ETag", sgcl::string(etag));
                }
                if (lm) {
                    w.set_header("Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT");
                }
                net::http::serve_content(req, w, "file.bin", sgcl::string(content), {.etag = net::http::etag_kind::none});
            });
            s.route("GET /missing", [](net::http::request, net::http::response_writer w) { w.error(404); });
            return s;
        }
    };

    std::string big_content() {
        std::string c;
        for (int i : range(20000)) {
            c += std::to_string(i) + ",";
        }
        return c;
    }
}

TEST(HttpDownload_Tests, ContinuedWithinTheCall) {
    Dir d("sgcl_download_retry");
    tracked_ptr f = make_tracked<Flaky>();
    f->content = big_content();
    Running r(f->server());
    net::http::client web;
    auto res = web.download(r.url("/file"), sgcl::string(d.at("out.bin")));
    ASSERT_TRUE(res) << text(res.error().message());
    EXPECT_EQ(res->status(), 206);
    EXPECT_EQ(read(d.root / "out.bin"), f->content);
    EXPECT_FALSE(std::filesystem::exists(d.root / "out.bin.part"));
    EXPECT_FALSE(std::filesystem::exists(d.root / "out.bin.part.meta"));
    ASSERT_EQ(f->asked.size(), 2u);
    EXPECT_EQ(f->asked[0], "|");
    EXPECT_EQ(f->asked[1], "bytes=" + std::to_string(f->content.size() / 2) + "-|\"v1\"");
    // retries 0: the error, nothing left
    f->breaks = 1;
    auto none = web.download(r.url("/file"), sgcl::string(d.at("two.bin")), {.retries = 0});
    ASSERT_FALSE(none);
    EXPECT_FALSE(std::filesystem::exists(d.root / "two.bin"));
    EXPECT_FALSE(std::filesystem::exists(d.root / "two.bin.part"));
    EXPECT_FALSE(std::filesystem::exists(d.root / "two.bin.part.meta"));
}

TEST(HttpDownload_Tests, ResumedByTheNextCall) {
    Dir d("sgcl_download_resume");
    tracked_ptr f = make_tracked<Flaky>();
    f->content = big_content();
    Running r(f->server());
    net::http::client web;
    const sgcl::string out(d.at("out.bin"));
    auto first = web.download(r.url("/file"), out, {.resume = true, .retries = 0});
    ASSERT_FALSE(first);
    EXPECT_EQ(read(d.root / "out.bin.part"), f->content.substr(0, f->content.size() / 2));
    EXPECT_EQ(read(d.root / "out.bin.part.meta"), text(r.url("/file")) + "\n\"v1\"\n");
    auto second = web.download(r.url("/file"), out, {.resume = true});
    ASSERT_TRUE(second) << text(second.error().message());
    EXPECT_EQ(read(d.root / "out.bin"), f->content);
    EXPECT_FALSE(std::filesystem::exists(d.root / "out.bin.part.meta"));
    // without resume, a part left behind is started over
    write(d.root / "fresh.bin.part", "garbage");
    write(d.root / "fresh.bin.part.meta", text(r.url("/file")) + "\n\"v1\"\n");
    auto fresh = web.download(r.url("/file"), sgcl::string(d.at("fresh.bin")));
    ASSERT_TRUE(fresh);
    EXPECT_EQ(fresh->status(), 200);
    EXPECT_EQ(read(d.root / "fresh.bin"), f->content);
    // a meta of another URL: started over
    write(d.root / "other.bin.part", "garbage");
    write(d.root / "other.bin.part.meta", "http://elsewhere/\n\"v1\"\n");
    auto other = web.download(r.url("/file"), sgcl::string(d.at("other.bin")), {.resume = true});
    ASSERT_TRUE(other);
    EXPECT_EQ(other->status(), 200);
    EXPECT_EQ(read(d.root / "other.bin"), f->content);
}

TEST(HttpDownload_Tests, ChangedOrCompleteOrRefused) {
    Dir d("sgcl_download_changed");
    tracked_ptr f = make_tracked<Flaky>();
    f->content = big_content();
    f->breaks = 0;
    Running r(f->server());
    net::http::client web;
    // the file changed since the part: If-Range does not match, the whole comes
    write(d.root / "a.bin.part", "stale start");
    write(d.root / "a.bin.part.meta", text(r.url("/file")) + "\n\"v0\"\n");
    auto changed = web.download(r.url("/file"), sgcl::string(d.at("a.bin")), {.resume = true});
    ASSERT_TRUE(changed) << text(changed.error().message());
    EXPECT_EQ(changed->status(), 200);
    EXPECT_EQ(read(d.root / "a.bin"), f->content);
    // the part is the whole file already: 416 with its size completes it
    write(d.root / "b.bin.part", f->content);
    write(d.root / "b.bin.part.meta", text(r.url("/file")) + "\n\"v1\"\n");
    auto complete = web.download(r.url("/file"), sgcl::string(d.at("b.bin")), {.resume = true});
    ASSERT_TRUE(complete) << text(complete.error().message());
    EXPECT_EQ(complete->status(), 416);
    EXPECT_EQ(read(d.root / "b.bin"), f->content);
    EXPECT_FALSE(std::filesystem::exists(d.root / "b.bin.part.meta"));
    // a status that is an error: nothing kept, even with resume
    write(d.root / "c.bin.part", "x");
    write(d.root / "c.bin.part.meta", text(r.url("/missing")) + "\n\"v1\"\n");
    auto missing = web.download(r.url("/missing"), sgcl::string(d.at("c.bin")), {.resume = true});
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), net::errc::http_status);
    EXPECT_FALSE(std::filesystem::exists(d.root / "c.bin.part"));
    EXPECT_FALSE(std::filesystem::exists(d.root / "c.bin.part.meta"));
}

TEST(HttpDownload_Tests, TheValidators) {
    Dir d("sgcl_download_validators");
    // no validator: never continued, whatever the retries
    tracked_ptr bare = make_tracked<Flaky>();
    bare->content = big_content();
    bare->etag = "";
    Running r(bare->server());
    net::http::client web;
    auto none = web.download(r.url("/file"), sgcl::string(d.at("a.bin")), {.resume = true, .retries = 3});
    ASSERT_FALSE(none);
    EXPECT_EQ(bare->requests.load(), 1);
    EXPECT_FALSE(std::filesystem::exists(d.root / "a.bin.part"));   // nothing to resume by: not kept
    // a weak ETag is no validator for If-Range; Last-Modified is
    tracked_ptr weak = make_tracked<Flaky>();
    weak->content = big_content();
    weak->etag = "W/\"w\"";
    weak->lm = true;
    Running r2(weak->server());
    auto by_date = web.download(r2.url("/file"), sgcl::string(d.at("b.bin")));
    ASSERT_TRUE(by_date) << text(by_date.error().message());
    EXPECT_EQ(read(d.root / "b.bin"), weak->content);
    ASSERT_EQ(weak->asked.size(), 2u);
    EXPECT_EQ(weak->asked[1], "bytes=" + std::to_string(weak->content.size() / 2) + "-|Tue, 14 Nov 2023 22:13:20 GMT");
}
