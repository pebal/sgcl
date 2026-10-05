//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The pieces of resumption and of client certificates below the connection
// (sgcl/net/tls/detail/session.h, schedule.h, both handshake machines):
//
//   - RFC 8448 §3 and §4: the PSK of the ticket's nonce from the resumption
//     master secret, the early secret of the PSK, the binder of §4's
//     ClientHello (its prefix found by PreSharedKeys::truncated_size), the
//     handshake secret of the resumed handshake;
//   - the client's cache (SessionCacheState): the newest fresh session
//     taken once, four per server, the capacity, stale ones dropped, the
//     secrets zeroed when a session goes; the server's keys (TicketKeys):
//     sealed and opened, another key's ticket, a byte changed, a ticket cut
//     short, rotation by age and by hand; the ticket's content read back
//     and refused when malformed;
//   - our two machines, in memory, on clocks of the test's: a full handshake
//     whose ticket the next resumes, a ticket the server finds expired
//     (its clock) and one the client does not offer (its clock), a binder
//     or a ticket changed in flight, pre_shared_key without
//     psk_key_exchange_modes and with psk_ke alone, a ServerHello that
//     selects an identity not offered or a suite of another hash, a
//     NewSessionTicket of lifetime zero or past MaxTicket;
//   - client certificates: the chain verified at the server's time (an
//     expired one is certificate_expired), a CertificateVerify that does
//     not verify, of a scheme not asked for, a Certificate with a context.
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/net/tls/detail/server_handshake.h"
#include "tls_rfc8448.h"
#include "tls_server_identities.h"

#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace tls = sgcl::net::tls::detail;

namespace {
    using bytes_t = std::vector<uint8_t>;
    using tls::Action;
    using tls::AlertDescription;

    sgcl::slice<const sgcl::byte> view(const bytes_t& v) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(v.data()), v.size());
    }

    bytes_t of(const sgcl::slice<const sgcl::byte>& s) {
        auto p = reinterpret_cast<const uint8_t*>(s.data());
        return bytes_t(p, p + s.size());
    }

    bytes_t of(const tls::Secret& s) {
        return bytes_t(s.bytes, s.bytes + s.size);
    }

    bytes_t of(const std::vector<sgcl::byte>& v) {
        auto p = reinterpret_cast<const uint8_t*>(v.data());
        return bytes_t(p, p + v.size());
    }

    bytes_t unhex(const char* s) {
        bytes_t v;
        for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
            char b[3] = {s[i], s[i + 1], 0};
            v.push_back(uint8_t(std::strtoul(b, nullptr, 16)));
        }
        return v;
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    const std::vector<rfc8448::Step>& steps() {
        static const std::vector<rfc8448::Step> s = rfc8448::read();
        return s;
    }

    tls::Secret secret_of(const bytes_t& b) {
        tls::Secret s;
        std::memcpy(s.bytes, b.data(), b.size());
        s.size = uint8_t(b.size());
        return s;
    }

    // A clock the test moves: Unix milliseconds
    struct TestClock {
        int64_t ms = 1'800'000'000'000;

        static sgcl::time::datetime now(void* self) noexcept {
            return sgcl::time::datetime::from_unix_milli(static_cast<TestClock*>(self)->ms, sgcl::time::zone::utc());
        }

        tls::Clock clock() {
            return tls::Clock{&TestClock::now, this};
        }
    };

    // What a machine asked for, the sessions its tickets made among it
    struct Log {
        std::vector<bytes_t> sent;
        std::vector<std::string> kinds;
        std::vector<AlertDescription> alerts;
        sgcl::vector<sgcl::tracked_ptr<tls::Session>> sessions;

        void take(const tls::Step& step, const tls::ClientResult* client = nullptr) {
            for (auto& a : step.actions) {
                switch (a.kind) {
                case Action::Kind::send:
                    sent.push_back(of(step.bytes(a)));
                    kinds.push_back("send");
                    break;
                case Action::Kind::alert:
                    alerts.push_back(a.alert);
                    kinds.push_back("alert");
                    break;
                case Action::Kind::established:
                    kinds.push_back("established");
                    break;
                case Action::Kind::new_ticket: {
                    sgcl::tracked_ptr<tls::Session> s = sgcl::make_tracked<tls::Session>();
                    s->cipher = uint16_t(a.cipher);
                    auto t = step.bytes(a);
                    s->ticket.assign(t.data(), t.data() + t.size());
                    s->psk = std::make_unique<tls::Secret>();
                    std::memcpy(s->psk->bytes, a.secret.bytes, sizeof a.secret.bytes);
                    s->psk->size = a.secret.size;
                    s->received_ms = a.issued_ms;
                    s->lifetime = a.lifetime;
                    s->age_add = a.age_add;
                    if (client) {
                        s->peer_certificates = client->peer_certificates;
                    }
                    sessions.push_back(s);
                    kinds.push_back("new ticket");
                    break;
                }
                default:
                    kinds.push_back("other");
                    break;
                }
            }
        }
    };

    std::vector<bytes_t> split(const bytes_t& flight) {
        std::vector<bytes_t> out;
        size_t at = 0;
        while (at + 4 <= flight.size()) {
            size_t n = size_t(flight[at + 1]) << 16 | size_t(flight[at + 2]) << 8 | flight[at + 3];
            out.emplace_back(flight.begin() + long(at), flight.begin() + long(at + 4 + n));
            at += 4 + n;
        }
        return out;
    }

    // The machines' identities: a server's of tls_server_identities.h, the
    // client's (mTLS) of the PEM files
    struct Keys {
        sgcl::crypto::p256::private_key server_key, client_key;
        std::vector<std::vector<sgcl::byte>> server_chain, client_chain;

        static std::vector<sgcl::byte> bytes(const bytes_t& v) {
            auto p = reinterpret_cast<const sgcl::byte*>(v.data());
            return std::vector<sgcl::byte>(p, p + v.size());
        }

        static const tls_identities::Identity& find(const char* name) {
            for (auto& i : tls_identities::all) {
                if (std::string(i.name) == name) {
                    return i;
                }
            }
            throw std::logic_error("no such identity");
        }

        Keys()
        : server_key(sgcl::crypto::p256::private_key::from_pkcs8_der(view(unhex(find("p256").key))).value())
        , client_key(sgcl::crypto::p256::private_key::from_pem(sgcl::slice<const std::byte>(std::string_view(slurp(testdata("client_ecdsa.key"))))).value()) {
            server_chain = {bytes(unhex(find("p256").certificate))};
            auto pool = sgcl::crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("client_ecdsa.pem"))));
            auto der = pool.certificates()[0].raw();
            client_chain = {std::vector<sgcl::byte>(der.data(), der.data() + der.size())};
        }
    };

    const Keys& keys() {
        static const Keys k;
        return k;
    }

    // The test CA (a handle: made where it is used)
    sgcl::crypto::x509::certificate_pool ca() {
        return sgcl::crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
    }

    tls::ClientSettings client_settings() {
        tls::ClientSettings c;
        c.server_name = "example.test";
        c.insecure_skip_verify = true;
        c.resumption = true;
        return c;
    }

    tls::ServerSettings server_settings(tls::TicketKeys* tickets) {
        tls::ServerSettings s;
        s.identities.push_back(tls::identity_of(keys().server_chain, keys().server_key));
        s.tickets = tickets;
        return s;
    }

    // Our client and our server, in memory, each on its clock; `tamper`
    // may change a message on its way (the client's first, from: false;
    // the server's, from: true)
    struct Pair {
        TestClock client_clock, server_clock;
        tls::ClientHandshake client;
        tls::ServerHandshake server;
        Log client_log, server_log;
        std::function<void(bytes_t&, bool from_server)> tamper;

        Pair(const tls::ClientSettings& c, const tls::ServerSettings& s, int64_t client_ms = 1'800'000'000'000, int64_t server_ms = 1'800'000'000'000)
        : client_clock{client_ms}, server_clock{server_ms}, client(c, tls::Entropy(), client_clock.clock()), server(s, tls::Entropy(), server_clock.clock()) {
        }

        void run() {
            std::vector<bytes_t> to_server, to_client;
            auto deliver = [](const tls::Step& step, Log& log, std::vector<bytes_t>& to, const tls::ClientResult* r) {
                size_t before = log.sent.size();
                log.take(step, r);
                for (size_t i = before; i < log.sent.size(); ++i) {
                    for (auto& m : split(log.sent[i])) {
                        to.push_back(m);
                    }
                }
            };
            deliver(client.start(), client_log, to_server, &client.result());
            for (int round = 0; round < 10 && (!to_server.empty() || !to_client.empty()); ++round) {
                auto s = std::move(to_server);
                to_server.clear();
                for (auto& m : s) {
                    if (tamper) {
                        tamper(m, false);
                    }
                    deliver(server.feed(view(m)), server_log, to_client, nullptr);
                }
                auto c = std::move(to_client);
                to_client.clear();
                for (auto& m : c) {
                    if (tamper) {
                        tamper(m, true);
                    }
                    deliver(client.feed(view(m)), client_log, to_server, &client.result());
                }
            }
        }
    };

    // A full handshake of these settings: the session its ticket made
    sgcl::tracked_ptr<tls::Session> first_session(tls::TicketKeys& keys, int64_t at_ms = 1'800'000'000'000) {
        Pair p(client_settings(), server_settings(&keys), at_ms, at_ms);
        p.run();
        EXPECT_TRUE(p.client.established());
        EXPECT_TRUE(p.server.established());
        EXPECT_FALSE(p.client.result().resumed);
        EXPECT_EQ(p.client_log.sessions.size(), 1u);
        return p.client_log.sessions.empty() ? sgcl::tracked_ptr<tls::Session>() : p.client_log.sessions[0];
    }

    // The first message of a type in a log's sends
    bytes_t message_of(const Log& log, tls::HandshakeType type) {
        for (auto& f : log.sent) {
            for (auto& m : split(f)) {
                if (m[0] == uint8_t(type)) {
                    return m;
                }
            }
        }
        return {};
    }
}

// --- RFC 8448 --------------------------------------------------------------------

TEST(TlsResumptionPieces, Rfc8448) {
    if (steps().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    // §3: the PSK of the ticket's nonce (00 00) from the resumption master secret
    auto res = rfc8448::find(steps(), 3, "server", "generate resumption secret");
    ASSERT_NE(res, nullptr);
    tls::Secret psk;
    tls::resumption_psk(tls::Hash::sha256, psk, secret_of(res->fields.at("PRK")), view(res->fields.at("hash")));
    EXPECT_EQ(of(psk), res->fields.at("expanded"));
    // the resumption master secret itself, from the master secret and the transcript
    auto master = rfc8448::find(steps(), 3, "client", "derive secret \"tls13 res master\"");
    ASSERT_NE(master, nullptr);
    tls::Secret m;
    tls::derive_secret(tls::Hash::sha256, m, secret_of(master->fields.at("PRK")), "res master", view(master->fields.at("hash")));
    EXPECT_EQ(of(m), master->fields.at("expanded"));
    // §4: the early secret of the PSK
    auto early = rfc8448::find(steps(), 4, "client", "extract secret \"early\"");
    ASSERT_NE(early, nullptr);
    EXPECT_EQ(early->fields.at("IKM"), of(psk));
    tls::KeySchedule k(tls::Hash::sha256, psk);
    EXPECT_EQ(of(k.early_secret()), early->fields.at("secret"));
    // the binder: the ClientHello's prefix, its hash, the binder
    auto binder = rfc8448::find(steps(), 4, "client", "calculate PSK binder");
    ASSERT_NE(binder, nullptr);
    const bytes_t& prefix = binder->fields.at("ClientHello prefix");
    tls::Transcript t(tls::Hash::sha256);
    t.update(view(prefix));
    bytes_t h(32);
    t.value_to(h.data());
    EXPECT_EQ(h, binder->fields.at("binder hash"));
    bytes_t b(32);
    k.binder(b.data(), view(h));
    EXPECT_EQ(b, binder->fields.at("finished"));
    // the whole ClientHello (the payload of its record): the prefix found by truncated_size, the binder in it
    bytes_t ch;
    for (auto& msg : rfc8448::messages(steps())) {
        if (msg.section == 4 && msg.who == "client" && msg.name == "ClientHello") {
            ch = msg.bytes;
        }
    }
    ASSERT_GT(ch.size(), prefix.size());
    auto hs = tls::read_handshake(view(ch));
    ASSERT_TRUE(hs);
    auto hello = tls::read_client_hello(hs->body);
    ASSERT_TRUE(hello);
    auto keys = tls::read_pre_shared_keys(*hello->extensions.find(tls::ExtensionType::pre_shared_key));
    ASSERT_TRUE(keys);
    EXPECT_EQ(keys->count, 1u);
    EXPECT_EQ(keys->truncated_size(view(ch)), prefix.size());
    EXPECT_EQ(of(keys->binder(0)), b);
    EXPECT_EQ(keys->identity(0).obfuscated_age, 0xfad6aacbu);
    // the handshake secret of the resumed handshake: the PSK's early secret and the (EC)DHE
    auto hss = rfc8448::find(steps(), 4, "server", "extract secret \"handshake\"");
    ASSERT_NE(hss, nullptr);
    auto sh_hash = rfc8448::find(steps(), 4, "server", "derive secret \"tls13 c hs traffic\"");
    ASSERT_NE(sh_hash, nullptr);
    k.handshake(view(hss->fields.at("IKM")), view(sh_hash->fields.at("hash")));
    EXPECT_EQ(of(k.handshake_secret()), hss->fields.at("secret"));
    EXPECT_EQ(of(k.client_handshake_traffic), sh_hash->fields.at("expanded"));
}

// --- the cache and the keys -----------------------------------------------------

namespace {
    sgcl::tracked_ptr<tls::Session> session(int64_t received_ms, uint32_t lifetime = 3600, uint8_t mark = 1) {
        sgcl::tracked_ptr<tls::Session> s = sgcl::make_tracked<tls::Session>();
        s->cipher = 0x1301;
        s->ticket.assign(4, sgcl::byte(mark));
        s->psk = std::make_unique<tls::Secret>();
        s->psk->size = 32;
        std::memset(s->psk->bytes, mark, 32);
        s->received_ms = received_ms;
        s->lifetime = lifetime;
        return s;
    }
}

TEST(TlsResumptionPieces, TheCache) {
    tls::SessionCacheState c(8);
    const int64_t t = 1'000'000;
    EXPECT_FALSE(c.take("a", t));   // empty
    auto old = session(t, 3600, 1), newer = session(t + 1, 3600, 2);
    c.put("a", old);
    c.put("a", newer);
    c.put("b", session(t, 3600, 3));
    EXPECT_EQ(c.size(), 3u);
    auto got = c.take("a", t + 10);
    ASSERT_TRUE(got);
    EXPECT_EQ(got.get(), newer.get());   // the newest
    EXPECT_FALSE(newer->psk->empty());   // taken whole: the taker zeroes it
    EXPECT_EQ(c.size(), 2u);
    // stale ones of the key dropped and zeroed on the way; another key's left
    EXPECT_FALSE(c.take("a", t + 3600 * 1000 + 1));
    EXPECT_TRUE(old->psk->empty());
    EXPECT_EQ(c.size(), 1u);
    // a time before the session came: not fresh
    EXPECT_FALSE(c.take("b", t - 1));
    // four per server: the fifth drops the oldest of the server
    tls::SessionCacheState d(100);
    sgcl::vector<sgcl::tracked_ptr<tls::Session>> five;
    for (int i = 0; i < 5; ++i) {
        five.push_back(session(t + i, 3600, uint8_t(10 + i)));
        d.put("s", five.back());
    }
    EXPECT_EQ(d.size(), 4u);
    EXPECT_TRUE(five[0]->psk->empty());
    // the capacity: the oldest of all dropped
    tls::SessionCacheState e(2);
    auto x = session(t, 3600, 20), y = session(t, 3600, 21), z = session(t, 3600, 22);
    e.put("x", x);
    e.put("y", y);
    e.put("z", z);
    EXPECT_EQ(e.size(), 2u);
    EXPECT_TRUE(x->psk->empty());
    EXPECT_FALSE(e.take("x", t));
    EXPECT_TRUE(e.take("z", t));
    // a capacity of zero keeps nothing, and zeroes what it is given
    tls::SessionCacheState none(0);
    auto w = session(t);
    none.put("w", w);
    EXPECT_EQ(none.size(), 0u);
    EXPECT_TRUE(w->psk->empty());
    // clear
    e.clear();
    EXPECT_EQ(e.size(), 0u);
    EXPECT_TRUE(y->psk->empty());
    // the key: name, port, protocols
    EXPECT_EQ(tls::session_key("a.test", 443, {}), "a.test:443");
    EXPECT_EQ(tls::session_key("a.test", 443, {sgcl::string("h2"), sgcl::string("http/1.1")}), "a.test:443\nh2\nhttp/1.1");
    EXPECT_NE(tls::session_key("a.test", 443, {sgcl::string("h2")}), tls::session_key("a.test", 443, {sgcl::string("http/1.1")}));
}

TEST(TlsResumptionPieces, TheTicketKeys) {
    tls::TicketKeys k;
    const bytes_t content = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<sgcl::byte> ticket;
    k.seal(ticket, view(content), 1000, 60'000, tls::Entropy());
    ASSERT_EQ(ticket.size(), content.size() + tls::TicketKeys::Overhead);
    bytes_t out(content.size());
    auto n = k.open(tls::bytes_of(ticket.data(), ticket.size()), out.data());
    ASSERT_TRUE(n);
    EXPECT_EQ(*n, content.size());
    EXPECT_EQ(out, content);
    // a byte changed anywhere: refused, nothing written
    for (size_t i = 0; i < ticket.size(); ++i) {
        auto bad = ticket;
        bad[i] ^= sgcl::byte(1);
        std::fill(out.begin(), out.end(), 0xEE);
        EXPECT_FALSE(k.open(tls::bytes_of(bad.data(), bad.size()), out.data())) << i;
    }
    // cut short, and shorter than the overhead
    EXPECT_FALSE(k.open(tls::bytes_of(ticket.data(), ticket.size() - 1), out.data()));
    EXPECT_FALSE(k.open(tls::bytes_of(ticket.data(), tls::TicketKeys::Overhead - 1), out.data()));
    EXPECT_FALSE(k.open(tls::Bytes(), out.data()));
    // another object's keys
    tls::TicketKeys other;
    EXPECT_FALSE(other.open(tls::bytes_of(ticket.data(), ticket.size()), out.data()));
    // rotated once: still opens; twice: gone
    k.rotate();
    EXPECT_TRUE(k.open(tls::bytes_of(ticket.data(), ticket.size()), out.data()));
    k.rotate();
    EXPECT_FALSE(k.open(tls::bytes_of(ticket.data(), ticket.size()), out.data()));
    // rotation by age: a seal past the lifetime of the current key makes a new one
    tls::TicketKeys aged;
    std::vector<sgcl::byte> t1, t2, t3;
    aged.seal(t1, view(content), 0, 1000, tls::Entropy());
    aged.seal(t2, view(content), 999, 1000, tls::Entropy());
    EXPECT_TRUE(std::equal(t1.begin(), t1.begin() + 8, t2.begin()));    // the same key's name
    aged.seal(t3, view(content), 1000, 1000, tls::Entropy());
    EXPECT_FALSE(std::equal(t1.begin(), t1.begin() + 8, t3.begin()));   // a new key
    EXPECT_TRUE(aged.open(tls::bytes_of(t1.data(), t1.size()), out.data()));   // the previous still opens
    std::vector<sgcl::byte> t4;
    aged.seal(t4, view(content), 2000, 1000, tls::Entropy());
    EXPECT_FALSE(aged.open(tls::bytes_of(t1.data(), t1.size()), out.data()));
    // a clock gone back: a new key too
    std::vector<sgcl::byte> t5;
    aged.seal(t5, view(content), 10, 1000, tls::Entropy());
    EXPECT_FALSE(std::equal(t4.begin(), t4.begin() + 8, t5.begin()));
    // keys never sealed with open nothing
    tls::TicketKeys fresh;
    EXPECT_FALSE(fresh.open(tls::bytes_of(ticket.data(), ticket.size()), out.data()));
}

TEST(TlsResumptionPieces, TheTicketsContent) {
    std::vector<sgcl::byte> out;
    tls::Builder w(out);
    tls::Secret psk;
    psk.size = 32;
    std::memset(psk.bytes, 7, 32);
    sgcl::crypto::x509::chain chain;
    chain.push_back(ca().certificates()[0]);
    tls::write_ticket_content(w, 0x1302, 123456789012345, 3600, 0xDEADBEEF, psk, chain);
    auto t = tls::read_ticket_content(tls::bytes_of(out.data(), out.size()));
    ASSERT_TRUE(t);
    EXPECT_EQ(t->cipher, 0x1302);
    EXPECT_EQ(t->issued_ms, 123456789012345);
    EXPECT_EQ(t->lifetime, 3600u);
    EXPECT_EQ(t->age_add, 0xDEADBEEFu);
    EXPECT_EQ(of(t->psk), of(psk));
    EXPECT_EQ(t->certificates.size(), 3 + chain[0].raw().size());
    // every prefix refused, a byte more refused, another version refused
    for (size_t n = 0; n < out.size(); ++n) {
        EXPECT_FALSE(tls::read_ticket_content(tls::bytes_of(out.data(), n))) << n;
    }
    auto longer = out;
    longer.push_back(sgcl::byte(0));
    EXPECT_FALSE(tls::read_ticket_content(tls::bytes_of(longer.data(), longer.size())));
    auto version = out;
    version[0] = sgcl::byte(2);
    EXPECT_FALSE(tls::read_ticket_content(tls::bytes_of(version.data(), version.size())));
}

// --- the machines ----------------------------------------------------------------

TEST(TlsResumptionMachines, AFullHandshakeThenAResumedOne) {
    tls::TicketKeys keys;
    auto s = first_session(keys);
    ASSERT_TRUE(s);
    EXPECT_EQ(s->lifetime, 86400u);
    tls::ClientSettings c = client_settings();
    c.session = s;
    Pair p(c, server_settings(&keys));
    p.run();
    ASSERT_TRUE(p.client.established()) << (p.client_log.alerts.empty() ? 0 : int(p.client_log.alerts[0]));
    ASSERT_TRUE(p.server.established()) << (p.server_log.alerts.empty() ? 0 : int(p.server_log.alerts[0]));
    EXPECT_TRUE(p.client.result().resumed);
    EXPECT_TRUE(p.server.result().resumed);
    EXPECT_TRUE(s->psk->empty());   // offered once: zeroed
    // no certificate went: the server's flight is EncryptedExtensions and Finished
    EXPECT_TRUE(message_of(p.server_log, tls::HandshakeType::certificate).empty());
    EXPECT_TRUE(message_of(p.server_log, tls::HandshakeType::certificate_verify).empty());
    // the resumed connection's ticket, for the next
    ASSERT_EQ(p.client_log.sessions.size(), 1u);
    tls::ClientSettings c2 = client_settings();
    c2.session = p.client_log.sessions[0];
    Pair q(c2, server_settings(&keys));
    q.run();
    EXPECT_TRUE(q.client.result().resumed);
    // a session offered again (its PSK zeroed): not offered
    tls::ClientSettings again = client_settings();
    again.session = s;
    Pair r(again, server_settings(&keys));
    r.run();
    EXPECT_TRUE(r.client.established());
    EXPECT_FALSE(r.client.result().resumed);
    auto ch = tls::read_client_hello(tls::read_handshake(view(message_of(r.client_log, tls::HandshakeType::client_hello)))->body);
    EXPECT_FALSE(ch->extensions.has(tls::ExtensionType::pre_shared_key));
}

TEST(TlsResumptionMachines, ExpiredByTheServersClockAndByTheClients) {
    tls::TicketKeys keys;
    const int64_t t = 1'800'000'000'000;
    auto s = first_session(keys, t);
    ASSERT_TRUE(s);
    // the client's clock within the lifetime, the server's past it: offered, refused
    tls::ClientSettings c = client_settings();
    c.session = s;
    Pair p(c, server_settings(&keys), t + 1000, t + 86400 * 1000 + 1);
    p.run();
    ASSERT_TRUE(p.client.established());
    EXPECT_FALSE(p.client.result().resumed);
    EXPECT_FALSE(p.server.result().resumed);
    auto ch = tls::read_client_hello(tls::read_handshake(view(message_of(p.client_log, tls::HandshakeType::client_hello)))->body);
    EXPECT_TRUE(ch->extensions.has(tls::ExtensionType::pre_shared_key));
    // the lifetime's last millisecond still resumes
    auto s2 = first_session(keys, t);
    tls::ClientSettings c2 = client_settings();
    c2.session = s2;
    Pair q(c2, server_settings(&keys), t + 1000, t + 86400 * 1000);
    q.run();
    EXPECT_TRUE(q.client.result().resumed);
    // the client's clock past it: not offered
    auto s3 = first_session(keys, t);
    tls::ClientSettings c3 = client_settings();
    c3.session = s3;
    Pair r(c3, server_settings(&keys), t + 86400 * 1000, t);
    r.run();
    EXPECT_FALSE(r.client.result().resumed);
    auto ch3 = tls::read_client_hello(tls::read_handshake(view(message_of(r.client_log, tls::HandshakeType::client_hello)))->body);
    EXPECT_FALSE(ch3->extensions.has(tls::ExtensionType::pre_shared_key));
    // a ticket from the server's future (its clock gone back): refused
    auto s4 = first_session(keys, t);
    tls::ClientSettings c4 = client_settings();
    c4.session = s4;
    Pair u(c4, server_settings(&keys), t + 1000, t - 1);
    u.run();
    EXPECT_FALSE(u.server.result().resumed);
}

TEST(TlsResumptionMachines, ABinderChangedInFlight) {
    tls::TicketKeys keys;
    auto s = first_session(keys);
    tls::ClientSettings c = client_settings();
    c.session = s;
    Pair p(c, server_settings(&keys));
    p.tamper = [](bytes_t& m, bool from_server) {
        if (!from_server && m[0] == uint8_t(tls::HandshakeType::client_hello)) {
            m.back() ^= 1;   // the last byte of the binder
        }
    };
    p.run();
    ASSERT_EQ(p.server_log.alerts.size(), 1u);
    EXPECT_EQ(p.server_log.alerts[0], AlertDescription::decrypt_error);
}

TEST(TlsResumptionMachines, ATicketChangedInFlightIsAFullHandshake) {
    tls::TicketKeys keys;
    auto s = first_session(keys);
    s->ticket[10] ^= sgcl::byte(1);   // inside the sealed part
    tls::ClientSettings c = client_settings();
    c.session = s;
    Pair p(c, server_settings(&keys));
    p.run();
    EXPECT_TRUE(p.client.established());
    EXPECT_TRUE(p.server.established());
    EXPECT_FALSE(p.server.result().resumed);
}

TEST(TlsResumptionMachines, TicketsOffOnTheServer) {
    tls::TicketKeys keys;
    auto s = first_session(keys);
    tls::ClientSettings c = client_settings();
    c.session = s;
    Pair p(c, server_settings(nullptr));
    p.run();
    EXPECT_TRUE(p.client.established());
    EXPECT_FALSE(p.server.result().resumed);
    EXPECT_TRUE(p.client_log.sessions.empty());   // and none issued
    EXPECT_TRUE(message_of(p.server_log, tls::HandshakeType::new_session_ticket).empty());
}

TEST(TlsResumptionMachines, TheClientWithoutResumptionKeepsNoTicket) {
    tls::TicketKeys keys;
    tls::ClientSettings c = client_settings();
    c.resumption = false;
    Pair p(c, server_settings(&keys));
    p.run();
    EXPECT_TRUE(p.client.established());
    EXPECT_FALSE(message_of(p.server_log, tls::HandshakeType::new_session_ticket).empty());
    EXPECT_TRUE(p.client_log.sessions.empty());
}

namespace {
    // A ClientHello of one's own with a real X25519 share and the
    // extensions `more` writes last
    template<class F>
    bytes_t own_hello(F&& more) {
        static tls::ClientShares shares;
        if (shares.size() == 0) {
            shares.add(tls::Group::x25519, tls::Entropy());
        }
        std::vector<sgcl::byte> out;
        tls::Builder w(out);
        uint8_t random[32] = {1};
        std::vector<uint16_t> suites = {0x1301};
        tls::write_client_hello(w, tls::bytes_of(random, 32), tls::Bytes(), suites, [&](tls::Builder& w) {
            {
                auto e = w.extension(tls::ExtensionType::supported_versions);
                std::vector<uint16_t> v = {tls::Tls13};
                tls::write_versions_offered(w, v);
            }
            {
                auto e = w.extension(tls::ExtensionType::supported_groups);
                std::vector<uint16_t> g = {0x001D};
                tls::write_groups(w, g);
            }
            {
                auto e = w.extension(tls::ExtensionType::signature_algorithms);
                std::vector<uint16_t> s = {0x0403};
                tls::write_signature_schemes(w, s);
            }
            {
                auto e = w.extension(tls::ExtensionType::key_share);
                auto list = w.block16();
                w.u16(0x001D);
                auto k = w.block16();
                w.bytes(shares.public_share(0));
            }
            more(w);
        });
        return of(out);
    }

    void psk_extension(tls::Builder& w, const tls::Session& s) {
        auto e = w.extension(tls::ExtensionType::pre_shared_key);
        {
            auto ids = w.block16();
            {
                auto id = w.block16();
                w.bytes(s.ticket.data(), s.ticket.size());
            }
            w.u32(0);
        }
        auto binders = w.block16();
        auto one = w.block8();
        for (int i = 0; i < 32; ++i) {
            w.u8(0);
        }
    }
}

TEST(TlsResumptionMachines, TheModesOfAPsk) {
    tls::TicketKeys keys;
    auto s = first_session(keys);
    TestClock clock;   // the time the session was made at
    // pre_shared_key without psk_key_exchange_modes: missing_extension (§4.2.9)
    {
        tls::ServerHandshake server(server_settings(&keys), tls::Entropy(), clock.clock());
        Log log;
        log.take(server.feed(view(own_hello([&](tls::Builder& w) { psk_extension(w, *s); }))));
        ASSERT_EQ(log.alerts.size(), 1u);
        EXPECT_EQ(log.alerts[0], AlertDescription::missing_extension);
    }
    // psk_ke alone: the PSK passed over (its binder never read), a full handshake
    {
        tls::ServerHandshake server(server_settings(&keys), tls::Entropy(), clock.clock());
        Log log;
        log.take(server.feed(view(own_hello([&](tls::Builder& w) {
            {
                auto e = w.extension(tls::ExtensionType::psk_key_exchange_modes);
                const uint8_t ke = 0;
                tls::write_psk_modes(w, tls::bytes_of(&ke, 1));
            }
            psk_extension(w, *s);
        }))));
        EXPECT_TRUE(log.alerts.empty());
        EXPECT_FALSE(server.result().resumed);
        auto sh = tls::read_server_hello(tls::read_handshake(view(split(log.sent.at(0))[0]))->body);
        EXPECT_FALSE(sh->extensions.has(tls::ExtensionType::pre_shared_key));
    }
    // psk_dhe_ke and a binder of zeros: decrypt_error
    {
        tls::ServerHandshake server(server_settings(&keys), tls::Entropy(), clock.clock());
        Log log;
        log.take(server.feed(view(own_hello([&](tls::Builder& w) {
            {
                auto e = w.extension(tls::ExtensionType::psk_key_exchange_modes);
                const uint8_t dhe = 1;
                tls::write_psk_modes(w, tls::bytes_of(&dhe, 1));
            }
            psk_extension(w, *s);
        }))));
        ASSERT_EQ(log.alerts.size(), 1u);
        EXPECT_EQ(log.alerts[0], AlertDescription::decrypt_error);
    }
    // an identity that is no ticket of these keys: passed over, a full handshake
    {
        tls::Session junk;
        junk.ticket.assign(40, sgcl::byte(0x5A));
        tls::ServerHandshake server(server_settings(&keys), tls::Entropy(), clock.clock());
        Log log;
        log.take(server.feed(view(own_hello([&](tls::Builder& w) {
            {
                auto e = w.extension(tls::ExtensionType::psk_key_exchange_modes);
                const uint8_t dhe = 1;
                tls::write_psk_modes(w, tls::bytes_of(&dhe, 1));
            }
            psk_extension(w, junk);
        }))));
        EXPECT_TRUE(log.alerts.empty());
        EXPECT_FALSE(server.result().resumed);
    }
}

// The client refuses a ServerHello that selects an identity it did not
// offer, or a suite of another hash than the PSK's
TEST(TlsResumptionMachines, TheClientChecksTheServersChoice) {
    for (int which : {0, 1}) {
        SCOPED_TRACE(which);
        tls::TicketKeys keys;
        auto s = first_session(keys);
        tls::ClientSettings c = client_settings();
        c.session = s;
        Pair p(c, server_settings(&keys));
        p.tamper = [which](bytes_t& m, bool from_server) {
            if (!from_server || m[0] != uint8_t(tls::HandshakeType::server_hello)) {
                return;
            }
            if (which == 0) {
                m.back() = 1;   // pre_shared_key, the last extension: identity 1
            } else {
                const size_t at = 4 + 2 + 32 + 1 + m[4 + 2 + 32];   // the cipher suite after the session id
                m[at + 1] = 0x02;   // TLS_AES_256_GCM_SHA384
            }
        };
        p.run();
        ASSERT_EQ(p.client_log.alerts.size(), 1u);
        EXPECT_EQ(p.client_log.alerts[0], AlertDescription::illegal_parameter);
    }
}

TEST(TlsResumptionMachines, NewSessionTicketsPassedOver) {
    tls::TicketKeys keys;
    Pair p(client_settings(), server_settings(&keys));
    p.run();
    ASSERT_TRUE(p.client.established());
    auto nst = [&](uint32_t lifetime, size_t ticket) {
        std::vector<sgcl::byte> out;
        tls::Builder w(out);
        auto m = w.message(tls::HandshakeType::new_session_ticket);
        w.u32(lifetime);
        w.u32(0);
        {
            auto n = w.block8();
            w.u8(1);
        }
        {
            auto t = w.block16();
            for (size_t i = 0; i < ticket; ++i) {
                w.u8(uint8_t(i));
            }
        }
        auto x = w.block16();
        m.close();
        return of(out);
    };
    Log log;
    log.take(p.client.feed(view(nst(0, 10))), &p.client.result());          // lifetime zero
    log.take(p.client.feed(view(nst(60, tls::MaxTicket + 1))), &p.client.result());   // too large
    EXPECT_TRUE(log.alerts.empty());
    EXPECT_TRUE(log.sessions.empty());
    log.take(p.client.feed(view(nst(60, tls::MaxTicket))), &p.client.result());
    ASSERT_EQ(log.sessions.size(), 1u);
    EXPECT_EQ(log.sessions[0]->lifetime, 60u);
    EXPECT_EQ(log.sessions[0]->ticket.size(), tls::MaxTicket);
    // past seven days: illegal_parameter
    log.take(p.client.feed(view(nst(604801, 10))), &p.client.result());
    ASSERT_EQ(log.alerts.size(), 1u);
    EXPECT_EQ(log.alerts[0], AlertDescription::illegal_parameter);
}

// --- client certificates ------------------------------------------------------------

namespace {
    tls::ServerSettings mtls_server(tls::TicketKeys* tickets = nullptr, uint8_t auth = 2) {
        tls::ServerSettings s = server_settings(tickets);
        s.client_auth = auth;
        s.client_roots = ca();
        return s;
    }

    tls::ClientSettings mtls_client() {
        tls::ClientSettings c = client_settings();
        c.identities.push_back(tls::identity_of(keys().client_chain, keys().client_key));
        return c;
    }
}

TEST(TlsClientAuthMachines, TheChainAtTheServersTime) {
    Pair p(mtls_client(), mtls_server());
    p.run();
    ASSERT_TRUE(p.server.established()) << (p.server_log.alerts.empty() ? 0 : int(p.server_log.alerts[0]));
    EXPECT_EQ(p.server.result().peer_certificates.size(), 1u);
    EXPECT_TRUE(p.client.result().certificate_sent);
    // the server's clock past the certificate's end (2126)
    Pair late(mtls_client(), mtls_server(), 1'800'000'000'000, 5'000'000'000'000);
    late.run();
    ASSERT_EQ(late.server_log.alerts.size(), 1u);
    EXPECT_EQ(late.server_log.alerts[0], AlertDescription::certificate_expired);
    EXPECT_EQ(late.server.verify_reason(), sgcl::crypto::x509::reason::expired);
}

TEST(TlsClientAuthMachines, ACertificateVerifyThatDoesNotVerify) {
    Pair p(mtls_client(), mtls_server());
    p.tamper = [](bytes_t& m, bool from_server) {
        if (!from_server && m[0] == uint8_t(tls::HandshakeType::certificate_verify)) {
            m[m.size() - 5] ^= 1;   // inside the signature
        }
    };
    p.run();
    ASSERT_EQ(p.server_log.alerts.size(), 1u);
    EXPECT_EQ(p.server_log.alerts[0], AlertDescription::decrypt_error);
}

TEST(TlsClientAuthMachines, ASchemeNotAskedForAndAContext) {
    for (int which : {0, 1}) {
        SCOPED_TRACE(which);
        Pair p(mtls_client(), mtls_server());
        p.tamper = [which](bytes_t& m, bool from_server) {
            if (from_server) {
                return;
            }
            if (which == 0 && m[0] == uint8_t(tls::HandshakeType::certificate_verify)) {
                m[4] = 0x02;   // 0x0203: ecdsa_sha1, never asked for
                m[5] = 0x03;
            }
            if (which == 1 && m[0] == uint8_t(tls::HandshakeType::certificate)) {
                m[4] = 1;   // a context of one byte: the chain's length byte read as it
            }
        };
        p.run();
        ASSERT_EQ(p.server_log.alerts.size(), 1u);
        if (which == 0) {
            EXPECT_EQ(p.server_log.alerts[0], AlertDescription::illegal_parameter);
        } else {
            EXPECT_TRUE(p.server_log.alerts[0] == AlertDescription::illegal_parameter || p.server_log.alerts[0] == AlertDescription::decode_error);
        }
    }
}

TEST(TlsClientAuthMachines, RequiredAndNoneEmptyAndAskedFor) {
    Pair required(client_settings(), mtls_server(nullptr, 2));
    required.run();
    ASSERT_EQ(required.server_log.alerts.size(), 1u);
    EXPECT_EQ(required.server_log.alerts[0], AlertDescription::certificate_required);
    EXPECT_FALSE(required.client.result().certificate_sent);
    Pair asked(client_settings(), mtls_server(nullptr, 1));
    asked.run();
    EXPECT_TRUE(asked.server.established());
    EXPECT_TRUE(asked.server.result().peer_certificates.empty());
    // the CertificateRequest: signature_algorithms and the CA's subject
    auto cr = message_of(asked.server_log, tls::HandshakeType::certificate_request);
    ASSERT_FALSE(cr.empty());
    auto r = tls::read_certificate_request(tls::read_handshake(view(cr))->body);
    ASSERT_TRUE(r);
    auto names = tls::read_certificate_authorities(*r->extensions.find(tls::ExtensionType::certificate_authorities));
    ASSERT_TRUE(names);
    EXPECT_EQ(names->count, 1u);
}

TEST(TlsClientAuthMachines, AResumedSessionKeepsTheChainAndAsksNothing) {
    tls::TicketKeys keys;
    Pair p(mtls_client(), mtls_server(&keys));
    p.run();
    ASSERT_TRUE(p.server.established());
    ASSERT_EQ(p.client_log.sessions.size(), 1u);
    tls::ClientSettings c = mtls_client();
    c.session = p.client_log.sessions[0];
    Pair q(c, mtls_server(&keys));
    q.run();
    ASSERT_TRUE(q.server.established());
    EXPECT_TRUE(q.server.result().resumed);
    EXPECT_EQ(q.server.result().peer_certificates.size(), 1u);
    EXPECT_TRUE(message_of(q.server_log, tls::HandshakeType::certificate_request).empty());
    EXPECT_FALSE(q.client.result().certificate_sent);
    // the session's chain past its time at the server: a full handshake, asked again
    tls::ClientSettings c2 = mtls_client();
    c2.session = q.client_log.sessions.at(0);
    Pair late(c2, mtls_server(&keys), 1'800'000'000'000, 5'000'000'000'000);
    late.run();
    EXPECT_FALSE(late.server.result().resumed);
}
