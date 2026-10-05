//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/mdns_engine.h"
#include "error.h"
#include "interface.h"
#include "ip.h"
#include "../async/coroutine.h"
#include "../async/stop_token.h"
#include "../core/aliases.h"
#include "../core/string.h"
#include "../core/vector.h"

#include <initializer_list>
#include <mutex>
#include <unistd.h>

namespace sgcl::net {
    namespace detail {
        using namespace sgcl::detail;
        struct TxtAccess;
    }

    // DNS-based service discovery (RFC 6763) over multicast DNS: the
    // services of the link, found (browse, resolve, types) and published
    // (publish, mdns::responder). Its types first; its functions after
    // mdns, whose options and responder they take.
    namespace dns_sd {
        // A TXT record of DNS-SD (RFC 6763 §6): keys, each with a value or
        // alone (a boolean attribute, §6.4), in the order set. A key is
        // printable ASCII without '=', compared without regard to case; a
        // value any bytes; an entry, "key=value", at most 255 bytes. A key
        // set again keeps its place with the new value
        class txt_record {
        public:
            txt_record() noexcept = default;

            // Each pair a key and its value; invalid_argument as set
            txt_record(std::initializer_list<pair<string, string>> pairs) {
                for (auto& [key, value] : pairs) {
                    set(key, value);
                }
            }

            // The key with its value ("key=value"); invalid_argument for a
            // key that is empty, holds '=' or a byte outside 0x20-0x7E, and
            // for an entry past 255 bytes
            void set(const string& key, const string& value) {
                _set(key, string::concat(key.view(), "=", value.view()));
            }

            // The key alone: an attribute present with no value ("key")
            void set(const string& key) {
                _set(key, key);
            }

            bool contains(const string& key) const noexcept {
                return _find(key.view()) != npos;
            }

            // The value; nullopt for a key not there and for a key alone
            optional<string> get(const string& key) const noexcept {
                size_t i = _find(key.view());
                if (i == npos) {
                    return nullopt;
                }
                std::string_view e = _entries[i].view();
                size_t eq = e.find('=');
                if (eq == std::string_view::npos) {
                    return nullopt;
                }
                return string(e.substr(eq + 1));
            }

            // Whether the key was there
            bool remove(const string& key) noexcept {
                size_t i = _find(key.view());
                if (i == npos) {
                    return false;
                }
                _entries.erase(_entries.begin() + ptrdiff_t(i));
                return true;
            }

            SGCL_INLINE_HOT size_t size() const noexcept {
                return _entries.size();
            }

            SGCL_INLINE_HOT bool empty() const noexcept {
                return _entries.empty();
            }

            // The keys in their order
            vector<string> keys() const noexcept {
                vector<string> out;
                out.reserve(_entries.size());
                for (auto& e : _entries) {
                    out.push_back(string(detail::mdns_txt_key(e.view())));
                }
                return out;
            }

            // The entries as the record carries them, "key=value" or "key"
            SGCL_INLINE_HOT const vector<string>& entries() const noexcept {
                return _entries;
            }

            friend bool operator==(const txt_record& a, const txt_record& b) noexcept {
                return a._entries == b._entries;
            }

        private:
            static constexpr size_t npos = size_t(-1);

            size_t _find(std::string_view key) const noexcept {
                for (size_t i = 0; i < _entries.size(); ++i) {
                    if (detail::mdns_txt_key_equal(detail::mdns_txt_key(_entries[i].view()), key)) {
                        return i;
                    }
                }
                return npos;
            }

            void _set(const string& key, string entry) {
                if (!detail::mdns_txt_key_valid(key.view())) {
                    throw invalid_argument("sgcl::net::dns_sd::txt_record: a key is printable ASCII without '=', at least one character");
                }
                if (entry.size() > 255) {
                    throw invalid_argument("sgcl::net::dns_sd::txt_record: an entry is at most 255 bytes");
                }
                size_t i = _find(key.view());
                if (i == npos) {
                    _entries.push_back(std::move(entry));
                } else {
                    _entries[i] = std::move(entry);
                }
            }

            friend struct sgcl::net::detail::TxtAccess;

            vector<string> _entries;
        };

        // A service instance (RFC 6763 §4.1): its name ("Living Room"),
        // its type ("_http._tcp"), its domain ("local."), the host it runs
        // on and its port, its TXT record; to publish, its subtypes
        // ("_printer", §7.1); found, the host's addresses and the interface
        // it was seen on
        struct service {
            string name;
            string type;
            string domain;                  // empty: "local."
            string host;                    // "mymac.local."; to publish, empty: the responder's
            uint16_t port = 0;
            txt_record txt;
            vector<string> subtypes;
            vector<ip_address> addresses;
            uint32_t interface = 0;
        };

        // What a browse saw: an instance come or gone (its record expired,
        // or its goodbye)
        struct event {
            bool added = false;
            string name;
            string type;
            string domain;
            uint32_t interface = 0;
        };

        class browser;
    }

    namespace detail {
        struct TxtAccess {
            static void assign(dns_sd::txt_record& t, const std::vector<std::string>& entries) noexcept {
                t._entries.clear();
                for (auto& e : entries) {
                    t._entries.push_back(string(std::string_view(e)));
                }
            }
        };

        // A label as text: printable, at most 63 bytes, not empty
        SGCL_INLINE_HOT bool mdns_label_ok(std::string_view l) noexcept {
            return !l.empty() && l.size() <= DnsMaxLabel;
        }

        // "_http._tcp" and the domain: the type's name in wire form; false
        // for a type that is not two labels, the first "_" and a name
        // (RFC 6335's 1-15 characters), the second "_tcp" or "_udp"
        inline bool mdns_type_name(const string& type, const string& domain, DnsName& type_out, DnsName& domain_out) noexcept {
            std::string d = domain.empty() ? std::string("local.") : std::string(domain.view());
            if (d.back() != '.') {
                d += '.';
            }
            if (!dns_name_from_text(d, domain_out)) {
                return false;
            }
            std::string_view t = type.view();
            if (!t.empty() && t.back() == '.') {
                t.remove_suffix(1);
            }
            size_t dot = t.find('.');
            if (dot == std::string_view::npos) {
                return false;
            }
            std::string_view app = t.substr(0, dot), proto = t.substr(dot + 1);
            if (size_t second = proto.find('.'); second != std::string_view::npos) {
                // the type with its domain: "_http._tcp.local."
                std::string rest(proto.substr(second + 1));
                rest += '.';
                proto = proto.substr(0, second);
                if (!dns_name_from_text(rest, domain_out)) {
                    return false;
                }
            }
            if (app.size() < 2 || app.size() > 16 || app[0] != '_' || (proto != "_tcp" && proto != "_udp")) {
                return false;
            }
            for (char c : app.substr(1)) {
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) {
                    return false;
                }
            }
            return mdns_name({app, proto}, domain_out, type_out);
        }

        // An instance's name: its label before the type
        inline bool mdns_instance_name(const string& name, const DnsName& type, DnsName& out) noexcept {
            return mdns_label_ok(name.view()) && mdns_name({name.view()}, type, out);
        }

        // This machine's name, its first label, as a host of .local:
        // gethostname's, the characters a host name takes kept and the
        // rest made '-'
        inline std::string mdns_machine_label() {
            char buf[256] = {};
            if (::gethostname(buf, sizeof(buf) - 1) != 0 || buf[0] == 0) {
                return "sgcl";
            }
            std::string label;
            for (char* p = buf; *p && *p != '.'; ++p) {
                char c = *p;
                label += (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ? c : '-';
            }
            if (label.empty()) {
                label = "sgcl";
            }
            if (label.size() > DnsMaxLabel) {
                label.resize(DnsMaxLabel);
            }
            return label;
        }

        inline MdnsSettings mdns_settings(const vector<network_interface>& interfaces, bool v4, bool v6) {
            MdnsSettings s;
            for (auto& i : interfaces) {
                s.interfaces.push_back(i);
            }
            s.v4 = v4;
            s.v6 = v6;
            return s;
        }

        inline io::error mdns_error(int code, const char* op, const string& what) noexcept {
            return system_error(code, op, what);
        }

        // A responder: its engine, its host entry, its services
        struct ResponderState {
            tracked_ptr<MdnsEngine> engine;
            tracked_ptr<MdnsEntry> host;
            std::mutex m;
            vector<tracked_ptr<MdnsEntry>> services;
            vector<dns_sd::service> published;   // as given, their names as they are kept up to date
            bool closed = false;

            ~ResponderState() {
                if (engine && !closed) {
                    mdns_stop(*engine);   // dropped unclosed: no goodbye, the sockets closed
                }
            }
        };

        struct ResponderAccess;

        // A browse's state: its engine (shared), its inbox, the name browsed
        struct BrowserState {
            tracked_ptr<MdnsEngine> engine;
            tracked_ptr<MdnsInbox> inbox;
            DnsName browsed;
            std::mutex m;
            bool closed = false;

            void close() noexcept {
                {
                    std::lock_guard lock(m);
                    if (closed) {
                        return;
                    }
                    closed = true;
                }
                mdns_unsubscribe(*engine, inbox, true);
                mdns_release(engine);
            }

            ~BrowserState() {
                if (engine && !closed) {
                    closed = true;
                    mdns_unsubscribe(*engine, inbox, true);
                    mdns_release(engine);
                }
            }
        };

        struct BrowserAccess;
    }

    // Multicast DNS (RFC 6762): names under .local answered by the hosts
    // of the link themselves. lookup asks for a host's addresses; the
    // responder answers for this one: its name, its addresses, the
    // services published on it. A structure of static functions and the
    // responder's handle, as dns is; the services themselves are dns_sd's.
    // A name under .local given to dns's lookups goes here too.
    struct mdns {
        // Which interfaces and families, how long a lookup waits, whether
        // its first query asks for unicast answers (QU, §5.4), and a
        // responder's host name
        struct options {
            vector<network_interface> interfaces;   // empty: every interface up and able to multicast, with an address (the loopback when there is no other)
            bool ipv4 = true;
            bool ipv6 = true;
            duration timeout = {};                  // a lookup's wait; zero: 2 s
            bool unicast_response = false;
            string host;                            // a responder's name ("mymac", ".local" added); empty: this machine's
        };

        // The addresses of a host of the link ("printer.local", or
        // "printer" with ".local" added): its A and AAAA records, by the
        // first answer; an IPv6 link-local address with the zone of the
        // interface it came on. net::errc::host_not_found when no host
        // answered within the timeout
        // `mdns::lookup(...)` on this thread, `co_await mdns::async_lookup(...)` in a task
        SGCL_INLINE_HOT static expected<vector<ip_address>, io::error> lookup(const string& host) {
            return _co_lookup(host, options(), async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static expected<vector<ip_address>, io::error> lookup(const string& host, const options& o) {
            return _co_lookup(host, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host, async::stop_token stop = {}) noexcept {
            return _co_lookup(host, options(), std::move(stop));
        }

        SGCL_INLINE_HOT static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup(host, o, std::move(stop));
        }

        // A multicast DNS responder (RFC 6762 §6, §8, §9): sockets of its
        // own on the interfaces, the host's name probed for and announced
        // with the addresses of each interface, the services published on
        // it, every query answered; renamed on a conflict. A handle of one
        // word: copies are the same responder. close() says goodbye (the
        // records with TTL 0) and closes the sockets; a responder dropped
        // without it closes them when the collector finds it, silently
        class responder {
        public:
            responder() noexcept = default;   // none; an operation on it is a contract violation

            // Started: the host's name probed for (a conflict renames it,
            // "mymac-2") and announced; waits for the probing, about a
            // second
            // `responder::start(...)` on this thread, `co_await responder::async_start(...)` in a task
            SGCL_INLINE_HOT static expected<responder, io::error> start() {
                return _co_start(options()).wait();
            }

            SGCL_INLINE_HOT static expected<responder, io::error> start(const options& o) {
                return _co_start(o).wait();
            }

            SGCL_INLINE_HOT static async::task<expected<responder, io::error>> async_start() noexcept {
                return _co_start(options());
            }

            SGCL_INLINE_HOT static async::task<expected<responder, io::error>> async_start(const options& o) noexcept {
                return _co_start(o);
            }

            // The host's name as it is held now, absolute ("mymac.local.")
            string host_name() const noexcept {
                std::lock_guard lock(_get().engine->m);
                return detail::dns_name_string(_get().host->name());
            }

            // A service published (its PTR, SRV, TXT, the type's PTR under
            // _services._dns-sd._udp, a PTR per subtype): probed for and
            // announced; its name, renamed on a conflict ("name (2)")
            // `publish(...)` on this thread, `co_await async_publish(...)` in a task
            SGCL_INLINE_HOT expected<string, io::error> publish(const dns_sd::service& s) const {
                return _co_publish(_s, s).wait();
            }

            SGCL_INLINE_HOT async::task<expected<string, io::error>> async_publish(const dns_sd::service& s) const noexcept {
                return _co_publish(_s, s);
            }

            // A service published withdrawn, by its name (as publish gave
            // it) and type: its goodbye sent. False when none was published
            // `remove(...)` on this thread, `co_await async_remove(...)` in a task
            SGCL_INLINE_HOT bool remove(const string& name, const string& type) const {
                return _co_remove(_s, name, type).wait();
            }

            SGCL_INLINE_HOT async::task<bool> async_remove(const string& name, const string& type) const noexcept {
                return _co_remove(_s, name, type);
            }

            // The services published, each with its name as it is now
            vector<dns_sd::service> services() const noexcept {
                auto& st = _get();
                std::lock_guard lock(st.m);
                vector<dns_sd::service> out;
                for (size_t i = 0; i < st.services.size(); ++i) {
                    dns_sd::service s = st.published[i];
                    std::lock_guard elock(st.engine->m);
                    s.name = string(std::string_view(st.services[i]->label));
                    out.push_back(std::move(s));
                }
                return out;
            }

            // Goodbye for every record, the sockets closed; a second close
            // does nothing
            // `close()` on this thread, `co_await async_close()` in a task
            SGCL_INLINE_HOT void close() const {
                _co_close(_s).wait();
            }

            SGCL_INLINE_HOT async::task<void> async_close() const noexcept {
                return _co_close(_s);
            }

            SGCL_INLINE_HOT explicit operator bool() const noexcept {
                return (bool)_s;
            }

            SGCL_INLINE_HOT friend bool operator==(const responder& a, const responder& b) noexcept {
                return a._s == b._s;
            }

        private:
            friend struct detail::ResponderAccess;

            SGCL_INLINE_HOT explicit responder(const tracked_ptr<detail::ResponderState>& s) noexcept
            : _s(s) {
            }

            SGCL_INLINE_HOT detail::ResponderState& _get() const noexcept {
                assert(_s && "an empty net::mdns::responder");
                return *_s;
            }

            static async::task<expected<responder, io::error>> _co_start(options o) noexcept {
                auto e = detail::mdns_start(detail::mdns_settings(o.interfaces, o.ipv4, o.ipv6));
                if (!e) {
                    co_return detail::fail(io::error(e.error().code(), "mdns", string()));
                }
                tracked_ptr<detail::ResponderState> st = make_tracked<detail::ResponderState>();
                st->engine = *e;
                tracked_ptr<detail::MdnsEntry> h = make_tracked<detail::MdnsEntry>();
                h->host = true;
                std::string label = o.host.empty() ? detail::mdns_machine_label() : std::string(o.host.view());
                if (label.size() > 6 && label.ends_with(".local")) {
                    label.resize(label.size() - 6);
                } else if (label.size() > 7 && label.ends_with(".local.")) {
                    label.resize(label.size() - 7);
                }
                if (!detail::mdns_label_ok(label) || label.find('.') != std::string::npos) {
                    detail::mdns_stop(**e);
                    st->closed = true;
                    co_return detail::fail(detail::mdns_error(EINVAL, "mdns", o.host));
                }
                h->base = label;
                h->label = label;
                detail::dns_name_from_text("local.", h->domain);
                st->host = h;
                detail::mdns_add_entry(*e, h);
                optional<bool> ok = co_await h->settled.receive();
                if (!ok || !*ok) {
                    detail::mdns_stop(**e);
                    st->closed = true;
                    co_return detail::fail(io::error(io::errc::closed, "mdns", string()));
                }
                co_return responder(st);
            }

            static async::task<expected<string, io::error>> _co_publish(tracked_ptr<detail::ResponderState> st, dns_sd::service s) noexcept {
                tracked_ptr<detail::MdnsEntry> en = make_tracked<detail::MdnsEntry>();
                detail::DnsName type, domain, instance;
                if (!detail::mdns_type_name(s.type, s.domain, type, domain) || !detail::mdns_instance_name(s.name, type, instance)) {
                    co_return detail::fail(detail::mdns_error(EINVAL, "publish", s.name + "." + s.type));
                }
                en->base = std::string(s.name.view());
                en->label = en->base;
                en->type = type;
                en->domain = domain;
                en->port = s.port;
                {
                    std::lock_guard lock(st->engine->m);
                    en->target = st->host->name();
                }
                if (!s.host.empty()) {
                    std::string h(s.host.view());
                    if (h.back() != '.') {
                        h += '.';
                    }
                    if (!detail::dns_name_from_text(h, en->target)) {
                        co_return detail::fail(detail::mdns_error(EINVAL, "publish", s.host));
                    }
                }
                std::vector<std::string> entries;
                for (auto& e : s.txt.entries()) {
                    entries.emplace_back(e.view());
                }
                en->txt = detail::mdns_txt_rdata(entries);
                for (auto& sub : s.subtypes) {
                    detail::DnsName n;
                    if (sub.size() < 2 || sub.view()[0] != '_' || !detail::mdns_name({sub.view(), "_sub"}, type, n)) {
                        co_return detail::fail(detail::mdns_error(EINVAL, "publish", sub));
                    }
                    en->subtypes.push_back(n);
                }
                {
                    std::lock_guard lock(st->m);
                    if (st->closed) {
                        co_return detail::fail(io::error(io::errc::closed, "publish", s.name));
                    }
                    st->services.push_back(en);
                    st->published.push_back(s);
                }
                detail::mdns_add_entry(st->engine, en);
                optional<bool> ok = co_await en->settled.receive();
                if (!ok || !*ok) {
                    co_return detail::fail(io::error(io::errc::closed, "publish", s.name));
                }
                std::lock_guard lock(st->engine->m);
                co_return string(std::string_view(en->label));
            }

            static async::task<bool> _co_remove(tracked_ptr<detail::ResponderState> st, string name, string type) noexcept {
                detail::DnsName t, d;
                tracked_ptr<detail::MdnsEntry> found;
                {
                    std::lock_guard lock(st->m);
                    for (size_t i = 0; i < st->services.size(); ++i) {
                        bool match;
                        {
                            std::lock_guard elock(st->engine->m);
                            match = st->services[i]->label == name.view() && detail::mdns_type_name(type, st->published[i].domain, t, d) && t == st->services[i]->type;
                        }
                        if (match) {
                            found = st->services[i];
                            st->services.erase(st->services.begin() + ptrdiff_t(i));
                            st->published.erase(st->published.begin() + ptrdiff_t(i));
                            break;
                        }
                    }
                }
                if (!found) {
                    co_return false;
                }
                co_await detail::mdns_withdraw(st->engine, found);
                co_return true;
            }

            static async::task<void> _co_close(tracked_ptr<detail::ResponderState> st) noexcept {
                vector<tracked_ptr<detail::MdnsEntry>> all;
                {
                    std::lock_guard lock(st->m);
                    if (st->closed) {
                        co_return;
                    }
                    st->closed = true;
                    all = st->services;
                    all.push_back(st->host);
                    st->services.clear();
                    st->published.clear();
                }
                for (auto& en : all) {
                    co_await detail::mdns_withdraw(st->engine, en);
                }
                detail::mdns_stop(*st->engine);
            }

            tracked_ptr<detail::ResponderState> _s;
        };

    private:
        friend struct detail::ResponderAccess;

        static async::task<expected<vector<ip_address>, io::error>> _co_lookup(string host, options o, async::stop_token stop) noexcept {
            std::string h(host.view());
            if (!h.empty() && h.back() == '.') {
                h.pop_back();
            }
            if (!h.ends_with(".local")) {
                h += ".local";
            }
            h += '.';
            detail::DnsName name;
            if (host.empty() || !detail::dns_name_from_text(h, name)) {
                co_return detail::fail(detail::net_error(errc::host_not_found, "lookup", host));
            }
            auto e = detail::mdns_acquire(detail::mdns_settings(o.interfaces, o.ipv4, o.ipv6));
            if (!e) {
                co_return detail::fail(io::error(e.error().code(), "lookup", host));
            }
            std::vector<std::pair<detail::DnsName, uint16_t>> questions;
            questions.emplace_back(name, detail::dns_type::a);
            questions.emplace_back(name, detail::dns_type::aaaa);
            duration wait = o.timeout > duration::zero() ? o.timeout : detail::MdnsLookupTime;
            auto got = co_await detail::mdns_query(*e, questions, o.unicast_response, sgcl::clock::now() + wait,
                                                   [](const std::vector<detail::MdnsCached>& c) { return !c.empty(); }, stop);
            detail::mdns_release(*e);
            if (stop.stop_requested()) {
                co_return detail::fail(detail::system_error(ECANCELED, "lookup", host));
            }
            vector<ip_address> out;
            for (auto& c : got) {
                ip_address a;
                auto& rd = c.r.rdata;
                if (c.r.type == detail::dns_type::a && rd.size() == 4) {
                    a = ip_address::v4(uint8_t(rd[0]), uint8_t(rd[1]), uint8_t(rd[2]), uint8_t(rd[3]));
                } else if (c.r.type == detail::dns_type::aaaa && rd.size() == 16) {
                    array<uint8_t, 16> b;
                    detail::copy_bytes(b.data(), rd.data(), 16);
                    a = ip_address::v6(b);
                    if (a.is_link_local() && c.interface) {
                        if (auto ifi = detail::interface_at(c.interface)) {
                            a = a.with_zone(ifi->name);
                        }
                    }
                } else {
                    continue;
                }
                bool dup = false;
                for (auto& x : out) {
                    dup = dup || x == a;
                }
                if (!dup) {
                    out.push_back(a);
                }
            }
            std::stable_partition(out.begin(), out.end(), [](const ip_address& a) { return a.is_v4(); });
            if (out.empty()) {
                co_return detail::fail(detail::net_error(errc::host_not_found, "lookup", host));
            }
            co_return out;
        }
    };

    namespace detail {
        struct ResponderAccess {
            static mdns::responder make(const tracked_ptr<ResponderState>& s) noexcept {
                return mdns::responder(s);
            }
        };
    }

    namespace dns_sd {
        // A browse (RFC 6763 §4): the instances of a service type as they
        // come and go, asked for again and again (RFC 6762 §5.2) for as
        // long as the browser is open. A handle of one word: copies are the
        // same browse. close() ends it; a browser dropped without it ends
        // when the collector finds it
        class browser {
        public:
            browser() noexcept = default;   // none; an operation on it is a contract violation

            // The next instance come or gone, waited for; io::errc::closed
            // once closed
            // `next()` on this thread, `co_await async_next(...)` in a task
            SGCL_INLINE_HOT expected<event, io::error> next() const {
                return _co_next(_s, async::stop_token()).wait();
            }

            SGCL_INLINE_HOT async::task<expected<event, io::error>> async_next(async::stop_token stop = {}) const noexcept {
                return _co_next(_s, std::move(stop));
            }

            // Ended: no more queries, a wait in next ended with closed
            SGCL_INLINE_HOT void close() const noexcept {
                _s->close();
            }

            SGCL_INLINE_HOT explicit operator bool() const noexcept {
                return (bool)_s;
            }

            SGCL_INLINE_HOT friend bool operator==(const browser& a, const browser& b) noexcept {
                return a._s == b._s;
            }

        private:
            friend struct sgcl::net::detail::BrowserAccess;

            SGCL_INLINE_HOT explicit browser(const tracked_ptr<detail::BrowserState>& s) noexcept
            : _s(s) {
            }

            static async::task<expected<event, io::error>> _co_next(tracked_ptr<detail::BrowserState> s, async::stop_token stop) noexcept {
                for (;;) {
                    if (s->inbox->is_closed()) {
                        co_return detail::fail(io::error(io::errc::closed, "browse", detail::dns_name_string(s->browsed)));   // what was not taken, dropped
                    }
                    while (auto ch = s->inbox->pop()) {
                        if (ch->record.type != detail::dns_type::ptr || !(ch->record.name == s->browsed)) {
                            continue;
                        }
                        detail::DnsName target;
                        if (!detail::mdns_rdata_name(ch->record.rdata, 0, target) || target.is_root()) {
                            continue;
                        }
                        event ev;
                        ev.added = ch->added;
                        ev.interface = ch->interface;
                        if (detail::mdns_first_label(s->browsed) == "_services") {
                            // the types themselves: "_http._tcp" as the name
                            detail::DnsName dom = detail::mdns_parent(detail::mdns_parent(target));
                            std::string t(detail::dns_name_string(target).view());
                            std::string d(detail::dns_name_string(dom).view());
                            if (t.size() > d.size()) {
                                t.resize(t.size() - d.size() - 1);
                            }
                            ev.name = string(std::string_view(t));
                            ev.type = "_services._dns-sd._udp";
                            ev.domain = string(std::string_view(d));
                            co_return ev;
                        }
                        ev.name = string(detail::mdns_first_label(target));
                        detail::DnsName rest = detail::mdns_parent(target);
                        detail::DnsName dom = detail::mdns_parent(detail::mdns_parent(rest));
                        std::string t(detail::dns_name_string(rest).view());
                        std::string d(detail::dns_name_string(dom).view());
                        if (t.size() > d.size()) {
                            t.resize(t.size() - d.size() - 1);
                        }
                        ev.type = string(std::string_view(t));
                        ev.domain = string(std::string_view(d));
                        ev.interface = ch->interface;
                        co_return ev;
                    }
                    if (s->inbox->is_closed()) {
                        co_return detail::fail(io::error(io::errc::closed, "browse", detail::dns_name_string(s->browsed)));
                    }
                    optional<bool> v;
                    bool stopped = false;
                    if (stop.stop_possible()) {
                        co_await sgcl::async::select(s->inbox->ready.on_receive([&](optional<bool> x) { v = x; }), stop.on_stop([&] { stopped = true; }));
                    } else {
                        v = co_await s->inbox->ready.receive();
                    }
                    if (stopped) {
                        co_return detail::fail(detail::system_error(ECANCELED, "browse", detail::dns_name_string(s->browsed)));
                    }
                }
            }

            tracked_ptr<detail::BrowserState> _s;
        };
    }

    namespace detail {
        struct BrowserAccess {
            static dns_sd::browser make(const tracked_ptr<BrowserState>& s) noexcept {
                return dns_sd::browser(s);
            }
        };

        // A PTR's target, the instance, back as a service's name, type and domain
        inline async::task<expected<dns_sd::service, io::error>> dns_sd_resolve(string name, string type, mdns::options o, async::stop_token stop) noexcept {
            DnsName t, d, instance;
            string what = name + "." + type;
            if (!mdns_type_name(type, string(), t, d) || !mdns_instance_name(name, t, instance)) {
                // the type with its domain ("_http._tcp.local.")
                co_return fail(mdns_error(EINVAL, "resolve", what));
            }
            auto e = mdns_acquire(mdns_settings(o.interfaces, o.ipv4, o.ipv6));
            if (!e) {
                co_return fail(io::error(e.error().code(), "resolve", what));
            }
            duration wait = o.timeout > duration::zero() ? o.timeout : MdnsLookupTime;
            time_point deadline = sgcl::clock::now() + wait;
            std::vector<std::pair<DnsName, uint16_t>> questions;
            questions.emplace_back(instance, dns_type::srv);
            questions.emplace_back(instance, dns_type::txt);
            auto both = [](const std::vector<MdnsCached>& c) {
                bool srv = false, txt = false;
                for (auto& x : c) {
                    srv = srv || x.r.type == dns_type::srv;
                    txt = txt || x.r.type == dns_type::txt;
                }
                return srv && txt;
            };
            auto got = co_await mdns_query(*e, questions, o.unicast_response, deadline, both, stop);
            dns_sd::service out;
            out.name = name;
            out.type = type;
            out.domain = dns_name_string(d);
            DnsName host;
            bool have_srv = false;
            for (auto& c : got) {
                if (c.r.type == dns_type::srv && !have_srv) {
                    DnsRecordData rd;
                    if (mdns_record_data(c.r, rd) && mdns_rdata_name(c.r.rdata, 6, host)) {
                        out.port = rd.third;
                        out.host = rd.name;
                        out.interface = c.interface;
                        have_srv = true;
                    }
                } else if (c.r.type == dns_type::txt) {
                    std::vector<std::string> strings;
                    if (mdns_txt_strings(c.r.rdata, strings)) {
                        TxtAccess::assign(out.txt, mdns_txt_entries(strings));
                    }
                }
            }
            if (!have_srv) {
                mdns_release(*e);
                if (stop.stop_requested()) {
                    co_return fail(system_error(ECANCELED, "resolve", what));
                }
                co_return fail(net_error(errc::host_not_found, "resolve", what));
            }
            std::vector<std::pair<DnsName, uint16_t>> addr;
            addr.emplace_back(host, dns_type::a);
            addr.emplace_back(host, dns_type::aaaa);
            auto addresses = co_await mdns_query(*e, addr, false, std::max(deadline, sgcl::clock::now() + std::chrono::milliseconds(500)),
                                                 [](const std::vector<MdnsCached>& c) { return !c.empty(); }, stop);
            mdns_release(*e);
            for (auto& c : addresses) {
                ip_address a;
                auto& rd = c.r.rdata;
                if (c.r.type == dns_type::a && rd.size() == 4) {
                    a = ip_address::v4(uint8_t(rd[0]), uint8_t(rd[1]), uint8_t(rd[2]), uint8_t(rd[3]));
                } else if (c.r.type == dns_type::aaaa && rd.size() == 16) {
                    array<uint8_t, 16> b;
                    copy_bytes(b.data(), rd.data(), 16);
                    a = ip_address::v6(b);
                    if (a.is_link_local() && c.interface) {
                        if (auto ifi = interface_at(c.interface)) {
                            a = a.with_zone(ifi->name);
                        }
                    }
                } else {
                    continue;
                }
                bool dup = false;
                for (auto& x : out.addresses) {
                    dup = dup || x == a;
                }
                if (!dup) {
                    out.addresses.push_back(a);
                }
            }
            std::stable_partition(out.addresses.begin(), out.addresses.end(), [](const ip_address& a) { return a.is_v4(); });
            co_return out;
        }

        inline async::task<expected<vector<string>, io::error>> dns_sd_types(mdns::options o, async::stop_token stop) noexcept {
            DnsName services;
            DnsName local;
            dns_name_from_text("local.", local);
            mdns_name({"_services", "_dns-sd", "_udp"}, local, services);
            auto e = mdns_acquire(mdns_settings(o.interfaces, o.ipv4, o.ipv6));
            if (!e) {
                co_return fail(io::error(e.error().code(), "types", string()));
            }
            duration wait = o.timeout > duration::zero() ? o.timeout : MdnsLookupTime;
            std::vector<std::pair<DnsName, uint16_t>> questions;
            questions.emplace_back(services, dns_type::ptr);
            auto got = co_await mdns_query(*e, questions, o.unicast_response, sgcl::clock::now() + wait,
                                           [](const std::vector<MdnsCached>&) { return false; }, stop);
            mdns_release(*e);
            if (stop.stop_requested()) {
                co_return fail(system_error(ECANCELED, "types", string()));
            }
            vector<string> out;
            for (auto& c : got) {
                DnsName target;
                if (c.r.type != dns_type::ptr || !mdns_rdata_name(c.r.rdata, 0, target)) {
                    continue;
                }
                std::string t(dns_name_string(target).view());
                if (t.size() > 7 && t.ends_with(".local.")) {
                    t.resize(t.size() - 7);
                }
                string s{std::string_view(t)};
                bool dup = false;
                for (auto& x : out) {
                    dup = dup || x == s;
                }
                if (!dup) {
                    out.push_back(s);
                }
            }
            std::sort(out.begin(), out.end());
            co_return out;
        }

        inline async::task<expected<mdns::responder, io::error>> dns_sd_publish(dns_sd::service s, mdns::options o) noexcept {
            auto r = co_await mdns::responder::async_start(o);
            if (!r) {
                co_return fail(r);
            }
            auto name = co_await r->async_publish(s);
            if (!name) {
                co_await r->async_close();
                co_return fail(name);
            }
            co_return *r;
        }
    }

    namespace dns_sd {
        // Browses a service type ("_http._tcp", the domain local.; a
        // subtype "_printer._sub._http._tcp"; "_services._dns-sd._udp" for
        // the types themselves): the instances as events, those the cache
        // holds first. It never waits, so it has no task form
        inline expected<browser, io::error> browse(const string& type, const mdns::options& o = {}) noexcept {
            detail::DnsName t, d;
            std::string_view v = type.view();
            size_t sub = v.find("._sub.");
            if (sub != std::string_view::npos) {
                // a subtype (RFC 6763 §7.1): "_printer._sub._http._tcp"
                detail::DnsName base;
                if (!detail::mdns_type_name(string(v.substr(sub + 6)), string(), base, d) || sub < 2 || v[0] != '_'
                    || !detail::mdns_name({v.substr(0, sub), "_sub"}, base, t)) {
                    return detail::fail(detail::mdns_error(EINVAL, "browse", type));
                }
            } else if (!detail::mdns_type_name(type, string(), t, d)) {
                if (!(type == "_services._dns-sd._udp" || type == "_services._dns-sd._udp.")) {
                    return detail::fail(detail::mdns_error(EINVAL, "browse", type));
                }
                detail::dns_name_from_text("_services._dns-sd._udp.local.", t);
            }
            auto e = detail::mdns_acquire(detail::mdns_settings(o.interfaces, o.ipv4, o.ipv6));
            if (!e) {
                return detail::fail(io::error(e.error().code(), "browse", type));
            }
            tracked_ptr<detail::BrowserState> st = make_tracked<detail::BrowserState>();
            st->engine = *e;
            st->browsed = t;
            std::vector<std::pair<detail::DnsName, uint16_t>> questions;
            questions.emplace_back(t, detail::dns_type::ptr);
            st->inbox = detail::mdns_subscribe(**e, questions, true);
            for (auto& c : detail::mdns_cached(**e, t, detail::dns_type::ptr)) {
                detail::MdnsChange ch;
                ch.added = true;
                ch.record = c.r;
                ch.interface = c.interface;
                st->inbox->push(std::move(ch));
            }
            return detail::BrowserAccess::make(st);
        }

        // An instance's host, port, TXT and the host's addresses (its SRV
        // and TXT, then the A and AAAA of the SRV's target): name "Living
        // Room", type "_http._tcp"
        // `dns_sd::resolve(...)` on this thread, `co_await dns_sd::async_resolve(...)` in a task
        SGCL_INLINE_HOT expected<service, io::error> resolve(const string& name, const string& type, const mdns::options& o = {}) {
            return detail::dns_sd_resolve(name, type, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT async::task<expected<service, io::error>> async_resolve(const string& name, const string& type, const mdns::options& o = {}, async::stop_token stop = {}) noexcept {
            return detail::dns_sd_resolve(name, type, o, std::move(stop));
        }

        // The service types of the link (RFC 6763 §9, the PTRs of
        // _services._dns-sd._udp.local.), sorted, each once: those that
        // answered within the timeout
        // `dns_sd::types(...)` on this thread, `co_await dns_sd::async_types(...)` in a task
        SGCL_INLINE_HOT expected<vector<string>, io::error> types(const mdns::options& o = {}) {
            return detail::dns_sd_types(o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT async::task<expected<vector<string>, io::error>> async_types(const mdns::options& o = {}, async::stop_token stop = {}) noexcept {
            return detail::dns_sd_types(o, std::move(stop));
        }

        // A service published by a responder of its own (its host this
        // machine's name unless the service names one): probed for and
        // announced, about a second; close() of the responder withdraws it
        // `dns_sd::publish(...)` on this thread, `co_await dns_sd::async_publish(...)` in a task
        SGCL_INLINE_HOT expected<mdns::responder, io::error> publish(const service& s, const mdns::options& o = {}) {
            return detail::dns_sd_publish(s, o).wait();
        }

        SGCL_INLINE_HOT async::task<expected<mdns::responder, io::error>> async_publish(const service& s, const mdns::options& o = {}) noexcept {
            return detail::dns_sd_publish(s, o);
        }

        // The one-line form: a service of a name, a type, a port and a TXT
        // record on every interface
        SGCL_INLINE_HOT expected<mdns::responder, io::error> publish(const string& name, const string& type, uint16_t port, const txt_record& txt = {}) {
            service s;
            s.name = name;
            s.type = type;
            s.port = port;
            s.txt = txt;
            return detail::dns_sd_publish(s, mdns::options()).wait();
        }
    }
}
