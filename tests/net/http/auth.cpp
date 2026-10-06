//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The authentications (auth.h, detail/auth.h, the client's 401): MD5 by
// RFC 1321's test suite; Digest's response by RFC 2617's and RFC 7616's
// examples (MD5, SHA-256, SHA-512/256 with userhash and username*); the
// challenges and credentials read; Basic both ways (Go's r.BasicAuth and
// SetBasicAuth); the server's digest_auth against curl --digest and
// Python's urllib (HTTPDigestAuthHandler); the client's Digest and Basic
// against a Digest server of Python's standard library (py_digest), every
// algorithm, auth-int, userhash, stale; the client's protection spaces
// remembered, a wrong password, a redirect to another host, a URL's
// user:password@.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace sgcl;

namespace {
    namespace http = sgcl::net::http;
    using http::detail::DigestAlgorithm;
    using http::detail::DigestHash;
    using http::detail::DigestInput;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string md5(std::string_view s) {
        http::detail::Md5 m;
        m.update(s.data(), s.size());
        auto d = m.digest();
        std::string out;
        http::detail::append_hex_bytes(out, d.data(), d.size());
        return out;
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

    // The Python Digest server: its base URL, "" when there is no python3
    struct PyDigest {
        FILE* p = nullptr;
        std::string base;

        PyDigest() {
            if (std::system("command -v python3 > /dev/null 2>&1") != 0) {
                return;
            }
            auto src = source_root() / "tests/net/http/py_digest/server.py";
            p = popen(("python3 '" + src.string() + "'").c_str(), "r");
            char line[64] = {};
            if (!p || !fgets(line, sizeof line, p)) {
                return;
            }
            base = "http://127.0.0.1:" + std::to_string(std::atoi(line + 5));
        }

        ~PyDigest() {
            if (!base.empty()) {
                http::client c;
                (void)c.get(sgcl::string(base + "/quit"));
            }
            if (p) {
                pclose(p);
            }
        }
    };

    std::optional<sgcl::string> ann_password(const sgcl::string& user) {
        if (user == "ann" || user == "Jäsøn Doe") {
            return sgcl::string("secret");
        }
        return std::nullopt;
    }
}

// --- MD5 and Digest's computations -----------------------------------------------------------

TEST(HttpAuthDigest_Tests, Md5TheTestSuite) {
    // RFC 1321 §A.5
    EXPECT_EQ(md5(""), "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(md5("a"), "0cc175b9c0f1b6a831c399e269772661");
    EXPECT_EQ(md5("abc"), "900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(md5("message digest"), "f96b697d7cb7938d525a2f31aaf161d0");
    EXPECT_EQ(md5("abcdefghijklmnopqrstuvwxyz"), "c3fcd3d76192e4007dfb496cca67e13b");
    EXPECT_EQ(md5("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"), "d174ab98d277d9f5a5611c2c9f419d9f");
    EXPECT_EQ(md5("12345678901234567890123456789012345678901234567890123456789012345678901234567890"), "57edf4a22be3c955ac49da2e2107b67a");
    // pieces of any size give the digest of the whole; lengths about a block
    std::string big;
    for (int i : range(100000)) {
        big += char('a' + i % 26);
    }
    for (size_t step : {1u, 7u, 63u, 64u, 65u, 4096u}) {
        http::detail::Md5 m;
        for (size_t at = 0; at < big.size(); at += step) {
            m.update(big.data() + at, std::min(step, big.size() - at));
        }
        auto d = m.digest();
        std::string hex;
        http::detail::append_hex_bytes(hex, d.data(), d.size());
        EXPECT_EQ(hex, md5(big)) << step;
    }
    for (size_t n : {55u, 56u, 57u, 63u, 64u, 119u, 120u}) {
        const std::string s(n, 'x');
        const std::string py = shell("python3 -c \"import hashlib; print(hashlib.md5(b'x'*" + std::to_string(n) + ").hexdigest(), end='')\"");
        if (!py.empty()) {
            EXPECT_EQ(md5(s), py) << n;
        }
    }
}

TEST(HttpAuthDigest_Tests, TheRfcExamples) {
    // RFC 2617 §3.5 (MD5, qop auth)
    DigestInput a;
    a.algorithm = DigestAlgorithm{DigestHash::md5, false};
    a.user = "Mufasa";
    a.realm = "testrealm@host.com";
    a.password = "Circle Of Life";
    a.method = "GET";
    a.uri = "/dir/index.html";
    a.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093";
    a.cnonce = "0a4f113b";
    a.nc = "00000001";
    a.qop = "auth";
    EXPECT_EQ(http::detail::digest_response(a), "6629fae49393a05397450978507c4ef1");
    // RFC 7616 §3.9.1, MD5 and SHA-256
    DigestInput b;
    b.algorithm = DigestAlgorithm{DigestHash::md5, false};
    b.user = "Mufasa";
    b.realm = "http-auth@example.org";
    b.password = "Circle of Life";
    b.method = "GET";
    b.uri = "/dir/index.html";
    b.nonce = "7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v";
    b.cnonce = "f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ";
    b.nc = "00000001";
    b.qop = "auth";
    EXPECT_EQ(http::detail::digest_response(b), "8ca523f5e9506fed4657c9700eebdbec");
    b.algorithm = DigestAlgorithm{DigestHash::sha256, false};
    EXPECT_EQ(http::detail::digest_response(b), "753927fa0e85d155564e2e272a28d1802ca10daf4496794697cf8db5856cb6c1");
    // RFC 7616 §3.9.2, SHA-512-256 and userhash: the inputs of the RFC's
    // example; its printed userhash and response are not the hashes of
    // them (Python's hashlib gives these, as this does)
    DigestInput c;
    c.algorithm = DigestAlgorithm{DigestHash::sha512_256, false};
    c.user = "J\xc3\xa4s\xc3\xb8n Doe";
    c.realm = "api@example.org";
    c.password = "Secret, or not?";
    c.method = "GET";
    c.uri = "/doe.json";
    c.nonce = "5TsQWLVdgBdmrQ0XsxbDODV+57QdFR34I9HAbC/RVvkK";
    c.cnonce = "NTg6RKcb9boFIAS3KrFK9BGeh+iDa/sm6jUMp2wds69v";
    c.nc = "00000001";
    c.qop = "auth";
    EXPECT_EQ(http::detail::digest_response(c), "3798d4131c277846293534c3edc11bd8a5e4cdcbff78b05db9d95eeb1cec68a5");
    EXPECT_EQ(http::detail::digest_hex(DigestHash::sha512_256, "J\xc3\xa4s\xc3\xb8n Doe:api@example.org"),
              "793263caabb707a56211940d90411ea4a575adeccb7e360aeb624ed06ece9b0b");
    EXPECT_EQ(http::detail::encode_ext_value("J\xc3\xa4s\xc3\xb8n Doe"), "UTF-8''J%C3%A4s%C3%B8n%20Doe");
    EXPECT_EQ(*http::detail::decode_ext_value("UTF-8''J%C3%A4s%C3%B8n%20Doe"), "J\xc3\xa4s\xc3\xb8n Doe");
    EXPECT_FALSE(http::detail::decode_ext_value("ISO-8859-1''abc"));
    EXPECT_FALSE(http::detail::decode_ext_value("UTF-8''%G1"));
    // the algorithms' names both ways
    for (const char* name : {"MD5", "MD5-sess", "SHA-256", "SHA-256-sess", "SHA-512-256", "SHA-512-256-sess"}) {
        auto x = http::detail::digest_algorithm(name);
        ASSERT_TRUE(x) << name;
        EXPECT_EQ(http::detail::digest_algorithm_name(*x), name);
    }
    EXPECT_TRUE(http::detail::digest_algorithm("sha-256"));
    EXPECT_FALSE(http::detail::digest_algorithm("SHA-1"));
    EXPECT_FALSE(http::detail::digest_algorithm("-sess"));
}

TEST(HttpAuthDigest_Tests, ChallengesRead) {
    using http::detail::parse_challenges;
    auto one = parse_challenges(R"(Digest realm="a, b", qop="auth, auth-int", nonce="x\"y", algorithm=SHA-256, stale=true)");
    ASSERT_EQ(one.size(), 1u);
    EXPECT_EQ(one[0].scheme, "Digest");
    EXPECT_EQ(*one[0].param("realm"), "a, b");
    EXPECT_EQ(*one[0].param("qop"), "auth, auth-int");
    EXPECT_EQ(*one[0].param("nonce"), "x\"y");
    EXPECT_EQ(*one[0].param("ALGORITHM"), "SHA-256");
    EXPECT_EQ(*one[0].param("stale"), "true");
    auto many = parse_challenges(R"(Newauth realm="apps", type=1, title="Login to \"apps\"", Basic realm="simple", Bearer abc.def==, Negotiate)");
    ASSERT_EQ(many.size(), 4u);
    EXPECT_EQ(many[0].scheme, "Newauth");
    EXPECT_EQ(*many[0].param("title"), "Login to \"apps\"");
    EXPECT_EQ(many[1].scheme, "Basic");
    EXPECT_EQ(*many[1].param("realm"), "simple");
    EXPECT_EQ(many[2].scheme, "Bearer");
    EXPECT_EQ(many[2].token68, "abc.def==");
    EXPECT_EQ(many[3].scheme, "Negotiate");
    EXPECT_TRUE(many[3].params.empty());
    // what does not read ends the list there: an unterminated string, a
    // control escaped or not (RFC 9110 §5.6.4: quoted-pair is HTAB, SP,
    // VCHAR or obs-text), DEL
    EXPECT_EQ(parse_challenges(R"(Digest realm="unterminated)").size(), 1u);
    for (std::string bad : {std::string("Digest realm=\"a\\\x01b\", nonce=\"n\""), std::string("Digest realm=\"a\x01b\", nonce=\"n\""),
                            std::string("Digest realm=\"a\x7f\", nonce=\"n\"")}) {
        auto got = parse_challenges(bad);
        ASSERT_EQ(got.size(), 1u);
        EXPECT_FALSE(got[0].param("realm"));
        EXPECT_FALSE(got[0].param("nonce"));
    }
    auto obs = parse_challenges("Digest realm=\"caf\xc3\xa9 \\\"x\\\"\"");
    ASSERT_EQ(obs.size(), 1u);
    EXPECT_EQ(*obs[0].param("realm"), "caf\xc3\xa9 \"x\"");   // obs-text and quoted-pairs kept
    EXPECT_TRUE(parse_challenges("").empty());
    EXPECT_TRUE(parse_challenges(", ,").empty());
    EXPECT_EQ(parse_challenges("Basic").size(), 1u);
    // Basic's credentials
    auto creds = http::detail::read_basic("Basic " + text(encoding::base64::standard.encode("ann:pa:ss")));
    ASSERT_TRUE(creds);
    EXPECT_EQ(creds->first, "ann");
    EXPECT_EQ(creds->second, "pa:ss");   // the first colon parts them
    EXPECT_TRUE(http::detail::read_basic("basic " + text(encoding::base64::standard.encode("a:b"))));   // a scheme without case
    EXPECT_FALSE(http::detail::read_basic("Basic !!!"));
    EXPECT_FALSE(http::detail::read_basic("Basic " + text(encoding::base64::standard.encode("nocolon"))));
    EXPECT_FALSE(http::detail::read_basic("Bearer x"));
    EXPECT_EQ(text(http::detail::basic_credentials("Aladdin", "open sesame")), "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");   // RFC 7617 §2
}

// --- Basic ----------------------------------------------------------------------------------

TEST(HttpAuthBasic_Tests, BothSides) {
    tracked_ptr calls = make_tracked<std::atomic<int>>(0);
    http::server s;
    s.use(http::basic_auth("admin area", [calls](const sgcl::string& u, const sgcl::string& p) {
        ++*calls;
        return u == "ann" && p == "pa:ss";
    }));
    s.route("GET /me", [](http::request r, http::response_writer w) { w.write(r.authenticated_user()); });
    Running run(s);
    http::client c;
    auto none = c.get(run.url("/me"));
    ASSERT_TRUE(none);
    EXPECT_EQ(none->status(), 401);
    EXPECT_EQ(none->header("WWW-Authenticate"), "Basic realm=\"admin area\", charset=\"UTF-8\"");
    (void)none->text();
    http::request good("GET", run.url("/me"));
    good.set_basic_auth("ann", "pa:ss");
    EXPECT_EQ(good.header("Authorization"), "Basic " + encoding::base64::standard.encode("ann:pa:ss"));
    auto basic = good.basic_auth();
    ASSERT_TRUE(basic);
    EXPECT_EQ(basic->user, "ann");
    EXPECT_EQ(basic->password, "pa:ss");
    auto ok = c.send(good);
    ASSERT_TRUE(ok);
    EXPECT_EQ(*ok->text(), "ann");
    http::request bad("GET", run.url("/me"));
    bad.set_basic_auth("ann", "nope");
    auto refused = c.send(bad);
    ASSERT_TRUE(refused);
    EXPECT_EQ(refused->status(), 401);
    (void)refused->text();
    EXPECT_EQ(calls->load(), 2);
    // curl as the client
    if (std::system("command -v curl > /dev/null 2>&1") == 0) {
        EXPECT_EQ(shell("curl -s -u 'ann:pa:ss' " + text(run.url("/me"))), "ann");
        EXPECT_EQ(shell("curl -s -o /dev/null -w '%{http_code}' -u ann:x " + text(run.url("/me"))), "401");
    }
    // without a middleware, no user; a request of no Authorization has no basic_auth
    EXPECT_EQ(http::test_request("GET", "/").authenticated_user(), "");
    EXPECT_FALSE(http::test_request("GET", "/").basic_auth());
}

// --- Digest, the server ---------------------------------------------------------------------

TEST(HttpAuthDigestServer_Tests, AgainstCurlAndPython) {
    for (int k : {0, 1, 2}) {
        http::digest_auth::options o;
        o.algorithms = k == 0 ? vector<http::digest_auth::algorithm>{http::digest_auth::algorithm::sha256, http::digest_auth::algorithm::md5}
                       : k == 1 ? vector<http::digest_auth::algorithm>{http::digest_auth::algorithm::md5}
                                : vector<http::digest_auth::algorithm>{http::digest_auth::algorithm::sha512_256};
        http::server s;
        s.use(http::digest_auth("files", ann_password, o));
        s.route("/me", [](http::request r, http::response_writer w) { w.write(r.authenticated_user()); });
        Running run(s);
        http::client c;
        auto none = c.get(run.url("/me"));
        ASSERT_TRUE(none);
        EXPECT_EQ(none->status(), 401);
        auto challenges = none->headers().get_all("WWW-Authenticate");
        EXPECT_EQ(challenges.size(), o.algorithms.size());
        EXPECT_NE(text(challenges[0]).find("qop=\"auth\""), std::string::npos);
        (void)none->text();
        if (std::system("command -v curl > /dev/null 2>&1") == 0) {
            EXPECT_EQ(shell("curl -s --digest -u ann:secret " + text(run.url("/me?x=1"))), "ann") << k;
            EXPECT_EQ(shell("curl -s -o /dev/null -w '%{http_code}' --digest -u ann:wrong " + text(run.url("/me"))), "401") << k;
        }
        if (k != 2) {   // Python's urllib: MD5 and SHA-256
            const std::string py = "python3 -c \"import urllib.request as u; m=u.HTTPPasswordMgrWithDefaultRealm(); m.add_password(None,'" + run.base +
                                   "','ann','secret'); o=u.build_opener(u.HTTPDigestAuthHandler(m)); print(o.open('" + run.base +
                                   "/me').read().decode(), end='')\" 2>&1";
            EXPECT_EQ(shell(py), "ann") << k;
        }
    }
}

TEST(HttpAuthDigestServer_Tests, ReplaysStaleAndAuthInt) {
    async::manual_clock clock;
    clock.install();
    http::server s;
    s.use(http::digest_auth("r", ann_password, {.algorithms = {http::digest_auth::algorithm::sha256}, .auth_int = true}));
    s.route("/me", [](http::request r, http::response_writer w) -> async::task<> {
        auto body = co_await r.async_text();
        w.write(r.authenticated_user() + ":" + *body);
    });
    auto ask = [&](const char* method, const sgcl::string& authorization, const char* body) {
        auto req = http::test_request(method, "/me", sgcl::string(body));
        if (!authorization.empty()) {
            req.set_header("Authorization", authorization);
        }
        http::response_recorder rec;
        rec.serve(s, req);
        return std::make_pair(rec.status(), rec);
    };
    auto first = ask("GET", "", "");
    ASSERT_EQ(first.first, 401);
    auto ch = http::detail::parse_challenges(first.second.header("WWW-Authenticate").view());
    ASSERT_EQ(ch.size(), 1u);
    const std::string nonce = *ch[0].param("nonce");
    const std::string opaque = *ch[0].param("opaque");
    auto authorization = [&](const char* method, const char* qop, const char* nc, std::string_view body) {
        DigestInput in;
        in.algorithm = DigestAlgorithm{DigestHash::sha256, false};
        in.user = "ann";
        in.realm = "r";
        in.password = "secret";
        in.method = method;
        in.uri = "/me";
        in.nonce = nonce;
        in.cnonce = "c0";
        in.nc = nc;
        in.qop = qop;
        const std::string bh = http::detail::digest_hex(DigestHash::sha256, body);
        in.body_hash = bh;
        return sgcl::string("Digest username=\"ann\", realm=\"r\", nonce=\"" + nonce + "\", uri=\"/me\", algorithm=SHA-256, qop=" + qop +
                            ", nc=" + nc + ", cnonce=\"c0\", opaque=\"" + opaque + "\", response=\"" + http::detail::digest_response(in) + "\"");
    };
    auto ok = ask("GET", authorization("GET", "auth", "00000001", ""), "");
    EXPECT_EQ(ok.first, 200);
    EXPECT_EQ(text(ok.second.body()), "ann:");
    EXPECT_EQ(ask("GET", authorization("GET", "auth", "00000001", ""), "").first, 401);   // the same count again: a replay
    EXPECT_EQ(ask("GET", authorization("GET", "auth", "00000002", ""), "").first, 200);
    // auth-int: the body in the hash, and the handler reads it after
    auto integ = ask("POST", authorization("POST", "auth-int", "00000003", "the body"), "the body");
    EXPECT_EQ(integ.first, 200);
    EXPECT_EQ(text(integ.second.body()), "ann:the body");
    EXPECT_EQ(ask("POST", authorization("POST", "auth-int", "00000004", "the body"), "another").first, 401);   // the body changed
    // another uri, another realm, a nonce not ours: refused
    std::string forged = text(authorization("GET", "auth", "00000005", ""));
    auto replace = [](std::string s, const std::string& a, const std::string& b) {
        s.replace(s.find(a), a.size(), b);
        return s;
    };
    EXPECT_EQ(ask("GET", sgcl::string(replace(forged, "uri=\"/me\"", "uri=\"/other\"")), "").first, 401);
    EXPECT_EQ(ask("GET", sgcl::string(replace(forged, "nonce=\"" + nonce, "nonce=\"AAAA" + nonce.substr(4))), "").first, 401);
    // the right password on an old nonce: stale=true, a new nonce offered
    clock.advance(std::chrono::minutes(6));
    auto stale = ask("GET", authorization("GET", "auth", "00000006", ""), "");
    EXPECT_EQ(stale.first, 401);
    EXPECT_NE(text(stale.second.header("WWW-Authenticate")).find("stale=true"), std::string::npos);
    clock.uninstall();
    EXPECT_THROW(http::digest_auth("r", ann_password, {.algorithms = {}}), std::invalid_argument);
}

// --- the client ------------------------------------------------------------------------------

TEST(HttpAuthClient_Tests, AgainstPythonsDigestServer) {
    PyDigest py;
    if (py.base.empty()) {
        GTEST_SKIP() << "no python3";
    }
    http::client c;
    c.credentials = http::credentials{"ann", "secret"};
    for (const char* alg : {"MD5", "MD5-sess", "SHA-256", "SHA-256-sess", "SHA-512-256", "SHA-512-256-sess"}) {
        auto r = c.get(sgcl::string(py.base + "/" + alg + "/a?b=c"));
        ASSERT_TRUE(r) << alg;
        EXPECT_EQ(r->status(), 200) << alg;
        EXPECT_EQ(text(*r->text()).substr(0, 7), "ok ann ") << alg;
    }
    // auth-int with a body in memory
    auto posted = c.post(sgcl::string(py.base + "/SHA-256/post"), "text/plain", "a body");
    ASSERT_TRUE(posted);
    EXPECT_EQ(posted->status(), 200);
    (void)posted->text();
    // stale: the new nonce answered without asking again for the password
    auto stale = c.get(sgcl::string(py.base + "/stale/x"));
    ASSERT_TRUE(stale);
    EXPECT_EQ(stale->status(), 200);
    (void)stale->text();
    // userhash and a name past ASCII
    http::client jason;
    jason.credentials = http::credentials{"J\xc3\xa4s\xc3\xb8n Doe", "secret"};
    auto hashed = jason.get(sgcl::string(py.base + "/userhash/doe.json"));
    ASSERT_TRUE(hashed);
    EXPECT_EQ(hashed->status(), 200);
    EXPECT_EQ(text(*hashed->text()).substr(0, 15), "ok J\xc3\xa4s\xc3\xb8n Doe ");
    auto star = jason.get(sgcl::string(py.base + "/SHA-256/star"));
    ASSERT_TRUE(star);
    EXPECT_EQ(star->status(), 200);
    (void)star->text();
    // Basic when that is what is asked
    auto basic = c.get(sgcl::string(py.base + "/basic/x"));
    ASSERT_TRUE(basic);
    EXPECT_EQ(basic->status(), 200);
    (void)basic->text();
    // a wrong password: the 401 after one try, not a loop
    http::client wrong;
    wrong.credentials = http::credentials{"ann", "nope"};
    auto refused = wrong.get(sgcl::string(py.base + "/SHA-256/x"));
    ASSERT_TRUE(refused);
    EXPECT_EQ(refused->status(), 401);
    (void)refused->text();
}

TEST(HttpAuthClient_Tests, SpacesRememberedAndKeptToTheOrigin) {
    tracked_ptr seen = make_tracked<std::vector<std::string>>();
    tracked_ptr lock = make_tracked<std::mutex>();
    http::server s;
    s.use([seen, lock](http::request& r, http::response_writer&) {
        std::lock_guard<std::mutex> g(*lock);
        seen->push_back(text(r.url().path()) + (r.header("Authorization").empty() ? " -" : " auth"));
        return true;
    });
    s.use(http::digest_auth("space", ann_password));
    s.route("/me", [](http::request r, http::response_writer w) { w.write(r.authenticated_user()); });
    Running run(s);
    http::client c;
    c.credentials = http::credentials{"ann", "secret"};
    for (int i : range(3)) {
        (void)i;
        auto r = c.get(run.url("/me"));
        ASSERT_TRUE(r);
        EXPECT_EQ(*r->text(), "ann");
    }
    {
        std::lock_guard<std::mutex> g(*lock);
        const std::vector<std::string> expected = {"/me -", "/me auth", "/me auth", "/me auth"};   // asked once, then sent at once
        EXPECT_EQ(*seen, expected);
    }
    // a request's own credentials win over the client's
    http::request mine("GET", run.url("/me"));
    mine.set_credentials("ann", "secret");
    http::client bare;
    auto own = bare.send(mine);
    ASSERT_TRUE(own);
    EXPECT_EQ(*own->text(), "ann");
    // a redirect to another host does not take the credentials there
    tracked_ptr leaked = make_tracked<std::atomic<int>>(0);
    http::server other;
    other.route("/", [leaked](http::request r, http::response_writer w) {
        if (!r.header("Authorization").empty()) {
            ++*leaked;
        }
        w.add_header("WWW-Authenticate", "Basic realm=\"x\"");
        w.error(401);
    });
    Running elsewhere(other);
    http::server bouncer;
    bouncer.route("/go", [&elsewhere](http::request, http::response_writer w) { w.redirect(elsewhere.url("/")); });
    Running from(bouncer);
    auto bounced = c.get(sgcl::string("http://localhost:" + std::to_string(from.listener.local_endpoint().port()) + "/go"));
    ASSERT_TRUE(bounced);
    EXPECT_EQ(bounced->status(), 401);   // the other origin's 401, unanswered
    (void)bounced->text();
    EXPECT_EQ(leaked->load(), 0);
}

TEST(HttpAuthClient_Tests, AUrlsUserInfo) {
    http::server s;
    s.use(http::basic_auth("x", [](const sgcl::string& u, const sgcl::string& p) { return u == "ann" && p == "s3cret"; }));
    s.route("/me", [](http::request r, http::response_writer w) { w.write(r.authenticated_user()); });
    Running run(s);
    http::client c;
    auto r = c.get(sgcl::string("http://ann:s3cret@127.0.0.1:" + std::to_string(run.listener.local_endpoint().port()) + "/me"));
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(*r->text(), "ann");
}

// DESIGN 408: the client at its ends — a body it cannot send again, a
// challenge of an algorithm it does not have, Basic beside Digest, Digest
// of RFC 2069 (no qop), empty credentials, a default credentials struct
TEST(HttpAuthClient_Tests, Boundaries) {
    tracked_ptr seen = make_tracked<std::vector<std::string>>();
    tracked_ptr lock = make_tracked<std::mutex>();
    http::server s;
    s.route("/both", [seen, lock](http::request r, http::response_writer w) {
        auto a = r.header("Authorization");
        {
            std::lock_guard<std::mutex> g(*lock);
            seen->push_back(text(a).substr(0, 6));
        }
        if (a.empty()) {
            w.add_header("WWW-Authenticate", "Basic realm=\"b\"");
            w.add_header("WWW-Authenticate", "Digest realm=\"b\", qop=\"auth\", algorithm=MD5, nonce=\"n1\"");
            w.error(401);
            return;
        }
        w.write("in");
    });
    s.route("/unknown", [](http::request, http::response_writer w) {
        w.add_header("WWW-Authenticate", "Digest realm=\"u\", qop=\"auth\", algorithm=SHA-1, nonce=\"n\"");
        w.add_header("WWW-Authenticate", "Negotiate");
        w.error(401);
    });
    s.route("/old", [](http::request r, http::response_writer w) {
        // RFC 2069: no qop, the response of HA1:nonce:HA2
        auto ch = http::detail::parse_challenges(r.header("Authorization").view());
        if (ch.size() == 1 && ch[0].param("response") && !ch[0].param("qop") && !ch[0].param("nc")) {
            DigestInput in;
            in.algorithm = DigestAlgorithm{DigestHash::md5, false};
            in.user = "ann";
            in.realm = "old";
            in.password = "secret";
            in.method = "GET";
            in.uri = "/old";
            in.nonce = "abc";
            if (*ch[0].param("response") == http::detail::digest_response(in)) {
                w.write("old ok");
                return;
            }
        }
        w.add_header("WWW-Authenticate", "Digest realm=\"old\", nonce=\"abc\"");
        w.error(401);
    });
    s.route("/empty", [](http::request r, http::response_writer w) {
        auto b = r.basic_auth();
        if (b && b->user.empty() && b->password.empty()) {
            w.write("empty ok");
            return;
        }
        w.add_header("WWW-Authenticate", "Basic realm=\"e\"");
        w.error(401);
    });
    Running run(s);
    http::client c;
    c.credentials = http::credentials{"ann", "secret"};
    auto both = c.get(run.url("/both"));
    ASSERT_TRUE(both);
    EXPECT_EQ(*both->text(), "in");
    {
        std::lock_guard<std::mutex> g(*lock);
        ASSERT_EQ(seen->size(), 2u);
        EXPECT_EQ((*seen)[1], "Digest");   // Digest chosen over Basic
    }
    auto unknown = c.get(run.url("/unknown"));
    ASSERT_TRUE(unknown);
    EXPECT_EQ(unknown->status(), 401);   // nothing of ours offered: the 401
    (void)unknown->text();
    auto old = c.get(run.url("/old"));
    ASSERT_TRUE(old);
    EXPECT_EQ(*old->text(), "old ok");
    http::client nobody;
    nobody.credentials = http::credentials{};
    auto empty = nobody.get(run.url("/empty"));
    ASSERT_TRUE(empty);
    EXPECT_EQ(*empty->text(), "empty ok");
    // a stream body cannot go again: the 401 is the answer
    http::request streamed("POST", run.url("/both"));
    streamed.set_body(io::reader(make_tracked<io::buffer>(sgcl::string("data"))));
    http::client fresh;
    fresh.credentials = http::credentials{"ann", "secret"};
    auto refused = fresh.send(streamed);
    ASSERT_TRUE(refused);
    EXPECT_EQ(refused->status(), 401);
    (void)refused->text();
    // a client without credentials: the 401 as it came
    http::client none;
    auto plain = none.get(run.url("/both"));
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->status(), 401);
    (void)plain->text();
}
