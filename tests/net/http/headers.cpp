//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
    // kept as given, and judged where they are written: the client's send
    // and the server's response refuse what invalid_field names
    EXPECT_FALSE(net::http::detail::invalid_field(h));
    h.set("X", sgcl::string(std::string("a\r\nb\0c", 6)));
    EXPECT_EQ(h.get("x"), sgcl::string(std::string("a\r\nb\0c", 6)));
    EXPECT_EQ(net::http::detail::invalid_field(h), "invalid header value: X");
    h.erase("x");
    h.set("X", "a\tb \x80");                                     // a tab, a space, obs-text: a value may
    EXPECT_FALSE(net::http::detail::invalid_field(h));
    for (auto bad : {"", "a b", "a:b", "a\r\n", "caf\xC3\xA9", "(x)"}) {
        net::http::headers one;
        one.add(bad, "v");
        auto e = net::http::detail::invalid_field(one);
        ASSERT_TRUE(e) << bad;
        EXPECT_EQ(e->view().substr(0, 21), "invalid header name: ") << bad;
    }
    net::http::headers named;
    named.add("a\r\n", "v");
    EXPECT_EQ(net::http::detail::invalid_field(named), "invalid header name: a\\x0D\\x0A");   // no line break into a log
}

TEST(HttpHeaders_Tests, Status) {
    EXPECT_STREQ(net::http::reason(net::http::status::ok), "OK");
    EXPECT_STREQ(net::http::reason(404), "Not Found");
    EXPECT_STREQ(net::http::reason(413), "Content Too Large");
    EXPECT_STREQ(net::http::reason(431), "Request Header Fields Too Large");
    EXPECT_STREQ(net::http::reason(599), "");
    for (int code : {0, -1, 99, 1000, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {   // DESIGN 408
        EXPECT_STREQ(net::http::reason(code), "") << code;
    }
    EXPECT_STREQ(net::http::reason(100), "Continue");
    EXPECT_STREQ(net::http::reason(511), "Network Authentication Required");
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
    ASSERT_FALSE(none);
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

// A Set-Cookie literal constructs (DESIGN 234): parse's value, or its
// bad_expected_access with parse's message; explicit, so no text becomes a
// cookie by itself
TEST(HttpHeaders_Tests, CookieFromALiteral) {
    net::http::cookie c("id=42; Domain=.Example.COM; Secure; Max-Age=60");
    EXPECT_EQ(c.name, "id");
    EXPECT_EQ(c.value, "42");
    EXPECT_EQ(c.domain, "example.com");
    EXPECT_TRUE(c.secure);
    EXPECT_EQ(c.max_age->nanoseconds(), 60'000'000'000);
    EXPECT_EQ(c.to_string(), net::http::cookie::parse("id=42; Domain=.Example.COM; Secure; Max-Age=60")->to_string());
    try {
        net::http::cookie bad("novalue");
        FAIL() << "a cookie without '=' is none";
    } catch (const bad_expected_access<io::error>& x) {
        EXPECT_EQ(x.error().code(), net::errc::invalid_cookie);
        EXPECT_STREQ(x.what(), "parse cookie novalue: invalid cookie");
    }
    net::http::cookie pair("session", "abc");                   // the two-argument form: name and value, not parsed
    EXPECT_EQ(pair.to_string(), "session=abc");
    static_assert(!std::is_convertible_v<const char*, net::http::cookie>, "explicit: no text becomes a cookie by itself");
    static_assert(!std::is_convertible_v<string, net::http::cookie>, "explicit");
}

// Found by the fuzzer of the fields: a Domain of several leading dots lost
// one at every reading and one more at every writing, so a cookie written
// and read back was never the same twice. What is left after the one dot a
// reader takes off must be a host name, and "..x" is none: left out
TEST(HttpHeaders_Tests, CookieDomainOfLeadingDots) {
    auto c = net::http::cookie::parse("a=b; Domain=...x");
    ASSERT_TRUE(c);
    EXPECT_EQ(c->domain, "..x");                                   // RFC 6265 §5.2.3 takes one dot off
    EXPECT_EQ(c->to_string(), "a=b");
    net::http::cookie d("a", "b");
    d.domain = "..example.com";
    EXPECT_EQ(d.to_string(), "a=b");
    d.domain = ".example.com";
    EXPECT_EQ(d.to_string(), "a=b; Domain=example.com");
}

// Found by the fuzzer of the fields (tests/net/http/fuzz/http_fields_fuzz.cpp):
// a Max-Age of more than 18 digits went on being summed past the int64
// (signed overflow, UBSan's report). Past 18 significant digits it is not
// summed and saturates, as 18 nines do (DESIGN 408 sweep: it was ignored,
// leading zeros among the count)
TEST(HttpHeaders_Tests, CookieMaxAgeOfManyDigits) {
    auto many = net::http::cookie::parse("a=b; Max-Age=" + sgcl::string(std::string(40, '9')));
    ASSERT_TRUE(many && many->max_age);
    EXPECT_EQ(*many->max_age, duration::max());                    // a number still: saturated, never summed past 18 digits
    auto most = net::http::cookie::parse("a=b; Max-Age=" + sgcl::string(std::string(18, '9')));
    ASSERT_TRUE(most && most->max_age);
    EXPECT_EQ(*most->max_age, duration::max());
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

// DESIGN 408: the list empty, moved from, one field, a name of no bytes,
// the ends of set and erase, a value of the list given back to it, dates
// at the ends of the range
TEST(HttpHeaders_Tests, BoundaryEmptyMovedAndAliased) {
    net::http::headers none;
    EXPECT_EQ(none.get(""), "");
    EXPECT_EQ(none.get("x"), "");
    EXPECT_TRUE(none.get_all("x").empty());
    EXPECT_FALSE(none.contains(""));
    EXPECT_FALSE(none.date("date"));
    EXPECT_EQ(none.size(), 0u);
    EXPECT_TRUE(none.begin() == none.end());
    none.erase("x");                                             // nothing to erase
    EXPECT_TRUE(none.empty());
    // a name of no bytes is a name as any other (refused where it is written)
    none.add("", "v");
    EXPECT_TRUE(none.contains(""));
    EXPECT_EQ(none.get(""), "v");
    none.set("", "w");
    EXPECT_EQ(none.get_all("").size(), 1u);
    none.erase("");
    EXPECT_TRUE(none.empty());
    // one field: set in its place, the iterator past it
    net::http::headers one;
    one.set("A", "1");
    auto it = one.begin();
    EXPECT_EQ((*it++).second, "1");
    EXPECT_TRUE(it == one.end());
    one.set("a", "");                                            // an empty value is a value
    EXPECT_TRUE(one.contains("A"));
    EXPECT_EQ(one.get("A"), "");
    EXPECT_EQ(one.size(), 1u);
    // set keeps the first of its name where it was and the others' order
    net::http::headers h;
    h.add("X", "1").add("A", "a").add("x", "2").add("B", "b").add("X", "3");
    h.set("x", "9");
    std::vector<std::string> order;
    for (auto [n, v] : h) {
        order.push_back(std::string(n.view()) + "=" + std::string(v.view()));
    }
    EXPECT_EQ(order, (std::vector<std::string>{"X=9", "A=a", "B=b"}));
    // a value of the list given back to it, as a value and as a name
    h.set("A", h.get("B"));
    h.add(h.get("A"), h.get("X"));
    EXPECT_EQ(h.get("A"), "b");
    EXPECT_EQ(h.get("b"), "b");                                  // B's own, the first of the name
    ASSERT_EQ(h.get_all("b").size(), 2u);
    EXPECT_EQ(h.get_all("b")[1], "9");
    for (auto [n, v] : net::http::headers(h)) {                  // a copy walked while the list changes
        h.set(n, v + "!");
    }
    EXPECT_EQ(h.get("x"), "9!");
    EXPECT_EQ(h.size(), 3u);
    // moved from: the list is empty and every member works on it
    net::http::headers to(std::move(h));
    EXPECT_EQ(to.get("x"), "9!");
    EXPECT_TRUE(h.empty());
    EXPECT_EQ(h.get("x"), "");
    EXPECT_TRUE(h.begin() == h.end());
    h.add("y", "1");
    EXPECT_EQ(h.get("Y"), "1");
    net::http::headers assigned;
    assigned = std::move(to);
    EXPECT_TRUE(to.empty());
    EXPECT_EQ(assigned.get("A"), "b!");
    auto& self = assigned;
    assigned = self;                                             // into itself: kept
    EXPECT_EQ(assigned.size(), 3u);
    // dates: a field present but empty is none; the ends of the range
    net::http::headers d;
    d.set("Date", "");
    EXPECT_FALSE(d.date("date"));
    auto first = time::datetime::from_unix(-9223372036, time::zone::utc());
    d.set_date("Date", first);
    EXPECT_EQ(d.get("date"), "Tue, 21 Sep 1677 00:12:44 GMT");
    EXPECT_EQ(d.date("date"), first);
    auto last = time::datetime::from_unix(9223372036, time::zone::utc());
    d.set_date("Date", last);
    EXPECT_EQ(d.get("date"), "Fri, 11 Apr 2262 23:47:16 GMT");
    EXPECT_EQ(d.date("date"), last);
}

// DESIGN 408: a cookie default-constructed and moved from, the limits of
// its attributes both ways
TEST(HttpHeaders_Tests, CookieBoundaries) {
    net::http::cookie none;
    EXPECT_EQ(none.name, "");
    EXPECT_FALSE(none.expires);
    EXPECT_FALSE(none.max_age);
    EXPECT_THROW((void)none.to_string(), std::invalid_argument);   // no name is no token
    net::http::cookie c("id", "v");
    c.path = "/";
    c.max_age = 5s;
    net::http::cookie moved(std::move(c));
    EXPECT_EQ(moved.to_string(), "id=v; Path=/; Max-Age=5");
    EXPECT_EQ(c.to_string(), "id=v; Path=/; Max-Age=5");          // its strings are words: the same cookie
    auto& self = moved;
    moved = self;
    EXPECT_EQ(moved.to_string(), "id=v; Path=/; Max-Age=5");
    // the value: empty, every byte dropped, a lone quote
    EXPECT_EQ(net::http::cookie("n", "").to_string(), "n=");
    EXPECT_EQ(net::http::cookie("n", "\";\\\x7f").to_string(), "n=");
    // Max-Age written: the ends of duration, a part of a second dropped
    net::http::cookie age("a", "b");
    age.max_age = duration::max();
    EXPECT_EQ(age.to_string(), "a=b; Max-Age=9223372036");
    age.max_age = duration::min();
    EXPECT_EQ(age.to_string(), "a=b; Max-Age=0");
    age.max_age = 999ms;
    EXPECT_EQ(age.to_string(), "a=b; Max-Age=0");
    age.max_age = 1999ms;
    EXPECT_EQ(age.to_string(), "a=b; Max-Age=1");
    // Domain and Path written at their ends
    net::http::cookie where("a", "b");
    where.domain = ".";
    where.path = "/\x01;x";
    EXPECT_EQ(where.to_string(), "a=b; Path=/x");
    // Expires at the ends of the range
    net::http::cookie when("a", "b");
    when.expires = time::datetime::from_unix(-9223372036, time::zone::utc());
    EXPECT_EQ(when.to_string(), "a=b; Expires=Tue, 21 Sep 1677 00:12:44 GMT");
    // read: the smallest cookies, empty attributes
    auto bare = net::http::cookie::parse("a=");
    ASSERT_TRUE(bare);
    EXPECT_EQ(bare->value, "");
    EXPECT_FALSE(net::http::cookie::parse(""));
    EXPECT_FALSE(net::http::cookie::parse("="));
    EXPECT_FALSE(net::http::cookie::parse(";a=b"));
    auto empty_attrs = net::http::cookie::parse("a=\"\"; ; ;=; Path; Domain=.; Max-Age; Expires=; SameSite");
    ASSERT_TRUE(empty_attrs);
    EXPECT_EQ(empty_attrs->value, "");
    EXPECT_EQ(empty_attrs->path, "");
    EXPECT_EQ(empty_attrs->domain, "");
    EXPECT_FALSE(empty_attrs->max_age);
    EXPECT_FALSE(empty_attrs->expires);
    EXPECT_EQ(empty_attrs->same_site, "");
    EXPECT_EQ(net::http::cookie::parse("a=\"")->value, "\"");   // a lone quote is not quoted
    // Max-Age read: a number of any length, an optional '-' and digits
    auto max_age = [](const std::string& v) -> optional<int64_t> {
        auto c = net::http::cookie::parse(sgcl::string("a=b; Max-Age=" + v));
        if (!c || !c->max_age) {
            return nullopt;
        }
        return c->max_age->nanoseconds();
    };
    EXPECT_EQ(max_age("0"), 0);
    EXPECT_EQ(max_age("-0"), 0);
    EXPECT_EQ(max_age("1"), 1'000'000'000);
    EXPECT_EQ(max_age("9223372036"), 9223372036'000'000'000);
    EXPECT_EQ(max_age("9223372037"), duration::max().nanoseconds());   // saturated: past the range of duration
    EXPECT_EQ(max_age(std::string(19, '9')), duration::max().nanoseconds());
    EXPECT_EQ(max_age("1" + std::string(30, '0')), duration::max().nanoseconds());
    EXPECT_EQ(max_age(std::string(30, '0') + "60"), 60'000'000'000);   // leading zeros
    EXPECT_EQ(max_age("-" + std::string(30, '9')), 0);                  // less than zero, however long
    EXPECT_FALSE(max_age(""));
    EXPECT_FALSE(max_age("-"));
    EXPECT_FALSE(max_age("+1"));
    EXPECT_FALSE(max_age("1 2"));
    EXPECT_FALSE(max_age(std::string(30, '9') + "x"));
    // the last of two Max-Ages wins, a bad one is passed over
    EXPECT_EQ(net::http::cookie::parse("a=b; Max-Age=5; Max-Age=7")->max_age->nanoseconds(), 7'000'000'000);
    EXPECT_EQ(net::http::cookie::parse("a=b; Max-Age=5; Max-Age=x")->max_age->nanoseconds(), 5'000'000'000);
    // Expires read at the ends: 1601 is a date, saturated to the start of the range
    auto old = net::http::cookie::parse("a=b; Expires=Mon, 01 Jan 1601 00:00:00 GMT");
    ASSERT_TRUE(old && old->expires);
    EXPECT_EQ(old->expires->year(), 1677);
}
