//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: the client's proxies. NO_PROXY's matcher and the proxy's URL by
// table; the environment (each variable set and unset per test, restored
// after it); requests through a forward proxy written here
// (proxy_server.h): absolute-form for http://, CONNECT's tunnel for
// https:// with HTTP/1.1 and HTTP/2 to the origin over it, Basic
// credentials from the URL, 407 and other refusals as errors, answers that
// are not responses, the pool keyed by proxy and origin, an https:// proxy
// (TLS to the proxy, and TLS in TLS to an https:// origin), SOCKS5 proxies
// (socks5:// resolving here, socks5h:// at the proxy), redirects across
// both, the dial function given the proxy's URL, a timeout; and the client
// through a proxy written in Go (go_proxy/main.go; skipped without go).
#include "tests/types.h"
#include "tests/source_root.h"
#include "tests/net/socks5_server.h"
#include "tests/net/http/proxy_server.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <unistd.h>
#include <string>
#include <utility>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;
using sgcl_test::HttpProxyServer;
using sgcl_test::Socks5TestServer;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string body_of(const expected<net::http::response, io::error>& r) {
        if (!r) {
            return "<" + text(r.error().message()) + ">";
        }
        auto t = r->text();
        return t ? text(*t) : "<" + text(t.error().message()) + ">";
    }

    std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    net::tls::config server_tls(bool h2) {
        net::tls::config c;
        c.identities = {net::tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
        c.alpn = h2 ? vector<sgcl::string>{sgcl::string("h2"), sgcl::string("http/1.1")} : vector<sgcl::string>{sgcl::string("http/1.1")};
        return c;
    }

    crypto::x509::certificate_pool test_roots() {
        return crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
    }

    // The environment's proxy variables, all unset for the test and put
    // back as they were after it
    class ProxyEnvironment {
    public:
        ProxyEnvironment() {
            for (const char* n : Names) {
                const char* v = std::getenv(n);
                _saved.emplace_back(n, v ? std::optional<std::string>(v) : std::nullopt);
                ::unsetenv(n);
            }
        }

        ~ProxyEnvironment() {
            for (auto& [n, v] : _saved) {
                if (v) {
                    ::setenv(n.c_str(), v->c_str(), 1);
                } else {
                    ::unsetenv(n.c_str());
                }
            }
        }

        void set(const char* name, const std::string& value) {
            ::setenv(name, value.c_str(), 1);
        }

        static constexpr const char* Names[] = {"http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY", "all_proxy", "ALL_PROXY", "no_proxy", "NO_PROXY"};

    private:
        std::vector<std::pair<std::string, std::optional<std::string>>> _saved;
    };

    // An origin of the program's: GET /hi, a POST echo, the request's
    // target and Host told back, a redirect to a URL given in the query
    struct Origin {
        net::http::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        explicit Origin(const optional<net::tls::config>& tls = nullopt) {
            srv.route("GET /hi", [](net::http::request req, net::http::response_writer w) {
                w.write("hi over " + req.proto() + "\n");
            });
            srv.route("POST /echo", [](net::http::request req, net::http::response_writer w) -> async::task<> {
                auto b = co_await req.async_text();
                w.write(b ? *b : sgcl::string("<no body>"));
            });
            srv.route("GET /who", [](net::http::request req, net::http::response_writer w) {
                w.write(req.header("Host") + " " + req.header("Proxy-Authorization") + "|");
            });
            srv.route("GET /go", [](net::http::request req, net::http::response_writer w) { w.redirect(req.query("to")); });
            listener = tls ? *net::tls::listen("127.0.0.1:0", *tls) : *net::tcp::listen("127.0.0.1:0");
            serving = async::spawn(srv.async_serve(listener));
        }

        ~Origin() {
            srv.close();
            (void)serving.wait();
        }

        uint16_t port() const {
            return listener.local_endpoint().port();
        }

        sgcl::string url(const std::string& path, const char* scheme = "http", const char* host = "127.0.0.1") const {
            return sgcl::string(std::string(scheme) + "://" + host + ":" + std::to_string(port()) + path);
        }
    };

    bool matches(const char* list, const char* host, uint16_t port = 80) {
        return net::http::detail::no_proxy_matches(list, host, port);
    }

    bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
    }

    const std::string& go_proxy() {
        static std::string path = [] {
            if (!have("go")) {
                return std::string();
            }
            auto src = source_root() / "tests/net/go_proxy/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_go_proxy_http";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }
}

TEST(HttpProxyMatch_Tests, NoProxyByTable) {
    struct Case {
        const char* list;
        const char* host;
        uint16_t port;
        bool direct;
    };
    const Case cases[] = {
        {"", "example.com", 80, false},
        {"*", "example.com", 80, true},
        {"*", "10.0.0.1", 443, true},
        {"example.com", "example.com", 80, true},
        {"example.com", "www.example.com", 80, true},       // a name and every name under it
        {"example.com", "a.b.example.com", 80, true},
        {"example.com", "notexample.com", 80, false},
        {"example.com", "example.com.au", 80, false},
        {".example.com", "example.com", 80, true},          // curl: the leading dot changes nothing
        {".example.com", "www.example.com", 80, true},
        {"*.example.com", "www.example.com", 80, true},
        {"*.example.com", "example.com", 80, true},
        {"EXAMPLE.com", "www.Example.COM", 80, true},       // without regard to case
        {"example.com.", "example.com", 80, true},          // a trailing dot on either side
        {"example.com", "example.com.", 80, true},
        {"example.com:8080", "example.com", 8080, true},    // a port
        {"example.com:8080", "example.com", 80, false},
        {"localhost, 127.0.0.1", "127.0.0.1", 80, true},    // commas and spaces
        {"a.com b.com\tc.com", "c.com", 80, true},
        {",,a.com,,", "a.com", 80, true},
        {"10.0.0.1", "10.0.0.1", 80, true},                 // an address
        {"10.0.0.1", "10.0.0.2", 80, false},
        {"10.0.0.1:443", "10.0.0.1", 443, true},
        {"10.0.0.1:443", "10.0.0.1", 80, false},
        {"10.0.0.0/8", "10.200.1.1", 80, true},             // a network
        {"10.0.0.0/8", "11.0.0.1", 80, false},
        {"10.0.0.0/8", "ten.example", 80, false},           // nothing is resolved
        {"::1", "[::1]", 80, true},
        {"[::1]", "::1", 80, true},
        {"[::1]:8080", "[::1]", 8080, true},
        {"[::1]:8080", "[::1]", 80, false},
        {"2001:db8::/32", "[2001:db8::5]", 80, true},
        {"2001:db8::/32", "[2001:db9::5]", 80, false},
        {"::ffff:10.0.0.1", "10.0.0.1", 80, true},          // the same address mapped
        {"127.0.0.1", "localhost", 80, false},              // an address never matches a name
        {"localhost", "127.0.0.1", 80, false},
        {"example.com:http", "example.com", 80, false},     // a port that is not a number
        {"[::1", "::1", 80, false},
        {".", "example.com", 80, false},
        {"*.", "example.com", 80, false},
    };
    for (auto& k : cases) {
        EXPECT_EQ(matches(k.list, k.host, k.port), k.direct) << "'" << k.list << "' " << k.host << ":" << k.port;
    }
}

TEST(HttpProxyMatch_Tests, TheProxysUrl) {
    using net::http::detail::parse_proxy_url;
    using net::http::detail::ProxyKind;
    auto p = parse_proxy_url("http://proxy.example:3128");
    ASSERT_TRUE(p);
    EXPECT_EQ(p->kind, ProxyKind::http);
    EXPECT_EQ(text(p->address()), "proxy.example:3128");
    EXPECT_FALSE(p->has_credentials());
    auto bare = parse_proxy_url("proxy.example:3128");   // http:// when no scheme is written
    ASSERT_TRUE(bare);
    EXPECT_EQ(bare->kind, ProxyKind::http);
    EXPECT_EQ(text(bare->shown), "http://proxy.example:3128");
    EXPECT_EQ(parse_proxy_url("http://p")->port, 80);   // the scheme's ports
    EXPECT_EQ(parse_proxy_url("https://p")->port, 443);
    EXPECT_EQ(parse_proxy_url("socks5://p")->port, 1080);
    EXPECT_EQ(parse_proxy_url("socks5h://p")->port, 1080);
    EXPECT_EQ(parse_proxy_url("SOCKS5H://p")->kind, ProxyKind::socks5h);
    auto creds = parse_proxy_url("http://us%40er:p%3Ass@[::1]:8080/ignored?x#y");
    ASSERT_TRUE(creds);
    EXPECT_EQ(text(creds->username), "us@er");
    EXPECT_EQ(text(creds->password), "p:ss");
    EXPECT_EQ(text(creds->host), "[::1]");
    EXPECT_EQ(text(creds->address()), "[::1]:8080");
    EXPECT_EQ(text(creds->shown), "http://[::1]:8080");   // no credentials where an error shows it
    EXPECT_EQ(text(creds->basic()), "Basic dXNAZXI6cDpzcw==");
    EXPECT_EQ(parse_proxy_url("ftp://p:21").error().code(), net::errc::unsupported_scheme);
    EXPECT_EQ(parse_proxy_url("socks4://p").error().code(), net::errc::unsupported_scheme);
    EXPECT_EQ(parse_proxy_url("http://").error().code(), net::errc::invalid_url);
    EXPECT_EQ(parse_proxy_url("http://p:99999").error().code(), net::errc::invalid_url);
    EXPECT_EQ(parse_proxy_url("http://p:0").error().code(), net::errc::invalid_url);
    EXPECT_EQ(parse_proxy_url("").error().code(), net::errc::invalid_url);
    EXPECT_EQ(parse_proxy_url("socks5://").error().code(), net::errc::invalid_url);
}

TEST(HttpProxyMatch_Tests, ForUrl) {
    net::http::proxy none;
    EXPECT_TRUE(none.http.empty() && none.https.empty() && none.no_proxy.empty());
    EXPECT_FALSE(*none.for_url(net::url("http://example.com/")));
    net::http::proxy one("socks5h://u:p@127.0.0.1:1080");
    EXPECT_EQ(text(one.http), "socks5h://u:p@127.0.0.1:1080");
    EXPECT_EQ(text(one.https), "socks5h://u:p@127.0.0.1:1080");
    EXPECT_EQ(text((*one.for_url(net::url("https://example.com/")))->to_string()), "socks5h://u:p@127.0.0.1:1080");
    net::http::proxy split;
    split.http = "proxy:3128";
    split.no_proxy = "internal.example";
    EXPECT_EQ(text((*split.for_url(net::url("http://example.com/")))->to_string()), "http://proxy:3128/");
    EXPECT_FALSE(*split.for_url(net::url("https://example.com/")));     // no https proxy: direct
    EXPECT_FALSE(*split.for_url(net::url("http://a.internal.example/")));
    EXPECT_FALSE(*split.for_url(net::url("ftp://example.com/")));       // neither scheme
    split.http = "gopher://x";
    EXPECT_EQ(split.for_url(net::url("http://example.com/")).error().code(), net::errc::unsupported_scheme);
    EXPECT_FALSE(*split.for_url(net::url("http://a.internal.example/")));   // a direct one never reads the URL
}

TEST(HttpProxyEnv_Tests, TheEnvironmentAsCurlReadsIt) {
    ProxyEnvironment env;
    auto p = net::http::proxy::from_environment();
    EXPECT_TRUE(p.http.empty() && p.https.empty() && p.no_proxy.empty());

    env.set("HTTP_PROXY", "http://upper:1");   // not read: a CGI program gets a request's Proxy field so
    EXPECT_TRUE(net::http::proxy::from_environment().http.empty());
    env.set("http_proxy", "http://lower:1");
    EXPECT_EQ(text(net::http::proxy::from_environment().http), "http://lower:1");

    env.set("HTTPS_PROXY", "http://upper-s:1");
    EXPECT_EQ(text(net::http::proxy::from_environment().https), "http://upper-s:1");
    env.set("https_proxy", "http://lower-s:1");   // the lower case first
    EXPECT_EQ(text(net::http::proxy::from_environment().https), "http://lower-s:1");
    env.set("https_proxy", "");                    // empty is none: the upper case then
    EXPECT_EQ(text(net::http::proxy::from_environment().https), "http://upper-s:1");

    ::unsetenv("http_proxy");
    ::unsetenv("HTTPS_PROXY");
    env.set("ALL_PROXY", "socks5h://all:1");       // for both when their own is not set
    p = net::http::proxy::from_environment();
    EXPECT_EQ(text(p.http), "socks5h://all:1");
    EXPECT_EQ(text(p.https), "socks5h://all:1");
    env.set("all_proxy", "socks5://lower-all:1");
    env.set("https_proxy", "http://own:1");
    p = net::http::proxy::from_environment();
    EXPECT_EQ(text(p.http), "socks5://lower-all:1");
    EXPECT_EQ(text(p.https), "http://own:1");

    env.set("NO_PROXY", "upper.example");
    EXPECT_EQ(text(net::http::proxy::from_environment().no_proxy), "upper.example");
    env.set("no_proxy", "lower.example");
    EXPECT_EQ(text(net::http::proxy::from_environment().no_proxy), "lower.example");
}

TEST(HttpProxyEnv_Tests, AClientTakesTheEnvironmentWhenItIsMade) {
    ProxyEnvironment env;
    Origin origin;
    auto proxy = HttpProxyServer::start();
    env.set("http_proxy", text(proxy->url()));
    net::http::client c;   // reads it now
    EXPECT_EQ(text(c.proxy.http), text(proxy->url()));
    ::unsetenv("http_proxy");
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
    EXPECT_EQ(proxy->forwarded.load(), 1);

    net::http::client later;   // made after the variable went: direct
    EXPECT_EQ(body_of(later.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
    EXPECT_EQ(proxy->forwarded.load(), 1);

    env.set("http_proxy", text(proxy->url()));
    env.set("no_proxy", "127.0.0.1");
    net::http::client excepted;
    EXPECT_EQ(body_of(excepted.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
    EXPECT_EQ(proxy->forwarded.load(), 1);

    // download honours it as every entry point does
    env.set("no_proxy", "");
    net::http::client fetching;
    auto dir = std::filesystem::temp_directory_path() / ("sgcl_proxy_download_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    auto file = (dir / "hi.txt").string();
    auto saved = fetching.download(origin.url("/hi"), sgcl::string(file));
    ASSERT_TRUE(saved) << text(saved.error().message());
    EXPECT_EQ(slurp(file), "hi over HTTP/1.1\n");
    EXPECT_EQ(proxy->forwarded.load(), 2);
    std::filesystem::remove_all(dir);
    proxy->close();
}

TEST(HttpProxy_Tests, AbsoluteFormThroughAForwardProxy) {
    Origin origin;
    Origin other;
    auto proxy = HttpProxyServer::start();
    net::http::client c;
    c.proxy = net::http::proxy(proxy->url());
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
    EXPECT_EQ(body_of(c.post(origin.url("/echo"), "text/plain", "posted")), "posted");
    EXPECT_EQ(body_of(c.get(other.url("/who?x=1#frag"))), "127.0.0.1:" + std::to_string(other.port()) + " |");
    auto lines = proxy->lines();
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "GET http://127.0.0.1:" + std::to_string(origin.port()) + "/hi HTTP/1.1");
    EXPECT_EQ(lines[1], "POST http://127.0.0.1:" + std::to_string(origin.port()) + "/echo HTTP/1.1");
    EXPECT_EQ(lines[2], "GET http://127.0.0.1:" + std::to_string(other.port()) + "/who?x=1 HTTP/1.1");   // no fragment
    EXPECT_EQ(proxy->connections.load(), 1);   // one kept connection for every http:// origin
    EXPECT_EQ(proxy->connects.load(), 0);
    auto res = c.get(origin.url("/hi"));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->header("X-Forwarded-By"), "test-proxy");
    (void)res->text();

    // the userinfo of a target never goes on the wire
    EXPECT_EQ(body_of(c.get(sgcl::string("http://user:pw@127.0.0.1:" + std::to_string(origin.port()) + "/hi"))), "hi over HTTP/1.1\n");
    EXPECT_EQ(proxy->lines().back(), "GET http://127.0.0.1:" + std::to_string(origin.port()) + "/hi HTTP/1.1");
    proxy->close();
}

TEST(HttpProxy_Tests, BasicCredentialsFromTheUrl) {
    Origin origin;
    HttpProxyServer::Behavior b;
    b.username = "us@er";
    b.password = "p:ss";
    auto proxy = HttpProxyServer::start(b);
    net::http::client c;
    c.proxy = net::http::proxy(sgcl::string("http://us%40er:p%3Ass@127.0.0.1:" + std::to_string(proxy->port())));
    EXPECT_EQ(body_of(c.get(origin.url("/who"))), "127.0.0.1:" + std::to_string(origin.port()) + " |");   // not passed on to the origin
    EXPECT_EQ(proxy->authorization(), "Basic dXNAZXI6cDpzcw==");

    // a forwarded request without them: the 407 is the response, as in Go
    net::http::client anonymous;
    anonymous.proxy = net::http::proxy(proxy->url());
    auto refused = anonymous.get(origin.url("/hi"));
    ASSERT_TRUE(refused);
    EXPECT_EQ(refused->status(), 407);
    EXPECT_EQ(refused->header("Proxy-Authenticate"), "Basic realm=\"test\"");
    (void)refused->text();

    // a request's own Proxy-Authorization wins
    net::http::request own("GET", origin.url("/hi"));
    own.set_header("Proxy-Authorization", "Basic b3duOm93bg==");
    auto mine = c.send(own);
    ASSERT_TRUE(mine);
    EXPECT_EQ(mine->status(), 407);
    EXPECT_EQ(proxy->authorization(), "Basic b3duOm93bg==");
    (void)mine->text();
    proxy->close();
}

TEST(HttpProxy_Tests, ATunnelForHttpsWithHttp1AndHttp2) {
    for (bool h2 : {false, true}) {
        Origin origin(server_tls(h2));
        auto proxy = HttpProxyServer::start();
        net::http::client c;
        c.tls.roots = test_roots();
        c.proxy = net::http::proxy(proxy->url());
        std::string want = h2 ? "hi over HTTP/2.0\n" : "hi over HTTP/1.1\n";
        EXPECT_EQ(body_of(c.get(origin.url("/hi", "https", "localhost"))), want);
        EXPECT_EQ(body_of(c.get(origin.url("/hi", "https", "localhost"))), want);
        EXPECT_EQ(body_of(c.post(origin.url("/echo", "https", "localhost"), "text/plain", "tunnelled")), "tunnelled");
        auto lines = proxy->lines();
        ASSERT_EQ(lines.size(), 1u) << h2;   // one tunnel, kept by the pool
        EXPECT_EQ(lines[0], "CONNECT localhost:" + std::to_string(origin.port()) + " HTTP/1.1");
        EXPECT_EQ(proxy->connects.load(), 1);
        proxy->close();
    }
}

TEST(HttpProxy_Tests, TheTunnelsCredentialsAndItsRefusals) {
    Origin origin(server_tls(false));
    auto https_url = origin.url("/hi", "https", "localhost");
    HttpProxyServer::Behavior b;
    b.username = "alice";
    b.password = "pw";
    auto proxy = HttpProxyServer::start(b);
    net::http::client c;
    c.tls.roots = test_roots();
    c.proxy = net::http::proxy(sgcl::string("http://alice:pw@127.0.0.1:" + std::to_string(proxy->port())));
    EXPECT_EQ(body_of(c.get(https_url)), "hi over HTTP/1.1\n");
    EXPECT_EQ(proxy->authorization(), "Basic YWxpY2U6cHc=");

    c.proxy = net::http::proxy(sgcl::string("http://alice:wrong@127.0.0.1:" + std::to_string(proxy->port())));
    auto refused = c.get(https_url);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::proxy_auth_required);
    EXPECT_EQ(text(refused.error().message()), "GET " + text(https_url) + " (through proxy http://127.0.0.1:" + std::to_string(proxy->port()) +
                                                   ", which answered 407 Proxy Authentication Required): proxy authentication required");
    proxy->close();

    auto failing = [&](HttpProxyServer::Behavior x) {
        auto p = HttpProxyServer::start(x);
        net::http::client k;
        k.tls.roots = test_roots();
        k.proxy = net::http::proxy(p->url());
        auto r = k.get(https_url);
        p->close();
        return r ? error_code() : r.error().code();
    };
    HttpProxyServer::Behavior forbidden;
    forbidden.connect_status = 403;
    EXPECT_EQ(failing(forbidden), net::errc::proxy_refused);
    HttpProxyServer::Behavior garbage;
    garbage.connect_answer = "not a response\r\n\r\n";
    EXPECT_EQ(failing(garbage), net::errc::malformed_proxy_response);
    HttpProxyServer::Behavior extra;
    extra.connect_answer = "HTTP/1.1 200 OK\r\n\r\nearly bytes";   // nothing may come before the client speaks
    EXPECT_EQ(failing(extra), net::errc::malformed_proxy_response);
    HttpProxyServer::Behavior informational;
    informational.connect_answer = "HTTP/1.1 100 Continue\r\n\r\n";
    EXPECT_EQ(failing(informational), net::errc::malformed_proxy_response);
    HttpProxyServer::Behavior cut;
    cut.connect_answer = "HTTP/1.1 200 Conn";
    EXPECT_EQ(failing(cut), io::errc::unexpected_eof);
    HttpProxyServer::Behavior other_2xx;
    other_2xx.connect_status = 299;   // any 2xx sets the tunnel up
    EXPECT_EQ(failing(other_2xx), error_code());

    // a proxy that answers nothing: the connect's timeout
    HttpProxyServer::Behavior silent;
    silent.silent = true;
    auto quiet = HttpProxyServer::start(silent);
    net::http::client k;
    k.connect_timeout = 200ms;
    k.proxy = net::http::proxy(quiet->url());
    auto t0 = std::chrono::steady_clock::now();
    auto timed = k.get(https_url);
    ASSERT_FALSE(timed);
    EXPECT_TRUE(timed.error().is_timeout()) << text(timed.error().message());
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 5s);
    quiet->close();
}

TEST(HttpProxy_Tests, AnHttpsProxy) {
    Origin plain;
    Origin secure(server_tls(true));
    auto proxy = HttpProxyServer::start(HttpProxyServer::Behavior(), *net::tls::listen("127.0.0.1:0", server_tls(false)));
    net::http::client c;
    c.tls.roots = test_roots();
    c.proxy = net::http::proxy(proxy->url("https", "localhost"));
    EXPECT_EQ(body_of(c.get(plain.url("/hi"))), "hi over HTTP/1.1\n");                           // forwarded over TLS to the proxy
    EXPECT_EQ(body_of(c.get(secure.url("/hi", "https", "localhost"))), "hi over HTTP/2.0\n");     // TLS in TLS
    EXPECT_EQ(proxy->forwarded.load(), 1);
    EXPECT_EQ(proxy->connects.load(), 1);

    // the proxy's certificate is verified as an origin's
    net::http::client strict;
    strict.proxy = net::http::proxy(proxy->url("https", "localhost"));
    auto refused = strict.get(plain.url("/hi"));
    ASSERT_FALSE(refused);
    EXPECT_NE(text(refused.error().message()).find("(through proxy https://localhost:"), std::string::npos) << text(refused.error().message());
    proxy->close();
}

TEST(HttpProxy_Tests, Socks5Proxies) {
    Origin origin;
    Origin secure(server_tls(true));
    auto socks = Socks5TestServer::start();
    net::http::client c;
    c.tls.roots = test_roots();
    c.proxy = net::http::proxy(sgcl::string("socks5h://" + text(socks->address())));
    EXPECT_EQ(body_of(c.get(origin.url("/hi", "http", "localhost"))), "hi over HTTP/1.1\n");
    {
        std::lock_guard g(socks->lock);
        EXPECT_EQ(socks->last_atyp, 3);   // the name, for the proxy to resolve
        EXPECT_EQ(socks->last_host, "localhost");
    }
    c.proxy = net::http::proxy(sgcl::string("socks5://" + text(socks->address())));
    EXPECT_EQ(body_of(c.get(origin.url("/hi", "http", "localhost"))), "hi over HTTP/1.1\n");
    {
        std::lock_guard g(socks->lock);
        EXPECT_NE(socks->last_atyp, 3);   // resolved here: an address sent
    }
    EXPECT_EQ(body_of(c.get(secure.url("/hi", "https", "localhost"))), "hi over HTTP/2.0\n");
    EXPECT_EQ(body_of(c.get(secure.url("/hi", "https", "localhost"))), "hi over HTTP/2.0\n");
    EXPECT_EQ(socks->tunnels.load(), 3);   // the HTTP/2 connection shared
    socks->close();

    Socks5TestServer::Behavior b;
    b.username = "sam";
    b.password = "pw";
    auto authed = Socks5TestServer::start(b);
    c.proxy = net::http::proxy(sgcl::string("socks5h://sam:pw@" + text(authed->address())));
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
    c.proxy = net::http::proxy(sgcl::string("socks5h://sam:no@" + text(authed->address())));
    auto refused = c.get(origin.url("/hi"));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::proxy_auth_required);
    EXPECT_NE(text(refused.error().message()).find("(through proxy socks5h://" + text(authed->address()) + ")"), std::string::npos);
    authed->close();
}

TEST(HttpProxy_Tests, ThePoolKeepsRoutesApart) {
    Origin origin;
    auto first = HttpProxyServer::start();
    auto second = HttpProxyServer::start();
    net::http::client c;
    c.proxy = net::http::proxy();
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");   // direct: pooled under the origin
    c.proxy = net::http::proxy(first->url());
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");   // not the direct connection
    EXPECT_EQ(first->forwarded.load(), 1);
    c.proxy = net::http::proxy(second->url());
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
    EXPECT_EQ(second->forwarded.load(), 1);
    c.proxy = net::http::proxy(first->url());
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
    EXPECT_EQ(first->connections.load(), 1);   // its own kept connection again
    c.proxy = net::http::proxy(sgcl::string("http://other:creds@127.0.0.1:" + std::to_string(first->port())));
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
    EXPECT_EQ(first->connections.load(), 2);   // other credentials, other connections
    first->close();
    second->close();
}

TEST(HttpProxy_Tests, RedirectsAcrossSchemes) {
    Origin secure(server_tls(false));
    Origin plain;
    auto proxy = HttpProxyServer::start();
    net::http::client c;
    c.tls.roots = test_roots();
    c.proxy = net::http::proxy(proxy->url());
    auto to = secure.url("/hi", "https", "localhost");
    auto res = c.get(sgcl::string(text(plain.url("/go")) + "?to=" + text(to)));
    EXPECT_EQ(body_of(res), "hi over HTTP/1.1\n");
    auto lines = proxy->lines();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].substr(0, 11), "GET http://");   // forwarded
    EXPECT_EQ(lines[1].substr(0, 8), "CONNECT ");        // then a tunnel
    proxy->close();
}

TEST(HttpProxy_Tests, TheDialFunctionIsGivenTheProxy) {
    Origin origin;
    auto proxy = HttpProxyServer::start();
    net::http::client c;
    c.proxy = net::http::proxy(sgcl::string("http://proxy.invalid:" + std::to_string(proxy->port())));
    tracked_ptr<std::string> asked = make_tracked<std::string>();
    uint16_t port = proxy->port();
    c.dial = [asked, port](const net::url& u, async::stop_token) -> async::task<expected<net::connection, io::error>> {
        *asked = std::string(u.to_string().view());
        co_return co_await net::tcp::async_connect(net::endpoint(net::ip_address::loopback_v4(), port));
    };
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
    EXPECT_EQ(*asked, "http://proxy.invalid:" + std::to_string(port) + "/");
    proxy->close();
}

TEST(HttpProxy_Tests, ErrorsOfTheProxyItself) {
    Origin origin;
    net::http::client c;
    auto gone = HttpProxyServer::start();
    auto port = gone->port();
    gone->close();
    c.proxy = net::http::proxy(sgcl::string("http://u:secret@127.0.0.1:" + std::to_string(port)));
    auto down = c.get(origin.url("/hi"));
    ASSERT_FALSE(down);
    EXPECT_EQ(down.error().code(), std::errc::connection_refused);
    EXPECT_EQ(text(down.error().message()), "GET " + text(origin.url("/hi")) + " (through proxy http://127.0.0.1:" + std::to_string(port) +
                                                "): Connection refused");   // never the password
    c.proxy = net::http::proxy("ftp://u:secret@proxy:21");
    auto scheme = c.get(origin.url("/hi"));
    ASSERT_FALSE(scheme);
    EXPECT_EQ(scheme.error().code(), net::errc::unsupported_scheme);
    EXPECT_EQ(text(scheme.error().message()), "GET " + text(origin.url("/hi")) + " (proxy ftp://***@proxy:21): unsupported protocol scheme");
    c.proxy = net::http::proxy("http://[bad");
    auto bad = c.get(origin.url("/hi"));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), net::errc::invalid_url);
    c.proxy.no_proxy = "*";   // a direct request never reads the proxy's URL
    EXPECT_EQ(body_of(c.get(origin.url("/hi"))), "hi over HTTP/1.1\n");
}

TEST(HttpProxy_Tests, TheTaskForms) {
    Origin origin;
    auto proxy = HttpProxyServer::start();
    net::http::client c;
    c.proxy = net::http::proxy(proxy->url());
    auto run = [](net::http::client c, sgcl::string url) -> async::task<std::string> {
        auto res = co_await c.async_get(url);
        if (!res) {
            co_return text(res.error().message());
        }
        auto t = co_await res->async_text();
        co_return t ? text(*t) : std::string("<read>");
    };
    EXPECT_EQ(async::spawn(run(c, origin.url("/hi"))).wait(), "hi over HTTP/1.1\n");
    EXPECT_EQ(proxy->forwarded.load(), 1);
    proxy->close();
}

// The client through an HTTP proxy written in Go (go_proxy http): forwarded
// requests, CONNECT, Basic credentials; and Go's own client through the
// proxy of these tests
TEST(HttpProxyGo_Tests, TheClientThroughGosProxy) {
    if (go_proxy().empty()) {
        GTEST_SKIP() << "no go to build the helper with";
    }
    Origin origin;
    Origin secure(server_tls(true));
    FILE* p = popen(("'" + go_proxy() + "' http alice pw").c_str(), "r");
    ASSERT_TRUE(p);
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof(line), p));
    int port = std::atoi(line + 5);
    ASSERT_GT(port, 0);
    std::string at = "127.0.0.1:" + std::to_string(port);
    net::http::client c;
    c.tls.roots = test_roots();
    c.proxy = net::http::proxy(sgcl::string("http://alice:pw@" + at));
    auto res = c.get(origin.url("/hi"));
    ASSERT_TRUE(res) << text(res.error().message());
    EXPECT_EQ(res->header("X-Forwarded-By"), "go_proxy");
    EXPECT_EQ(body_of(res), "hi over HTTP/1.1\n");
    EXPECT_EQ(body_of(c.post(origin.url("/echo"), "text/plain", "through go")), "through go");
    EXPECT_EQ(body_of(c.get(secure.url("/hi", "https", "localhost"))), "hi over HTTP/2.0\n");
    c.proxy = net::http::proxy(sgcl::string("http://alice:no@" + at));
    auto refused = c.get(secure.url("/hi", "https", "localhost"));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::proxy_auth_required);
    auto forwarded_407 = c.get(origin.url("/hi"));
    ASSERT_TRUE(forwarded_407);
    EXPECT_EQ(forwarded_407->status(), 407);
    (void)forwarded_407->text();
    net::http::client quit;
    quit.proxy = net::http::proxy(sgcl::string("http://" + at));
    (void)quit.get("http://quit.invalid/");   // the helper's way out
    pclose(p);

    // Go's net/http through the proxy of these tests: forwarded and tunnelled
    auto proxy = HttpProxyServer::start();
    FILE* g = popen(("'" + go_proxy() + "' get " + text(proxy->url()) + " " + text(origin.url("/hi"))).c_str(), "r");
    char out[256] = {};
    size_t n = fread(out, 1, sizeof(out) - 1, g);
    pclose(g);
    EXPECT_EQ(std::string(out, n), "200 hi over HTTP/1.1\n");
    EXPECT_EQ(proxy->lines().back(), "GET " + text(origin.url("/hi")) + " HTTP/1.1");
    FILE* t = popen(("'" + go_proxy() + "' get " + text(proxy->url()) + " " + text(secure.url("/hi", "https", "localhost")) + " " + testdata("ca.pem")).c_str(), "r");
    n = fread(out, 1, sizeof(out) - 1, t);
    pclose(t);
    EXPECT_EQ(std::string(out, n), "200 hi over HTTP/2.0\n");
    EXPECT_EQ(proxy->lines().back(), "CONNECT localhost:" + std::to_string(secure.port()) + " HTTP/1.1");
    proxy->close();
}
