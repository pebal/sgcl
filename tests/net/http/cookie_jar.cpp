//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::cookie_jar by RFC 6265 §5.1.3–5.1.4 (domains and paths), §5.3 (the
// storage model), §5.4 (the Cookie field) and RFC 6265bis §5.7 (the rules
// browsers follow: Secure only from a secure origin and never shadowed from
// an insecure one, the __Secure- and __Host- prefixes, SameSite=None with
// Secure, 400 days, 4096 bytes, no controls), the public suffix list, the
// limits and their eviction, the times on the manual clock, the removals,
// persistence, the handle's copies and moves, and threads sharing one jar.
// The IETF httpwg cookie test corpus is not on this machine (and nothing is
// downloaded): the cases are written from the RFCs.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    void put(const net::http::cookie_jar& jar, const char* url, std::initializer_list<const char*> fields) {
        vector<net::http::cookie> list;
        for (auto f : fields) {
            auto c = net::http::cookie::parse(f);
            EXPECT_TRUE(c) << f;
            if (c) {
                list.push_back(*c);
            }
        }
        jar.set_cookies(net::url(url), list);
    }

    std::string sent(const net::http::cookie_jar& jar, const char* url) {
        return text(jar.header(net::url(url)));
    }

    std::string temp_path(const char* name) {
        return (std::filesystem::temp_directory_path() / ("sgcl_cookie_jar_" + std::to_string(::getpid()) + "_" + name)).string();
    }
}

TEST(HttpCookieJar_Tests, AHostOnlyCookieGoesToItsHostAlone) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=1"});
    EXPECT_EQ(sent(jar, "http://example.com/"), "a=1");
    EXPECT_EQ(sent(jar, "http://example.com/any/path?q=1"), "a=1");
    EXPECT_EQ(sent(jar, "http://www.example.com/"), "");
    EXPECT_EQ(sent(jar, "http://example.org/"), "");
    EXPECT_EQ(sent(jar, "http://EXAMPLE.com:8080/"), "a=1");   // the port is not the cookie's (RFC 6265 §8.5)
    EXPECT_EQ(sent(jar, "https://example.com/"), "a=1");
    EXPECT_EQ(jar.size(), 1u);
}

TEST(HttpCookieJar_Tests, ADomainCookieGoesToTheDomainAndUnderIt) {
    net::http::cookie_jar jar;
    put(jar, "http://www.example.com/", {"a=1; Domain=example.com"});
    EXPECT_EQ(sent(jar, "http://example.com/"), "a=1");
    EXPECT_EQ(sent(jar, "http://www.example.com/"), "a=1");
    EXPECT_EQ(sent(jar, "http://a.b.example.com/"), "a=1");
    EXPECT_EQ(sent(jar, "http://notexample.com/"), "");
    EXPECT_EQ(sent(jar, "http://example.com.evil.org/"), "");
    // a dot in front, any case, are the same domain
    net::http::cookie_jar other;
    put(other, "http://www.example.com/", {"b=2; Domain=.Example.COM"});
    EXPECT_EQ(sent(other, "http://foo.example.com/"), "b=2");
    // the host's own name as Domain is a domain cookie too
    put(other, "http://example.com/", {"c=3; Domain=example.com"});
    EXPECT_EQ(sent(other, "http://x.example.com/"), "b=2; c=3");
}

TEST(HttpCookieJar_Tests, ADomainTheHostIsNotUnderIsRefused) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=1; Domain=other.com", "b=2; Domain=www.example.com", "c=3; Domain=ample.com",
                                    "d=4; Domain=example.com."});
    EXPECT_EQ(jar.size(), 0u);
    EXPECT_EQ(sent(jar, "http://www.example.com/"), "");
}

TEST(HttpCookieJar_Tests, APublicSuffixIsNoCookiesDomain) {
    net::http::cookie_jar jar;
    put(jar, "http://www.example.co.uk/", {"a=1; Domain=co.uk", "b=2; Domain=uk"});
    put(jar, "http://foo.github.io/", {"c=3; Domain=github.io"});
    put(jar, "http://www.example.com/", {"d=4; Domain=com"});
    put(jar, "http://b.c.mm/", {"e=5; Domain=c.mm"});
    EXPECT_EQ(jar.size(), 0u);
    // the suffix's own host: a host-only cookie (RFC 6265 §5.3 step 5)
    put(jar, "http://co.uk/", {"f=6; Domain=co.uk"});
    EXPECT_EQ(sent(jar, "http://co.uk/"), "f=6");
    EXPECT_EQ(sent(jar, "http://www.example.co.uk/"), "");
    // an exception of a wildcard is a name to register: www.ck is no suffix
    put(jar, "http://a.www.ck/", {"g=7; Domain=www.ck"});
    EXPECT_EQ(sent(jar, "http://b.www.ck/"), "g=7");
    // the private section counts as in browsers; a domain under a suffix is fine
    put(jar, "http://a.foo.github.io/", {"h=8; Domain=foo.github.io"});
    EXPECT_EQ(sent(jar, "http://foo.github.io/"), "h=8");
    EXPECT_EQ(sent(jar, "http://bar.github.io/"), "");
}

TEST(HttpCookieJar_Tests, TheDefaultPathIsTheDirectoryOfTheRequest) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/a/b/c", {"a=1"});
    put(jar, "http://example.com/x", {"b=2"});
    put(jar, "http://example.com/a/b/c", {"c=3; Path=relative"});   // not a path: the default
    EXPECT_EQ(sent(jar, "http://example.com/a/b"), "a=1; c=3; b=2");
    EXPECT_EQ(sent(jar, "http://example.com/a/b/"), "a=1; c=3; b=2");
    EXPECT_EQ(sent(jar, "http://example.com/a/b/d/e"), "a=1; c=3; b=2");
    EXPECT_EQ(sent(jar, "http://example.com/a/bc"), "b=2");
    EXPECT_EQ(sent(jar, "http://example.com/a"), "b=2");
    vector<net::http::cookie> all = jar.all();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(text(all[0].path), "/a/b");
    EXPECT_EQ(text(all[1].path), "/");
}

TEST(HttpCookieJar_Tests, PathsMatchAtTheirSlashes) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=1; Path=/foo/", "b=2; Path=/foo", "c=3; Path=/"});
    EXPECT_EQ(sent(jar, "http://example.com/foo/bar"), "a=1; b=2; c=3");
    EXPECT_EQ(sent(jar, "http://example.com/foo/"), "a=1; b=2; c=3");
    EXPECT_EQ(sent(jar, "http://example.com/foo"), "b=2; c=3");
    EXPECT_EQ(sent(jar, "http://example.com/foobar"), "c=3");
    EXPECT_EQ(sent(jar, "http://example.com/fo"), "c=3");
    EXPECT_EQ(sent(jar, "http://example.com/FOO/"), "c=3");   // a path is compared byte for byte
}

// §5.4 step 2: the longer path first, then the older creation; a cookie
// replaced keeps its creation (§5.3 step 11)
TEST(HttpCookieJar_Tests, TheOrderIsPathThenCreation) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=1"});
    put(jar, "http://example.com/", {"b=2"});
    put(jar, "http://example.com/", {"c=3; Path=/x"});
    EXPECT_EQ(sent(jar, "http://example.com/x/y"), "c=3; a=1; b=2");
    put(jar, "http://example.com/", {"a=9"});
    EXPECT_EQ(sent(jar, "http://example.com/x/y"), "c=3; a=9; b=2");
    // the same name at another path, or in another domain, is another cookie
    put(jar, "http://example.com/", {"a=10; Path=/x"});
    EXPECT_EQ(sent(jar, "http://example.com/x/y"), "c=3; a=10; a=9; b=2");
    EXPECT_EQ(jar.size(), 4u);
}

// RFC 6265bis: the host-only flag is part of a cookie's identity
TEST(HttpCookieJar_Tests, AHostOnlyAndADomainCookieOfOneNameAreTwo) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=host"});
    put(jar, "http://example.com/", {"a=domain; Domain=example.com"});
    EXPECT_EQ(jar.size(), 2u);
    EXPECT_EQ(sent(jar, "http://example.com/"), "a=host; a=domain");
    EXPECT_EQ(sent(jar, "http://www.example.com/"), "a=domain");
}

TEST(HttpCookieJar_Tests, MaxAgeAndExpiresDelete) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=1", "b=2", "c=3", "d=4", "e=5"});
    put(jar, "http://example.com/", {"a=x; Max-Age=0", "b=x; Max-Age=-5", "c=x; Expires=Thu, 01 Jan 1970 00:00:00 GMT"});
    EXPECT_EQ(sent(jar, "http://example.com/"), "d=4; e=5");
    // Max-Age wins over Expires, either way
    put(jar, "http://example.com/", {"d=x; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Max-Age=100",
                                    "e=x; Expires=Fri, 01 Jan 2100 00:00:00 GMT; Max-Age=0"});
    EXPECT_EQ(sent(jar, "http://example.com/"), "d=x");
    // a deletion of a cookie there is not stores nothing
    put(jar, "http://example.com/", {"z=1; Max-Age=0"});
    EXPECT_EQ(jar.size(), 1u);
    // a deletion matches the identity: another path deletes nothing
    put(jar, "http://example.com/", {"d=x; Path=/other; Max-Age=0"});
    EXPECT_EQ(sent(jar, "http://example.com/"), "d=x");
}

TEST(HttpCookieJar_Tests, CookiesExpireOnTheClock) {
    async::manual_clock clock;
    clock.install();
    net::http::cookie_jar jar;
    int64_t start = time::now().unix();
    put(jar, "http://example.com/", {"a=1; Max-Age=10", "b=2", "c=3; Max-Age=20"});
    auto in = time::datetime::from_unix(start + 15, time::zone::utc()).format(time::http);
    put(jar, "http://example.com/", {sgcl::string::concat("d=4; Expires=", in).c_str()});
    EXPECT_EQ(sent(jar, "http://example.com/"), "a=1; b=2; c=3; d=4");
    clock.advance(11s);
    EXPECT_EQ(sent(jar, "http://example.com/"), "b=2; c=3; d=4");
    clock.advance(5s);
    EXPECT_EQ(sent(jar, "http://example.com/"), "b=2; c=3");
    EXPECT_EQ(jar.all().size(), 2u);
    clock.advance(5s);
    EXPECT_EQ(sent(jar, "http://example.com/"), "b=2");
}

TEST(HttpCookieJar_Tests, AgesAreHeldTo400Days) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=1; Max-Age=999999999999999999999", "b=2; Expires=Fri, 01 Jan 2100 00:00:00 GMT",
                                    "c=3; Max-Age=3600"});
    int64_t now = time::now().unix();
    for (auto& c : jar.all()) {
        ASSERT_TRUE(c.expires);
        int64_t left = c.expires->unix() - now;
        if (c.name == "c") {
            EXPECT_NEAR(double(left), 3600.0, 5.0);
        } else {
            EXPECT_NEAR(double(left), 400.0 * 86400, 5.0) << text(c.name);
        }
    }
    EXPECT_FALSE(jar.all().empty());
}

TEST(HttpCookieJar_Tests, SecureCookiesComeAndGoOverSecureOrigins) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=1; Secure"});
    EXPECT_EQ(jar.size(), 0u);   // Secure from an insecure origin (RFC 6265bis §5.7)
    put(jar, "https://example.com/", {"a=1; Secure", "b=2"});
    EXPECT_EQ(sent(jar, "https://example.com/"), "a=1; b=2");
    EXPECT_EQ(sent(jar, "wss://example.com/"), "a=1; b=2");
    EXPECT_EQ(sent(jar, "http://example.com/"), "b=2");
    EXPECT_EQ(sent(jar, "ws://example.com/"), "b=2");
    // the loopback is a secure origin, as browsers take it
    put(jar, "http://localhost:8080/", {"l=1; Secure"});
    EXPECT_EQ(sent(jar, "http://localhost/"), "l=1");
    put(jar, "http://app.localhost/", {"m=1; Secure"});
    EXPECT_EQ(sent(jar, "http://app.localhost/"), "m=1");
    put(jar, "http://127.0.0.1/", {"n=1; Secure"});
    EXPECT_EQ(sent(jar, "http://127.0.0.1/"), "n=1");
    put(jar, "http://127.1.2.3/", {"o=1; Secure"});
    EXPECT_EQ(sent(jar, "http://127.1.2.3/"), "o=1");
    put(jar, "http://[::1]/", {"p=1; Secure"});
    EXPECT_EQ(sent(jar, "http://[::1]/"), "p=1");
    put(jar, "http://10.0.0.1/", {"q=1; Secure"});
    EXPECT_EQ(sent(jar, "http://10.0.0.1/"), "");
}

// RFC 6265bis §5.7: an insecure origin leaves Secure cookies alone
TEST(HttpCookieJar_Tests, AnInsecureOriginCannotShadowASecureCookie) {
    net::http::cookie_jar jar;
    put(jar, "https://www.example.com/", {"s=1; Secure; Domain=example.com", "t=1; Secure; Path=/deep"});
    put(jar, "http://www.example.com/", {"s=2"});                       // same name, its domain under the cookie's
    put(jar, "http://example.com/", {"s=3; Domain=example.com"});       // the same domain
    put(jar, "http://a.www.example.com/", {"s=4; Path=/x"});            // a path under the cookie's
    put(jar, "http://www.example.com/", {"t=2; Path=/deep/er"});
    EXPECT_EQ(sent(jar, "https://www.example.com/deep/er"), "t=1; s=1");
    EXPECT_EQ(sent(jar, "http://www.example.com/deep/er"), "");
    // a path above the Secure cookie's is not under it: stored
    put(jar, "http://www.example.com/", {"t=3; Path=/"});
    EXPECT_EQ(sent(jar, "http://www.example.com/"), "t=3");
    // another name, or a domain beside: stored
    put(jar, "http://other.example.org/", {"s=5"});
    EXPECT_EQ(sent(jar, "http://other.example.org/"), "s=5");
    // a secure origin replaces it
    put(jar, "https://www.example.com/", {"s=6; Domain=example.com"});
    EXPECT_EQ(sent(jar, "http://www.example.com/"), "s=6; t=3");
}

TEST(HttpCookieJar_Tests, ThePrefixesAreEnforced) {
    net::http::cookie_jar jar;
    put(jar, "https://www.example.com/", {"__Secure-a=1", "__Secure-b=1; Secure", "__secure-c=1", "__SECURE-d=1; Secure"});
    EXPECT_EQ(sent(jar, "https://www.example.com/"), "__Secure-b=1; __SECURE-d=1");
    put(jar, "https://www.example.com/x/", {"__Host-a=1; Secure; Path=/", "__Host-b=1; Secure", "__Host-c=1; Path=/",
                                           "__Host-d=1; Secure; Path=/; Domain=example.com", "__Host-e=1; Secure; Path=/x",
                                           "__host-f=1; Secure", "__HOST-g=1; Secure; Path=/"});
    EXPECT_EQ(sent(jar, "https://www.example.com/x/"), "__Secure-b=1; __SECURE-d=1; __Host-a=1; __HOST-g=1");
    // not from an insecure origin, which cannot set Secure at all
    net::http::cookie_jar plain;
    put(plain, "http://example.com/", {"__Secure-a=1; Secure", "__Host-b=1; Secure; Path=/"});
    EXPECT_EQ(plain.size(), 0u);
    // a name that only contains the prefix is a name
    put(plain, "http://example.com/", {"x__Secure-a=1"});
    EXPECT_EQ(plain.size(), 1u);
}

TEST(HttpCookieJar_Tests, SameSiteIsKeptAndNoneNeedsSecure) {
    net::http::cookie_jar jar;
    put(jar, "https://example.com/", {"a=1; SameSite=Strict", "b=1; SameSite=lax", "c=1; SameSite=None",
                                     "d=1; SameSite=None; Secure", "e=1; SameSite=bogus"});
    EXPECT_EQ(sent(jar, "https://example.com/"), "a=1; b=1; d=1; e=1");   // sent whatever SameSite says
    vector<net::http::cookie> all = jar.all();
    ASSERT_EQ(all.size(), 4u);
    EXPECT_EQ(text(all[0].same_site), "Strict");
    EXPECT_EQ(text(all[1].same_site), "Lax");
    EXPECT_EQ(text(all[2].same_site), "None");
    EXPECT_EQ(text(all[3].same_site), "");
}

TEST(HttpCookieJar_Tests, SizesAndControls) {
    net::http::cookie_jar jar;
    net::url u("http://example.com/");
    net::http::cookie at_limit(sgcl::string(std::string(96, 'n')), sgcl::string(std::string(4000, 'v')));
    net::http::cookie past(sgcl::string(std::string(97, 'm')), sgcl::string(std::string(4000, 'v')));
    jar.set_cookies(u, {at_limit, past});
    ASSERT_EQ(jar.size(), 1u);
    EXPECT_EQ(jar.all()[0].name, at_limit.name);
    jar.clear();
    net::http::cookie control("a", "x\x01y");
    net::http::cookie del("b", "x\x7fy");
    net::http::cookie tab("c", "x\ty");
    net::http::cookie empty("", "v");
    net::http::cookie name_control("d\x02", "v");
    jar.set_cookies(u, {control, del, tab, empty, name_control});
    EXPECT_EQ(sent(jar, "http://example.com/"), "c=x\ty");
    // an attribute past 1024 bytes is ignored: the default path, the host alone
    jar.clear();
    std::string path = "/" + std::string(1024, 'p');
    std::string domain = std::string(1020, 'd') + ".example.com";
    net::http::cookie long_attrs("e", "1");
    long_attrs.path = sgcl::string(path);
    long_attrs.domain = sgcl::string(domain);
    jar.set_cookies(net::url("http://example.com/a/b"), {long_attrs});
    ASSERT_EQ(jar.size(), 1u);
    EXPECT_EQ(text(jar.all()[0].path), "/a");
    EXPECT_EQ(text(jar.all()[0].domain), "example.com");
}

TEST(HttpCookieJar_Tests, HttpOnlyPartitionedAndExpiresAreShown) {
    net::http::cookie_jar jar;
    put(jar, "https://www.example.com/a/", {"a=1; HttpOnly; Partitioned; Secure; Max-Age=60; Domain=example.com; Path=/"});
    vector<net::http::cookie> got = jar.cookies(net::url("https://example.com/"));
    ASSERT_EQ(got.size(), 1u);
    EXPECT_EQ(text(got[0].name), "a");
    EXPECT_EQ(text(got[0].value), "1");
    EXPECT_EQ(text(got[0].domain), ".example.com");
    EXPECT_EQ(text(got[0].path), "/");
    EXPECT_TRUE(got[0].http_only);
    EXPECT_TRUE(got[0].partitioned);
    EXPECT_TRUE(got[0].secure);
    ASSERT_TRUE(got[0].expires);
    EXPECT_FALSE(got[0].max_age);
    EXPECT_NEAR(double(got[0].expires->unix() - time::now().unix()), 60.0, 3.0);
    // a session cookie has no Expires, a host-only one its host as the domain
    put(jar, "http://www.example.com/", {"b=2"});
    got = jar.cookies(net::url("http://www.example.com/"));
    ASSERT_EQ(got.size(), 1u);
    EXPECT_FALSE(got[0].expires);
    EXPECT_EQ(text(got[0].domain), "www.example.com");
    EXPECT_TRUE(jar.cookies(net::url("http://nowhere.example.org/")).empty());
}

TEST(HttpCookieJar_Tests, AddressesAreHostsOfTheirOwn) {
    net::http::cookie_jar jar;
    put(jar, "http://192.168.1.1/", {"a=1", "b=2; Domain=192.168.1.1", "c=3; Domain=168.1.1", "d=4; Domain=1"});
    EXPECT_EQ(sent(jar, "http://192.168.1.1/"), "a=1; b=2");
    EXPECT_EQ(sent(jar, "http://10.168.1.1/"), "");
    EXPECT_EQ(sent(jar, "http://1.1.1.1/"), "");
    for (auto& c : jar.all()) {
        EXPECT_EQ(text(c.domain), "192.168.1.1");   // both host-only
    }
    put(jar, "http://[2001:db8::1]/", {"e=5", "f=6; Domain=2001:db8::1"});
    EXPECT_EQ(sent(jar, "http://[2001:db8::1]/"), "e=5; f=6");   // the address itself as Domain: its own cookie
    EXPECT_EQ(sent(jar, "http://[2001:db8::2]/"), "");
}

TEST(HttpCookieJar_Tests, NamesInUnicodeAreComparedAsALabels) {
    net::http::cookie_jar jar;
    put(jar, "http://www.bücher.de/", {"a=1; Domain=bücher.de", "b=2; Domain=BÜCHER.DE"});
    EXPECT_EQ(sent(jar, "http://xn--bcher-kva.de/"), "a=1; b=2");
    EXPECT_EQ(sent(jar, "http://shop.bücher.de/"), "a=1; b=2");
    EXPECT_EQ(text(jar.all()[0].domain), ".xn--bcher-kva.de");
    // a Domain IDNA refuses refuses the cookie
    net::http::cookie bad("c", "3");
    bad.domain = "a\xff.de";
    jar.set_cookies(net::url("http://www.bücher.de/"), {bad});
    EXPECT_EQ(jar.size(), 2u);
}

TEST(HttpCookieJar_Tests, ADotAtTheEndOfTheHostIsTheSameHost) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com./", {"a=1"});
    EXPECT_EQ(sent(jar, "http://example.com/"), "a=1");
    EXPECT_EQ(sent(jar, "http://example.com./"), "a=1");
}

TEST(HttpCookieJar_Tests, OtherSchemesAndNoHostSetAndGetNothing) {
    net::http::cookie_jar jar;
    put(jar, "ftp://example.com/", {"a=1"});
    put(jar, "file:///tmp/x", {"b=1"});
    put(jar, "mailto:x@example.com", {"c=1"});
    EXPECT_EQ(jar.size(), 0u);
    put(jar, "ws://example.com/", {"d=1"});
    put(jar, "wss://example.com/", {"e=1"});
    EXPECT_EQ(sent(jar, "http://example.com/"), "d=1; e=1");
    EXPECT_EQ(sent(jar, "ftp://example.com/"), "");
    EXPECT_TRUE(jar.cookies(net::url("ftp://example.com/")).empty());
    jar.set_cookies(net::url("http://example.com/"), vector<net::http::cookie>());
    EXPECT_EQ(jar.size(), 2u);
}

TEST(HttpCookieJar_Tests, AValueWithASpaceOrACommaIsQuoted) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=\"x y\"", "b=1,2", "c=\"q\""});
    EXPECT_EQ(sent(jar, "http://example.com/"), "a=\"x y\"; b=\"1,2\"; c=q");
}

TEST(HttpCookieJar_Tests, TheLimitOfADomainEvictsTheLeastRecentlyUsed) {
    net::http::cookie_jar::options o;
    o.max_cookies_per_domain = 3;
    net::http::cookie_jar jar(o);
    async::manual_clock clock;
    clock.install();
    put(jar, "http://a.example.com/", {"a=1"});
    clock.advance(1s);
    put(jar, "http://b.example.com/", {"b=1"});
    clock.advance(1s);
    put(jar, "http://example.com/", {"c=1"});
    clock.advance(1s);
    EXPECT_EQ(sent(jar, "http://a.example.com/"), "a=1");   // a used now: b is the least recent
    clock.advance(1s);
    put(jar, "http://example.com/", {"d=1"});
    EXPECT_EQ(jar.size(), 3u);
    EXPECT_EQ(sent(jar, "http://b.example.com/"), "");
    EXPECT_EQ(sent(jar, "http://a.example.com/"), "a=1");
    // the domain is the registrable one: another site has room of its own
    put(jar, "http://example.org/", {"x=1", "y=1", "z=1"});
    EXPECT_EQ(jar.size(), 6u);
    // cookies without Secure go before Secure ones, however recent
    net::http::cookie_jar mixed(o);
    put(mixed, "https://example.com/", {"s1=1; Secure"});
    clock.advance(1s);
    put(mixed, "https://example.com/", {"s2=1; Secure"});
    clock.advance(1s);
    put(mixed, "https://example.com/", {"p1=1"});
    clock.advance(1s);
    put(mixed, "https://example.com/", {"p2=1"});
    EXPECT_EQ(sent(mixed, "https://example.com/"), "s1=1; s2=1; p2=1");
    // expired ones go first
    clock.advance(1s);
    net::http::cookie_jar expiring(o);
    put(expiring, "http://example.com/", {"old=1", "short=1; Max-Age=1", "mid=1"});
    clock.advance(2s);
    put(expiring, "http://example.com/", {"new=1"});
    EXPECT_EQ(sent(expiring, "http://example.com/"), "old=1; mid=1; new=1");
}

TEST(HttpCookieJar_Tests, TheLimitOfTheJarEvictsTheLeastRecentlyUsedOfAll) {
    net::http::cookie_jar::options o;
    o.max_cookies = 4;
    net::http::cookie_jar jar(o);
    async::manual_clock clock;
    clock.install();
    for (const char* host : {"http://a.com/", "http://b.com/", "http://c.com/", "http://d.com/"}) {
        put(jar, host, {"k=1"});
        clock.advance(1s);
    }
    EXPECT_EQ(sent(jar, "http://a.com/"), "k=1");
    clock.advance(1s);
    put(jar, "http://e.com/", {"k=1"});
    EXPECT_EQ(jar.size(), 4u);
    EXPECT_EQ(sent(jar, "http://b.com/"), "");
    EXPECT_EQ(sent(jar, "http://a.com/"), "k=1");
    // limits of zero keep nothing
    net::http::cookie_jar::options none;
    none.max_cookies = 0;
    net::http::cookie_jar empty(none);
    put(empty, "http://a.com/", {"k=1"});
    EXPECT_EQ(empty.size(), 0u);
    none.max_cookies = 10;
    none.max_cookies_per_domain = 0;
    net::http::cookie_jar empty2(none);
    put(empty2, "http://a.com/", {"k=1"});
    EXPECT_EQ(empty2.size(), 0u);
}

TEST(HttpCookieJar_Tests, AllAndSize) {
    net::http::cookie_jar jar;
    EXPECT_TRUE(jar.empty());
    EXPECT_TRUE(jar.all().empty());
    put(jar, "http://www.b.com/", {"x=1", "y=2; Domain=b.com"});
    put(jar, "http://a.com/", {"z=3"});
    EXPECT_EQ(jar.size(), 3u);
    EXPECT_FALSE(jar.empty());
    vector<net::http::cookie> all = jar.all();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(text(all[0].domain), "a.com");
    EXPECT_EQ(text(all[1].domain), ".b.com");
    EXPECT_EQ(text(all[2].domain), "www.b.com");
}

TEST(HttpCookieJar_Tests, RemoveOneByDomainAndAll) {
    net::http::cookie_jar jar;
    put(jar, "http://www.example.com/", {"a=1", "a=2; Domain=example.com", "a=3; Path=/x", "b=4"});
    put(jar, "http://example.org/", {"c=5"});
    EXPECT_FALSE(jar.remove("www.example.com", "/", "nope"));
    EXPECT_FALSE(jar.remove("www.example.com", "/y", "a"));
    EXPECT_FALSE(jar.remove("", "/", "a"));
    EXPECT_TRUE(jar.remove("www.example.com", "/", "a"));
    EXPECT_EQ(sent(jar, "http://www.example.com/x"), "a=3; a=2; b=4");
    EXPECT_TRUE(jar.remove(".EXAMPLE.com", "/", "a"));   // the domain cookie, a dot in front and any case
    EXPECT_EQ(sent(jar, "http://www.example.com/x"), "a=3; b=4");
    EXPECT_EQ(jar.remove("example.com"), 2u);           // what is under the domain too
    EXPECT_EQ(jar.remove("example.com"), 0u);
    EXPECT_EQ(jar.remove(""), 0u);
    EXPECT_EQ(jar.size(), 1u);
    EXPECT_EQ(jar.remove("org"), 1u);
    put(jar, "http://example.org/", {"c=5", "d=6"});
    jar.clear();
    EXPECT_TRUE(jar.empty());
    EXPECT_EQ(sent(jar, "http://example.org/"), "");
    jar.clear();
    EXPECT_TRUE(jar.empty());
}

TEST(HttpCookieJar_Tests, ClearExpiredAndClearSession) {
    async::manual_clock clock;
    clock.install();
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"s=1", "t=2", "p=3; Max-Age=100", "q=4; Max-Age=5"});
    EXPECT_EQ(jar.clear_expired(), 0u);
    clock.advance(6s);
    EXPECT_EQ(jar.size(), 4u);   // found when looked at
    EXPECT_EQ(jar.clear_expired(), 1u);
    EXPECT_EQ(jar.size(), 3u);
    EXPECT_EQ(jar.clear_session(), 2u);
    EXPECT_EQ(sent(jar, "http://example.com/"), "p=3");
    EXPECT_EQ(jar.clear_session(), 0u);
}

TEST(HttpCookieJar_Tests, ACopyAndAMoveAreTheSameJar) {
    net::http::cookie_jar jar;
    net::http::cookie_jar copy = jar;
    put(copy, "http://example.com/", {"a=1"});
    EXPECT_EQ(sent(jar, "http://example.com/"), "a=1");
    net::http::cookie_jar moved = std::move(copy);
    put(moved, "http://example.com/", {"b=2"});
    EXPECT_EQ(sent(jar, "http://example.com/"), "a=1; b=2");
    EXPECT_EQ(sent(copy, "http://example.com/"), "a=1; b=2");   // a moved-from handle is the same jar
    net::http::cookie_jar other;
    other = jar;
    EXPECT_EQ(other.size(), 2u);
}

TEST(HttpCookieJar_Tests, TheJsonRoundTrip) {
    net::http::cookie_jar jar;
    put(jar, "https://www.example.com/", {"p=1; Max-Age=3600; Secure; HttpOnly; SameSite=Lax; Domain=example.com",
                                         "s=2", "q=\"a b\"; Max-Age=60; Path=/x; Partitioned; Secure"});
    put(jar, "http://192.168.0.1/", {"ip=1; Max-Age=60"});
    sgcl::string persistent = jar.to_json();
    sgcl::string everything = jar.to_json(true);
    net::http::cookie_jar back;
    ASSERT_TRUE(back.load_json(persistent));
    EXPECT_EQ(back.size(), 3u);
    EXPECT_EQ(sent(back, "https://www.example.com/x/y"), "q=\"a b\"; p=1");
    EXPECT_EQ(sent(back, "http://192.168.0.1/"), "ip=1");
    vector<net::http::cookie> a = jar.all(), b = back.all();
    ASSERT_EQ(b.size(), 3u);
    for (size_t i = 0, j = 0; i < a.size(); ++i) {
        if (!a[i].expires) {
            continue;
        }
        EXPECT_EQ(a[i].to_string(), b[j].to_string());
        EXPECT_EQ(a[i].expires->unix_nano(), b[j].expires->unix_nano());
        ++j;
    }
    net::http::cookie_jar all_back;
    ASSERT_TRUE(all_back.load_json(everything));
    EXPECT_EQ(all_back.size(), 4u);
    EXPECT_EQ(sent(all_back, "https://www.example.com/"), "p=1; s=2");
    // the order of creation is the file's: a cookie loaded is older than one set after
    put(all_back, "https://www.example.com/", {"n=0"});
    EXPECT_EQ(sent(all_back, "https://www.example.com/"), "p=1; s=2; n=0");
    // written again, the same text
    EXPECT_EQ(text(back.to_json()), text(net::http::cookie_jar(back).to_json()));
    // loaded beside what is there, replacing by identity
    net::http::cookie_jar mixed;
    put(mixed, "https://www.example.com/", {"s=old", "other=1"});
    ASSERT_TRUE(mixed.load_json(everything));
    EXPECT_EQ(sent(mixed, "https://www.example.com/"), "p=1; s=2; other=1");
}

// a cookie's value and path are bytes: what is not UTF-8 goes in hexadecimal
TEST(HttpCookieJar_Tests, BytesThatAreNotUtf8SurviveTheFile) {
    net::http::cookie_jar jar;
    net::http::cookie c("b", sgcl::string(std::string_view("\xd1\xff ok")));
    c.path = sgcl::string(std::string_view("/p\xe9"));
    c.max_age = std::chrono::hours(1);
    jar.set_cookies(net::url("http://example.com/"), {c});
    put(jar, "http://example.com/", {"u=zażółć; Max-Age=60"});
    sgcl::string saved = jar.to_json();
    EXPECT_NE(text(saved).find("\"value_hex\": \"d1ff206f6b\""), std::string::npos) << text(saved);
    EXPECT_NE(text(saved).find("\"path_hex\": \"2f70e9\""), std::string::npos);
    EXPECT_NE(text(saved).find("zażółć"), std::string::npos);
    net::http::cookie_jar back;
    ASSERT_TRUE(back.load_json(saved));
    EXPECT_EQ(text(back.to_json()), text(saved));
    EXPECT_EQ(text(back.header(net::url(sgcl::string(std::string_view("http://example.com/p\xe9"))))), text(jar.header(net::url(sgcl::string(std::string_view("http://example.com/p\xe9"))))));
    // a value given both ways, or hexadecimal that is not, is refused
    auto both = sgcl::string(R"({"version":1,"cookies":[{"name":"a","value":"x","value_hex":"78","domain":"example.com","path":"/"}]})");
    auto odd = sgcl::string(R"({"version":1,"cookies":[{"name":"a","value_hex":"7","domain":"example.com","path":"/"}]})");
    EXPECT_FALSE(back.load_json(both));
    EXPECT_FALSE(back.load_json(odd));
    // a name that is not a token is no cookie (as cookie::parse reads one)
    jar.set_cookies(net::url("http://example.com/"), {net::http::cookie("a b", "1")});
    EXPECT_EQ(jar.size(), 2u);
}

TEST(HttpCookieJar_Tests, LoadSkipsTheExpiredAndHoldsTheLimits) {
    async::manual_clock clock;
    clock.install();
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=1; Max-Age=10", "b=2; Max-Age=100", "c=3; Max-Age=100"});
    sgcl::string saved = jar.to_json();
    clock.advance(20s);
    net::http::cookie_jar::options o;
    o.max_cookies_per_domain = 1;
    net::http::cookie_jar small(o);
    ASSERT_TRUE(small.load_json(saved));
    EXPECT_EQ(sent(small, "http://example.com/"), "c=3");
    EXPECT_EQ(net::http::cookie_jar().to_json(), "{\n  \"version\": 1,\n  \"cookies\": []\n}");
}

TEST(HttpCookieJar_Tests, ABadFileIsRefusedWhole) {
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"keep=1"});
    auto good = [](const char* cookie) {
        return sgcl::string::concat("{\"version\":1,\"cookies\":[{\"name\":\"ok\",\"value\":\"1\",\"domain\":\"example.com\",\"host_only\":true,"
                                    "\"path\":\"/\",\"secure\":false,\"http_only\":false,\"created\":\"2026-01-01T00:00:00Z\","
                                    "\"last_access\":\"2026-01-01T00:00:00Z\"},",
                                    cookie, "]}");
    };
    const char* bad_cookies[] = {
        R"({"name":"","value":"1","domain":"example.com","path":"/"})",
        R"({"name":"a b","value":"1","domain":"example.com","path":"/"})",
        R"({"name":"a","value":"1\u0001","domain":"example.com","path":"/"})",
        R"({"name":"a","value":"1","domain":"","path":"/"})",
        R"({"name":"a","value":"1","domain":".example.com","path":"/"})",
        R"({"name":"a","value":"1","domain":"Example.com","path":"/"})",
        R"({"name":"a","value":"1","domain":"co.uk","host_only":false,"path":"/"})",
        R"({"name":"a","value":"1","domain":"example.com","path":"x"})",
        R"({"name":"a","value":"1","domain":"example.com","path":"/","expires":"tomorrow"})",
        R"({"name":"a","value":"1","domain":"example.com","path":"/","created":"now"})",
        R"({"name":"a","value":"1","domain":"example.com","path":"/","same_site":"Sometimes"})",
        R"({"name":"a","value":"1","domain":"example.com","path":"/","same_site":"None"})",
        R"({"name":"__Host-a","value":"1","domain":"example.com","path":"/","secure":true,"host_only":false})",
        R"({"name":"__Secure-a","value":"1","domain":"example.com","path":"/"})",
        R"({"value":"1","domain":"example.com","path":"/"})",
        R"({"name":"a","value":1,"domain":"example.com","path":"/"})",
    };
    for (const char* c : bad_cookies) {
        auto r = jar.load_json(good(c));
        ASSERT_FALSE(r) << c;
        EXPECT_EQ(r.error().code(), net::errc::invalid_cookie) << c;
    }
    for (const char* t : {"", "{}", "[]", "null", "{\"version\":2,\"cookies\":[]}", "{\"version\":1}", "{\"version\":1,\"cookies\":{}}",
                          "{\"version\":1,\"cookies\":[]", "\xff"}) {
        EXPECT_FALSE(jar.load_json(t)) << t;
    }
    EXPECT_EQ(jar.size(), 1u);
    EXPECT_EQ(sent(jar, "http://example.com/"), "keep=1");
    // and the good one alone loads
    ASSERT_TRUE(jar.load_json(good(R"({"name":"also","value":"2","domain":"example.com","path":"/"})")));
    EXPECT_EQ(sent(jar, "http://example.com/"), "ok=1; keep=1; also=2");   // by creation: the file's time, then now
}

TEST(HttpCookieJar_Tests, SaveAndLoadAFile) {
    std::string path = temp_path("save.json");
    net::http::cookie_jar jar;
    put(jar, "https://example.com/", {"login=secret; Max-Age=3600; Secure", "session=1"});
    ASSERT_TRUE(jar.save(sgcl::string(path)));
    struct stat st;
    ASSERT_EQ(::stat(path.c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0600);
    EXPECT_FALSE(std::filesystem::exists(path + ".tmp"));
    net::http::cookie_jar back;
    ASSERT_TRUE(back.load(sgcl::string(path)));
    EXPECT_EQ(sent(back, "https://example.com/"), "login=secret");
    ASSERT_TRUE(jar.save(sgcl::string(path), true));   // over the file there
    net::http::cookie_jar all;
    ASSERT_TRUE(all.load(sgcl::string(path)));
    EXPECT_EQ(sent(all, "https://example.com/"), "login=secret; session=1");
    std::filesystem::remove(path);
    auto missing = back.load(sgcl::string(path));
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), std::errc::no_such_file_or_directory);
    auto nowhere = jar.save(sgcl::string(temp_path("no/such/dir/jar.json")));
    EXPECT_FALSE(nowhere);
    // a file that is not a jar
    ASSERT_TRUE(io::write_file(sgcl::string(path), sgcl::string("not json")));
    auto bad = back.load(sgcl::string(path));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), net::errc::invalid_cookie);
    EXPECT_NE(text(bad.error().message()).find(path), std::string::npos);
    std::filesystem::remove(path);
}

TEST(HttpCookieJar_Tests, SaveAndLoadInATask) {
    std::string path = temp_path("async.json");
    net::http::cookie_jar jar;
    put(jar, "http://example.com/", {"a=1; Max-Age=60"});
    net::http::cookie_jar back;
    auto run = [&]() -> async::task<bool> {
        auto w = co_await jar.async_save(sgcl::string(path));
        if (!w) {
            co_return false;
        }
        auto r = co_await back.async_load(sgcl::string(path));
        co_return bool(r);
    };
    EXPECT_TRUE(async::spawn(run()).wait());
    EXPECT_EQ(sent(back, "http://example.com/"), "a=1");
    std::filesystem::remove(path);
    auto missing = [&]() -> async::task<bool> {
        auto r = co_await back.async_load(sgcl::string(path));
        co_return !r && r.error().code() == std::errc::no_such_file_or_directory;
    };
    EXPECT_TRUE(async::spawn(missing()).wait());
}

TEST(HttpCookieJar_Tests, ResponseCookiesReadEverySetCookie) {
    net::http::server srv;
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.add_cookie(net::http::cookie("a", "1"));
        w.headers().add("Set-Cookie", "=broken");
        w.headers().add("Set-Cookie", "b=2; Path=/x; Secure");
        w.write("ok");
    });
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l);
    auto serving = async::spawn(srv.async_serve(*l));
    net::http::client c;
    c.proxy = net::http::proxy();
    auto res = c.get(sgcl::string("http://127.0.0.1:" + std::to_string(l->local_endpoint().port()) + "/"));
    ASSERT_TRUE(res);
    vector<net::http::cookie> got = res->cookies();
    ASSERT_EQ(got.size(), 2u);
    EXPECT_EQ(text(got[0].name), "a");
    EXPECT_EQ(text(got[1].path), "/x");
    EXPECT_TRUE(got[1].secure);
    res->close();
    srv.close();
    (void)serving.wait();
}

// Threads sharing one jar, setting and reading at once (TSan reads this)
TEST(HttpCookieJar_Tests, ThreadsShareOneJar) {
    net::http::cookie_jar jar;
    rooted<net::http::cookie_jar> shared(jar);
    std::atomic<int> bad{0};
    std::vector<std::thread> threads;
    for (int t : range(8)) {
        threads.emplace_back([&shared, &bad, t] {
            net::http::cookie_jar j = *shared;
            std::string host = "http://h" + std::to_string(t % 4) + ".example.com/";
            for (int i : range(300)) {
                std::string field = "k" + std::to_string(t) + "=" + std::to_string(i) + "; Domain=example.com";
                j.set_cookies(net::url(sgcl::string(host)), {net::http::cookie(sgcl::string(field))});
                auto h = j.header(net::url(sgcl::string(host)));
                if (h.view().find("k" + std::to_string(t) + "=" + std::to_string(i)) == std::string_view::npos) {
                    bad.fetch_add(1);
                }
                if (i % 50 == 0) {
                    (void)j.all();
                    (void)j.to_json(true);
                    (void)j.clear_expired();
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(bad.load(), 0);
    EXPECT_EQ(jar.size(), 8u);
}
