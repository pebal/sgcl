//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A minimal NATS server for the client's tests, written from the client
// protocol's documentation (no nats-server here): INFO, CONNECT with user
// and password, a token or an nkey's signed nonce, PING and PONG, SUB with
// wildcards and queue groups, UNSUB with a count, PUB and HPUB, MSG and
// HMSG, no responders (503) for a request nobody subscribes to, -ERR for
// an authorization violation, a payload past max_payload and a subject
// refused. What the client is verified against here is this server and
// the documentation; no other implementation of NATS is installed on
// this machine.
#pragma once

#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"
#include "sgcl/net/nats.h"

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace nats_test {
    using namespace sgcl;
    namespace nd = sgcl::net::nats::detail;
    namespace nats = sgcl::net::nats;

    inline std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    struct Conn {
        int id = 0;
        net::connection c;
        async::channel<std::string> out{1 << 16};
        bool headers = false;
        bool no_responders = false;
        bool authed = false;
    };

    struct Sub {
        int conn = 0;
        std::string sid;
        std::string subject;
        std::string queue;
        uint64_t max = 0;
        uint64_t delivered = 0;
    };

    struct Server {
        net::listener listener;
        async::task<> serving;
        std::mutex mu;
        map<int, tracked_ptr<Conn>> conns;
        std::vector<Sub> subs;
        std::map<std::string, size_t> turns;   // a queue group's next member
        int next_conn = 0;
        // authentication: none, user/password, a token, an nkey (its public key)
        std::string user, password, token, nkey;
        std::string nonce = "aBcDeFgHiJkLmNoP";
        std::string deny_subscribe;            // a subject whose subscription is refused
        size_t max_payload = 1 << 20;
        bool headers = true;
        std::atomic<bool> silent{false};       // PINGs no longer answered
        std::atomic<int> connections{0};
        std::atomic<long> published{0};
        std::string last_connect;              // the last CONNECT's JSON

        Server() {
            listener = net::tcp::listen("127.0.0.1:0").value();
            serving = async::spawn(accept_loop(this, listener));
        }

        ~Server() {
            (void)listener.close();
            serving.wait();
            {
                std::lock_guard g(mu);
                for (auto& [id, c] : conns) {
                    (void)c->c.close();
                    c->out.close();
                }
            }
            for (int i = 0; i < 500 && connections.load() > 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        uint16_t port() const {
            return listener.local_endpoint().port();
        }

        sgcl::string url(const std::string& credentials = "") const {
            return sgcl::string("nats://" + credentials + "127.0.0.1:" + std::to_string(port()));
        }

        size_t subscriptions() {
            std::lock_guard g(mu);
            return subs.size();
        }

        static async::task<> accept_loop(Server* self, net::listener l) {
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    co_return;
                }
                ++self->connections;
                tracked_ptr conn = make_tracked<Conn>();
                conn->c = *c;
                {
                    std::lock_guard g(self->mu);
                    conn->id = ++self->next_conn;
                    self->conns[conn->id] = conn;
                }
                async::go(writer(self, conn));
                async::go(serve(self, conn));
            }
        }

        static async::task<> writer(Server* self, tracked_ptr<Conn> conn) {
            for (;;) {
                auto b = co_await conn->out.receive();
                if (!b) {
                    co_return;
                }
                std::string all = std::move(*b);
                while (auto more = conn->out.try_receive()) {
                    all += *more;
                }
                if (!co_await conn->c.async_write(slice<const byte>(reinterpret_cast<const byte*>(all.data()), all.size()))) {
                    co_return;
                }
            }
        }

        std::string info() const {
            std::string s = "INFO {\"server_id\":\"NTEST\",\"server_name\":\"test\",\"version\":\"2.10.0\",\"proto\":1,\"host\":\"127.0.0.1\",\"port\":" +
                            std::to_string(port()) + ",\"max_payload\":" + std::to_string(max_payload) + ",\"headers\":" + (headers ? "true" : "false");
            if (!user.empty() || !token.empty() || !nkey.empty()) {
                s += ",\"auth_required\":true";
            }
            if (!nkey.empty()) {
                s += ",\"nonce\":\"" + nonce + "\"";
            }
            s += "}\r\n";
            return s;
        }

        // An nkey's signature over the nonce checked: the public key's text
        // read back (base32, prefix, CRC) and Ed25519 verified
        bool nkey_ok(const std::string& pub, const std::string& sig) {
            if (pub != nkey) {
                return false;
            }
            auto raw = nd::NkeyBase32.decode(sgcl::string(pub));
            if (!raw || raw->size() != 35) {
                return false;
            }
            auto b = reinterpret_cast<const uint8_t*>(raw->data());
            if (uint16_t(b[33] | b[34] << 8) != nd::nats_crc16(b, 33) || b[0] != nd::NkeyUserPrefix) {
                return false;
            }
            auto key = crypto::ed25519::public_key::from_bytes(slice<const byte>(raw->data() + 1, 32));
            auto s = encoding::base64::raw_url.decode(sgcl::string(sig));
            if (!key || !s || s->size() != 64) {
                return false;
            }
            return key->verify(slice<const byte>(reinterpret_cast<const byte*>(nonce.data()), nonce.size()), slice<const byte>(s->data(), s->size()));
        }

        static bool match(std::string_view filter, std::string_view subject) {
            return nd::nats_match(filter, subject);
        }

        // A message to every subscription of its subject, one member of
        // each queue group
        void route(const std::string& subject, const std::string& reply, const std::string& hdr, const std::string& data, bool has_headers) {
            std::map<std::string, std::vector<size_t>> groups;
            std::vector<size_t> plain;
            for (size_t i = 0; i < subs.size(); ++i) {
                if (match(subs[i].subject, subject)) {
                    if (subs[i].queue.empty()) {
                        plain.push_back(i);
                    } else {
                        groups[subs[i].queue].push_back(i);
                    }
                }
            }
            for (auto& [q, members] : groups) {
                size_t& t = turns[q];
                plain.push_back(members[t++ % members.size()]);
            }
            std::vector<size_t> done;
            for (size_t i : plain) {
                Sub& s = subs[i];
                auto c = conns.find(s.conn);
                if (c == conns.end()) {
                    continue;
                }
                std::string out;
                if (has_headers && c->second->headers) {
                    out = "HMSG " + subject + " " + s.sid + (reply.empty() ? "" : " " + reply) + " " + std::to_string(hdr.size()) + " " +
                          std::to_string(hdr.size() + data.size()) + "\r\n" + hdr + data + "\r\n";
                } else {
                    out = "MSG " + subject + " " + s.sid + (reply.empty() ? "" : " " + reply) + " " + std::to_string(data.size()) + "\r\n" + data + "\r\n";
                }
                (void)c->second->out.try_send(std::move(out));
                if (s.max && ++s.delivered >= s.max) {
                    done.push_back(i);
                }
            }
            for (auto it = done.rbegin(); it != done.rend(); ++it) {
                subs.erase(subs.begin() + long(*it));
            }
            if (plain.empty() && !reply.empty()) {
                // no responders: a 503 to the requester's reply subject, where it asked for them
                for (auto& s : subs) {
                    if (match(s.subject, reply)) {
                        auto c = conns.find(s.conn);
                        if (c != conns.end() && c->second->no_responders && c->second->headers) {
                            std::string h = "NATS/1.0 503\r\n\r\n";
                            (void)c->second->out.try_send("HMSG " + reply + " " + s.sid + " " + std::to_string(h.size()) + " " + std::to_string(h.size()) + "\r\n" + h + "\r\n");
                        }
                    }
                }
            }
        }

        static std::vector<std::string> words(std::string_view line) {
            std::vector<std::string> out;
            size_t i = 0;
            while (i < line.size()) {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                    ++i;
                }
                size_t s = i;
                while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
                    ++i;
                }
                if (i > s) {
                    out.emplace_back(line.substr(s, i - s));
                }
            }
            return out;
        }

        static async::task<> serve(Server* self, tracked_ptr<Conn> conn) {
            (void)conn->out.try_send(self->info());
            std::string buf;
            std::vector<char> block(65536);
            bool connected = false;
            for (;;) {
                size_t eol = buf.find("\r\n");
                if (eol == std::string::npos) {
                    auto r = co_await conn->c.async_read(slice<byte>(reinterpret_cast<byte*>(block.data()), block.size()));
                    if (!r || *r == 0) {
                        break;
                    }
                    buf.append(block.data(), *r);
                    continue;
                }
                std::string line = buf.substr(0, eol);
                auto w = words(line);
                if (w.empty()) {
                    buf.erase(0, eol + 2);
                    continue;
                }
                std::string op = w[0];
                for (char& ch : op) {
                    ch = char(std::toupper(uint8_t(ch)));
                }
                if (op == "PUB" || op == "HPUB") {
                    bool h = op == "HPUB";
                    if (w.size() < (h ? 4u : 3u)) {
                        (void)conn->out.try_send("-ERR 'Unknown Protocol Operation'\r\n");
                        break;
                    }
                    size_t total = std::stoul(w.back());
                    size_t hlen = h ? std::stoul(w[w.size() - 2]) : 0;
                    if (total > self->max_payload) {
                        (void)conn->out.try_send("-ERR 'Maximum Payload Violation'\r\n");
                        break;
                    }
                    if (buf.size() < eol + 2 + total + 2) {
                        auto r = co_await conn->c.async_read(slice<byte>(reinterpret_cast<byte*>(block.data()), block.size()));
                        if (!r || *r == 0) {
                            break;
                        }
                        buf.append(block.data(), *r);
                        continue;
                    }
                    std::string payload = buf.substr(eol + 2, total);
                    buf.erase(0, eol + 2 + total + 2);
                    std::string subject = w[1];
                    std::string reply = w.size() == (h ? 5u : 4u) ? w[2] : std::string();
                    ++self->published;
                    std::lock_guard g(self->mu);
                    self->route(subject, reply, payload.substr(0, hlen), payload.substr(hlen), h);
                    continue;
                }
                buf.erase(0, eol + 2);
                if (op == "CONNECT") {
                    std::string json = line.substr(8);
                    auto j = encoding::json::parse(sgcl::string(json));
                    if (!j) {
                        (void)conn->out.try_send("-ERR 'Unknown Protocol Operation'\r\n");
                        break;
                    }
                    {
                        std::lock_guard g(self->mu);
                        self->last_connect = json;
                    }
                    conn->headers = (*j)["headers"].as_bool().value_or(false);
                    conn->no_responders = (*j)["no_responders"].as_bool().value_or(false);
                    bool ok = true;
                    if (!self->user.empty()) {
                        ok = str((*j)["user"].as_string(sgcl::string())) == self->user && str((*j)["pass"].as_string(sgcl::string())) == self->password;
                    } else if (!self->token.empty()) {
                        ok = str((*j)["auth_token"].as_string(sgcl::string())) == self->token;
                    } else if (!self->nkey.empty()) {
                        ok = self->nkey_ok(str((*j)["nkey"].as_string(sgcl::string())), str((*j)["sig"].as_string(sgcl::string())));
                    }
                    if (!ok) {
                        (void)conn->out.try_send("-ERR 'Authorization Violation'\r\n");
                        break;
                    }
                    connected = true;
                    continue;
                }
                if (!connected) {
                    (void)conn->out.try_send("-ERR 'Authorization Violation'\r\n");
                    break;
                }
                if (op == "PING") {
                    if (!self->silent.load()) {
                        (void)conn->out.try_send("PONG\r\n");
                    }
                } else if (op == "PONG") {
                } else if (op == "SUB") {
                    if (w.size() != 3 && w.size() != 4) {
                        (void)conn->out.try_send("-ERR 'Unknown Protocol Operation'\r\n");
                        break;
                    }
                    if (!self->deny_subscribe.empty() && w[1] == self->deny_subscribe) {
                        (void)conn->out.try_send("-ERR 'Permissions Violation for Subscription to \"" + w[1] + "\"'\r\n");
                        continue;
                    }
                    std::lock_guard g(self->mu);
                    self->subs.push_back({conn->id, w.back(), w[1], w.size() == 4 ? w[2] : std::string(), 0, 0});
                } else if (op == "UNSUB") {
                    std::lock_guard g(self->mu);
                    for (auto it = self->subs.begin(); it != self->subs.end(); ++it) {
                        if (it->conn == conn->id && it->sid == w[1]) {
                            if (w.size() == 3) {
                                it->max = std::stoull(w[2]);
                                if (it->delivered >= it->max) {
                                    self->subs.erase(it);
                                }
                            } else {
                                self->subs.erase(it);
                            }
                            break;
                        }
                    }
                } else {
                    (void)conn->out.try_send("-ERR 'Unknown Protocol Operation'\r\n");
                    break;
                }
            }
            {
                std::lock_guard g(self->mu);
                std::erase_if(self->subs, [&](const Sub& s) { return s.conn == conn->id; });
                self->conns.erase(conn->id);
            }
            conn->out.close();
            co_await async::sleep_until(sgcl::clock::now() + std::chrono::milliseconds(20));
            (void)conn->c.close();
            --self->connections;
        }
    };
}
