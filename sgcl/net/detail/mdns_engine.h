//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "dns_resolver.h"
#include "mdns_message.h"
#include "../interface.h"
#include "../socket.h"
#include "../../async/channel.h"
#include "../../async/coroutine.h"
#include "../../async/select.h"
#include "../../async/timer.h"
#include "../../core/detail/hash_bytes.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/root_ptr.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

// Multicast DNS (RFC 6762): the engine under net::mdns and net::dns_sd.
//
// An engine is a socket per interface and family, each bound to the port
// 5353 of its family's wildcard with SO_REUSEADDR and SO_REUSEPORT (so
// that the system's responder, mDNSResponder on macOS or Avahi, keeps its
// own socket at the port beside it) and joined to 224.0.0.251 or ff02::fb
// on its interface alone, which is also the one it sends by, with a TTL
// (hop limit) of 255 (§11) and the multicast loopback on, so that the
// other programs of the machine hear it. A task per socket reads what
// comes; a message the engine sent itself, looped back, is known by its
// bytes and dropped. A message whose source is not on one of the engine's
// links is dropped (§11), as is one of another opcode or an rcode (§18).
//
// The querier (§5): questions asked once (a lookup) or again and again (a
// browse: the first after 20-120 ms, the next a second later, then each
// interval twice the last, up to an hour, §5.2), the answers the cache
// holds and the querier believes past half their TTL listed in the query
// so that responders leave them out (known-answer suppression, §7.1), a
// record asked again at 80, 85, 90 and 95% of its TTL while a question
// wants it (§5.2), the cache (§10): records as they came, a goodbye (TTL
// 0) or a cache-flush (§10.2) of the records of a name and type older
// than a second making them expire a second later, an expired record
// removed and told. What the cache gains and loses goes to the inboxes of
// the questions it answers.
//
// The responder (§6, §8, §9): the records of entries (a host's addresses,
// a service of DNS-SD: SRV, TXT, its PTRs) probed for (three queries for
// the names 250 ms apart, the proposed records in the authority section,
// the first QU; a response with a conflicting record or a simultaneous
// probe that wins the tie-break of §8.2 is a conflict), renamed on a
// conflict ("name (2)", a host "name-2"; after 15 conflicts in ten
// seconds a wait of five seconds, §8.1), announced twice a second apart
// (§8.3), answered (the records of a question, less its known answers;
// those of a name owned uniquely at once, shared ones after 20-120 ms
// (§6); the PTR's SRV, TXT and addresses, the SRV's addresses in the
// additional section, as DNS-SD §12 asks; a question with QU, or from a
// port other than 5353 (legacy unicast, §6.7: the id and question echoed,
// TTLs of 10 s at most, no cache-flush bit), answered by unicast), and a
// conflict after the announcement taken back to probing (§9). Its
// addresses on an interface are that interface's. A goodbye (the records
// with TTL 0) goes out when an entry is withdrawn and when the engine
// stops.
//
// Interfaces chosen at the start; one that comes up later is not joined.
namespace sgcl::net::detail {
    // The interfaces and families of an engine
    struct MdnsSettings {
        vector<network_interface> interfaces;   // empty: every interface up, multicast, with an address (loopback only when there is no other)
        bool v4 = true;
        bool v6 = true;
    };

    struct MdnsLink {
        udp::socket s;
        network_interface ifi;
        bool v6 = false;
        endpoint group;
    };

    // A record of the cache: as it came, when, and when it ends
    struct MdnsCached {
        MdnsRecord r;
        uint32_t interface = 0;
        time_point received;
        time_point expires;
        int refreshed = 0;      // the refresh queries sent of the four (§5.2)
    };

    // What the cache gained or lost, for an inbox
    struct MdnsChange {
        bool added = false;
        MdnsRecord record;
        uint32_t interface = 0;
    };

    // The questions of a lookup or a browse, and what came for them
    struct MdnsInbox {
        SGCL_INLINE_HOT MdnsInbox() noexcept
        : ready(1) {
        }

        std::vector<std::pair<DnsName, uint16_t>> questions;
        std::mutex m;
        std::deque<MdnsChange> queue;
        bool closed = false;
        async::detail::ChannelState<bool> ready;

        bool wants(const MdnsRecord& r) const noexcept {
            for (auto& [name, type] : questions) {
                if ((type == r.type || type == dns_type::any) && name == r.name) {
                    return true;
                }
            }
            return false;
        }

        void push(MdnsChange c) {
            {
                std::lock_guard lock(m);
                if (closed) {
                    return;
                }
                queue.push_back(std::move(c));
            }
            ready.try_send(true);
        }

        optional<MdnsChange> pop() {
            std::lock_guard lock(m);
            if (queue.empty()) {
                return nullopt;
            }
            MdnsChange c = std::move(queue.front());
            queue.pop_front();
            return c;
        }

        void close() {
            {
                std::lock_guard lock(m);
                closed = true;
            }
            ready.try_send(false);
        }

        bool is_closed() {
            std::lock_guard lock(m);
            return closed;
        }
    };

    // A question asked again and again (§5.2)
    struct MdnsAsking {
        DnsName name;
        uint16_t type = 0;
        time_point next;
        duration interval = {};
        int users = 0;
    };

    // One name the responder owns: a host's addresses, or a service
    struct MdnsEntry {
        enum class State : uint8_t {
            probing,
            announced,
            withdrawn
        };

        SGCL_INLINE_HOT MdnsEntry() noexcept
        : poke(1)
        , settled(1) {
        }

        bool host = false;
        std::string base;                       // the label as asked
        std::string label;                      // as it is now: after renames, "name (2)", "host-2"
        int number = 1;
        DnsName type;                           // a service's "_http._tcp.local." (root for a host)
        DnsName domain;                         // "local."
        DnsName target;                         // a service's host
        uint16_t port = 0;
        std::string txt;                        // the TXT rdata
        std::vector<DnsName> subtypes;          // "_printer._sub._http._tcp.local."
        State state = State::probing;
        bool conflict = false;                  // seen by the reader: rename
        bool lost = false;                      // a simultaneous probe won: probe again in a second
        int failures = 0;                       // conflicts since `since`
        time_point since;
        async::detail::ChannelState<bool> poke;     // wakes the entry's task
        async::detail::ChannelState<bool> settled;  // the first probing done

        // The entry's own name: the label before the type (a service's
        // instance) or the domain (a host)
        DnsName name() const noexcept {
            DnsName out;
            size_t len = label.size();
            const DnsName& rest = host ? domain : type;
            if (len == 0 || len > DnsMaxLabel || len + 1 + rest.size > DnsMaxName) {
                return out;
            }
            out.bytes[0] = uint8_t(len);
            copy_bytes(out.bytes + 1, label.data(), len);
            copy_bytes(out.bytes + 1 + len, rest.bytes, rest.size);
            out.size = uint16_t(1 + len + rest.size);
            return out;
        }

        // Whether a name is one the entry's records are owned by: its own,
        // a service's type, its subtypes, _services._dns-sd._udp of its
        // domain; compared before the records are made
        bool owns_name(const DnsName& n) const noexcept {
            if (n == name()) {
                return true;
            }
            if (host) {
                return false;
            }
            if (n == type) {
                return true;
            }
            for (auto& sub : subtypes) {
                if (n == sub) {
                    return true;
                }
            }
            return n.size == domain.size + 23 && n == services_name();
        }

        DnsName services_name() const noexcept {
            DnsName out;
            static constexpr uint8_t head[] = {9, '_', 's', 'e', 'r', 'v', 'i', 'c', 'e', 's', 7, '_', 'd', 'n', 's', '-', 's', 'd', 4, '_', 'u', 'd', 'p'};
            if (sizeof(head) + domain.size > DnsMaxName) {
                return out;
            }
            copy_bytes(out.bytes, head, sizeof(head));
            copy_bytes(out.bytes + sizeof(head), domain.bytes, domain.size);
            out.size = uint16_t(sizeof(head) + domain.size);
            return out;
        }

        // The next name after a conflict (§9): "name (2)", "name (3)", or
        // for a host "name-2"
        void rename() {
            ++number;
            std::string suffix = host ? "-" + std::to_string(number) : " (" + std::to_string(number) + ")";
            std::string b = base;
            if (b.size() + suffix.size() > DnsMaxLabel) {
                b.resize(DnsMaxLabel - suffix.size());
            }
            label = b + suffix;
        }
    };

    class MdnsEngine {
    public:
        SGCL_INLINE_HOT MdnsEngine() noexcept
        : wake(1) {
        }

        std::mutex m;
        vector<MdnsLink> links;                     // made at the start, closed at the stop
        std::vector<ip_address> own;                // the addresses of every link's interface
        std::vector<ip_network> networks;           // and their networks, for the source check
        std::vector<MdnsCached> cache;
        vector<tracked_ptr<MdnsInbox>> inboxes;
        std::vector<MdnsAsking> asking;
        vector<tracked_ptr<MdnsEntry>> entries;
        uint64_t sent[64] = {};                     // the hashes of what it sent lately
        time_point sent_at[64] = {};
        size_t sent_next = 0;
        bool stopped = false;
        int users = 0;                              // a shared engine's lookups and browses
        string key;
        async::detail::ChannelState<bool> wake;     // the timer task, woken for a new question

        // Its own message looped back, by its bytes, within two seconds
        bool is_own(const uint8_t* p, size_t n) noexcept {
            uint64_t h = hash_bytes(p, n);
            time_point now = sgcl::clock::now();
            std::lock_guard lock(m);
            for (size_t i = 0; i < 64; ++i) {
                if (sent[i] == h && now - sent_at[i] < std::chrono::seconds(2)) {
                    return true;
                }
            }
            return false;
        }

        void note_sent(const uint8_t* p, size_t n) noexcept {
            uint64_t h = hash_bytes(p, n);
            std::lock_guard lock(m);
            sent[sent_next] = h;
            sent_at[sent_next] = sgcl::clock::now();
            sent_next = (sent_next + 1) % 64;
        }

        bool on_link(const ip_address& from) const noexcept {
            ip_address a = from.unmap().with_zone(string());
            if (a.is_link_local() || a.is_loopback()) {
                return true;
            }
            for (auto& n : networks) {
                if (n.contains(a)) {
                    return true;
                }
            }
            return false;
        }

        bool owns(const std::string& rdata, uint16_t type) const noexcept {
            if ((type != dns_type::a || rdata.size() != 4) && (type != dns_type::aaaa || rdata.size() != 16)) {
                return false;
            }
            for (auto& a : own) {
                auto b = a.bytes();
                if (type == dns_type::a && a.is_v4() && std::string_view(reinterpret_cast<const char*>(b.data() + 12), 4) == rdata) {
                    return true;
                }
                if (type == dns_type::aaaa && a.is_v6() && std::string_view(reinterpret_cast<const char*>(b.data()), 16) == rdata) {
                    return true;
                }
            }
            return false;
        }
    };

    SGCL_INLINE_HOT duration mdns_ms(uint32_t ms) noexcept {
        return std::chrono::milliseconds(ms);
    }

    SGCL_INLINE_HOT duration mdns_random_ms(uint32_t from, uint32_t to) noexcept {
        return mdns_ms(from + dns_random_upto(to - from));
    }

    // A name of labels: each a label given, then the rest
    inline bool mdns_name(std::initializer_list<std::string_view> labels, const DnsName& rest, DnsName& out) noexcept {
        size_t w = 0;
        for (auto l : labels) {
            if (l.empty() || l.size() > DnsMaxLabel || w + 1 + l.size() + rest.size > DnsMaxName) {
                return false;
            }
            out.bytes[w] = uint8_t(l.size());
            copy_bytes(out.bytes + w + 1, l.data(), l.size());
            w += 1 + l.size();
        }
        if (w + rest.size > DnsMaxName) {
            return false;
        }
        copy_bytes(out.bytes + w, rest.bytes, rest.size);
        out.size = uint16_t(w + rest.size);
        return true;
    }

    // The first label of a name, and the name after it
    inline std::string_view mdns_first_label(const DnsName& n) noexcept {
        if (n.is_root()) {
            return {};
        }
        return std::string_view(reinterpret_cast<const char*>(n.bytes + 1), n.bytes[0]);
    }

    inline DnsName mdns_parent(const DnsName& n) noexcept {
        DnsName out;
        if (n.is_root()) {
            return out;
        }
        size_t skip = size_t(n.bytes[0]) + 1;
        copy_bytes(out.bytes, n.bytes + skip, n.size - skip);
        out.size = uint16_t(n.size - skip);
        return out;
    }

    // Whether a name ends with "local." (RFC 6762 §3), or lies under a
    // reverse zone of link-local addresses (§4: 254.169.in-addr.arpa.,
    // the ip6.arpa. of fe80::/10)
    inline bool mdns_is_local(const DnsName& n) noexcept {
        static const DnsName local = [] {
            DnsName d;
            dns_name_from_text("local.", d);
            return d;
        }();
        static const DnsName v4 = [] {
            DnsName d;
            dns_name_from_text("254.169.in-addr.arpa.", d);
            return d;
        }();
        auto ends = [&](const DnsName& suffix) {
            if (n.size < suffix.size) {
                return false;
            }
            DnsName tail;
            size_t at = 0;
            while (n.size - at > suffix.size) {
                at += size_t(n.bytes[at]) + 1;
            }
            if (n.size - at != suffix.size) {
                return false;
            }
            copy_bytes(tail.bytes, n.bytes + at, suffix.size);
            tail.size = suffix.size;
            return tail == suffix;
        };
        if (ends(local) || ends(v4)) {
            return true;
        }
        // fe80::/10 reversed: ...8.e.f.ip6.arpa. with the third nibble 8 to b
        static const char* const v6[] = {"8.e.f.ip6.arpa.", "9.e.f.ip6.arpa.", "a.e.f.ip6.arpa.", "b.e.f.ip6.arpa."};
        for (auto t : v6) {
            DnsName d;
            dns_name_from_text(t, d);
            if (ends(d)) {
                return true;
            }
        }
        return false;
    }

    // --- the responder's records ----------------------------------------------

    inline MdnsRecord mdns_ptr(const DnsName& owner, const DnsName& target, uint32_t ttl) {
        MdnsRecord r;
        r.name = owner;
        r.type = dns_type::ptr;
        r.ttl = ttl;
        mdns_append_name(r.rdata, target);
        return r;
    }

    // An entry's records on a link: a host's addresses (that interface's),
    // a service's SRV and TXT (unique) and PTRs (shared); `unique_only`
    // for a probe
    inline std::vector<MdnsRecord> mdns_entry_records(const MdnsEntry& e, const MdnsLink& link, bool unique_only) {
        std::vector<MdnsRecord> out;
        DnsName name = e.name();
        if (e.host) {
            for (auto& n : link.ifi.addresses) {
                ip_address a = n.address();
                MdnsRecord r;
                r.name = name;
                r.unique = true;
                r.ttl = MdnsHostTtl;
                auto b = a.bytes();
                if (a.is_v4()) {
                    r.type = dns_type::a;
                    r.rdata.assign(reinterpret_cast<const char*>(b.data() + 12), 4);
                } else {
                    r.type = dns_type::aaaa;
                    r.rdata.assign(reinterpret_cast<const char*>(b.data()), 16);
                }
                out.push_back(std::move(r));
            }
            return out;
        }
        MdnsRecord srv;
        srv.name = name;
        srv.type = dns_type::srv;
        srv.unique = true;
        srv.ttl = MdnsHostTtl;
        mdns_append_u16(srv.rdata, 0);
        mdns_append_u16(srv.rdata, 0);
        mdns_append_u16(srv.rdata, e.port);
        mdns_append_name(srv.rdata, e.target);
        out.push_back(std::move(srv));
        MdnsRecord txt;
        txt.name = name;
        txt.type = dns_type::txt;
        txt.unique = true;
        txt.ttl = MdnsOtherTtl;
        txt.rdata = e.txt;
        out.push_back(std::move(txt));
        if (unique_only) {
            return out;
        }
        out.push_back(mdns_ptr(e.type, name, MdnsOtherTtl));
        DnsName services;
        if (mdns_name({"_services", "_dns-sd", "_udp"}, e.domain, services)) {
            out.push_back(mdns_ptr(services, e.type, MdnsOtherTtl));
        }
        for (auto& sub : e.subtypes) {
            out.push_back(mdns_ptr(sub, name, MdnsOtherTtl));
        }
        return out;
    }

    // --- sending --------------------------------------------------------------

    // A message sent on a link, to its group or to one address; the bytes
    // noted, to be known when they loop back
    inline async::task<void> mdns_send(tracked_ptr<MdnsEngine> e, size_t link, vector<byte> message, endpoint to) noexcept {
        if (message.empty() || link >= e->links.size()) {
            co_return;
        }
        e->note_sent(reinterpret_cast<const uint8_t*>(message.data()), message.size());
        const MdnsLink& l = e->links[link];
        (void)co_await l.s.async_send_to(message, to.is_valid() ? to : l.group);   // a link gone: the next send says so again, nothing to do
    }

    inline vector<byte> mdns_bytes(const uint8_t* p, size_t n) {
        vector<byte> v(n);
        copy_bytes(v.data(), p, n);
        return v;
    }

    // Records into messages under the size limit, as many as fit in each:
    // `questions` in the first alone (a probe's, a query's), the records
    // into answers or (a probe) authorities
    inline vector<vector<byte>> mdns_pack(uint16_t id, uint16_t flags, const std::vector<MdnsQuestion>& questions, const std::vector<MdnsRecord>& records,
                                               int section, const std::vector<MdnsRecord>& additionals, bool legacy) {
        vector<vector<byte>> out;
        uint8_t buf[MdnsSendLimit];
        size_t next = 0, next_add = 0;
        bool first = true;
        while (first || next < records.size()) {
            MdnsWriter w(buf, sizeof(buf), id, flags);
            if (first || legacy) {
                for (auto& q : questions) {
                    w.question(q.name, q.type, q.unicast);
                }
            }
            size_t put = 0;
            while (next < records.size()) {
                const MdnsRecord& r = records[next];
                uint32_t ttl = legacy && r.ttl > MdnsLegacyTtl ? MdnsLegacyTtl : r.ttl;
                if (!w.record(section, r, ttl, r.unique && !legacy && flags & DnsFlagResponse)) {
                    break;
                }
                ++next;
                ++put;
            }
            if (put == 0 && next < records.size() && !(first && !questions.empty())) {
                ++next;   // a record larger than a message: left out
                continue;
            }
            while (next == records.size() && next_add < additionals.size()) {
                const MdnsRecord& r = additionals[next_add];
                uint32_t ttl = legacy && r.ttl > MdnsLegacyTtl ? MdnsLegacyTtl : r.ttl;
                if (!w.record(2, r, ttl, r.unique && !legacy)) {
                    break;   // the rest of the additional records: optional, left out (§6)
                }
                ++next_add;
            }
            size_t n = w.finish();
            if (n && !w.empty()) {
                out.push_back(mdns_bytes(buf, n));
            }
            first = false;
        }
        return out;
    }

    // --- the querier ----------------------------------------------------------

    // The cache's records of a question, believed past `fraction` of their
    // TTL (the known answers of a query: half, §7.1)
    inline std::vector<MdnsRecord> mdns_known(MdnsEngine& e, const DnsName& name, uint16_t type, double fraction) {
        std::vector<MdnsRecord> out;
        time_point now = sgcl::clock::now();
        for (auto& c : e.cache) {
            if ((c.r.type == type || type == dns_type::any) && c.r.name == name && c.expires > now) {
                double left = std::chrono::duration<double>(c.expires - now).count();
                if (left >= double(c.r.ttl) * fraction) {
                    MdnsRecord r = c.r;
                    r.ttl = uint32_t(left);
                    out.push_back(std::move(r));
                }
            }
        }
        return out;
    }

    // Queries of questions on every link, with their known answers
    inline async::task<void> mdns_ask(tracked_ptr<MdnsEngine> e, std::vector<MdnsQuestion> questions) noexcept {
        std::vector<MdnsRecord> known;
        {
            std::lock_guard lock(e->m);
            for (auto& q : questions) {
                auto k = mdns_known(*e, q.name, q.type, 0.5);
                known.insert(known.end(), k.begin(), k.end());
            }
        }
        for (auto& k : known) {
            k.unique = false;   // a known answer carries no cache-flush bit
        }
        auto messages = mdns_pack(0, 0, questions, known, 0, {}, false);
        for (size_t i = 0; i < e->links.size(); ++i) {
            for (auto& m : messages) {
                co_await mdns_send(e, i, m, endpoint());
            }
        }
    }

    // A response's records into the cache (§10): each new one told to the
    // inboxes that want it, a goodbye or a cache-flush making the records
    // it ends expire a second later
    inline void mdns_take(MdnsEngine& e, const MdnsMessage& msg, uint32_t interface) {
        time_point now = sgcl::clock::now();
        std::vector<MdnsChange> told;
        auto take = [&](const MdnsRecord& r) {
            if (r.type == dns_type::opt || r.klass != DnsClassIn) {
                return;
            }
            if (r.unique) {
                // §10.2: the others of the name, type and class, older than a second, go
                for (auto& c : e.cache) {
                    if (c.r.type == r.type && c.r.klass == r.klass && c.r.name == r.name && c.r.rdata != r.rdata && now - c.received > std::chrono::seconds(1)
                        && c.expires > now + std::chrono::seconds(1)) {
                        c.expires = now + std::chrono::seconds(1);
                    }
                }
            }
            for (auto& c : e.cache) {
                if (c.r.same(r)) {
                    if (r.ttl == 0) {
                        c.expires = std::min(c.expires, now + std::chrono::seconds(1));   // §10.1: a goodbye, a second later
                    } else {
                        c.r.ttl = r.ttl;
                        c.r.unique = r.unique;
                        c.received = now;
                        c.expires = now + std::chrono::seconds(r.ttl);
                        c.refreshed = 0;
                    }
                    return;
                }
            }
            if (r.ttl == 0) {
                return;
            }
            MdnsCached c;
            c.r = r;
            c.interface = interface;
            c.received = now;
            c.expires = now + std::chrono::seconds(r.ttl);
            e.cache.push_back(c);
            MdnsChange ch;
            ch.added = true;
            ch.record = r;
            ch.interface = interface;
            told.push_back(std::move(ch));
        };
        for (auto& r : msg.answers) {
            take(r);
        }
        for (auto& r : msg.additionals) {
            take(r);
        }
        for (auto& ch : told) {
            for (auto& in : e.inboxes) {
                if (in->wants(ch.record)) {
                    in->push(ch);
                }
            }
        }
    }

    // The cache's expired records out, told; the next point something is
    // due: an expiry, a refresh of a record a question wants, a question
    inline time_point mdns_expire(MdnsEngine& e, std::vector<MdnsQuestion>& refresh) {
        time_point now = sgcl::clock::now();
        time_point next = now + std::chrono::hours(1);
        std::vector<MdnsChange> told;
        for (size_t i = 0; i < e.cache.size();) {
            MdnsCached& c = e.cache[i];
            if (c.expires <= now) {
                MdnsChange ch;
                ch.added = false;
                ch.record = c.r;
                ch.interface = c.interface;
                told.push_back(std::move(ch));
                e.cache[i] = std::move(e.cache.back());
                e.cache.pop_back();
                continue;
            }
            next = std::min(next, c.expires);
            bool wanted = false;
            for (auto& a : e.asking) {
                if ((a.type == c.r.type || a.type == dns_type::any) && a.name == c.r.name) {
                    wanted = true;
                    break;
                }
            }
            if (wanted && c.refreshed < 4 && c.r.ttl > 0) {
                // 80, 85, 90, 95% of the TTL (§5.2; the 0-2% of random spread left out)
                auto at = c.received + duration(std::chrono::milliseconds(int64_t(c.r.ttl) * (800 + 50 * c.refreshed)));
                if (at <= now) {
                    ++c.refreshed;
                    MdnsQuestion q;
                    q.name = c.r.name;
                    q.type = c.r.type;
                    bool dup = false;
                    for (auto& o : refresh) {
                        dup = dup || (o.type == q.type && o.name == q.name);
                    }
                    if (!dup) {
                        refresh.push_back(q);
                    }
                } else {
                    next = std::min(next, at);
                }
            }
            ++i;
        }
        for (auto& ch : told) {
            for (auto& in : e.inboxes) {
                if (in->wants(ch.record)) {
                    in->push(ch);
                }
            }
        }
        return next;
    }

    // The timer task: questions asked when due, refreshes, expiries
    inline async::task<void> mdns_timer(tracked_ptr<MdnsEngine> e) noexcept {
        for (;;) {
            std::vector<MdnsQuestion> due;
            time_point next;
            {
                std::lock_guard lock(e->m);
                if (e->stopped) {
                    co_return;
                }
                next = mdns_expire(*e, due);
                time_point now = sgcl::clock::now();
                for (auto& a : e->asking) {
                    if (a.next <= now) {
                        MdnsQuestion q;
                        q.name = a.name;
                        q.type = a.type;
                        due.push_back(q);
                        a.interval = a.interval == duration() ? duration(std::chrono::seconds(1)) : std::min(a.interval * 2, duration(std::chrono::hours(1)));
                        a.next = now + a.interval;
                    }
                    next = std::min(next, a.next);
                }
            }
            if (!due.empty()) {
                co_await mdns_ask(e, std::move(due));
            }
            optional<bool> woken;
            co_await sgcl::async::select(e->wake.on_receive([&](optional<bool> v) { woken = v; }), sgcl::async::timeout(next, [] {}));
            if (woken && !*woken) {
                co_return;
            }
        }
    }

    // --- the responder --------------------------------------------------------

    inline bool mdns_rr_less(const MdnsRecord& a, const MdnsRecord& b) noexcept {
        if (a.klass != b.klass) {
            return a.klass < b.klass;
        }
        if (a.type != b.type) {
            return a.type < b.type;
        }
        return std::lexicographical_compare(a.rdata.begin(), a.rdata.end(), b.rdata.begin(), b.rdata.end(),
                                            [](char x, char y) { return uint8_t(x) < uint8_t(y); });
    }

    // §8.2: the records of the two probes sorted and compared in turn; the
    // first difference decides, and of two that agree as far as the
    // shorter goes the longer wins. Negative: ours lose
    inline int mdns_tie_break(std::vector<MdnsRecord> ours, std::vector<MdnsRecord> theirs) noexcept {
        std::sort(ours.begin(), ours.end(), mdns_rr_less);
        std::sort(theirs.begin(), theirs.end(), mdns_rr_less);
        for (size_t i = 0; i < ours.size() && i < theirs.size(); ++i) {
            if (mdns_rr_less(ours[i], theirs[i])) {
                return -1;
            }
            if (mdns_rr_less(theirs[i], ours[i])) {
                return 1;
            }
        }
        return ours.size() == theirs.size() ? 0 : ours.size() < theirs.size() ? -1 : 1;
    }

    // What a message says to the entries: a response's record of a name
    // the responder owns uniquely, its type and class, with other rdata
    // (and not one of the machine's own addresses), is a conflict (§9); a
    // probe of such a name whose records win the tie-break makes a probing
    // entry probe again a second later (§8.2)
    inline void mdns_check_entries(MdnsEngine& e, const MdnsMessage& msg, size_t link) {
        for (auto& en : e.entries) {
            if (en->state == MdnsEntry::State::withdrawn) {
                continue;
            }
            DnsName name = en->name();
            bool named = false;
            for (auto* section : {&msg.answers, &msg.additionals, &msg.authorities}) {
                for (auto& r : *section) {
                    named = named || r.name == name;
                }
            }
            if (!named) {
                continue;   // nothing of its name: its records not even made
            }
            auto ours = mdns_entry_records(*en, e.links[link], true);
            if (msg.is_response()) {
                auto check = [&](const MdnsRecord& r) {
                    if (!(r.name == name) || r.ttl == 0) {
                        return;
                    }
                    for (auto& o : ours) {
                        if (o.type == r.type && o.klass == r.klass) {
                            bool mine = false;
                            for (auto& x : ours) {
                                mine = mine || (x.type == r.type && x.rdata == r.rdata);
                            }
                            if (!mine && !e.owns(r.rdata, r.type)) {
                                en->conflict = true;
                                en->poke.try_send(true);
                            }
                            return;
                        }
                    }
                };
                for (auto& r : msg.answers) {
                    check(r);
                }
                for (auto& r : msg.additionals) {
                    check(r);
                }
            } else if (en->state == MdnsEntry::State::probing && !msg.authorities.empty()) {
                std::vector<MdnsRecord> theirs;
                for (auto& r : msg.authorities) {
                    if (r.name == name) {
                        theirs.push_back(r);
                    }
                }
                if (!theirs.empty() && mdns_tie_break(ours, theirs) < 0) {
                    en->lost = true;
                    en->poke.try_send(true);
                }
            }
        }
    }

    // The additional records of answers (DNS-SD §12): a PTR's SRV, TXT and
    // the SRV's addresses, an SRV's addresses; none already among the
    // answers
    inline std::vector<MdnsRecord> mdns_additionals(MdnsEngine& e, size_t link, const std::vector<MdnsRecord>& answers) {
        std::vector<MdnsRecord> out;
        auto have = [&](const MdnsRecord& r) {
            for (auto& a : answers) {
                if (a.same(r)) {
                    return true;
                }
            }
            for (auto& a : out) {
                if (a.same(r)) {
                    return true;
                }
            }
            return false;
        };
        auto add_name = [&](const DnsName& n, uint16_t only) {
            for (auto& en : e.entries) {
                if (en->state != MdnsEntry::State::announced || !(en->name() == n)) {
                    continue;
                }
                for (auto& r : mdns_entry_records(*en, e.links[link], true)) {
                    if ((only == 0 || r.type == only || (only == dns_type::a && r.type == dns_type::aaaa)) && !have(r)) {
                        out.push_back(r);
                    }
                }
            }
        };
        for (auto& a : answers) {
            DnsName target;
            if (a.type == dns_type::ptr && mdns_rdata_name(a.rdata, 0, target)) {
                add_name(target, 0);
                for (auto& en : e.entries) {
                    if (!en->host && en->state == MdnsEntry::State::announced && en->name() == target) {
                        add_name(en->target, dns_type::a);
                    }
                }
            } else if (a.type == dns_type::srv && mdns_rdata_name(a.rdata, 6, target)) {
                add_name(target, dns_type::a);
            } else if (a.type == dns_type::a || a.type == dns_type::aaaa) {
                add_name(a.name, dns_type::a);   // §6.2: the host's addresses of the other family with them
            }
        }
        return out;
    }

    // What a query is answered with: the announced records of its
    // questions less its known answers, their additional records, whether
    // by unicast (a question with QU that is answered, or legacy) and
    // whether a shared record is among them. The engine's lock held
    struct MdnsReply {
        std::vector<MdnsRecord> answers;
        std::vector<MdnsRecord> additionals;
        bool unicast = false;
        bool shared = false;
    };

    inline MdnsReply mdns_reply(MdnsEngine& e, size_t link, const MdnsMessage& msg, bool legacy) {
        MdnsReply out;
        out.unicast = legacy;
        for (auto& q : msg.questions) {
            if (q.klass != DnsClassIn && q.klass != 255) {
                continue;
            }
            bool any = false;
            for (auto& en : e.entries) {
                if (en->state != MdnsEntry::State::announced || !en->owns_name(q.name)) {
                    continue;
                }
                for (auto& r : mdns_entry_records(*en, e.links[link], false)) {
                    if (!(q.type == r.type || q.type == dns_type::any) || !(r.name == q.name)) {
                        continue;
                    }
                    bool known = false;
                    for (auto& k : msg.answers) {
                        known = known || (k.same(r) && k.ttl >= r.ttl / 2);   // §7.1
                    }
                    bool dup = false;
                    for (auto& a : out.answers) {
                        dup = dup || a.same(r);
                    }
                    if (!known && !dup) {
                        out.shared = out.shared || !r.unique;
                        out.answers.push_back(r);
                        any = true;
                    }
                }
            }
            if (any && q.unicast) {
                out.unicast = true;
            }
        }
        if (!out.answers.empty()) {
            out.additionals = mdns_additionals(e, link, out.answers);
        }
        return out;
    }

    // The messages of a reply: legacy ones with the query's id and
    // questions, TTLs of 10 s at most and no cache-flush bit (§6.7)
    inline vector<vector<byte>> mdns_reply_messages(const MdnsMessage& msg, const MdnsReply& reply, bool legacy) {
        std::vector<MdnsQuestion> echo;
        if (legacy) {
            echo = msg.questions;
            for (auto& q : echo) {
                q.unicast = false;
            }
        }
        return mdns_pack(legacy ? msg.header.id : 0, DnsFlagResponse | DnsFlagAuthoritative, echo, reply.answers, 0, reply.additionals, legacy);
    }

    // Messages sent to a link's group after a wait
    inline async::task<void> mdns_send_later(tracked_ptr<MdnsEngine> e, size_t link, vector<vector<byte>> messages, duration wait) noexcept {
        co_await sgcl::async::sleep(wait);
        for (auto& m : messages) {
            co_await mdns_send(e, link, m, endpoint());
        }
    }

    // A query answered from the announced entries: at once, by the reader
    // of the link, when nothing waits
    inline async::task<void> mdns_respond(tracked_ptr<MdnsEngine> e, size_t link, MdnsMessage msg, endpoint from) noexcept {
        bool legacy = from.port() != MdnsPort;
        MdnsReply reply;
        {
            std::lock_guard lock(e->m);
            reply = mdns_reply(*e, link, msg, legacy);
        }
        if (reply.answers.empty()) {
            co_return;
        }
        if (reply.unicast && !legacy) {
            // a QU question of this machine: its port 5353 is every responder's
            // and querier's here (the system's among them), and a unicast
            // datagram to it reaches one of them alone; multicast reaches all
            ip_address a = from.address().unmap().with_zone(string());
            bool here = a.is_loopback();
            for (auto& o : e->own) {
                here = here || o == a;
            }
            reply.unicast = !here;
        }
        if (reply.shared && !reply.unicast) {
            // §6: a shared record waits, so that the answers of many
            // responders spread; the wait a task of its own, so that the
            // reader goes on meanwhile
            async::go(mdns_send_later(e, link, mdns_reply_messages(msg, reply, legacy), mdns_random_ms(20, 120)));
            co_return;
        }
        for (auto& m : mdns_reply_messages(msg, reply, legacy)) {
            co_await mdns_send(e, link, m, reply.unicast ? from : endpoint());
        }
    }

    // A message that came on a link: the querier's cache, the responder's
    // entries and answers
    inline async::task<void> mdns_handle(tracked_ptr<MdnsEngine> e, size_t link, const uint8_t* p, size_t n, endpoint from) noexcept {
        if (e->is_own(p, n) || !e->on_link(from.address())) {
            co_return;
        }
        MdnsMessage msg;
        if (!mdns_read(p, n, msg)) {
            co_return;
        }
        if (msg.header.rcode() != 0) {
            co_return;   // §18.11: a message with an rcode is silently ignored
        }
        uint32_t interface = e->links[link].ifi.index;
        bool ask = false;
        {
            std::lock_guard lock(e->m);
            if (e->stopped) {
                co_return;
            }
            if (msg.is_response()) {
                if (from.port() != MdnsPort) {
                    co_return;   // §6: a response from another port than 5353 is silently ignored
                }
                mdns_take(*e, msg, interface);
            } else {
                ask = !msg.questions.empty() && !e->entries.empty();
            }
            mdns_check_entries(*e, msg, link);
        }
        if (ask) {
            co_await mdns_respond(e, link, std::move(msg), from);
        }
    }

    inline async::task<void> mdns_read_link(tracked_ptr<MdnsEngine> e, size_t link) noexcept {
        vector<byte> buf(MdnsMaxMessage);
        for (;;) {
            auto d = co_await e->links[link].s.async_receive_from(buf.as_slice());
            if (!d) {
                if (e->links[link].s.is_closed()) {
                    co_return;
                }
                continue;   // an error of one datagram (ICMP of a unicast sent): the next
            }
            if (d->truncated) {
                continue;
            }
            co_await mdns_handle(e, link, reinterpret_cast<const uint8_t*>(buf.data()), d->size, d->from);
        }
    }

    // --- the engine's life ----------------------------------------------------

    // The interfaces of an engine whose settings name none, when not every
    // one: set in the tests alone, which keep to the loopback
    struct MdnsInterfacesBox {
        vector<network_interface> interfaces;
    };

    inline vector<network_interface>& mdns_default_interfaces() noexcept {
        static root_ptr<MdnsInterfacesBox>* box = new root_ptr<MdnsInterfacesBox>(make_tracked<MdnsInterfacesBox>());   // never destroyed
        return (*box)->interfaces;
    }

    // The interfaces an engine uses
    inline vector<network_interface> mdns_choose(const MdnsSettings& s) {
        vector<network_interface> out;
        const vector<network_interface>& given = s.interfaces.empty() ? mdns_default_interfaces() : s.interfaces;
        if (!given.empty()) {
            for (auto& i : given) {
                out.push_back(i);
            }
            return out;
        }
        auto all = list_interfaces();
        if (!all) {
            return out;
        }
        vector<network_interface> loop;
        for (auto& i : *all) {
            if (!i.up || !i.multicast || i.addresses.empty()) {
                continue;
            }
            (i.loopback ? loop : out).push_back(i);
        }
        return out.empty() ? loop : out;
    }

    inline string mdns_key(const MdnsSettings& s) {
        std::string k = s.v4 ? "4" : "-";
        k += s.v6 ? "6" : "-";
        for (auto& i : s.interfaces) {
            k += '|';
            k += std::to_string(i.index);
        }
        return string(std::string_view(k));
    }

    // An engine started: its sockets, its readers, its timer. EADDRNOTAVAIL
    // when no interface could be joined, the error of the first that could
    // not otherwise
    inline expected<tracked_ptr<MdnsEngine>, io::error> mdns_start(const MdnsSettings& s) {
        tracked_ptr<MdnsEngine> e = make_tracked<MdnsEngine>();
        optional<io::error> first;
        for (auto& ifi : mdns_choose(s)) {
            bool has4 = false, has6 = false;
            for (auto& n : ifi.addresses) {
                (n.address().is_v4() ? has4 : has6) = true;
                e->own.push_back(n.address());
                e->networks.push_back(n);
            }
            for (int fam = 0; fam < 2; ++fam) {
                bool v6 = fam == 1;
                if ((v6 && (!s.v6 || !has6)) || (!v6 && (!s.v4 || !has4))) {
                    continue;
                }
                std::string group = v6 ? "[ff02::fb%" + std::string(ifi.name.view()) + "]:5353" : "224.0.0.251:5353";
                auto sock = udp::listen_multicast(string(std::string_view(group)), ifi);
                if (!sock) {
                    if (!first) {
                        first = sock.error();
                    }
                    continue;
                }
                (void)sock->set_multicast_ttl(255);
                (void)sock->set_multicast_loopback(true);
                MdnsLink l;
                l.s = *sock;
                l.ifi = ifi;
                l.v6 = v6;
                l.group = v6 ? endpoint(ip_address(string(std::string_view("ff02::fb%" + std::string(ifi.name.view())))), MdnsPort) : endpoint(ip_address::v4(224, 0, 0, 251), MdnsPort);
                e->links.push_back(std::move(l));
            }
        }
        if (e->links.empty()) {
            return fail(first ? *first : system_error(EADDRNOTAVAIL, "mdns", string()));
        }
        for (size_t i = 0; i < e->links.size(); ++i) {
            async::go(mdns_read_link(e, i));
        }
        async::go(mdns_timer(e));
        return e;
    }

    // Stopped: the sockets closed, the timer ended, the inboxes told
    inline void mdns_stop(MdnsEngine& e) noexcept {
        vector<tracked_ptr<MdnsInbox>> inboxes;
        {
            std::lock_guard lock(e.m);
            if (e.stopped) {
                return;
            }
            e.stopped = true;
            inboxes = e.inboxes;
            e.inboxes.clear();
        }
        for (auto& in : inboxes) {
            in->close();
        }
        for (auto& l : e.links) {
            (void)l.s.close();
        }
        e.wake.try_send(false);
    }

    // The shared engines of lookups and browses, by their settings
    struct MdnsHub {
        std::mutex m;
        map<string, tracked_ptr<MdnsEngine>> engines;
    };

    inline MdnsHub& mdns_hub() noexcept {
        static root_ptr<MdnsHub>* hub = new root_ptr<MdnsHub>(make_tracked<MdnsHub>());   // never destroyed
        return **hub;
    }

    inline expected<tracked_ptr<MdnsEngine>, io::error> mdns_acquire(const MdnsSettings& s) {
        MdnsHub& hub = mdns_hub();
        string key = mdns_key(s);
        std::lock_guard lock(hub.m);
        auto it = hub.engines.find(key);
        if (it != hub.engines.end()) {
            ++it->second->users;
            return it->second;
        }
        auto e = mdns_start(s);
        if (!e) {
            return e;
        }
        (*e)->key = key;
        (*e)->users = 1;
        hub.engines.insert_or_assign(key, *e);
        return e;
    }

    inline void mdns_release(const tracked_ptr<MdnsEngine>& e) noexcept {
        MdnsHub& hub = mdns_hub();
        {
            std::lock_guard lock(hub.m);
            if (--e->users > 0) {
                return;
            }
            hub.engines.erase(e->key);
        }
        mdns_stop(*e);
    }

    // An inbox for questions, kept by the engine until let go; with
    // `continuous` the questions asked again and again
    inline tracked_ptr<MdnsInbox> mdns_subscribe(MdnsEngine& e, std::vector<std::pair<DnsName, uint16_t>> questions, bool continuous) {
        tracked_ptr<MdnsInbox> in = make_tracked<MdnsInbox>();
        in->questions = std::move(questions);
        std::lock_guard lock(e.m);
        e.inboxes.push_back(in);
        if (continuous) {
            for (auto& [name, type] : in->questions) {
                bool found = false;
                for (auto& a : e.asking) {
                    if (a.type == type && a.name == name) {
                        ++a.users;
                        found = true;
                    }
                }
                if (!found) {
                    MdnsAsking a;
                    a.name = name;
                    a.type = type;
                    a.users = 1;
                    a.next = sgcl::clock::now() + mdns_random_ms(20, 120);   // §5.2
                    e.asking.push_back(a);
                }
            }
            e.wake.try_send(true);
        }
        return in;
    }

    inline void mdns_unsubscribe(MdnsEngine& e, const tracked_ptr<MdnsInbox>& in, bool continuous) {
        {
            std::lock_guard lock(e.m);
            for (size_t i = 0; i < e.inboxes.size(); ++i) {
                if (e.inboxes[i] == in) {
                    e.inboxes.erase(e.inboxes.begin() + ptrdiff_t(i));
                    break;
                }
            }
            if (continuous) {
                for (auto& [name, type] : in->questions) {
                    for (size_t i = 0; i < e.asking.size(); ++i) {
                        if (e.asking[i].type == type && e.asking[i].name == name && --e.asking[i].users == 0) {
                            e.asking.erase(e.asking.begin() + ptrdiff_t(i));
                            break;
                        }
                    }
                }
            }
        }
        in->close();
    }

    // The cache's records of a question now
    inline std::vector<MdnsCached> mdns_cached(MdnsEngine& e, const DnsName& name, uint16_t type) {
        std::vector<MdnsCached> out;
        time_point now = sgcl::clock::now();
        std::lock_guard lock(e.m);
        for (auto& c : e.cache) {
            if ((c.r.type == type || type == dns_type::any) && c.r.name == name && c.expires > now) {
                out.push_back(c);
            }
        }
        return out;
    }

    // A one-shot query (§5.1, §5.3): answered from the cache when it holds
    // the records; else asked (QU first when `unicast`, §5.4), and asked
    // again a second and three seconds later, until `enough` says the
    // records that came suffice or the deadline passes. Its records
    inline async::task<std::vector<MdnsCached>> mdns_query(tracked_ptr<MdnsEngine> e, std::vector<std::pair<DnsName, uint16_t>> questions, bool unicast,
                                                           time_point deadline, function<bool(const std::vector<MdnsCached>&)> enough, async::stop_token stop) noexcept {
        auto gather = [&]() {
            std::vector<MdnsCached> all;
            for (auto& [name, type] : questions) {
                auto c = mdns_cached(*e, name, type);
                all.insert(all.end(), c.begin(), c.end());
            }
            return all;
        };
        auto got = gather();
        if (!got.empty() && enough(got)) {
            co_return got;
        }
        tracked_ptr<MdnsInbox> in = mdns_subscribe(*e, questions, false);
        time_point start = sgcl::clock::now();
        int sent = 0;
        static constexpr int again_ms[] = {0, 1000, 3000};
        for (;;) {
            time_point now = sgcl::clock::now();
            if (sent < 3 && now >= start + mdns_ms(again_ms[sent])) {
                std::vector<MdnsQuestion> qs;
                for (auto& [name, type] : questions) {
                    MdnsQuestion q;
                    q.name = name;
                    q.type = type;
                    q.unicast = unicast && sent == 0;
                    qs.push_back(q);
                }
                ++sent;
                co_await mdns_ask(e, std::move(qs));
                continue;
            }
            time_point wake = sent < 3 ? std::min(deadline, start + mdns_ms(again_ms[sent])) : deadline;
            if (now >= deadline) {
                break;
            }
            optional<bool> v;
            bool stopped = false, timed_out = false;
            if (stop.stop_possible()) {
                co_await sgcl::async::select(in->ready.on_receive([&](optional<bool> x) { v = x; }), sgcl::async::timeout(wake, [&] { timed_out = true; }),
                                             stop.on_stop([&] { stopped = true; }));
            } else {
                co_await sgcl::async::select(in->ready.on_receive([&](optional<bool> x) { v = x; }), sgcl::async::timeout(wake, [&] { timed_out = true; }));
            }
            if (stopped || (v && !*v) || in->is_closed()) {
                break;
            }
            while (in->pop()) {
            }
            got = gather();
            if (!got.empty() && enough(got)) {
                break;
            }
        }
        mdns_unsubscribe(*e, in, false);
        co_return gather();
    }

    // --- an entry's life: probed, announced, defended -------------------------

    // The records of an entry on every link, sent in messages of a kind:
    // a probe (the question ANY of its name, the first QU, its unique
    // records as authorities), an announcement (a response of all its
    // records), a goodbye (the same with TTL 0)
    enum class MdnsSend : uint8_t {
        probe,
        announce,
        goodbye
    };

    inline async::task<void> mdns_send_entry(tracked_ptr<MdnsEngine> e, tracked_ptr<MdnsEntry> en, MdnsSend kind, bool unicast) noexcept {
        for (size_t i = 0; i < e->links.size(); ++i) {
            vector<vector<byte>> messages;
            {
                std::lock_guard lock(e->m);
                if (e->stopped) {
                    co_return;
                }
                auto records = mdns_entry_records(*en, e->links[i], kind == MdnsSend::probe);
                if (kind == MdnsSend::probe) {
                    MdnsQuestion q;
                    q.name = en->name();
                    q.type = dns_type::any;
                    q.unicast = unicast;
                    messages = mdns_pack(0, 0, {q}, records, 1, {}, false);
                } else {
                    if (kind == MdnsSend::goodbye) {
                        for (auto& r : records) {
                            r.ttl = 0;
                        }
                    }
                    messages = mdns_pack(0, DnsFlagResponse | DnsFlagAuthoritative, {}, records, 0, {}, false);
                }
            }
            for (auto& m : messages) {
                co_await mdns_send(e, i, m, endpoint());
            }
        }
    }

    // A wait of `d`, or less when the entry is poked; false when the
    // engine stopped meanwhile
    inline async::task<bool> mdns_entry_wait(tracked_ptr<MdnsEngine> e, tracked_ptr<MdnsEntry> en, duration d) noexcept {
        optional<bool> v;
        co_await sgcl::async::select(en->poke.on_receive([&](optional<bool> x) { v = x; }), sgcl::async::timeout(d, [] {}));
        std::lock_guard lock(e->m);
        co_return !e->stopped && en->state != MdnsEntry::State::withdrawn;
    }

    // Probed until it holds its name (renamed on each conflict), announced,
    // then defended: a conflict takes it back to probing (§9). Ends when
    // the entry is withdrawn or the engine stops
    inline async::task<void> mdns_run_entry(tracked_ptr<MdnsEngine> e, tracked_ptr<MdnsEntry> en) noexcept {
        bool first = true;
        for (;;) {
            {
                std::lock_guard lock(e->m);
                if (e->stopped || en->state == MdnsEntry::State::withdrawn) {
                    break;
                }
                en->state = MdnsEntry::State::probing;
                en->conflict = false;
                en->lost = false;
            }
            duration delay = mdns_random_ms(0, 250);   // §8.1
            {
                std::lock_guard lock(e->m);
                time_point now = sgcl::clock::now();
                if (now - en->since > std::chrono::seconds(10)) {
                    en->since = now;
                    en->failures = 0;
                }
                if (en->failures >= 15) {
                    delay = std::chrono::seconds(5);   // §8.1: fifteen conflicts in ten seconds
                }
            }
            if (!co_await mdns_entry_wait(e, en, delay)) {
                break;
            }
            bool conflict = false, lost = false;
            for (int i = 0; i < 3 && !conflict && !lost; ++i) {
                co_await mdns_send_entry(e, en, MdnsSend::probe, i == 0 && first);
                time_point until = sgcl::clock::now() + std::chrono::milliseconds(250);
                for (;;) {
                    duration left = until - sgcl::clock::now();
                    if (left <= duration::zero()) {
                        break;
                    }
                    if (!co_await mdns_entry_wait(e, en, left)) {
                        co_return;
                    }
                    std::lock_guard lock(e->m);
                    if (en->conflict || en->lost) {
                        conflict = en->conflict;
                        lost = en->lost;
                        break;
                    }
                }
            }
            first = false;
            if (conflict) {
                std::lock_guard lock(e->m);
                en->rename();
                ++en->failures;
                continue;
            }
            if (lost) {
                if (!co_await mdns_entry_wait(e, en, std::chrono::seconds(1))) {   // §8.2: the loser probes again a second later
                    break;
                }
                continue;
            }
            {
                std::lock_guard lock(e->m);
                en->state = MdnsEntry::State::announced;
                en->conflict = false;
            }
            en->settled.try_send(true);
            co_await mdns_send_entry(e, en, MdnsSend::announce, false);   // §8.3: twice, a second apart
            bool again = false;
            if (!co_await mdns_entry_wait(e, en, std::chrono::seconds(1))) {
                break;
            }
            {
                std::lock_guard lock(e->m);
                again = en->conflict;
            }
            if (!again) {
                co_await mdns_send_entry(e, en, MdnsSend::announce, false);
            }
            // defended: until a conflict, the withdrawal or the stop
            while (!again) {
                if (!co_await mdns_entry_wait(e, en, std::chrono::hours(1))) {
                    co_return;
                }
                std::lock_guard lock(e->m);
                again = en->conflict;
            }
        }
        en->settled.try_send(false);
    }

    // An entry added to a responder's engine and its task started
    inline void mdns_add_entry(const tracked_ptr<MdnsEngine>& e, const tracked_ptr<MdnsEntry>& en) {
        {
            std::lock_guard lock(e->m);
            en->since = sgcl::clock::now();
            e->entries.push_back(en);
        }
        async::go(mdns_run_entry(e, en));
    }

    // Withdrawn: its goodbye sent when it was announced, out of the engine
    inline async::task<void> mdns_withdraw(tracked_ptr<MdnsEngine> e, tracked_ptr<MdnsEntry> en) noexcept {
        bool announced;
        {
            std::lock_guard lock(e->m);
            announced = en->state == MdnsEntry::State::announced;
            en->state = MdnsEntry::State::withdrawn;
        }
        en->poke.try_send(true);
        if (announced) {
            co_await mdns_send_entry(e, en, MdnsSend::goodbye, false);
        }
        std::lock_guard lock(e->m);
        for (size_t i = 0; i < e->entries.size(); ++i) {
            if (e->entries[i] == en) {
                e->entries.erase(e->entries.begin() + ptrdiff_t(i));
                break;
            }
        }
    }

    // A cached record as the resolver's answer takes it
    inline bool mdns_record_data(const MdnsRecord& r, DnsRecordData& d) {
        switch (r.type) {
            case dns_type::a:
            case dns_type::aaaa:
                copy_bytes(d.address, r.rdata.data(), r.rdata.size());
                d.address_size = uint8_t(r.rdata.size());
                return true;
            case dns_type::ptr:
            case dns_type::cname:
            case dns_type::ns: {
                DnsName n;
                if (!mdns_rdata_name(r.rdata, 0, n)) {
                    return false;
                }
                d.name = dns_name_string(n);
                return true;
            }
            case dns_type::srv: {
                DnsName n;
                if (r.rdata.size() < 7 || !mdns_rdata_name(r.rdata, 6, n)) {
                    return false;
                }
                auto u16 = [&](size_t i) { return uint16_t(uint16_t(uint8_t(r.rdata[i])) << 8 | uint8_t(r.rdata[i + 1])); };
                d.first = u16(0);
                d.second = u16(2);
                d.third = u16(4);
                d.name = dns_name_string(n);
                return true;
            }
            case dns_type::txt: {
                std::vector<std::string> strings;
                if (!mdns_txt_strings(r.rdata, strings)) {
                    return false;
                }
                std::string joined;
                for (auto& s : strings) {
                    joined += s;
                }
                d.text = string(std::string_view(joined));
                return true;
            }
            default:
                return false;
        }
    }

    // A lookup of the stub resolver under .local, by multicast DNS: the
    // records of the name and type, each once (an address seen on two
    // interfaces once), NXDOMAIN when none came before the deadline (the
    // timeout of the options, else MdnsLookupTime). A record of a unique
    // type ends the wait at its first answer; a PTR waits to the end
    inline constexpr duration MdnsLookupTime = std::chrono::seconds(2);

    inline async::task<DnsAnswer> mdns_resolve(tracked_ptr<DnsContext> ctx, DnsName qname, uint16_t qtype) noexcept {
        MdnsSettings s;
        auto e = mdns_acquire(s);
        if (!e) {
            DnsAnswer a = dns_outcome(DnsStatus::failed);
            a.error = e.error();
            co_return a;
        }
        duration wait = ctx->settings.timeout > duration::zero() ? ctx->settings.timeout : MdnsLookupTime;
        std::vector<std::pair<DnsName, uint16_t>> questions;
        questions.emplace_back(qname, qtype);
        bool shared = qtype == dns_type::ptr;
        auto got = co_await mdns_query(*e, questions, false, sgcl::clock::now() + wait,
                                       [shared](const std::vector<MdnsCached>& c) { return !shared && !c.empty(); }, ctx->stop);
        mdns_release(*e);
        DnsAnswer out;
        out.end = qname;
        if (ctx->stop.stop_requested()) {
            co_return dns_outcome(DnsStatus::cancelled);
        }
        for (auto& c : got) {
            bool dup = false;
            for (auto& o : got) {
                if (&o == &c) {
                    break;
                }
                dup = dup || o.r.same(c.r);
            }
            DnsRecordData d;
            if (!dup && c.r.type == qtype && mdns_record_data(c.r, d)) {
                out.records.push_back(std::move(d));
            }
        }
        out.status = out.records.empty() ? DnsStatus::nxdomain : DnsStatus::ok;
        co_return out;
    }
}
