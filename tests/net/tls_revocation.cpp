//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Revocation in TLS (config::revocation, identity::set_ocsp_staple,
// config::ocsp_stapling): the chain of tests/crypto/data/revocation (make.sh)
// — a root, an intermediate whose AIA and CRL distribution point and its
// leaves' point at the loopback, 127.0.0.1:47811 for OCSP and :47812 for
// the CRLs — served by
//
//   - our own server, the staple given or fetched and refreshed, to our
//     client over TLS 1.3, and to Go's crypto/tls client
//     (tools/tls_ocsp_oracle.go -client);
//   - OpenSSL's s_server -status_file, TLS 1.3 and TLS 1.2
//     (CertificateStatus), and Go's crypto/tls server with OCSPStaple, both
//     versions;
//
// with the responder OpenSSL's `openssl ocsp -port 47811` over the
// fixtures' index, and the CRLs served from here (a server of HTTP/1.1 on
// :47812 that can also take a request and never answer). Every policy:
// off, staple_only, soft_fail, hard_fail; good, revoked, unknown, a staple
// that does not verify, Must-Staple with and without a staple, the CRLs of
// the program's and of the distribution points, the cache, the timeout, a
// client certificate revoked. The ports are fixed by the certificates: the
// tests that need them skip when another program holds one.
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/io/exec.h"
#include "sgcl/net/http.h"
#include "sgcl/net/tls.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace tls = sgcl::net::tls;
namespace x509 = sgcl::crypto::x509;

namespace {
    std::string rdir() {
        return (source_root() / "tests/crypto/data/revocation/").string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    std::string openssl_path() {
        if (const char* e = std::getenv("SGCL_OPENSSL")) {
            return e;
        }
        const char* p = "/opt/homebrew/opt/openssl@3/bin/openssl";
        return ::access(p, X_OK) == 0 ? p : "";
    }

    std::string go_path() {
        for (const char* p : {"/opt/homebrew/bin/go", "/usr/local/go/bin/go", "/usr/local/bin/go"}) {
            if (::access(p, X_OK) == 0) {
                return p;
            }
        }
        return "";
    }

    std::string scratch() {
        static int n = 0;
        auto d = std::filesystem::temp_directory_path() / ("sgcl_revocation_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
        std::filesystem::create_directories(d);
        return d.string();
    }

    uint16_t free_port() {
        auto l = net::tcp::listen("127.0.0.1:0");
        EXPECT_TRUE(l.has_value());
        uint16_t p = l->local_endpoint().port();
        (void)l->close();
        return p;
    }

    bool port_free(uint16_t p) {
        auto l = net::tcp::listen(sgcl::string("127.0.0.1:" + std::to_string(p)));
        if (!l) {
            return false;
        }
        (void)l->close();
        return true;
    }

    // A child whose output goes to a file, read as it grows
    struct Child {
        io::command cmd;
        std::string log;
        bool started = false;

        Child(const std::string& program, sgcl::vector<sgcl::string> args)
        : cmd(sgcl::string(program), std::move(args)) {
        }

        bool start() {
            log = scratch() + "/out.log";
            auto f = io::create(sgcl::string(log));
            if (!f) {
                return false;
            }
            cmd.out = *f;
            cmd.err = *f;
            started = (bool)cmd.start();
            (void)f->close();
            return started;
        }

        std::string wait_for(const std::string& what, int ms = 10000) {
            for (int i = 0; i < ms / 10; ++i) {
                std::string s = slurp(log);
                if (s.find(what) != std::string::npos) {
                    return s;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return "";
        }

        void finish() {
            if (started) {
                (void)cmd.process.kill();
                (void)cmd.wait();
                started = false;
            }
        }

        ~Child() {
            finish();
        }
    };

    // OpenSSL's responder over the fixtures' index, on the AIA's port
    struct Responder : Child {
        Responder()
        : Child(openssl_path(), {sgcl::string("ocsp"), sgcl::string("-index"), sgcl::string(rdir() + "index.txt"), sgcl::string("-port"), sgcl::string("47811"),
                                 sgcl::string("-rsigner"), sgcl::string(rdir() + "responder.pem"), sgcl::string("-rkey"), sgcl::string(rdir() + "responder.key"),
                                 sgcl::string("-CA"), sgcl::string(rdir() + "int.pem"), sgcl::string("-ndays"), sgcl::string("1"), sgcl::string("-ignore_err")}) {
        }

        // up once it says it waits: a connection opened and closed to see
        // whether it listens leaves OpenSSL's responder stuck on it
        bool up() {
            return start() && !wait_for("waiting for OCSP client connections").empty();
        }
    };

    // The CRLs and the issuer's certificate over HTTP/1.1, on the
    // distribution points' port: GET of a path gives the file, anything
    // else a 404; `silent` takes requests and never answers. Counts what
    // it served
    struct FileServer {
        net::listener listener;
        std::atomic<int> requests{0};
        std::atomic<bool> silent{false};

        static sgcl::tracked_ptr<FileServer> start(uint16_t port = 47812) {
            sgcl::tracked_ptr s = make_tracked<FileServer>();
            auto l = net::tcp::listen(sgcl::string("127.0.0.1:" + std::to_string(port)));
            if (!l) {
                return s;
            }
            s->listener = *l;
            async::go(loop(s));
            return s;
        }

        bool ok() const {
            return (bool)listener;
        }

        void close() {
            if (listener) {
                (void)listener.close();
            }
        }

        static async::task<> loop(sgcl::tracked_ptr<FileServer> s) {
            for (;;) {
                auto c = co_await s->listener.async_accept();
                if (!c) {
                    co_return;
                }
                async::go(serve(s, *c));
            }
        }

        static async::task<> serve(sgcl::tracked_ptr<FileServer> s, net::connection c) {
            auto line = co_await c.async_read_line();
            if (!line || !*line) {
                (void)c.close();
                co_return;
            }
            ++s->requests;
            std::string request(line->value().view());
            for (;;) {   // the rest of the head
                auto h = co_await c.async_read_line();
                if (!h || !*h || h->value().empty() || h->value() == "\r") {
                    break;
                }
            }
            if (s->silent) {
                (void)co_await c.async_read_all();
                (void)c.close();
                co_return;
            }
            std::string path;
            if (request.rfind("GET /", 0) == 0) {
                path = request.substr(5, request.find(' ', 5) - 5);
            }
            std::string body = path.empty() || path.find('/') != std::string::npos ? std::string() : slurp(rdir() + path);
            std::string head = body.empty() ? "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
                                            : "HTTP/1.1 200 OK\r\nContent-Type: application/pkix-crl\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n";
            (void)co_await c.async_write(sgcl::string(head + body));
            (void)c.close();
        }
    };

    tls::identity identity_of(const std::string& leaf) {
        return tls::identity(sgcl::string(slurp(rdir() + leaf + ".pem") + slurp(rdir() + "int.pem")), crypto::read_secret(sgcl::string(rdir() + leaf + ".key")).value());
    }

    sgcl::vector<sgcl::byte> der_of(const std::string& name) {
        std::string s = slurp(rdir() + name);
        return sgcl::vector<sgcl::byte>(reinterpret_cast<const sgcl::byte*>(s.data()), reinterpret_cast<const sgcl::byte*>(s.data()) + s.size());
    }

    x509::revocation_list crl_of(const std::string& name) {
        return x509::revocation_list::parse(der_of(name).as_slice()).value();
    }

    tls::config client_config(tls::revocation_mode mode) {
        tls::config c;
        c.roots = x509::certificate_pool::from_pem(sgcl::string(slurp(rdir() + "root.pem")));
        c.server_name = sgcl::string("localhost");
        c.revocation = mode;
        c.revocation_cache = tls::revocation_cache();   // a test's own: nothing of another test's
        return c;
    }

    // Our server of one identity (its staple as given), its address
    struct Server {
        net::listener listener;
        sgcl::string address;
    };

    Server serve(const tls::identity& id, bool stapling = false) {
        tls::config s;
        s.identities = {id};
        s.ocsp_stapling = stapling;
        Server out;
        auto l = tls::listen("127.0.0.1:0", s);
        EXPECT_TRUE(l.has_value());
        out.listener = *l;
        out.address = "127.0.0.1:" + sgcl::to_string(out.listener.local_endpoint().port());
        return out;
    }

    // A connection's revocation, as state_of says it
    struct Seen {
        bool connected = false;
        std::string error;
        optional<x509::reason> reason;
        optional<x509::revocation_status> status;
        tls::revocation_source source = tls::revocation_source::none;
    };

    Seen dial(const sgcl::string& address, const tls::config& c) {
        Seen s;
        auto conn = tls::connect(address, c);
        if (!conn) {
            s.error = std::string(conn.error().message().view());
            s.reason = tls::certificate_reason(conn.error());
            return s;
        }
        s.connected = true;
        auto st = tls::state_of(*conn);
        s.status = st->revocation;
        s.source = st->revocation_source;
        (void)conn->close();
        return s;
    }

}

// The staple API of an identity: checked against its leaf, read back,
// cleared
TEST(TlsRevocation, IdentityStaple) {
    auto id = identity_of("good");
    EXPECT_TRUE(id.ocsp_staple().empty());
    auto good = der_of("ocsp_good.der");
    ASSERT_TRUE(id.set_ocsp_staple(good.as_slice()).has_value());
    EXPECT_EQ(id.ocsp_staple(), good);
    auto copy = id;
    EXPECT_EQ(copy.ocsp_staple(), good);   // copies share it
    // another leaf's response, one the issuer did not authorize, garbage
    auto other = id.set_ocsp_staple(der_of("ocsp_revoked.der").as_slice());
    ASSERT_FALSE(other.has_value());
    EXPECT_EQ(other.error().code(), crypto::errc::verification);
    EXPECT_FALSE(id.set_ocsp_staple(der_of("ocsp_good_rogue.der").as_slice()).has_value());
    auto broken = id.set_ocsp_staple(der_of("good.pem").as_slice());
    ASSERT_FALSE(broken.has_value());
    EXPECT_EQ(broken.error().code(), crypto::errc::malformed);
    EXPECT_EQ(id.ocsp_staple(), good);   // a refused one leaves the old
    ASSERT_TRUE(id.set_ocsp_staple(sgcl::slice<const sgcl::byte>()).has_value());
    EXPECT_TRUE(id.ocsp_staple().empty());
    // a revoked leaf staples its revocation
    auto revoked = identity_of("revoked");
    EXPECT_TRUE(revoked.set_ocsp_staple(der_of("ocsp_revoked.der").as_slice()).has_value());
}

// staple_only against our server: a good staple, a revoked one, none, and
// Must-Staple with and without
TEST(TlsRevocation, StapleOnlyOwnServer) {
    auto id = identity_of("good");
    ASSERT_TRUE(id.set_ocsp_staple(der_of("ocsp_good.der").as_slice()).has_value());
    auto s = serve(id);
    auto seen = dial(s.address, client_config(tls::revocation_mode::staple_only));
    ASSERT_TRUE(seen.connected) << seen.error;
    ASSERT_TRUE(seen.status.has_value());
    EXPECT_EQ(*seen.status, x509::revocation_status::good);
    EXPECT_EQ(seen.source, tls::revocation_source::staple);
    // off: nothing asked, nothing checked
    auto off = dial(s.address, client_config(tls::revocation_mode::off));
    ASSERT_TRUE(off.connected);
    EXPECT_FALSE(off.status.has_value());
    EXPECT_EQ(off.source, tls::revocation_source::none);
    (void)s.listener.close();

    // revoked: the client refuses with certificate_revoked
    auto rid = identity_of("revoked");
    ASSERT_TRUE(rid.set_ocsp_staple(der_of("ocsp_revoked.der").as_slice()).has_value());
    auto r = serve(rid);
    auto rs = dial(r.address, client_config(tls::revocation_mode::staple_only));
    EXPECT_FALSE(rs.connected);
    ASSERT_TRUE(rs.reason.has_value()) << rs.error;
    EXPECT_EQ(*rs.reason, x509::reason::revoked);
    EXPECT_NE(rs.error.find("tls: certificate is revoked"), std::string::npos) << rs.error;
    // off does not look
    EXPECT_TRUE(dial(r.address, client_config(tls::revocation_mode::off)).connected);
    (void)r.listener.close();

    // no staple: staple_only takes the chain, the status unknown
    auto plain = serve(identity_of("good"));
    auto ps = dial(plain.address, client_config(tls::revocation_mode::staple_only));
    ASSERT_TRUE(ps.connected) << ps.error;
    ASSERT_TRUE(ps.status.has_value());
    EXPECT_EQ(*ps.status, x509::revocation_status::unknown);
    EXPECT_EQ(ps.source, tls::revocation_source::none);
    (void)plain.listener.close();

    // Must-Staple: without the staple refused, with it taken
    auto mid = identity_of("staple");
    auto m = serve(mid);
    auto ms = dial(m.address, client_config(tls::revocation_mode::staple_only));
    EXPECT_FALSE(ms.connected);
    ASSERT_TRUE(ms.reason.has_value());
    EXPECT_EQ(*ms.reason, x509::reason::revocation_unknown);
    ASSERT_TRUE(mid.set_ocsp_staple(der_of("ocsp_staple.der").as_slice()).has_value());
    auto ms2 = dial(m.address, client_config(tls::revocation_mode::staple_only));
    ASSERT_TRUE(ms2.connected) << ms2.error;
    EXPECT_EQ(ms2.source, tls::revocation_source::staple);
    // Must-Staple is the policy's: off connects without one
    ASSERT_TRUE(mid.set_ocsp_staple(sgcl::slice<const sgcl::byte>()).has_value());
    EXPECT_TRUE(dial(m.address, client_config(tls::revocation_mode::off)).connected);
    (void)m.listener.close();
}

// The program's CRLs, offline: the revoked leaf refused by int.crl, the
// held one by the list and good again by its delta, the intermediate by
// root.crl; a status they cannot give (a list of another issuer) unknown
TEST(TlsRevocation, ProgramCrls) {
    auto r = serve(identity_of("revoked"));
    auto c = client_config(tls::revocation_mode::staple_only);
    c.crls = {crl_of("int.crl"), crl_of("root.crl")};
    auto seen = dial(r.address, c);
    EXPECT_FALSE(seen.connected);
    ASSERT_TRUE(seen.reason.has_value());
    EXPECT_EQ(*seen.reason, x509::reason::revoked);
    (void)r.listener.close();

    auto g = serve(identity_of("good"));
    auto gs = dial(g.address, c);
    ASSERT_TRUE(gs.connected) << gs.error;
    EXPECT_EQ(*gs.status, x509::revocation_status::good);
    EXPECT_EQ(gs.source, tls::revocation_source::crl);
    // without root's list the intermediate is unknown, which staple_only takes
    c.crls = {crl_of("int.crl")};
    auto gu = dial(g.address, c);
    ASSERT_TRUE(gu.connected);
    EXPECT_EQ(*gu.status, x509::revocation_status::good);   // the leaf's
    EXPECT_EQ(gu.source, tls::revocation_source::crl);
    // hard_fail wants both, and has no network to ask: refused
    auto hard = client_config(tls::revocation_mode::hard_fail);
    hard.crls = {crl_of("int.crl")};
    hard.fetch_crls = false;
    hard.revocation_timeout = sgcl::duration(std::chrono::milliseconds(1));
    auto hs = dial(g.address, hard);
    if (port_free(47811)) {   // nothing answers on the responder's port
        EXPECT_FALSE(hs.connected);
        ASSERT_TRUE(hs.reason.has_value());
        EXPECT_EQ(*hs.reason, x509::reason::revocation_unknown);
    }
    (void)g.listener.close();

    auto h = serve(identity_of("held"));
    c.crls = {crl_of("int.crl"), crl_of("root.crl")};
    EXPECT_FALSE(dial(h.address, c).connected);
    c.crls = {crl_of("int.crl"), crl_of("root.crl"), crl_of("int_delta.crl")};
    auto hd = dial(h.address, c);
    EXPECT_TRUE(hd.connected) << hd.error;
    (void)h.listener.close();
}

// soft_fail and hard_fail online: OpenSSL's responder for the leaves, the
// CRLs of the distribution points for the intermediate and for a leaf
// without OCSP, the cache, a responder that is down
TEST(TlsRevocation, OnlineChecks) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no openssl (its ocsp responder is the oracle)";
    }
    if (!port_free(47811) || !port_free(47812)) {
        GTEST_SKIP() << "the ports of the fixtures' AIA and CRL DP, 47811 and 47812, are taken";
    }
    Responder responder;
    ASSERT_TRUE(responder.up()) << slurp(responder.log);
    auto files = FileServer::start();
    ASSERT_TRUE(files->ok());

    auto g = serve(identity_of("good"));
    auto soft = client_config(tls::revocation_mode::soft_fail);
    auto seen = dial(g.address, soft);
    ASSERT_TRUE(seen.connected) << seen.error;
    EXPECT_EQ(*seen.status, x509::revocation_status::good);
    EXPECT_EQ(seen.source, tls::revocation_source::ocsp);
    EXPECT_GE(files->requests.load(), 1);   // root.crl, for the intermediate
    auto hard = client_config(tls::revocation_mode::hard_fail);
    auto hs = dial(g.address, hard);
    ASSERT_TRUE(hs.connected) << hs.error;
    EXPECT_EQ(*hs.status, x509::revocation_status::good);
    // revoked, by OCSP
    auto r = serve(identity_of("revoked"));
    auto rs = dial(r.address, client_config(tls::revocation_mode::soft_fail));
    EXPECT_FALSE(rs.connected);
    ASSERT_TRUE(rs.reason.has_value());
    EXPECT_EQ(*rs.reason, x509::reason::revoked);
    (void)r.listener.close();
    // a leaf without AIA: its CRL
    auto c = serve(identity_of("crlonly"));
    auto cs = dial(c.address, client_config(tls::revocation_mode::hard_fail));
    ASSERT_TRUE(cs.connected) << cs.error;
    EXPECT_EQ(cs.source, tls::revocation_source::crl);
    (void)c.listener.close();
    // a leaf the responder does not know (unknown): the CRL decides
    auto u = serve(identity_of("unknown"));
    auto us = dial(u.address, client_config(tls::revocation_mode::hard_fail));
    ASSERT_TRUE(us.connected) << us.error;
    EXPECT_EQ(us.source, tls::revocation_source::crl);
    (void)u.listener.close();
    // a good staple needs no network for the leaf
    auto sid = identity_of("good");
    ASSERT_TRUE(sid.set_ocsp_staple(der_of("ocsp_good.der").as_slice()).has_value());
    auto st = serve(sid);
    auto ss = dial(st.address, client_config(tls::revocation_mode::hard_fail));
    ASSERT_TRUE(ss.connected) << ss.error;
    EXPECT_EQ(ss.source, tls::revocation_source::staple);
    (void)st.listener.close();
    // the cache: the answers kept, then the responder no longer needed
    auto cache = tls::revocation_cache();
    soft.revocation_cache = cache;
    ASSERT_TRUE(dial(g.address, soft).connected);
    EXPECT_GE(cache.size(), 2u);   // the leaf's OCSP answer and root's CRL
    responder.finish();
    int before = files->requests.load();
    auto cached = dial(g.address, soft);
    ASSERT_TRUE(cached.connected) << cached.error;
    EXPECT_EQ(*cached.status, x509::revocation_status::good);
    EXPECT_EQ(cached.source, tls::revocation_source::ocsp);
    EXPECT_EQ(files->requests.load(), before);
    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
    // the responder down, the leaf has a CRL to fall back on
    auto down = dial(g.address, soft);
    ASSERT_TRUE(down.connected) << down.error;
    EXPECT_EQ(down.source, tls::revocation_source::crl);
    EXPECT_EQ(*down.status, x509::revocation_status::good);
    auto rdown = serve(identity_of("revoked"));
    auto rds = dial(rdown.address, client_config(tls::revocation_mode::soft_fail));   // revoked, by the CRL
    EXPECT_FALSE(rds.connected);
    ASSERT_TRUE(rds.reason.has_value());
    EXPECT_EQ(*rds.reason, x509::reason::revoked);
    (void)rdown.listener.close();
    // and without fetching CRLs: soft takes it unknown, hard refuses
    soft.fetch_crls = false;
    auto unknown = dial(g.address, soft);
    ASSERT_TRUE(unknown.connected) << unknown.error;
    EXPECT_EQ(*unknown.status, x509::revocation_status::unknown);
    EXPECT_EQ(unknown.source, tls::revocation_source::none);
    hard.fetch_crls = false;
    auto refused = dial(g.address, hard);
    EXPECT_FALSE(refused.connected);
    ASSERT_TRUE(refused.reason.has_value());
    EXPECT_EQ(*refused.reason, x509::reason::revocation_unknown);
    (void)g.listener.close();
    files->close();
}

// The timeout of the online checks: a responder and a CRL server that take
// the request and never answer
TEST(TlsRevocation, OnlineTimeout) {
    if (!port_free(47811) || !port_free(47812)) {
        GTEST_SKIP() << "the ports of the fixtures' AIA and CRL DP, 47811 and 47812, are taken";
    }
    auto ocsp = FileServer::start(47811);
    auto files = FileServer::start(47812);
    ASSERT_TRUE(ocsp->ok() && files->ok());
    ocsp->silent = true;
    files->silent = true;
    auto g = serve(identity_of("good"));
    auto soft = client_config(tls::revocation_mode::soft_fail);
    soft.revocation_timeout = sgcl::duration(std::chrono::milliseconds(300));
    auto t0 = std::chrono::steady_clock::now();
    auto seen = dial(g.address, soft);
    auto took = std::chrono::steady_clock::now() - t0;
    ASSERT_TRUE(seen.connected) << seen.error;
    EXPECT_EQ(*seen.status, x509::revocation_status::unknown);
    EXPECT_LT(took, std::chrono::seconds(3));
    EXPECT_GE(ocsp->requests.load(), 1);
    auto hard = client_config(tls::revocation_mode::hard_fail);
    hard.revocation_timeout = sgcl::duration(std::chrono::milliseconds(300));
    auto hs = dial(g.address, hard);
    EXPECT_FALSE(hs.connected);
    ASSERT_TRUE(hs.reason.has_value());
    EXPECT_EQ(*hs.reason, x509::reason::revocation_unknown);
    // the handshake's timeout bounds it too
    hard.revocation_timeout = sgcl::duration(std::chrono::seconds(30));
    hard.handshake_timeout = sgcl::duration(std::chrono::milliseconds(500));
    t0 = std::chrono::steady_clock::now();
    EXPECT_FALSE(dial(g.address, hard).connected);
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::seconds(5));
    (void)g.listener.close();
    ocsp->close();
    files->close();
}

// The server fetches its staple from the responder and refreshes it
// (config::ocsp_stapling); the client sees it as the staple
TEST(TlsRevocation, ServerFetchesItsStaple) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no openssl (its ocsp responder is the oracle)";
    }
    if (!port_free(47811)) {
        GTEST_SKIP() << "the port of the fixtures' AIA, 47811, is taken";
    }
    Responder responder;
    ASSERT_TRUE(responder.up()) << slurp(responder.log);
    auto id = identity_of("staple");
    auto s = serve(id, true);
    for (int i = 0; i < 500 && id.ocsp_staple().empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_FALSE(id.ocsp_staple().empty());
    auto st = x509::ocsp_response::parse(id.ocsp_staple().as_slice());
    ASSERT_TRUE(st.has_value());
    auto chain = id.certificates();
    EXPECT_TRUE(st->verify(chain[0], x509::certificate::from_pem(sgcl::string(slurp(rdir() + "int.pem"))).value()).has_value());
    auto seen = dial(s.address, client_config(tls::revocation_mode::staple_only));
    ASSERT_TRUE(seen.connected) << seen.error;
    EXPECT_EQ(seen.source, tls::revocation_source::staple);
    (void)s.listener.close();
}

// OpenSSL's s_server -status_file, TLS 1.3 and TLS 1.2: a good staple, a
// revoked one, one of a signer the issuer never named (s_server sends no
// response of another certificate's CertID: that case is the crypto tests')
TEST(TlsRevocation, OpenSslServerStaples) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no openssl";
    }
    struct Case {
        const char* leaf;
        const char* staple;
        bool tls12;
        bool connects;
        x509::reason reason;
    };
    const Case cases[] = {
        {"good", "ocsp_good.der", false, true, x509::reason::none},
        {"good", "ocsp_good.der", true, true, x509::reason::none},
        {"revoked", "ocsp_revoked.der", false, false, x509::reason::revoked},
        {"revoked", "ocsp_revoked.der", true, false, x509::reason::revoked},
        {"good", "ocsp_good_rogue.der", false, false, x509::reason::revocation_unknown},
        {"good", "ocsp_good_rogue.der", true, false, x509::reason::revocation_unknown},
    };
    for (const auto& k : cases) {
        uint16_t port = free_port();
        sgcl::vector<sgcl::string> args = {sgcl::string("s_server"), sgcl::string("-accept"), sgcl::string("127.0.0.1:" + std::to_string(port)),
                                           sgcl::string("-cert"), sgcl::string(rdir() + k.leaf + ".pem"), sgcl::string("-key"), sgcl::string(rdir() + k.leaf + ".key"),
                                           sgcl::string("-cert_chain"), sgcl::string(rdir() + "int.pem"), sgcl::string("-status_file"), sgcl::string(rdir() + k.staple),
                                           sgcl::string("-naccept"), sgcl::string("1"), sgcl::string("-rev")};
        args.push_back(sgcl::string(k.tls12 ? "-tls1_2" : "-tls1_3"));
        Child server(openssl_path(), args);
        ASSERT_TRUE(server.start());
        ASSERT_FALSE(server.wait_for("ACCEPT").empty()) << slurp(server.log);
        auto seen = dial(sgcl::string("127.0.0.1:" + std::to_string(port)), client_config(tls::revocation_mode::staple_only));
        EXPECT_EQ(seen.connected, k.connects) << k.leaf << " " << k.staple << " tls12=" << k.tls12 << ": " << seen.error << " source " << int(seen.source);
        if (k.connects) {
            EXPECT_EQ(seen.source, tls::revocation_source::staple) << k.staple;
            EXPECT_EQ(*seen.status, x509::revocation_status::good);   // the leaf's; the intermediate unknown, taken by staple_only
        } else {
            ASSERT_TRUE(seen.reason.has_value()) << seen.error;
            EXPECT_EQ(*seen.reason, k.reason) << k.leaf << " " << k.staple << " tls12=" << k.tls12;
        }
    }
}

namespace {
    std::string go_oracle() {
        static std::string built = [] {
            if (go_path().empty()) {
                return std::string();
            }
            std::string src = (source_root() / "tools/tls_ocsp_oracle.go").string();
            std::string bin = scratch() + "/tls_ocsp_oracle";
            io::command b(sgcl::string(go_path()), sgcl::string("build"), sgcl::string("-o"), sgcl::string(bin), sgcl::string(src));
            auto r = b.combined_output();
            return r.has_value() ? bin : std::string();
        }();
        return built;
    }
}

// Go's crypto/tls server with OCSPStaple, TLS 1.3 and TLS 1.2; Go's client
// reading our server's staple
TEST(TlsRevocation, GoInterop) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    ASSERT_FALSE(go_oracle().empty()) << "go build tools/tls_ocsp_oracle.go failed";
    std::string chain = scratch() + "/chain.pem";
    {
        std::ofstream out(chain);
        out << slurp(rdir() + "good.pem") << slurp(rdir() + "int.pem");
    }
    for (bool v12 : {false, true}) {
        sgcl::vector<sgcl::string> args = {sgcl::string("-cert"), sgcl::string(chain), sgcl::string("-key"), sgcl::string(rdir() + "good.key"),
                                           sgcl::string("-ocsp"), sgcl::string(rdir() + "ocsp_good.der"), sgcl::string("-n"), sgcl::string("2")};
        if (v12) {
            args.push_back(sgcl::string("-tls12"));
        }
        Child go(go_oracle(), args);
        ASSERT_TRUE(go.start());
        std::string s = go.wait_for("LISTEN ");
        ASSERT_FALSE(s.empty());
        auto at = s.find("LISTEN ");
        sgcl::string address("127.0.0.1:" + s.substr(at + 7, s.find('\n', at) - at - 7));
        auto c = client_config(tls::revocation_mode::staple_only);
        c.crls = {crl_of("root.crl")};
        auto seen = dial(address, c);
        ASSERT_TRUE(seen.connected) << seen.error;
        EXPECT_EQ(seen.source, tls::revocation_source::staple);
        EXPECT_EQ(*seen.status, x509::revocation_status::good);
        // off: Go staples nothing to a client that does not ask, and nothing is checked
        auto off = dial(address, client_config(tls::revocation_mode::off));
        EXPECT_TRUE(off.connected) << off.error;
        EXPECT_FALSE(go.wait_for(v12 ? "STATE version=303" : "STATE version=304").empty());
    }
    // Go's client of our server: the staple read by crypto/tls
    auto id = identity_of("good");
    ASSERT_TRUE(id.set_ocsp_staple(der_of("ocsp_good.der").as_slice()).has_value());
    auto s = serve(id);
    sgcl::vector<sgcl::string> args = {sgcl::string("-client"), sgcl::string(s.address), sgcl::string("-ca"), sgcl::string(rdir() + "root.pem")};
    Child go(go_oracle(), args);
    ASSERT_TRUE(go.start());
    std::string out = go.wait_for("STAPLE", 10000);
    EXPECT_NE(out.find("STAPLE " + std::to_string(der_of("ocsp_good.der").size())), std::string::npos) << out;
    (void)s.listener.close();
}

// A client certificate revoked: the server's config checks it (its CRLs),
// and drops the connection with certificate_revoked
TEST(TlsRevocation, ClientCertificateRevoked) {
    tls::config server;
    server.identities = {identity_of("good")};
    server.client_auth = tls::client_auth::require;
    server.client_roots = x509::certificate_pool::from_pem(sgcl::string(slurp(rdir() + "root.pem")));
    server.revocation = tls::revocation_mode::staple_only;
    server.crls = {crl_of("int.crl"), crl_of("root.crl")};
    auto l = tls::listen("127.0.0.1:0", server);
    ASSERT_TRUE(l.has_value());
    sgcl::string address("127.0.0.1:" + sgcl::to_string(l->local_endpoint().port()));
    for (const char* who : {"revoked", "good"}) {
        auto c = client_config(tls::revocation_mode::off);
        c.identities = {identity_of(who)};
        auto conn = tls::connect(address, c);
        ASSERT_TRUE(conn.has_value()) << std::string(conn.error().message().view());   // TLS 1.3: the client is done first
        if (std::string(who) == "revoked") {
            auto r = conn->read_line();
            ASSERT_FALSE(r.has_value());
            EXPECT_TRUE(tls::is_remote(r.error()));
            EXPECT_EQ(tls::alert_of(r.error()), tls::alert::certificate_revoked) << std::string(r.error().message().view());
        } else {
            auto a = l->accept();
            ASSERT_TRUE(a.has_value());
            auto st = tls::state_of(*a);
            ASSERT_TRUE(st->revocation.has_value());
            EXPECT_EQ(*st->revocation, x509::revocation_status::good);
            EXPECT_EQ(st->revocation_source, tls::revocation_source::crl);
            (void)a->close();
        }
        (void)conn->close();
    }
    (void)l->close();
}

// The task forms check as the blocking ones do
TEST(TlsRevocation, TaskForms) {
    auto rid = identity_of("revoked");
    ASSERT_TRUE(rid.set_ocsp_staple(der_of("ocsp_revoked.der").as_slice()).has_value());
    auto r = serve(rid);
    auto c = client_config(tls::revocation_mode::staple_only);
    auto conn = async::spawn(tls::async_connect(r.address, c)).wait();
    ASSERT_FALSE(conn.has_value());
    EXPECT_EQ(tls::certificate_reason(conn.error()), x509::reason::revoked);
    // client() over a TCP connection of the program's
    auto tcp = net::tcp::connect(r.address);
    ASSERT_TRUE(tcp.has_value());
    auto viaclient = tls::client(*tcp, c);
    ASSERT_FALSE(viaclient.has_value());
    EXPECT_EQ(tls::certificate_reason(viaclient.error()), x509::reason::revoked);
    EXPECT_TRUE(tcp->is_closed());
    (void)r.listener.close();
}

// The defaults, the cache's boundaries, a config refused
TEST(TlsRevocation, DefaultsAndBoundaries) {
    tls::config c;
    EXPECT_EQ(c.revocation, tls::revocation_mode::off);
    EXPECT_EQ(c.revocation_timeout, sgcl::duration(std::chrono::seconds(5)));
    EXPECT_TRUE(c.crls.empty());
    EXPECT_TRUE(c.fetch_crls);
    EXPECT_FALSE(c.revocation_cache.has_value());
    EXPECT_FALSE(c.ocsp_stapling);
    tls::state s;
    EXPECT_FALSE(s.revocation.has_value());
    EXPECT_EQ(s.revocation_source, tls::revocation_source::none);
    tls::revocation_cache cache;
    EXPECT_EQ(cache.capacity(), 256u);
    EXPECT_EQ(cache.size(), 0u);
    tls::revocation_cache none(0);
    EXPECT_EQ(none.capacity(), 0u);
    auto copy = none;
    auto moved = std::move(copy);
    EXPECT_EQ(moved.capacity(), 0u);
    EXPECT_EQ(copy.capacity(), 0u);   // a moved-from handle is the same cache
    cache.clear();
    // a revocation of no value of its enumeration: EINVAL before anything is sent
    auto any = serve(identity_of("good"));
    auto bad = client_config(tls::revocation_mode(7));
    auto r = tls::connect(any.address, bad);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code(), std::make_error_code(std::errc::invalid_argument));
    (void)any.listener.close();
    tls::config server;
    server.identities = {identity_of("good")};
    server.revocation = tls::revocation_mode(9);
    auto l = tls::listen("127.0.0.1:0", server);
    ASSERT_FALSE(l.has_value());
    EXPECT_EQ(l.error().code(), std::make_error_code(std::errc::invalid_argument));
    // insecure_skip_verify: nothing verified, nothing checked, no status_request
    auto rid = identity_of("revoked");
    ASSERT_TRUE(rid.set_ocsp_staple(der_of("ocsp_revoked.der").as_slice()).has_value());
    auto srv = serve(rid);
    auto insecure = client_config(tls::revocation_mode::hard_fail);
    insecure.insecure_skip_verify = true;
    auto seen = dial(srv.address, insecure);
    ASSERT_TRUE(seen.connected) << seen.error;
    EXPECT_FALSE(seen.status.has_value());
    (void)srv.listener.close();
}

// A cache of no room keeps nothing; one of one keeps the last
TEST(TlsRevocation, CacheCapacity) {
    if (openssl_path().empty()) {
        GTEST_SKIP() << "no openssl (its ocsp responder is the oracle)";
    }
    if (!port_free(47811) || !port_free(47812)) {
        GTEST_SKIP() << "the ports of the fixtures' AIA and CRL DP, 47811 and 47812, are taken";
    }
    Responder responder;
    ASSERT_TRUE(responder.up()) << slurp(responder.log);
    auto files = FileServer::start();
    ASSERT_TRUE(files->ok());
    auto g = serve(identity_of("good"));
    auto c = client_config(tls::revocation_mode::soft_fail);
    c.revocation_cache = tls::revocation_cache(0);
    ASSERT_TRUE(dial(g.address, c).connected);
    EXPECT_EQ(c.revocation_cache->size(), 0u);
    c.revocation_cache = tls::revocation_cache(1);
    ASSERT_TRUE(dial(g.address, c).connected);
    EXPECT_EQ(c.revocation_cache->size(), 1u);   // the leaf's answer and root's CRL: the oldest dropped
    (void)g.listener.close();
    files->close();
}

// An http::client's config of TLS carries revocation: a server of HTTP over
// TLS whose leaf's staple says revoked is refused, a good one served. HTTP/1.1:
// the client's HTTP/2 over TLS has a race of its own under TSan, apart from
// revocation (close_write's flush recording a failure the read side reads
// without the record's lock, TlsImpl::_break against _read_once)
TEST(TlsRevocation, HttpClient) {
    for (bool http2 : {false}) {
        SCOPED_TRACE(http2);
        for (const char* leaf : {"good", "revoked"}) {
            auto id = identity_of(leaf);
            ASSERT_TRUE(id.set_ocsp_staple(der_of(std::string("ocsp_") + leaf + ".der").as_slice()).has_value());
            tls::config server_tls;
            server_tls.identities = {id};
            server_tls.alpn = {"h2", "http/1.1"};
            net::http::server server;
            server.route("GET /", [](net::http::request, net::http::response_writer w) {
                w.write("hello\n");
            });
            auto l = tls::listen("127.0.0.1:0", server_tls);
            ASSERT_TRUE(l.has_value());
            auto serving = async::spawn(server.async_serve(*l));
            net::http::client c;
            c.tls = client_config(tls::revocation_mode::staple_only);
            c.tls.server_name = sgcl::string();
            c.http2 = http2;
            auto res = c.get(sgcl::string("https://localhost:" + std::to_string(l->local_endpoint().port()) + "/"));
            if (std::string(leaf) == "good") {
                ASSERT_TRUE(res.has_value()) << std::string(res.error().message().view());
                EXPECT_EQ(res->status(), 200);
            } else {
                ASSERT_FALSE(res.has_value());
                EXPECT_EQ(tls::certificate_reason(res.error()), x509::reason::revoked) << std::string(res.error().message().view());
            }
            c.close_idle_connections();
            server.close();
            (void)serving.wait();
        }
    }
}
