//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: headers (the list, the case of names, the values made safe, the
// names refused), status and reason, cookie (to_string as Go's SetCookie
// writes it, parse as RFC 6265 §5.2 reads a Set-Cookie), the Cookie field
// of a request.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <string>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

TEST(HttpHeaders_Tests, TheList) {
    net::http::headers h;
    EXPECT_TRUE(h.empty());
    h.add("Set-Cookie", "a=1").add("set-cookie", "b=2").set("Content-Type", "text/plain");
    EXPECT_EQ(h.size(), 3u);
    EXPECT_EQ(h.get("SET-COOKIE"), "a=1");
    auto all = h.get_all("Set-Cookie");
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[1], "b=2");
    h.set("set-cookie", "c=3");                                  // the first's place, the others gone
    EXPECT_EQ(h.get_all("Set-Cookie").size(), 1u);
    std::vector<std::string> names;
    for (auto [name, value] : h) {
        names.push_back(std::string(name.data(), name.size()) + "=" + std::string(value.data(), value.size()));
    }
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "Set-Cookie=c=3");                      // the name as it was first written
    EXPECT_EQ(names[1], "Content-Type=text/plain");
    EXPECT_TRUE(h.contains("content-type"));
    h.erase("CONTENT-TYPE");
    EXPECT_FALSE(h.contains("content-type"));
    EXPECT_EQ(h.get("content-type"), "");
    h.set("X", sgcl::string(std::string("a\r\nb\0c", 6)));
    EXPECT_EQ(h.get("x"), "a  b c");
    for (auto bad : {"", "a b", "a:b", "a\r\n", "caf\xC3\xA9", "(x)"}) {
        EXPECT_THROW(h.set(bad, "v"), std::invalid_argument) << bad;
        EXPECT_THROW(h.add(bad, "v"), std::invalid_argument) << bad;
    }
}

TEST(HttpHeaders_Tests, Status) {
    EXPECT_STREQ(net::http::reason(net::http::status::ok), "OK");
    EXPECT_STREQ(net::http::reason(404), "Not Found");
    EXPECT_STREQ(net::http::reason(413), "Content Too Large");
    EXPECT_STREQ(net::http::reason(431), "Request Header Fields Too Large");
    EXPECT_STREQ(net::http::reason(599), "");
    EXPECT_EQ(net::http::status::payload_too_large, 413);
}

TEST(HttpHeaders_Tests, CookieWritten) {
    net::http::cookie c("id", "v1");
    EXPECT_EQ(c.to_string(), "id=v1");
    c.path = "/a";
    c.domain = ".example.com";
    c.max_age = 90s;
    c.secure = true;
    c.http_only = true;
    c.same_site = "strict";
    c.partitioned = true;
    EXPECT_EQ(c.to_string(), "id=v1; Path=/a; Domain=example.com; Max-Age=90; HttpOnly; Secure; SameSite=Strict; Partitioned");
    net::http::cookie odd("n", "a b,c\"d;e\\f\x01");
    EXPECT_EQ(odd.to_string(), "n=\"a b,cdef\"");                // spaces and commas quoted, the rest dropped (Go)
    net::http::cookie gone("n", "");
    gone.max_age = duration::zero();
    EXPECT_EQ(gone.to_string(), "n=; Max-Age=0");
    net::http::cookie bad("a b", "x");
    EXPECT_THROW((void)bad.to_string(), std::invalid_argument);
}

TEST(HttpHeaders_Tests, CookieRead) {
    auto c = net::http::cookie::parse("  id = \"v 1\" ; Path=/p; Domain=.EXAMPLE.com; Max-Age=120; Secure; HttpOnly; SameSite=None; Unknown=x");
    ASSERT_TRUE(c);
    EXPECT_EQ(c->name, "id");
    EXPECT_EQ(c->value, "v 1");
    EXPECT_EQ(c->path, "/p");
    EXPECT_EQ(c->domain, "example.com");
    EXPECT_EQ(c->max_age->nanoseconds(), 120'000'000'000);
    EXPECT_TRUE(c->secure);
    EXPECT_TRUE(c->http_only);
    EXPECT_EQ(c->same_site, "None");
    EXPECT_FALSE(net::http::cookie::parse("novalue"));
    EXPECT_FALSE(net::http::cookie::parse("=x"));
    auto none = net::http::cookie::parse("novalue");
    EXPECT_EQ(none.error().code(), net::errc::invalid_cookie);   // the reason, as every parse of the library gives one
    EXPECT_EQ(none.error().message(), "parse cookie novalue: invalid cookie");
    auto neg = net::http::cookie::parse("a=b; Max-Age=-5; Path=relative; SameSite=weird");
    ASSERT_TRUE(neg);
    EXPECT_EQ(neg->max_age->nanoseconds(), 0);                   // gone now
    EXPECT_EQ(neg->path, "");                                    // not a path: ignored
    EXPECT_EQ(neg->same_site, "");
    auto junk = net::http::cookie::parse("a=b; Max-Age=12x");
    EXPECT_FALSE(junk->max_age);
}

// The dates of HTTP through time (decision A7): IMF-fixdate written,
// the three forms of RFC 9110 §5.6.7 read
TEST(HttpHeaders_Tests, Dates) {
    net::http::headers h;
    auto t = time::datetime::from_unix(784111777, time::zone::utc());
    h.set_date("Last-Modified", t);
    EXPECT_EQ(h.get("last-modified"), "Sun, 06 Nov 1994 08:49:37 GMT");
    EXPECT_EQ(h.date("Last-Modified"), t);
    h.set("A", "Sunday, 06-Nov-94 08:49:37 GMT").set("B", "Sun Nov  6 08:49:37 1994").set("C", "yesterday");
    EXPECT_EQ(h.date("A"), t);
    EXPECT_EQ(h.date("B"), t);
    EXPECT_FALSE(h.date("C"));
    EXPECT_FALSE(h.date("missing"));
    // written in GMT whatever the zone of the instant
    h.set_date("D", t.in(time::zone::fixed(std::chrono::hours(2))));
    EXPECT_EQ(h.get("D"), "Sun, 06 Nov 1994 08:49:37 GMT");
}

// The cookie-date of RFC 6265 §5.1.1, what browsers take for Expires
TEST(HttpHeaders_Tests, CookieExpires) {
    auto at = [](const char* text) -> optional<int64_t> {
        auto c = net::http::cookie::parse(sgcl::string(std::string("a=b; Expires=") + text));
        if (!c || !c->expires) {
            return nullopt;
        }
        return c->expires->unix();
    };
    EXPECT_EQ(at("Wed, 09 Jun 2021 10:18:14 GMT"), 1623233894);
    EXPECT_EQ(at("Wed, 09-Jun-2021 10:18:14 GMT"), 1623233894);      // Netscape's
    EXPECT_EQ(at("Wednesday, 09-Jun-21 10:18:14 GMT"), 1623233894);  // RFC 850
    EXPECT_EQ(at("Wed Jun  9 10:18:14 2021"), 1623233894);           // asctime
    EXPECT_EQ(at("09 Jun 2021 10:18:14"), 1623233894);
    EXPECT_EQ(at("jun 9 10:18:14 2021 extra"), 1623233894);
    EXPECT_EQ(at("Thu, 01 Jan 1970 00:00:00 GMT"), 0);
    EXPECT_EQ(at("Fri, 01 Jan 99 00:00:00 GMT"), 915148800);         // 1999
    EXPECT_EQ(at("Mon, 01 Jan 69 00:00:00 GMT"), 3124224000);        // 2069
    EXPECT_FALSE(at("Wed, 31 Feb 2021 10:18:14 GMT"));               // no such day
    EXPECT_FALSE(at("Wed, 09 Jun 1600 10:18:14 GMT"));               // before 1601
    EXPECT_FALSE(at("Wed, 09 Jun 2021 24:00:00 GMT"));
    EXPECT_FALSE(at("Wed, 09 Jun 2021"));                             // no time
    EXPECT_FALSE(at("Wed, 09 Jun 2021 1a:18:14 GMT"));
    EXPECT_FALSE(at(""));
    auto far = at("Fri, 31 Dec 9999 23:59:59 GMT");                    // "never": the end of the range
    ASSERT_TRUE(far);
    EXPECT_EQ(time::datetime::from_unix(*far, time::zone::utc()).year(), 2262);
    net::http::cookie c("a", "b");
    c.expires = time::datetime::from_unix(1623233894, time::zone::utc());
    EXPECT_EQ(c.to_string(), "a=b; Expires=Wed, 09 Jun 2021 10:18:14 GMT");
}
