//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A minimal LDAP server for the client's tests, written from RFC 4511 on
// encoding::asn1 (no slapd here): a directory in memory, simple and SASL
// PLAIN/EXTERNAL binds, StartTLS and Who am I?, search with every filter
// of RFC 4515, the scopes, the limits, types only and paged results, a
// referral and search references, add, modify, delete, modify DN,
// compare, the Notice of Disconnection. OpenLDAP's command line clients
// (/usr/bin/ldapsearch and the others) check it in tests/net/ldap/interop.cpp.
#pragma once

#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/ldap.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace ldap_test {
    using namespace sgcl;
    using encoding::asn1;
    namespace ld = sgcl::net::ldap::detail;

    inline std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    inline std::string lower(std::string s) {
        for (char& c : s) {
            c = char(std::tolower(uint8_t(c)));
        }
        return s;
    }

    // A DN normalized for comparison: lower case, no spaces around "," and "="
    inline std::string norm(std::string_view dn) {
        std::string out;
        for (size_t i = 0; i < dn.size(); ++i) {
            char c = dn[i];
            if (c == ' ' && (out.empty() || out.back() == ',' || out.back() == '=' || (i + 1 < dn.size() && (dn[i + 1] == ',' || dn[i + 1] == '=')))) {
                continue;
            }
            out += char(std::tolower(uint8_t(c)));
        }
        return out;
    }

    inline std::string parent_of(const std::string& dn) {
        size_t c = dn.find(',');
        return c == std::string::npos ? std::string() : dn.substr(c + 1);
    }

    inline std::string slurp(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    inline std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    struct Entry {
        std::string dn;   // as written
        std::vector<std::pair<std::string, std::vector<std::string>>> attrs;

        std::vector<std::string>* find(const std::string& name) {
            for (auto& [n, v] : attrs) {
                if (lower(n) == lower(name)) {
                    return &v;
                }
            }
            return nullptr;
        }
    };

    struct Directory {
        std::mutex m;
        std::map<std::string, Entry> entries;   // by normalized DN
        std::atomic<int> searches{0};
        std::atomic<int> connections{0};

        void add(const std::string& dn, std::vector<std::pair<std::string, std::vector<std::string>>> attrs) {
            std::lock_guard g(m);
            entries[norm(dn)] = Entry{dn, std::move(attrs)};
        }

        // The tree the tests use
        void populate() {
            add("dc=example,dc=com", {{"objectClass", {"top", "domain"}}, {"dc", {"example"}}});
            add("ou=people,dc=example,dc=com", {{"objectClass", {"organizationalUnit"}}, {"ou", {"people"}}});
            add("cn=admin,dc=example,dc=com", {{"objectClass", {"person"}}, {"cn", {"admin"}}, {"sn", {"Admin"}}, {"userPassword", {"secret"}}});
            const char* people[][4] = {{"alice", "Alice", "Smith", "30"}, {"bob", "Bob", "Jones", "25"}, {"carol", "Carol", "Smith", "41"}, {"dave", "Dave", "Brown", "35"},
                                       {"eve", "Eve", "Black", "28"}};
            for (auto& p : people) {
                add(std::string("uid=") + p[0] + ",ou=people,dc=example,dc=com",
                    {{"objectClass", {"person", "inetOrgPerson"}}, {"uid", {p[0]}}, {"cn", {std::string(p[1]) + " " + p[2]}}, {"givenName", {p[1]}}, {"sn", {p[2]}},
                     {"mail", {std::string(p[0]) + "@example.com"}}, {"age", {p[3]}}, {"userPassword", {std::string(p[0]) + "-pw"}}});
            }
            add("ou=groups,dc=example,dc=com", {{"objectClass", {"organizationalUnit"}}, {"ou", {"groups"}}});
            add("cn=staff,ou=groups,dc=example,dc=com", {{"objectClass", {"groupOfNames"}}, {"cn", {"staff"}}, {"member", {"uid=alice,ou=people,dc=example,dc=com", "uid=bob,ou=people,dc=example,dc=com"}}});
        }
    };

    struct Session {
        std::string identity;   // "" anonymous, "dn:..." or "u:..."
        bool tls = false;
    };

    // Filter evaluation (RFC 4511 §4.5.1.7) over an entry: true, false, or
    // undefined (-1) as RFC 4511 has it
    inline int eval(const asn1& f, Entry& e) {
        auto text = [](const asn1& x) { return ld::ldap_text(x); };
        auto values = [&](const std::string& attr) -> std::vector<std::string>* {
            std::string name = attr.substr(0, attr.find(';'));
            return e.find(name);
        };
        auto prim = [](const asn1& x) {
            auto c = x.content();
            return std::string(reinterpret_cast<const char*>(c.data()), c.size());
        };
        if (f.cls() != asn1::tag_class::context_specific) {
            return -1;
        }
        switch (f.tag()) {
            case 0: {
                int r = 1;
                for (auto x : f) {
                    int v = eval(x, e);
                    if (v == 0) {
                        return 0;
                    }
                    if (v < 0) {
                        r = -1;
                    }
                }
                return r;
            }
            case 1: {
                int r = 0;
                for (auto x : f) {
                    int v = eval(x, e);
                    if (v == 1) {
                        return 1;
                    }
                    if (v < 0) {
                        r = -1;
                    }
                }
                return r;
            }
            case 2: {
                int v = eval(f[0], e);
                return v < 0 ? -1 : !v;
            }
            case 3:
            case 5:
            case 6:
            case 8: {
                auto vs = values(text(f[0]));
                if (!vs) {
                    return 0;
                }
                std::string want = lower(text(f[1]));
                for (auto& v : *vs) {
                    std::string lv = lower(v);
                    bool numeric = !lv.empty() && !want.empty() && std::all_of(lv.begin(), lv.end(), ::isdigit) && std::all_of(want.begin(), want.end(), ::isdigit);
                    int cmp = numeric ? (std::stoll(lv) < std::stoll(want) ? -1 : std::stoll(lv) > std::stoll(want)) : lv.compare(want);
                    if ((f.tag() == 3 && cmp == 0) || (f.tag() == 5 && cmp >= 0) || (f.tag() == 6 && cmp <= 0) || (f.tag() == 8 && cmp == 0)) {
                        return 1;
                    }
                }
                return 0;
            }
            case 4: {
                auto vs = values(text(f[0]));
                if (!vs) {
                    return 0;
                }
                for (auto& v0 : *vs) {
                    std::string v = lower(v0);
                    size_t at = 0;
                    bool ok = true;
                    for (auto part : f[1]) {
                        std::string p = lower(prim(part));
                        if (part.tag() == 0) {
                            ok &= v.compare(0, p.size(), p) == 0;
                            at = p.size();
                        } else if (part.tag() == 1) {
                            size_t found = v.find(p, at);
                            ok &= found != std::string::npos;
                            at = found == std::string::npos ? v.size() : found + p.size();
                        } else {
                            ok &= v.size() >= at + p.size() && v.compare(v.size() - p.size(), p.size(), p) == 0;
                        }
                    }
                    if (ok) {
                        return 1;
                    }
                }
                return 0;
            }
            case 7:
                return values(prim(f)) ? 1 : 0;
            case 9: {
                std::string rule, type, value;
                bool dn = false;
                for (auto x : f) {
                    if (x.tag() == 1) {
                        rule = prim(x);
                    } else if (x.tag() == 2) {
                        type = prim(x);
                    } else if (x.tag() == 3) {
                        value = prim(x);
                    } else if (x.tag() == 4) {
                        dn = true;
                    }
                }
                (void)dn;
                bool exact = rule == "caseExactMatch" || rule == "2.5.13.5";
                auto check = [&](const std::vector<std::string>& vs) {
                    for (auto& v : vs) {
                        if (exact ? v == value : lower(v) == lower(value)) {
                            return true;
                        }
                    }
                    return false;
                };
                if (!type.empty()) {
                    auto vs = values(type);
                    return vs && check(*vs) ? 1 : 0;
                }
                for (auto& [n, vs] : e.attrs) {
                    if (check(vs)) {
                        return 1;
                    }
                }
                return 0;
            }
        }
        return -1;
    }

    struct Server {
        Directory dir;
        net::listener listener;
        async::task<> serving;
        std::atomic<bool> disconnect_next_search{false};   // a Notice of Disconnection in place of a search's answer
        net::tls::config tls;
        std::mutex conns_lock;
        vector<net::connection> conns;   // closed by the destructor: a client the test leaves open does not hold it

        explicit Server(bool tls_listener = false) {
            dir.populate();
            tls.identities = {net::tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
            listener = tls_listener ? net::tls::listen("127.0.0.1:0", tls).value() : net::tcp::listen("127.0.0.1:0").value();
            serving = async::spawn(accept_loop(this, listener));
        }

        ~Server() {
            (void)listener.close();
            serving.wait();
            {
                std::lock_guard g(conns_lock);
                for (auto& c : conns) {
                    (void)c.close();
                }
            }
            for (int i = 0; i < 500 && dir.connections.load() > 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        uint16_t port() const {
            return listener.local_endpoint().port();
        }

        sgcl::string url(const char* scheme = "ldap", const char* host = "127.0.0.1") const {
            return sgcl::string(std::string(scheme) + "://" + host + ":" + std::to_string(port()));
        }

        static async::task<> accept_loop(Server* self, net::listener l) {
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    co_return;
                }
                ++self->dir.connections;
                {
                    std::lock_guard g(self->conns_lock);
                    self->conns.push_back(*c);
                }
                async::go(serve(self, *c));
            }
        }

        static asn1 result_op(uint32_t tag, int code, const std::string& message = {}, const std::string& matched = {}, const std::vector<std::string>& referrals = {}) {
            vector<asn1> parts{asn1::enumerated(code), ld::ldap_str(matched), ld::ldap_str(message)};
            if (!referrals.empty()) {
                vector<asn1> urls;
                for (auto& u : referrals) {
                    urls.push_back(ld::ldap_str(u));
                }
                parts.push_back(ld::ldap_cons(asn1::tag_class::context_specific, 3, urls));
            }
            return ld::ldap_app(tag, parts);
        }

        static async::task<bool> send(net::connection c, int32_t id, asn1 op, vector<asn1> controls = {}) {
            auto bytes = ld::ldap_message(id, op, controls);
            co_return bool(co_await c.async_write(slice<const byte>(bytes.data(), bytes.size())));
        }

        static async::task<> serve(Server* self, net::connection c) {
            Session sess;
            std::string buf;
            std::vector<char> block(16384);
            for (;;) {
                size_t total = 0;
                int f = ld::ldap_frame(buf, total);
                if (f < 0) {
                    break;
                }
                if (f == 0 || buf.size() < total) {
                    auto r = co_await c.async_read(slice<byte>(reinterpret_cast<byte*>(block.data()), block.size()));
                    if (!r || *r == 0) {
                        break;
                    }
                    buf.append(block.data(), *r);
                    continue;
                }
                vector<byte> bytes(reinterpret_cast<const byte*>(buf.data()), reinterpret_cast<const byte*>(buf.data()) + total);
                buf.erase(0, total);
                auto m = asn1::parse(bytes, asn1::ber);
                if (!m || m->size() < 2) {
                    break;
                }
                int32_t id = int32_t((*m)[0].as_int().value_or(0));
                asn1 op = (*m)[1];
                vector<asn1> controls;
                if (m->size() > 2 && (*m)[2].is_context(0)) {
                    for (auto x : (*m)[2]) {
                        controls.push_back(x);
                    }
                }
                if (op.tag() == ld::op::unbind_request) {
                    break;
                }
                if (op.tag() == ld::op::abandon_request) {
                    continue;
                }
                bool go_on = co_await handle(self, c, sess, id, op, controls, buf);
                if (!go_on) {
                    break;
                }
                if (sess.tls && !net::tls::state_of(c)) {
                    // StartTLS answered: the rest of the session over TLS
                    auto t = co_await net::tls::async_server(c, self->tls);
                    if (!t) {
                        break;
                    }
                    c = *t;
                }
            }
            (void)c.close();
            --self->dir.connections;
        }

        static async::task<bool> handle(Server* self, net::connection c, Session& sess, int32_t id, asn1 op, vector<asn1> controls, std::string& buf) {
            Directory& d = self->dir;
            switch (op.tag()) {
                case ld::op::bind_request: {
                    std::string name = ld::ldap_text(op[1]);
                    asn1 auth = op[2];
                    if (auth.is_context(0)) {
                        auto cc = auth.content();
                        std::string pw(reinterpret_cast<const char*>(cc.data()), cc.size());
                        if (name.empty() && pw.empty()) {
                            sess.identity.clear();
                            co_return co_await send(c, id, result_op(ld::op::bind_response, 0));
                        }
                        std::lock_guard g(d.m);
                        auto it = d.entries.find(norm(name));
                        auto p = it == d.entries.end() ? nullptr : it->second.find("userPassword");
                        if (!p || std::find(p->begin(), p->end(), pw) == p->end()) {
                            co_return co_await send(c, id, result_op(ld::op::bind_response, 49, "invalid credentials"));
                        }
                        sess.identity = "dn:" + it->second.dn;
                        co_return co_await send(c, id, result_op(ld::op::bind_response, 0));
                    }
                    if (auth.is_context(3)) {
                        std::string mech = ld::ldap_text(auth[0]);
                        std::string cred = auth.size() > 1 ? ld::ldap_text(auth[1]) : std::string();
                        if (mech == "PLAIN") {
                            size_t a = cred.find('\0'), b = a == std::string::npos ? a : cred.find('\0', a + 1);
                            if (b == std::string::npos) {
                                co_return co_await send(c, id, result_op(ld::op::bind_response, 49));
                            }
                            std::string user = cred.substr(a + 1, b - a - 1), pw = cred.substr(b + 1);
                            std::lock_guard g(d.m);
                            auto it = d.entries.find(norm("uid=" + user + ",ou=people,dc=example,dc=com"));
                            auto p = it == d.entries.end() ? nullptr : it->second.find("userPassword");
                            if (!p || (*p)[0] != pw) {
                                co_return co_await send(c, id, result_op(ld::op::bind_response, 49, "invalid credentials"));
                            }
                            sess.identity = "u:" + user;
                            co_return co_await send(c, id, result_op(ld::op::bind_response, 0));
                        }
                        if (mech == "EXTERNAL") {
                            if (!sess.tls && !net::tls::state_of(c)) {
                                co_return co_await send(c, id, result_op(ld::op::bind_response, 48, "EXTERNAL needs TLS"));
                            }
                            sess.identity = cred.empty() ? "dn:cn=tls-client" : cred;
                            co_return co_await send(c, id, result_op(ld::op::bind_response, 0));
                        }
                        co_return co_await send(c, id, result_op(ld::op::bind_response, 7, "unsupported mechanism"));
                    }
                    co_return co_await send(c, id, result_op(ld::op::bind_response, 2));
                }
                case ld::op::extended_request: {
                    std::string oid;
                    for (auto x : op) {
                        if (x.is_context(0)) {
                            auto cc = x.content();
                            oid.assign(reinterpret_cast<const char*>(cc.data()), cc.size());
                        }
                    }
                    if (oid == ld::OidWhoAmI) {
                        vector<asn1> parts{asn1::enumerated(0), ld::ldap_str(""), ld::ldap_str("")};
                        parts.push_back(ld::ldap_prim(asn1::tag_class::context_specific, 11, sess.identity));
                        co_return co_await send(c, id, ld::ldap_app(ld::op::extended_response, parts));
                    }
                    if (oid == ld::OidStartTls) {
                        if (sess.tls || net::tls::state_of(c) || !buf.empty()) {
                            co_return co_await send(c, id, result_op(ld::op::extended_response, 1, "TLS already in place"));
                        }
                        vector<asn1> parts{asn1::enumerated(0), ld::ldap_str(""), ld::ldap_str(""),
                                           ld::ldap_prim(asn1::tag_class::context_specific, 10, std::string(ld::OidStartTls))};
                        sess.tls = true;
                        co_return co_await send(c, id, ld::ldap_app(ld::op::extended_response, parts));
                    }
                    co_return co_await send(c, id, result_op(ld::op::extended_response, 2, "unknown extended operation"));
                }
                case ld::op::search_request: {
                    ++d.searches;
                    if (self->disconnect_next_search.exchange(false)) {
                        vector<asn1> parts{asn1::enumerated(52), ld::ldap_str(""), ld::ldap_str("going away"),
                                           ld::ldap_prim(asn1::tag_class::context_specific, 10, std::string(ld::OidNoticeOfDisconnection))};
                        (void)co_await send(c, 0, ld::ldap_app(ld::op::extended_response, parts));
                        co_return false;
                    }
                    std::string base = norm(ld::ldap_text(op[0]));
                    int scope = int(op[1].as_int().value_or(2));
                    int64_t size_limit = op[3].as_int().value_or(0);
                    bool types_only = op[5].as_bool().value_or(false);
                    asn1 filter = op[6];
                    std::vector<std::string> want;
                    for (auto a : op[7]) {
                        want.push_back(lower(ld::ldap_text(a)));
                    }
                    if (base.rfind("ou=never,", 0) == 0) {
                        co_return true;   // a search this server never answers: the client's timeout
                    }
                    if (base.size() >= 16 && base.find("ou=elsewhere") != std::string::npos) {
                        co_return co_await send(c, id, result_op(ld::op::search_done, 10, "elsewhere", "", {"ldap://other.example.com/ou=elsewhere,dc=example,dc=com"}));
                    }
                    // paged results: the cookie is the count already sent
                    int64_t page = 0, offset = 0;
                    for (auto& ctl : controls) {
                        if (ld::ldap_text(ctl[0]) == ld::OidPagedResults) {
                            auto v = asn1::parse(*ctl[ctl.size() - 1].as_bytes(), asn1::ber);
                            page = (*v)[0].as_int().value_or(0);
                            std::string cookie = ld::ldap_text((*v)[1]);
                            offset = cookie.empty() ? 0 : std::stoll(cookie);
                        }
                    }
                    std::vector<Entry> matched;
                    {
                        std::lock_guard g(d.m);
                        if (d.entries.find(base) == d.entries.end()) {
                            std::string m = base;
                            while (!m.empty() && d.entries.find(m) == d.entries.end()) {
                                m = parent_of(m);
                            }
                            co_return co_await send(c, id, result_op(ld::op::search_done, 32, "no such object", m.empty() ? "" : d.entries[m].dn));
                        }
                        for (auto& [k, e] : d.entries) {
                            bool in = k == base ? scope != 1 : (scope == 1 ? parent_of(k) == base : scope == 2 && k.size() > base.size() && k.compare(k.size() - base.size(), base.size(), base) == 0 && k[k.size() - base.size() - 1] == ',');
                            if (in && eval(filter, e) == 1) {
                                matched.push_back(e);
                            }
                        }
                    }
                    int64_t end = page ? std::min<int64_t>(int64_t(matched.size()), offset + page) : int64_t(matched.size());
                    int64_t sent = 0;
                    for (int64_t i = offset; i < end; ++i) {
                        if (size_limit && sent >= size_limit) {
                            co_return co_await send(c, id, result_op(ld::op::search_done, 4));
                        }
                        Entry& e = matched[size_t(i)];
                        vector<asn1> attrs;
                        for (auto& [n, vs] : e.attrs) {
                            if (lower(n) == "userpassword") {
                                continue;
                            }
                            if (!want.empty() && std::find(want.begin(), want.end(), lower(n)) == want.end() && std::find(want.begin(), want.end(), "*") == want.end()) {
                                continue;
                            }
                            vector<asn1> vals;
                            if (!types_only) {
                                for (auto& v : vs) {
                                    vals.push_back(ld::ldap_str(v));
                                }
                            }
                            attrs.push_back(asn1::sequence({ld::ldap_str(n), ld::ldap_cons(asn1::tag_class::universal, 17, vals)}));
                        }
                        if (!co_await send(c, id, ld::ldap_app(ld::op::search_entry, vector<asn1>{ld::ldap_str(e.dn), asn1::sequence(attrs)}))) {
                            co_return false;
                        }
                        ++sent;
                    }
                    if (scope != 0 && base == "dc=example,dc=com" && offset == 0) {
                        (void)co_await send(c, id, ld::ldap_app(ld::op::search_reference, vector<asn1>{ld::ldap_str("ldap://replica.example.com/dc=example,dc=com")}));
                    }
                    vector<asn1> out_controls;
                    if (page) {
                        std::string cookie = end < int64_t(matched.size()) ? std::to_string(end) : std::string();
                        out_controls.push_back(ld::ldap_control(ld::OidPagedResults, false, asn1::sequence({asn1::integer(0), ld::ldap_str(cookie)})));
                    }
                    co_return co_await send(c, id, result_op(ld::op::search_done, 0), out_controls);
                }
                case ld::op::add_request: {
                    std::string dn = ld::ldap_text(op[0]);
                    Entry e{dn, {}};
                    for (auto a : op[1]) {
                        std::vector<std::string> vs;
                        for (auto v : a[1]) {
                            vs.push_back(ld::ldap_text(v));
                        }
                        e.attrs.emplace_back(ld::ldap_text(a[0]), vs);
                    }
                    std::lock_guard g(d.m);
                    if (d.entries.count(norm(dn))) {
                        co_return co_await send(c, id, result_op(ld::op::add_response, 68, "entry already exists"));
                    }
                    if (!d.entries.count(parent_of(norm(dn)))) {
                        co_return co_await send(c, id, result_op(ld::op::add_response, 32, "no parent"));
                    }
                    d.entries[norm(dn)] = e;
                    co_return co_await send(c, id, result_op(ld::op::add_response, 0));
                }
                case ld::op::modify_request: {
                    std::string dn = norm(ld::ldap_text(op[0]));
                    std::lock_guard g(d.m);
                    auto it = d.entries.find(dn);
                    if (it == d.entries.end()) {
                        co_return co_await send(c, id, result_op(ld::op::modify_response, 32));
                    }
                    Entry copy = it->second;
                    for (auto ch : op[1]) {
                        int kind = int(ch[0].as_int().value_or(-1));
                        std::string name = ld::ldap_text(ch[1][0]);
                        std::vector<std::string> vs;
                        for (auto v : ch[1][1]) {
                            vs.push_back(ld::ldap_text(v));
                        }
                        auto cur = copy.find(name);
                        if (kind == 0) {
                            if (!cur) {
                                copy.attrs.emplace_back(name, std::vector<std::string>());
                                cur = &copy.attrs.back().second;
                            }
                            for (auto& v : vs) {
                                if (std::find(cur->begin(), cur->end(), v) != cur->end()) {
                                    co_return co_await send(c, id, result_op(ld::op::modify_response, 20, "value exists"));
                                }
                                cur->push_back(v);
                            }
                        } else if (kind == 1) {
                            if (!cur) {
                                co_return co_await send(c, id, result_op(ld::op::modify_response, 16, "no such attribute"));
                            }
                            if (vs.empty()) {
                                cur->clear();
                            }
                            for (auto& v : vs) {
                                cur->erase(std::remove(cur->begin(), cur->end(), v), cur->end());
                            }
                        } else if (kind == 2) {
                            if (cur) {
                                *cur = vs;
                            } else if (!vs.empty()) {
                                copy.attrs.emplace_back(name, vs);
                            }
                        } else if (kind == 3) {
                            if (!cur || cur->empty() || vs.size() != 1) {
                                co_return co_await send(c, id, result_op(ld::op::modify_response, 21));
                            }
                            (*cur)[0] = std::to_string(std::stoll((*cur)[0]) + std::stoll(vs[0]));
                        } else {
                            co_return co_await send(c, id, result_op(ld::op::modify_response, 2));
                        }
                        copy.attrs.erase(std::remove_if(copy.attrs.begin(), copy.attrs.end(), [](auto& a) { return a.second.empty(); }), copy.attrs.end());
                    }
                    it->second = copy;
                    co_return co_await send(c, id, result_op(ld::op::modify_response, 0));
                }
                case ld::op::del_request: {
                    auto cc = op.content();
                    std::string dn = norm(std::string(reinterpret_cast<const char*>(cc.data()), cc.size()));
                    std::lock_guard g(d.m);
                    if (!d.entries.count(dn)) {
                        co_return co_await send(c, id, result_op(ld::op::del_response, 32));
                    }
                    for (auto& [k, e] : d.entries) {
                        if (parent_of(k) == dn) {
                            co_return co_await send(c, id, result_op(ld::op::del_response, 66, "not a leaf"));
                        }
                    }
                    d.entries.erase(dn);
                    co_return co_await send(c, id, result_op(ld::op::del_response, 0));
                }
                case ld::op::moddn_request: {
                    std::string dn = norm(ld::ldap_text(op[0]));
                    std::string rdn = ld::ldap_text(op[1]);
                    bool delete_old = op[2].as_bool().value_or(true);
                    std::string superior;
                    if (op.size() > 3 && op[3].is_context(0)) {
                        auto cc = op[3].content();
                        superior = std::string(reinterpret_cast<const char*>(cc.data()), cc.size());
                    }
                    std::lock_guard g(d.m);
                    auto it = d.entries.find(dn);
                    if (it == d.entries.end()) {
                        co_return co_await send(c, id, result_op(ld::op::moddn_response, 32));
                    }
                    Entry e = it->second;
                    std::string parent = superior.empty() ? e.dn.substr(e.dn.find(',') + 1) : superior;
                    std::string old_rdn = e.dn.substr(0, e.dn.find(','));
                    std::string newdn = rdn + "," + parent;
                    if (d.entries.count(norm(newdn))) {
                        co_return co_await send(c, id, result_op(ld::op::moddn_response, 68));
                    }
                    auto split = [](const std::string& r) { return std::make_pair(r.substr(0, r.find('=')), r.substr(r.find('=') + 1)); };
                    auto [oa, ov] = split(old_rdn);
                    auto [na, nv] = split(rdn);
                    if (delete_old) {
                        if (auto v = e.find(oa)) {
                            v->erase(std::remove(v->begin(), v->end(), ov), v->end());
                        }
                    }
                    auto v = e.find(na);
                    if (!v) {
                        e.attrs.emplace_back(na, std::vector<std::string>{nv});
                    } else if (std::find(v->begin(), v->end(), nv) == v->end()) {
                        v->push_back(nv);
                    }
                    e.dn = newdn;
                    d.entries.erase(it);
                    d.entries[norm(newdn)] = e;
                    co_return co_await send(c, id, result_op(ld::op::moddn_response, 0));
                }
                case ld::op::compare_request: {
                    std::string dn = norm(ld::ldap_text(op[0]));
                    std::string attr = ld::ldap_text(op[1][0]), value = ld::ldap_text(op[1][1]);
                    std::lock_guard g(d.m);
                    auto it = d.entries.find(dn);
                    if (it == d.entries.end()) {
                        co_return co_await send(c, id, result_op(ld::op::compare_response, 32));
                    }
                    auto vs = it->second.find(attr);
                    if (!vs) {
                        co_return co_await send(c, id, result_op(ld::op::compare_response, 16));
                    }
                    bool has = false;
                    for (auto& v : *vs) {
                        has |= lower(v) == lower(value);
                    }
                    co_return co_await send(c, id, result_op(ld::op::compare_response, has ? 6 : 5));
                }
            }
            co_return co_await send(c, id, result_op(ld::op::extended_response, 2, "unknown operation"));
        }
    };
}
