//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The client's private cache (RFC 9111): the rules alone (directives, age,
// freshness, what is stored and served), then the client against a server
// of the library's that counts its requests: a fresh response served with
// none sent (HEAD too); validation by ETag and by Last-Modified (a 304
// refreshing the stored head); what is never stored (no-store, a body past
// the limit, a request with Authorization, a body not read to its end);
// Vary's variants; stale-while-revalidate with its background revalidation;
// stale-if-error (a 503, a server gone); unsafe methods invalidating; the
// request's own directives (no-cache, max-age=0, only-if-cached); least
// recently used eviction; a cache in a directory read back by another;
// a decoded (gzip) body stored as decoded.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#include <unistd.h>

using namespace sgcl;

namespace http = sgcl::net::http;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }
}

// --- the rules of RFC 9111, alone ------------------------------------------------------------

namespace {
    net::http::headers fields_of(std::initializer_list<std::pair<const char*, const char*>> f) {
        net::http::headers h;
        for (auto& x : f) {
            h.add(x.first, x.second);
        }
        return h;
    }
}

TEST(HttpCacheRules_Tests, Directives) {
    using net::http::detail::cache_control;
    auto cc = cache_control(fields_of({{"Cache-Control", "public, max-age=60, s-maxage=\"120\""}, {"Cache-Control", "no-cache=\"Set-Cookie, X\", max-age=5"}}));
    EXPECT_TRUE(cc.is_public);
    EXPECT_EQ(cc.max_age, 60);          // the first wins
    EXPECT_EQ(cc.s_maxage, 120);        // a quoted value taken
    EXPECT_FALSE(cc.no_cache);          // no-cache="fields" is about fields
    auto bad = cache_control(fields_of({{"Cache-Control", "max-age=abc, MAX-STALE, stale-while-revalidate=30, Stale-If-Error=600, no-store"}}));
    EXPECT_EQ(bad.max_age, 0);          // an invalid max-age is stale
    EXPECT_EQ(bad.max_stale, INT64_MAX);
    EXPECT_EQ(bad.stale_while_revalidate, 30);
    EXPECT_EQ(bad.stale_if_error, 600);
    EXPECT_TRUE(bad.no_store);
    EXPECT_EQ(net::http::detail::delta_seconds("99999999999"), 2147483648);
    EXPECT_EQ(net::http::detail::delta_seconds("-1"), -1);
    EXPECT_EQ(net::http::detail::delta_seconds(""), -1);
}

TEST(HttpCacheRules_Tests, AgeAndFreshness) {
    using namespace net::http::detail;
    // §4.2.3: the corrected initial age, then the time resident
    CacheTimes t{.request_time = 1000, .response_time = 1002, .date = 990, .age = 5};
    EXPECT_EQ(current_age(t, 1002), 12);   // max(1002-990, 5+2) = 12
    EXPECT_EQ(current_age(t, 1052), 62);
    auto h = fields_of({{"Date", "Tue, 14 Nov 2023 22:13:20 GMT"}, {"Expires", "Tue, 14 Nov 2023 22:23:20 GMT"}});
    auto times = cache_times(h, 1700000000, 1700000000);
    EXPECT_EQ(freshness_lifetime(h, cache_control(h), 200, times, 0.1, 86400), 600);
    auto ma = fields_of({{"Cache-Control", "max-age=30"}, {"Expires", "Tue, 14 Nov 2023 22:23:20 GMT"}});
    EXPECT_EQ(freshness_lifetime(ma, cache_control(ma), 200, times, 0.1, 86400), 30);   // max-age wins
    auto garbage = fields_of({{"Expires", "0"}});
    EXPECT_EQ(freshness_lifetime(garbage, cache_control(garbage), 200, times, 0.1, 86400), 0);   // in the past
    auto lm = fields_of({{"Date", "Tue, 14 Nov 2023 22:13:20 GMT"}, {"Last-Modified", "Tue, 14 Nov 2023 12:13:20 GMT"}});
    auto lt = cache_times(lm, 1700000000, 1700000000);
    EXPECT_EQ(freshness_lifetime(lm, cache_control(lm), 200, lt, 0.1, 86400), 3600);   // a tenth of ten hours
    EXPECT_EQ(freshness_lifetime(lm, cache_control(lm), 200, lt, 0.1, 600), 600);      // capped
    EXPECT_EQ(freshness_lifetime(lm, cache_control(lm), 302, lt, 0.1, 86400), 0);      // not heuristically cacheable
}

TEST(HttpCacheRules_Tests, StoredAndServed) {
    using namespace net::http::detail;
    net::http::headers none;
    auto plain = fields_of({});
    EXPECT_TRUE(storable("GET", 200, none, cache_control(none), plain, cache_control(plain)));
    EXPECT_FALSE(storable("POST", 200, none, cache_control(none), plain, cache_control(plain)));
    EXPECT_FALSE(storable("GET", 302, none, cache_control(none), plain, cache_control(plain)));
    auto explicit_302 = fields_of({{"Cache-Control", "max-age=60"}});
    EXPECT_TRUE(storable("GET", 302, none, cache_control(none), explicit_302, cache_control(explicit_302)));
    auto nostore = fields_of({{"Cache-Control", "no-store"}});
    EXPECT_FALSE(storable("GET", 200, none, cache_control(none), nostore, cache_control(nostore)));
    EXPECT_FALSE(storable("GET", 200, nostore, cache_control(nostore), plain, cache_control(plain)));
    auto authed = fields_of({{"Authorization", "Bearer x"}});
    EXPECT_FALSE(storable("GET", 200, authed, cache_control(authed), plain, cache_control(plain)));
    auto pub = fields_of({{"Cache-Control", "public"}});
    EXPECT_TRUE(storable("GET", 200, authed, cache_control(authed), pub, cache_control(pub)));
    auto star = fields_of({{"Vary", "Accept, *"}});
    EXPECT_FALSE(storable("GET", 200, none, cache_control(none), star, cache_control(star)));
    EXPECT_FALSE(storable("GET", 206, none, cache_control(none), plain, cache_control(plain)));
    // served: fresh, stale with swr, validate; the request's own limits
    CacheControl req, res;
    EXPECT_EQ(cache_verdict(req, res, 60, 10).use, CacheUse::fresh);
    EXPECT_EQ(cache_verdict(req, res, 60, 70).use, CacheUse::validate);
    res.stale_while_revalidate = 30;
    EXPECT_EQ(cache_verdict(req, res, 60, 70).use, CacheUse::stale_revalidating);
    EXPECT_EQ(cache_verdict(req, res, 60, 100).use, CacheUse::validate);
    res.must_revalidate = true;
    EXPECT_EQ(cache_verdict(req, res, 60, 70).use, CacheUse::validate);   // never stale
    res = CacheControl();
    res.stale_if_error = 100;
    EXPECT_TRUE(cache_verdict(req, res, 60, 120).stale_on_error);
    EXPECT_FALSE(cache_verdict(req, res, 60, 200).stale_on_error);
    req.max_age = 5;
    EXPECT_EQ(cache_verdict(req, CacheControl(), 60, 10).use, CacheUse::validate);
    req = CacheControl();
    req.min_fresh = 55;
    EXPECT_EQ(cache_verdict(req, CacheControl(), 60, 10).use, CacheUse::validate);
    req = CacheControl();
    req.max_stale = 20;
    EXPECT_EQ(cache_verdict(req, CacheControl(), 60, 75).use, CacheUse::fresh);
    req.no_cache = true;
    EXPECT_EQ(cache_verdict(req, CacheControl(), 60, 10).use, CacheUse::validate);
}

// --- the client and its cache ---------------------------------------------------------------

namespace {
    // A server whose routes count their requests and say what the cache needs
    struct Origin {
        std::atomic<int> fresh{0}, etag{0}, lm{0}, nostore{0}, vary{0}, swr{0}, sie{0}, big{0}, auth{0}, gz{0};
        std::atomic<int> validated{0};
        http::server srv;

        Origin() {
            Origin* o = this;
            srv.route("GET /fresh", [o](http::request, http::response_writer w) {
                w.set_header("Cache-Control", "max-age=60");
                w.write("fresh-" + to_string(++o->fresh));
            });
            srv.route("POST /fresh", [](http::request, http::response_writer w) { w.write("posted"); });
            srv.route("GET /etag", [o](http::request r, http::response_writer w) {
                ++o->etag;
                w.set_header("Cache-Control", "no-cache");
                w.set_header("ETag", "\"v1\"");
                if (r.header("If-None-Match") == "\"v1\"") {
                    ++o->validated;
                    w.set_header("X-Renewed", "yes");
                    w.set_status(304);
                    return;
                }
                w.write("etag-" + to_string(o->etag.load()));
            });
            srv.route("GET /lm", [o](http::request r, http::response_writer w) {
                ++o->lm;
                w.set_header("Cache-Control", "max-age=0");
                w.set_header("Last-Modified", "Tue, 14 Nov 2023 12:00:00 GMT");
                if (r.header("If-Modified-Since") == "Tue, 14 Nov 2023 12:00:00 GMT") {
                    ++o->validated;
                    w.set_status(304);
                    return;
                }
                w.write("lm-" + to_string(o->lm.load()));
            });
            srv.route("GET /nostore", [o](http::request, http::response_writer w) {
                w.set_header("Cache-Control", "no-store, max-age=60");
                w.write("nostore-" + to_string(++o->nostore));
            });
            srv.route("GET /vary", [o](http::request r, http::response_writer w) {
                w.set_header("Cache-Control", "max-age=60");
                w.set_header("Vary", "Accept-Language");
                w.write(r.header("Accept-Language") + "-" + to_string(++o->vary));
            });
            srv.route("GET /swr", [o](http::request, http::response_writer w) {
                w.set_header("Cache-Control", "max-age=0, stale-while-revalidate=60");
                w.write("swr-" + to_string(++o->swr));
            });
            srv.route("GET /sie", [o](http::request, http::response_writer w) {
                if (++o->sie > 1) {
                    w.set_status(503);
                    return;
                }
                w.set_header("Cache-Control", "max-age=0, stale-if-error=60");
                w.write("sie-1");
            });
            srv.route("GET /big", [o](http::request, http::response_writer w) {
                ++o->big;
                w.set_header("Cache-Control", "max-age=60");
                w.write(string(std::string(2000, 'b')));
            });
            srv.route("GET /auth", [o](http::request, http::response_writer w) {
                w.set_header("Cache-Control", "max-age=60");
                w.write("auth-" + to_string(++o->auth));
            });
        }
    };

    std::string body(const expected<http::response, io::error>& r) {
        if (!r) {
            return "error: " + text(r.error().message());
        }
        auto t = r->text();
        return t ? text(*t) : "error: " + text(t.error().message());
    }
}

TEST(HttpCache_Tests, FreshServedWithoutARequest) {
    Origin o;
    http::test_server ts(o.srv);
    http::client c;
    EXPECT_FALSE(c.cache);
    auto plain = c.get(ts.url() + "/fresh");
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->from_cache(), http::response::cache_status::none);   // no cache: none
    EXPECT_FALSE(plain->age());
    (void)plain->text();
    c.cache = http::cache();
    auto first = c.get(ts.url() + "/fresh");
    ASSERT_TRUE(first);
    EXPECT_EQ(first->from_cache(), http::response::cache_status::miss);
    EXPECT_EQ(body(first), "fresh-2");
    auto second = c.get(ts.url() + "/fresh");
    ASSERT_TRUE(second);
    EXPECT_EQ(second->from_cache(), http::response::cache_status::hit);
    EXPECT_EQ(body(second), "fresh-2");
    EXPECT_EQ(o.fresh.load(), 2);
    ASSERT_TRUE(second->age());
    EXPECT_FALSE(second->header("Age").empty());
    EXPECT_EQ(second->status(), 200);
    EXPECT_EQ(second->header("Cache-Control"), "max-age=60");
    // HEAD of what a GET stored: the head alone, from the cache
    auto head = c.send(http::request("HEAD", ts.url() + "/fresh"));
    ASSERT_TRUE(head);
    EXPECT_EQ(head->from_cache(), http::response::cache_status::hit);
    EXPECT_EQ(body(head), "");
    EXPECT_EQ(o.fresh.load(), 2);
    // a copy of the client shares the cache, and so does another client given it
    http::client other;
    other.cache = c.cache;
    EXPECT_EQ(body(other.get(ts.url() + "/fresh")), "fresh-2");
    EXPECT_EQ(c.cache->size(), 1u);
    EXPECT_EQ(c.cache->bytes(), 7u);
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, ValidatedByETagAndLastModified) {
    Origin o;
    http::test_server ts(o.srv);
    http::client c;
    c.cache = http::cache();
    EXPECT_EQ(body(c.get(ts.url() + "/etag")), "etag-1");
    auto again = c.get(ts.url() + "/etag");   // no-cache: asked again with If-None-Match, 304
    ASSERT_TRUE(again);
    EXPECT_EQ(again->from_cache(), http::response::cache_status::revalidated);
    EXPECT_EQ(again->status(), 200);
    EXPECT_EQ(again->header("X-Renewed"), "yes");   // the 304's fields merged into the stored head
    EXPECT_EQ(body(again), "etag-1");
    EXPECT_EQ(o.etag.load(), 2);
    EXPECT_EQ(body(c.get(ts.url() + "/lm")), "lm-1");
    auto lm = c.get(ts.url() + "/lm");
    ASSERT_TRUE(lm);
    EXPECT_EQ(lm->from_cache(), http::response::cache_status::revalidated);
    EXPECT_EQ(body(lm), "lm-1");
    EXPECT_EQ(o.validated.load(), 2);
    // a request with conditions of its own: the cache adds none, the 304 is the program's
    http::request own("GET", ts.url() + "/etag");
    own.set_header("If-None-Match", "\"other\"");
    auto mine = c.send(own);
    ASSERT_TRUE(mine);
    EXPECT_EQ(mine->status(), 200);
    EXPECT_EQ(body(mine), "etag-3");
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, WhatIsNotStored) {
    Origin o;
    http::test_server ts(o.srv);
    http::client c;
    http::cache::options opts;
    opts.max_entry_bytes = 1000;
    c.cache = http::cache(opts);
    EXPECT_EQ(body(c.get(ts.url() + "/nostore")), "nostore-1");
    EXPECT_EQ(body(c.get(ts.url() + "/nostore")), "nostore-2");
    (void)body(c.get(ts.url() + "/big"));
    (void)body(c.get(ts.url() + "/big"));
    EXPECT_EQ(o.big.load(), 2);   // past max_entry_bytes
    http::request authed("GET", ts.url() + "/auth");
    authed.set_header("Authorization", "Bearer x");
    EXPECT_EQ(body(c.send(authed)), "auth-1");
    EXPECT_EQ(body(c.send(authed)), "auth-2");   // §3.5: neither public nor must-revalidate
    auto unread = c.get(ts.url() + "/fresh");
    ASSERT_TRUE(unread);
    unread->close();   // the body not read to its end: nothing stored
    EXPECT_EQ(body(c.get(ts.url() + "/fresh")), "fresh-2");
    http::request nostore("GET", ts.url() + "/vary");
    nostore.set_header("Cache-Control", "no-store");
    (void)body(c.send(nostore));
    EXPECT_EQ(c.cache->size(), 1u);   // /fresh alone
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, VaryKeepsVariants) {
    Origin o;
    http::test_server ts(o.srv);
    http::client c;
    c.cache = http::cache();
    auto ask = [&](const char* lang) {
        http::request r("GET", ts.url() + "/vary");
        r.set_header("Accept-Language", lang);
        return body(c.send(r));
    };
    EXPECT_EQ(ask("pl"), "pl-1");
    EXPECT_EQ(ask("en"), "en-2");
    EXPECT_EQ(ask("pl"), "pl-1");
    EXPECT_EQ(ask("en"), "en-2");
    EXPECT_EQ(c.cache->size(), 2u);
    c.cache->erase(ts.url() + "/vary");
    EXPECT_EQ(c.cache->size(), 0u);
    EXPECT_EQ(ask("pl"), "pl-3");
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, StaleWhileRevalidate) {
    Origin o;
    http::test_server ts(o.srv);
    http::client c;
    c.cache = http::cache();
    EXPECT_EQ(body(c.get(ts.url() + "/swr")), "swr-1");
    auto stale = c.get(ts.url() + "/swr");   // stale at once (max-age=0): served, revalidated behind it
    ASSERT_TRUE(stale);
    EXPECT_EQ(stale->from_cache(), http::response::cache_status::stale);
    EXPECT_EQ(body(stale), "swr-1");
    for (int i = 0; i < 200 && o.swr.load() < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(o.swr.load(), 2);
    std::string now;
    for (int i = 0; i < 200; ++i) {   // the background's response stored as its body ends
        now = body(c.get(ts.url() + "/swr"));
        if (now != "swr-1") {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_NE(now, "swr-1");
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, StaleIfError) {
    Origin o;
    http::test_server ts(o.srv);
    http::client c;
    c.cache = http::cache();
    EXPECT_EQ(body(c.get(ts.url() + "/sie")), "sie-1");
    auto failed = c.get(ts.url() + "/sie");   // 503: the stored one instead
    ASSERT_TRUE(failed);
    EXPECT_EQ(failed->from_cache(), http::response::cache_status::stale);
    EXPECT_EQ(failed->status(), 200);
    EXPECT_EQ(body(failed), "sie-1");
    EXPECT_EQ(o.sie.load(), 2);
    // a server gone: the stored one too
    c.cache->clear();
    http::client d;
    d.cache = http::cache();
    {
        Origin p;
        http::test_server gone(p.srv);
        EXPECT_EQ(body(d.get(gone.url() + "/sie")), "sie-1");
        sgcl::string url = gone.url() + "/sie";
        gone.close();
        auto down = d.get(url);
        ASSERT_TRUE(down);
        EXPECT_EQ(down->from_cache(), http::response::cache_status::stale);
        EXPECT_EQ(body(down), "sie-1");
    }
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, UnsafeMethodsInvalidate) {
    Origin o;
    http::test_server ts(o.srv);
    http::client c;
    c.cache = http::cache();
    EXPECT_EQ(body(c.get(ts.url() + "/fresh")), "fresh-1");
    EXPECT_EQ(body(c.post(ts.url() + "/fresh", "text/plain", "x")), "posted");
    EXPECT_EQ(c.cache->size(), 0u);
    EXPECT_EQ(body(c.get(ts.url() + "/fresh")), "fresh-2");
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, RequestDirectives) {
    Origin o;
    http::test_server ts(o.srv);
    http::client c;
    c.cache = http::cache();
    http::request only("GET", ts.url() + "/fresh");
    only.set_header("Cache-Control", "only-if-cached");
    auto none = c.send(only);   // nothing stored: 504, nothing sent
    ASSERT_TRUE(none);
    EXPECT_EQ(none->status(), 504);
    EXPECT_EQ(o.fresh.load(), 0);
    EXPECT_EQ(body(c.get(ts.url() + "/fresh")), "fresh-1");
    EXPECT_EQ(body(c.send(only)), "fresh-1");
    http::request nocache("GET", ts.url() + "/fresh");
    nocache.set_header("Cache-Control", "no-cache");   // validated: no validator, so asked whole
    EXPECT_EQ(body(c.send(nocache)), "fresh-2");
    http::request young("GET", ts.url() + "/fresh");
    young.set_header("Cache-Control", "max-age=0");   // §5.2.1.1: an age of 0 is still within it
    EXPECT_EQ(body(c.send(young)), "fresh-2");
    http::request older("GET", ts.url() + "/fresh");
    older.set_header("Cache-Control", "min-fresh=120");   // fresh for 60 s at most: asked again
    EXPECT_EQ(body(c.send(older)), "fresh-3");
    EXPECT_EQ(body(c.get(ts.url() + "/fresh")), "fresh-3");
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, LeastRecentlyUsed) {
    Origin o;
    http::test_server ts(o.srv);
    http::client c;
    http::cache::options opts;
    opts.max_bytes = 2500;
    c.cache = http::cache(opts);
    auto ask = [&](const char* lang) {
        http::request r("GET", ts.url() + "/vary");
        r.set_header("Accept-Language", string(std::string(1000, lang[0])));
        return c.send(r);
    };
    (void)body(ask("a"));
    (void)body(ask("b"));
    (void)body(ask("a"));   // a used last
    (void)body(ask("c"));   // past 2500: b goes
    EXPECT_EQ(c.cache->size(), 2u);
    EXPECT_EQ(ask("a")->from_cache(), http::response::cache_status::hit);
    EXPECT_EQ(ask("b")->from_cache(), http::response::cache_status::miss);
    c.cache->clear();
    EXPECT_EQ(c.cache->size(), 0u);
    EXPECT_EQ(c.cache->bytes(), 0u);
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, OnDisk) {
    Origin o;
    http::test_server ts(o.srv);
    auto dir = std::filesystem::temp_directory_path() / ("sgcl_http_cache_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    sgcl::string d = sgcl::string(dir.string());
    {
        auto store = http::cache::on_disk(d);
        ASSERT_TRUE(store);
        http::client c;
        c.cache = *store;
        EXPECT_EQ(body(c.get(ts.url() + "/fresh")), "fresh-1");
        EXPECT_EQ(body(c.get(ts.url() + "/etag")), "etag-1");
        EXPECT_EQ(c.cache->size(), 2u);
    }
    std::filesystem::path junk = dir / "0000.head";
    std::FILE* f = std::fopen(junk.c_str(), "w");
    std::fputs("not a head", f);
    std::fclose(f);
    auto again = http::cache::on_disk(d);   // another cache of the directory: the entries read back, the junk removed
    ASSERT_TRUE(again);
    EXPECT_EQ(again->size(), 2u);
    EXPECT_FALSE(std::filesystem::exists(junk));
    http::client c;
    c.cache = *again;
    auto hit = c.get(ts.url() + "/fresh");
    ASSERT_TRUE(hit);
    EXPECT_EQ(hit->from_cache(), http::response::cache_status::hit);
    EXPECT_EQ(body(hit), "fresh-1");
    auto renewed = c.get(ts.url() + "/etag");
    ASSERT_TRUE(renewed);
    EXPECT_EQ(renewed->from_cache(), http::response::cache_status::revalidated);
    EXPECT_EQ(body(renewed), "etag-1");
    auto third = http::cache::on_disk(d);
    ASSERT_TRUE(third);
    http::client e;
    e.cache = *third;
    EXPECT_EQ(e.get(ts.url() + "/etag")->header("X-Renewed"), "yes");   // the 304's head written back
    third->erase(ts.url() + "/fresh");
    EXPECT_EQ(third->size(), 1u);
    third->clear();
    size_t files = 0;
    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        (void)entry;
        ++files;
    }
    EXPECT_EQ(files, 0u);
    std::filesystem::remove_all(dir);
    EXPECT_FALSE(http::cache::on_disk("/dev/null/x"));
    ts.close();   // the handlers ended before the counters go
}

TEST(HttpCache_Tests, DecodedBodyStored) {
    http::server srv;
    std::atomic<int> asked{0};
    srv.use(http::compression());
    srv.route("GET /json", [&asked](http::request, http::response_writer w) {
        ++asked;
        w.set_header("Content-Type", "application/json");
        w.set_header("Cache-Control", "max-age=60");
        w.write(string(std::string(4000, 'j')));
    });
    http::test_server ts(srv);
    http::client c;
    c.cache = http::cache();
    auto first = c.get(ts.url() + "/json");
    ASSERT_TRUE(first);
    EXPECT_TRUE(first->uncompressed());
    EXPECT_EQ(body(first).size(), 4000u);
    auto second = c.get(ts.url() + "/json");
    ASSERT_TRUE(second);
    EXPECT_EQ(second->from_cache(), http::response::cache_status::hit);
    EXPECT_TRUE(second->uncompressed());
    EXPECT_TRUE(second->header("Content-Encoding").empty());
    EXPECT_EQ(body(second), std::string(4000, 'j'));
    EXPECT_EQ(asked.load(), 1);
    ts.close();   // the handlers ended before the counters go
}
