//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "types.h"
#include "detail/filter.h"
#include "detail/protocol.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../tls.h"
#include "../url.h"
#include "../../async/channel.h"
#include "../../async/coroutine.h"
#include "../../async/mutex.h"
#include "../../async/promise.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../async/timer.h"
#include "../../core/aliases.h"
#include "../../core/array.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../encoding/asn1.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

// The client of LDAP (RFC 4511, RFC 4513): binds, searches with filters of
// RFC 4515 and paged results (RFC 2696), the updates, compare, extended
// operations with StartTLS and Who am I? (RFC 4532)
namespace sgcl::net::ldap {
    class client;

    namespace detail {
        using LdapBlock = array<std::byte, 16384>;

        // What an operation waits for: the messages of its id until the last
        struct LdapPending {
            vector<asn1> messages;              // the protocolOps, each a view of its own message's bytes
            vector<vector<asn1>> controls;      // the controls of each
            async::promise<bool> done;          // true: the last came; false: the connection ended
        };

        struct LdapClientState {
            net::connection conn;
            tracked_ptr<LdapBlock> block = make_tracked<LdapBlock>();
            async::mutex write_lock;
            std::mutex lock;
            map<int32_t, tracked_ptr<LdapPending>> pending;
            int32_t next_id = 0;
            optional<io::error> end;
            std::atomic<bool> closed = {false};
            bool tls = false;
            string server;
            duration timeout = 30 * second;
            bool reading = false;               // an operation or the reader task reads the connection
            bool handover = false;              // StartTLS on its way: no other operation starts
            std::string buf;                    // what came and was not yet given out: the reader's
        };

        inline io::error ldap_ended(LdapClientState& s) {
            std::lock_guard g(s.lock);
            return s.end ? *s.end : io::error(io::errc::closed, "ldap", s.server);
        }

        inline void ldap_end(LdapClientState& s, io::error e) {
            map<int32_t, tracked_ptr<LdapPending>> waiting;
            {
                std::lock_guard g(s.lock);
                if (!s.end) {
                    s.end = e;
                }
                waiting = std::move(s.pending);
                s.pending = map<int32_t, tracked_ptr<LdapPending>>();
            }
            s.closed.store(true);
            for (auto& [id, p] : waiting) {
                p->done.set_value(false);
            }
            (void)s.conn.close();
        }

        // Whether the operation of the message is its last (not an entry, a
        // reference or an intermediate response)
        inline bool ldap_final(const asn1& op) noexcept {
            if (op.cls() != asn1::tag_class::application) {
                return true;
            }
            uint32_t t = op.tag();
            return t != op::search_entry && t != op::search_reference && t != op::intermediate_response;
        }

        enum class LdapPump { done, timeout, ended };

        inline int32_t ldap_register(LdapClientState& s, const tracked_ptr<LdapPending>& p) {
            std::lock_guard g(s.lock);
            if (s.end || s.handover) {
                p->done.set_value(false);
                return s.end ? 0 : -1;
            }
            do {
                if (++s.next_id <= 0) {
                    s.next_id = 1;
                }
            } while (s.pending.find(s.next_id) != s.pending.end());
            s.pending.insert({s.next_id, p});
            return s.next_id;
        }

        inline async::task<expected<void, io::error>> ldap_write(tracked_ptr<LdapClientState> s, vector<byte> bytes) noexcept {
            if (s->closed.load()) {
                co_return unexpected(ldap_ended(*s));
            }
            auto guard = co_await s->write_lock.scoped_lock();
            if (s->timeout > duration::zero()) {
                s->conn.set_write_deadline(sgcl::clock::now() + s->timeout);
            }
            auto st = net::detail::ConnectionAccess::impl(s->conn).start_write(slice<const byte>(bytes.data(), bytes.size()));
            expected<size_t, io::error> w = std::move(st.done);
            if (st.rest) {
                w = co_await std::move(*st.rest);
            }
            if (!w) {
                co_return net::detail::fail(w);
            }
            co_return expected<void, io::error>();
        }

        inline async::task<> ldap_read_task(tracked_ptr<LdapClientState> s) noexcept;

        // After an operation read: the reader task for those still waiting,
        // unless StartTLS takes the connection
        inline void ldap_pass_reading(const tracked_ptr<LdapClientState>& s) {
            {
                std::lock_guard g(s->lock);
                if (s->reading || s->handover || s->end || s->pending.empty()) {
                    return;
                }
                s->reading = true;
            }
            async::go(ldap_read_task(s));
        }

        // An operation sent, its messages waited for: the pending entry once
        // its last message came; the end of the session, or the timeout.
        // Reading has no task of its own while one operation runs (as Go's
        // client reads in the goroutine that waits): the operation that
        // finds no one reading reads, gives every message to the operation
        // of its id and stops after its own last one (or at the read
        // deadline of its timeout, the connection kept); operations still
        // waiting then get the reader task, this function without an
        // operation (no deadline, each waits for its own), which reads
        // until none is left. One frame for the request, the reading and
        // the answer.
        inline async::task<expected<tracked_ptr<LdapPending>, io::error>> ldap_exchange(tracked_ptr<LdapClientState> s, asn1 operation, vector<asn1> controls,
                                                                                      bool handover = false) noexcept {
            tracked_ptr<LdapPending> own;
            int32_t id = 0;
            time_point deadline;
            bool lead = !operation;   // the reader task: s->reading set by ldap_pass_reading
            if (operation) {
                own = make_tracked<LdapPending>();
                id = ldap_register(*s, own);
                if (id <= 0) {
                    co_return unexpected(id < 0 ? ldap_error(result{1, string(), string("StartTLS in progress"), {}}, "ldap") : ldap_ended(*s));
                }
                if (handover) {
                    std::lock_guard g(s->lock);
                    s->handover = true;
                }
                deadline = s->timeout > duration::zero() ? sgcl::clock::now() + s->timeout : time_point();
                // the request written here when the lock is free and the socket
                // takes it (net's start_write, no frame), by ldap_write otherwise
                expected<void, io::error> written;
                if (!s->closed.load() && s->write_lock.try_lock()) {
                    if (s->timeout > duration::zero()) {
                        s->conn.set_write_deadline(deadline);
                    }
                    auto bytes = ldap_message(id, operation, controls);
                    auto st = net::detail::ConnectionAccess::impl(s->conn).start_write(slice<const byte>(bytes.data(), bytes.size()));
                    expected<size_t, io::error> w = std::move(st.done);
                    if (st.rest) {
                        w = co_await std::move(*st.rest);
                    }
                    s->write_lock.unlock();
                    if (!w) {
                        written = net::detail::fail(w);
                    }
                } else {
                    written = co_await ldap_write(s, ldap_message(id, operation, controls));
                }
                if (!written) {
                    ldap_end(*s, written.error());
                    co_return unexpected(written.error());
                }
                std::lock_guard g(s->lock);
                if (!own->done.done() && !s->reading) {
                    s->reading = lead = true;
                }
            }
            bool late = false;
            if (lead) {
                io::error end(io::errc::closed, "ldap", s->server);
                auto& impl = net::detail::ConnectionAccess::impl(s->conn);
                s->conn.set_read_deadline(deadline);
                std::string& buf = s->buf;
                size_t at = 0;
                LdapPump outcome = LdapPump::ended;
                for (;;) {
                    size_t total = 0;
                    int f = ldap_frame(std::string_view(buf).substr(at), total);
                    if (f < 0 || (f > 0 && total > (size_t(64) << 20))) {
                        end = ldap_error(errc::malformed, "ldap", string("a message that is no BER element"));
                        break;
                    }
                    if (f == 0 || buf.size() - at < total) {
                        if (at) {
                            buf.erase(0, at);
                            at = 0;
                        }
                        if (!own) {
                            std::lock_guard g(s->lock);
                            if (s->pending.empty()) {
                                s->reading = false;   // an operation that starts now reads itself
                                co_return expected<tracked_ptr<LdapPending>, io::error>(tracked_ptr<LdapPending>());
                            }
                        }
                        expected<size_t, io::error> r = size_t(0);
                        for (;;) {   // the bytes there now without a wait (net's try_read), the readiness waited for
                            bool slow = false;
                            auto t = impl.try_read(slice<byte>(s->block->data(), s->block->size()), slow);
                            if (slow) {
                                r = co_await s->conn.async_read(slice<byte>(s->block->data(), s->block->size()));
                                break;
                            }
                            if (!t) {
                                r = net::detail::fail(t);
                                break;
                            }
                            if (*t) {
                                r = **t;
                                break;
                            }
                            if (auto ready = co_await impl.raw_readable(); !ready) {
                                r = net::detail::fail(ready);
                                break;
                            }
                        }
                        if (!r && own && r.error().is_timeout() && !s->closed.load()) {
                            std::lock_guard g(s->lock);
                            s->reading = false;
                            outcome = LdapPump::timeout;
                            break;
                        }
                        if (!r) {
                            end = r.error();
                            break;
                        }
                        if (*r == 0) {
                            end = io::error(io::errc::unexpected_eof, "ldap", s->server);
                            break;
                        }
                        buf.append(reinterpret_cast<const char*>(s->block->data()), *r);
                        continue;
                    }
                    const uint8_t* m0 = reinterpret_cast<const uint8_t*>(buf.data() + at);
                    if (!ldap_definite(m0, total)) {
                        end = ldap_error(errc::malformed, "ldap", string("an LDAPMessage of an indefinite length or an element past its parent"));
                        break;
                    }
                    vector<byte> bytes(reinterpret_cast<const byte*>(m0), reinterpret_cast<const byte*>(m0) + total);
                    at += total;
                    auto m = asn1::parse(bytes, asn1::ber);
                    if (!m || !m->is(asn1::type::sequence) || m->size() < 2) {
                        end = ldap_error(errc::malformed, "ldap", string("an LDAPMessage that does not read"));
                        break;
                    }
                    auto id = (*m)[0].as_int();
                    asn1 operation = (*m)[1];
                    if (!id || operation.cls() != asn1::tag_class::application) {
                        end = ldap_error(errc::malformed, "ldap", string("an LDAPMessage that does not read"));
                        break;
                    }
                    vector<asn1> controls;
                    if (m->size() > 2 && (*m)[2].is_context(0)) {
                        for (auto c : (*m)[2]) {
                            controls.push_back(c);
                        }
                    }
                    if (*id == 0) {
                        // an unsolicited notification: the Notice of Disconnection ends the session
                        if (operation.tag() == op::extended_response) {
                            result r;
                            ldap_read_result(operation, r);
                            end = ldap_error(r.code ? r : result{52, string(), string("notice of disconnection"), {}}, "ldap");
                            break;
                        }
                        continue;
                    }
                    tracked_ptr<LdapPending> p;
                    bool last = ldap_final(operation);
                    {
                        std::lock_guard g(s->lock);
                        auto it = s->pending.find(int32_t(*id));
                        if (it != s->pending.end()) {
                            p = it->second;
                            p->messages.push_back(operation);
                            p->controls.push_back(controls);
                            if (last) {
                                s->pending.erase(it);
                            }
                        }
                    }
                    if (p && last) {
                        if (p == own) {
                            buf.erase(0, at);
                            std::lock_guard g(s->lock);
                            s->reading = false;
                            p->done.set_value(true);
                            outcome = LdapPump::done;
                            break;
                        }
                        p->done.set_value(true);
                    }
                }
                if (outcome == LdapPump::ended) {
                    {
                        std::lock_guard g(s->lock);
                        s->reading = false;
                    }
                    ldap_end(*s, end);
                }

                if (!own) {
                    co_return expected<tracked_ptr<LdapPending>, io::error>(tracked_ptr<LdapPending>());
                }
                if (!handover) {
                    ldap_pass_reading(s);
                }
                late = outcome == LdapPump::timeout;
            } else if (!own->done.done()) {
                // someone reads: the last message waited for
                if (s->timeout > duration::zero()) {
                    co_await async::select(own->done.on_done([] {}), async::timeout(deadline - sgcl::clock::now(), [&] { late = true; }));
                } else {
                    (void)co_await own->done;
                }
            }
            if (late && !own->done.done()) {
                {
                    std::lock_guard g(s->lock);
                    s->pending.erase(id);
                }
                // AbandonRequest (RFC 4511 §4.11): the server may stop the operation
                asn1 which = asn1::integer(id);
                auto c = which.content();
                int32_t abandon_id;
                {
                    std::lock_guard g(s->lock);
                    abandon_id = ++s->next_id;
                }
                (void)co_await ldap_write(s, ldap_message(abandon_id, ldap_prim(asn1::tag_class::application, op::abandon_request,
                                                                                std::string_view(reinterpret_cast<const char*>(c.data()), c.size()))));
                co_return unexpected(io::error(error_code(ETIMEDOUT, std::system_category()), "ldap", s->server));
            }
            if (!own->done.result()) {
                co_return unexpected(ldap_ended(*s));
            }
            co_return own;
        }

        inline async::task<> ldap_read_task(tracked_ptr<LdapClientState> s) noexcept {
            (void)co_await ldap_exchange(s, asn1(), vector<asn1>());
        }

        // The last message's result, success or the error of the operation op
        inline expected<result, io::error> ldap_outcome(const LdapPending& p, uint32_t want, const char* op, bool compare = false) {
            const asn1& last = p.messages[p.messages.size() - 1];
            result r;
            if (last.tag() != want || !ldap_read_result(last, r)) {
                return unexpected(ldap_error(errc::malformed, string::concat("ldap ", op), string("an unexpected response")));
            }
            if (r.code != 0 && !(compare && (r.code == 5 || r.code == 6)) && !(want == op::bind_response && r.code == 14)) {
                return unexpected(ldap_error(r, string::concat("ldap ", op)));
            }
            return r;
        }
    }

    // A session with an LDAP server (RFC 4511): connect makes it (TLS from
    // the first byte for ldaps://, StartTLS by the options), bind
    // authenticates, then search, add, modify, remove, rename, compare and
    // extended operations. A handle of one word: a copy is the same session;
    // several operations may be in flight at once, each waiting for its own
    // messages (they are told apart by their message ids).
    //
    //     auto c = net::ldap::client::connect("ldaps://directory.example.com");
    //     c->bind("cn=admin,dc=example,dc=com", "secret");
    //     auto r = c->search("dc=example,dc=com", "(mail=*@example.com)", {"cn", "mail"});
    //     c->unbind();
    //
    // A referral (result 10) is the error, result_of(e).referrals its URLs:
    // reported, never followed; a search's references to other servers are
    // its result's referrals.
    class client {
    public:
        struct options {
            ldap::security security = ldap::security::automatic;
            net::tls::config tls;                            // ldaps:// and StartTLS; the server's name the URL's host when none is set
            duration timeout = std::chrono::seconds(30);     // the connection and TLS, then each operation's wait; zero: none
            async::stop_token stop;                          // the connect cancelled
        };

        client() noexcept = default;   // no session; an operation on it is a contract violation

        // A session with the server of the URL: ldap://host[:389],
        // ldaps://host[:636], or "host[:port]"; its user and password, when
        // it has them, a simple bind's DN and password
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<client, io::error> connect(const string& url) {
            return async_connect(url, options()).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string url) noexcept {
            return _co_connect(std::move(url), net::connection(), options());
        }

        static expected<client, io::error> connect(const string& url, const options& o) {
            return async_connect(url, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string url, options o) noexcept {
            return _co_connect(std::move(url), net::connection(), std::move(o));
        }

        // The same over a connection there is (a unix socket, a tunnel)
        static expected<client, io::error> connect(const net::connection& transport, const options& o) {
            return async_connect(transport, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept {
            return _co_connect(string(), std::move(transport), std::move(o));
        }

        // A simple bind (RFC 4513 §5.1): the DN and its password; both empty
        // an anonymous bind. errc::invalid_credentials for a refusal
        // `bind(...)` on this thread, `co_await async_bind(...)` in a task
        expected<void, io::error> bind(const string& dn, const string& password) const {
            return async_bind(dn, password).wait();
        }

        async::task<expected<void, io::error>> async_bind(string dn, string password) const noexcept {
            if (!dn.empty() && password.empty()) {
                // an unauthenticated bind (RFC 4513 §5.1.2): a server takes it as anonymous, a program
                // that forgot the password would go on unnoticed
                return _co_fail(detail::ldap_error(result{49, string(), string("a DN without a password (an unauthenticated bind)"), {}}, "ldap bind"));
            }
            return _co_bind(_s, detail::ldap_app(detail::op::bind_request, vector<detail::asn1>{detail::asn1::integer(3), detail::ldap_str(dn.view()),
                                                                                                 detail::ldap_prim(detail::asn1::tag_class::context_specific, 0, password.view())}),
                            "bind");
        }

        // A SASL bind of PLAIN (RFC 4616): the user, its password, and the
        // identity to act as (empty: the user's own)
        // `bind_plain(...)` on this thread, `co_await async_bind_plain(...)` in a task
        expected<void, io::error> bind_plain(const string& user, const string& password, const string& authz = {}) const {
            return async_bind_plain(user, password, authz).wait();
        }

        async::task<expected<void, io::error>> async_bind_plain(string user, string password, string authz = {}) const noexcept {
            std::string cred(authz.view());
            cred += '\0';
            cred += user.view();
            cred += '\0';
            cred += password.view();
            return _co_bind(_s, _sasl("PLAIN", cred, true), "bind");
        }

        // A SASL bind of EXTERNAL (RFC 4422 Appendix A): the identity TLS's
        // client certificate (or the transport) gives, or the one asked
        // `bind_external(...)` on this thread, `co_await async_bind_external(...)` in a task
        expected<void, io::error> bind_external(const string& authz = {}) const {
            return async_bind_external(authz).wait();
        }

        async::task<expected<void, io::error>> async_bind_external(string authz = {}) const noexcept {
            return _co_bind(_s, _sasl("EXTERNAL", std::string(authz.view()), !authz.empty()), "bind");
        }

        // Who am I? (RFC 4532): the authorization identity of the session
        // ("dn:cn=admin,dc=example,dc=com", "u:alice"; "" when anonymous)
        // `who_am_i()` on this thread, `co_await async_who_am_i()` in a task
        expected<string, io::error> who_am_i() const {
            return async_who_am_i().wait();
        }

        async::task<expected<string, io::error>> async_who_am_i() const noexcept {
            return _co_who_am_i(_s);
        }

        // StartTLS (RFC 4511 §4.14): the session goes on over TLS. No other
        // operation may be in flight; connect does it by itself with
        // security::starttls and automatic
        // `start_tls()` on this thread, `co_await async_start_tls()` in a task
        expected<void, io::error> start_tls(const net::tls::config& c = {}) const {
            return async_start_tls(c).wait();
        }

        async::task<expected<void, io::error>> async_start_tls(net::tls::config c = {}) const noexcept {
            return _co_start_tls(_s, std::move(c));
        }

        // A search (RFC 4511 §4.5): every entry of the base and scope the
        // filter matches, and the references met; with page_size, pages
        // requested (RFC 2696) until the last. errc::invalid_filter for a
        // filter that breaks RFC 4515, before anything is sent
        // `search(...)` on this thread, `co_await async_search(...)` in a task
        expected<search_result, io::error> search(const search_request& r) const {
            return async_search(r).wait();
        }

        async::task<expected<search_result, io::error>> async_search(search_request r) const noexcept {
            return _co_search(_s, std::move(r));
        }

        // The same of the subtree of the base, in one line
        expected<search_result, io::error> search(const string& base, const string& filter, const vector<string>& attributes = {}) const {
            return async_search(base, filter, attributes).wait();
        }

        async::task<expected<search_result, io::error>> async_search(string base, string filter, vector<string> attributes = {}) const noexcept {
            search_request r;
            r.base = std::move(base);
            r.filter = std::move(filter);
            r.attributes = std::move(attributes);
            return _co_search(_s, std::move(r));
        }

        // AddRequest (RFC 4511 §4.7): the entry made
        // `add(...)` on this thread, `co_await async_add(...)` in a task
        expected<void, io::error> add(const entry& e) const {
            return async_add(e).wait();
        }

        async::task<expected<void, io::error>> async_add(entry e) const noexcept {
            return _co_simple(_s, detail::ldap_app(detail::op::add_request, vector<detail::asn1>{detail::ldap_str(e.dn.view()), detail::ldap_attributes(e.attributes)}),
                              detail::op::add_response, "add");
        }

        // ModifyRequest (RFC 4511 §4.6): the changes made to the entry at
        // once, all or none
        // `modify(...)` on this thread, `co_await async_modify(...)` in a task
        expected<void, io::error> modify(const string& dn, const vector<modification>& changes) const {
            return async_modify(dn, changes).wait();
        }

        async::task<expected<void, io::error>> async_modify(string dn, vector<modification> changes) const noexcept {
            vector<detail::asn1> list;
            for (auto& c : changes) {
                vector<detail::asn1> vals;
                for (auto& v : c.values) {
                    vals.push_back(detail::ldap_str(v.view()));
                }
                list.push_back(detail::asn1::sequence({detail::asn1::enumerated(int(c.op)),
                                                       detail::asn1::sequence({detail::ldap_str(c.attribute.view()),
                                                                               detail::ldap_cons(detail::asn1::tag_class::universal, 17, vals)})}));
            }
            return _co_simple(_s, detail::ldap_app(detail::op::modify_request, vector<detail::asn1>{detail::ldap_str(dn.view()), detail::asn1::sequence(list)}),
                              detail::op::modify_response, "modify");
        }

        // DelRequest (RFC 4511 §4.8): the entry removed; a leaf only
        // `remove(...)` on this thread, `co_await async_remove(...)` in a task
        expected<void, io::error> remove(const string& dn) const {
            return async_remove(dn).wait();
        }

        async::task<expected<void, io::error>> async_remove(string dn) const noexcept {
            return _co_simple(_s, detail::ldap_prim(detail::asn1::tag_class::application, detail::op::del_request, dn.view()), detail::op::del_response, "remove");
        }

        // ModifyDNRequest (RFC 4511 §4.9): the entry renamed to the new RDN,
        // the old RDN's values dropped or kept, moved under a new superior
        // when one is given
        // `rename(...)` on this thread, `co_await async_rename(...)` in a task
        expected<void, io::error> rename(const string& dn, const string& new_rdn, bool delete_old_rdn = true, const string& new_superior = {}) const {
            return async_rename(dn, new_rdn, delete_old_rdn, new_superior).wait();
        }

        async::task<expected<void, io::error>> async_rename(string dn, string new_rdn, bool delete_old_rdn = true, string new_superior = {}) const noexcept {
            vector<detail::asn1> parts{detail::ldap_str(dn.view()), detail::ldap_str(new_rdn.view()), detail::asn1::boolean(delete_old_rdn)};
            if (!new_superior.empty()) {
                parts.push_back(detail::ldap_prim(detail::asn1::tag_class::context_specific, 0, new_superior.view()));
            }
            return _co_simple(_s, detail::ldap_app(detail::op::moddn_request, parts), detail::op::moddn_response, "rename");
        }

        // CompareRequest (RFC 4511 §4.10): whether the entry's attribute has
        // the value
        // `compare(...)` on this thread, `co_await async_compare(...)` in a task
        expected<bool, io::error> compare(const string& dn, const string& attribute, const string& value) const {
            return async_compare(dn, attribute, value).wait();
        }

        async::task<expected<bool, io::error>> async_compare(string dn, string attribute, string value) const noexcept {
            return _co_compare(_s, std::move(dn), std::move(attribute), std::move(value));
        }

        // ExtendedRequest (RFC 4511 §4.12): the operation of the OID with its
        // value; the response's name (an OID, often "") and value
        // `extended(...)` on this thread, `co_await async_extended(...)` in a task
        expected<pair<string, vector<byte>>, io::error> extended(const string& oid, const slice<const byte>& value = {}) const {
            return async_extended(oid, vector<byte>(value.begin(), value.end())).wait();
        }

        async::task<expected<pair<string, vector<byte>>, io::error>> async_extended(string oid, vector<byte> value = {}) const noexcept {
            return _co_extended(_s, std::move(oid), std::move(value), "extended");
        }

        // UnbindRequest (RFC 4511 §4.3), then the connection closed
        // `unbind()` on this thread, `co_await async_unbind()` in a task
        expected<void, io::error> unbind() const {
            return async_unbind().wait();
        }

        async::task<expected<void, io::error>> async_unbind() const noexcept {
            return _co_unbind(_s);
        }

        // The connection closed without UnbindRequest
        expected<void, io::error> close() const noexcept {
            auto r = _s->conn.close();
            detail::ldap_end(*_s, io::error(io::errc::closed, "ldap", _s->server));
            return r;
        }

        // Whether the session runs over TLS (ldaps://, or after StartTLS)
        bool is_tls() const noexcept {
            return _s->tls;
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const client& a, const client& b) noexcept {
            return a._s == b._s;
        }

    private:
        explicit client(tracked_ptr<detail::LdapClientState> s) noexcept
        : _s(std::move(s)) {
        }

        static detail::asn1 _sasl(std::string_view mechanism, const std::string& credentials, bool with_credentials) {
            vector<detail::asn1> sasl{detail::ldap_str(mechanism)};
            if (with_credentials) {
                sasl.push_back(detail::ldap_str(credentials));
            }
            return detail::ldap_app(detail::op::bind_request, vector<detail::asn1>{detail::asn1::integer(3), detail::ldap_str(std::string_view()),
                                                                                   detail::ldap_cons(detail::asn1::tag_class::context_specific, 3, sasl)});
        }

        static async::task<expected<client, io::error>> _co_connect(string url, net::connection transport, options o) noexcept {
            using namespace detail;
            std::string host, dn, password;
            uint16_t port = 389;
            bool implicit = o.security == security::tls;
            if (!transport) {
                std::string_view u = url.view();
                if (u.substr(0, 7) == "ldap://" || u.substr(0, 8) == "ldaps://") {
                    auto parsed = net::url::parse(url);
                    if (!parsed || parsed->hostname().empty()) {
                        co_return unexpected(net::detail::net_error(net::errc::invalid_url, "ldap", url));
                    }
                    bool ldaps = parsed->scheme().view() == "ldaps";
                    implicit |= ldaps && o.security == security::automatic;
                    host = std::string(parsed->hostname().view());
                    if (parsed->host_address() && parsed->host_address()->is_v6()) {
                        host = "[" + host + "]";
                    }
                    port = parsed->port() ? *parsed->port() : (implicit ? 636 : 389);
                    dn = net::detail::url_unescape(parsed->username().view());
                    password = net::detail::url_unescape(parsed->password().view());
                } else {
                    size_t colon = u.rfind(':');
                    bool v6 = !u.empty() && u.front() == '[';
                    if (colon != std::string_view::npos && ((v6 && u.find(']') < colon) || (!v6 && u.find(':') == colon))) {
                        host = std::string(u.substr(0, colon));
                        int p = 0;
                        for (char c : u.substr(colon + 1)) {
                            p = c >= '0' && c <= '9' && p < 65536 ? p * 10 + (c - '0') : 70000;
                        }
                        if (p == 0 || p > 65535) {
                            co_return unexpected(net::detail::net_error(net::errc::invalid_address, "ldap", url));
                        }
                        port = uint16_t(p);
                    } else {
                        host = std::string(u);
                        port = implicit ? 636 : 389;
                    }
                    implicit |= port == 636 && o.security == security::automatic;
                }
                if (host.empty()) {
                    co_return unexpected(net::detail::net_error(net::errc::invalid_address, "ldap", url));
                }
                async::stop_source dial_stop;
                if (o.timeout > duration::zero()) {
                    dial_stop.stop_after(o.timeout);
                }
                if (o.stop.stop_possible()) {
                    async::go(_link_stop(o.stop, dial_stop));
                }
                auto t = co_await net::tcp::async_connect(string(host + ":" + std::to_string(port)), dial_stop.token());
                dial_stop.request_stop();
                if (!t) {
                    co_return unexpected(t.error());
                }
                transport = *t;
            }
            tracked_ptr s = make_tracked<LdapClientState>();
            s->server = transport.remote_endpoint().to_string();
            s->timeout = o.timeout;
            net::tls::config tls = o.tls;
            if (tls.server_name.empty()) {
                std::string bare = host.size() > 2 && host.front() == '[' ? host.substr(1, host.size() - 2) : host;
                tls.server_name = string(bare);
            }
            if (o.timeout > duration::zero()) {
                tls.handshake_timeout = o.timeout;
            }
            if (implicit) {
                auto tt = co_await net::tls::async_client(transport, tls);
                if (!tt) {
                    (void)transport.close();
                    co_return unexpected(tt.error());
                }
                transport = *tt;
                s->tls = true;
            }
            s->conn = transport;
            client c(s);
            if (!s->tls && (o.security == security::starttls || o.security == security::automatic)) {
                if (auto st = co_await _co_start_tls(s, tls); !st) {
                    (void)c.close();
                    co_return unexpected(st.error());
                }
            }
            if (!dn.empty() || !password.empty()) {
                if (auto b = co_await c.async_bind(string(dn), string(password)); !b) {
                    (void)c.close();
                    co_return unexpected(b.error());
                }
            }
            co_return c;
        }

        static async::task<void> _link_stop(async::stop_token from, async::stop_source to) noexcept {
            auto t = to.token();
            co_await async::select(from.on_stop([&] { to.request_stop(); }), t.on_stop([] {}));
        }

        static async::task<expected<void, io::error>> _co_fail(io::error e) noexcept {
            co_return unexpected(std::move(e));
        }

        static async::task<expected<void, io::error>> _co_bind(tracked_ptr<detail::LdapClientState> s, detail::asn1 request, const char* op) noexcept {
            auto p = co_await detail::ldap_exchange(s, request, {});
            if (!p) {
                co_return unexpected(p.error());
            }
            auto r = detail::ldap_outcome(**p, detail::op::bind_response, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            if (r->code == 14) {
                co_return unexpected(detail::ldap_error(*r, string::concat("ldap ", op)));   // a SASL step this client does not take
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<void, io::error>> _co_simple(tracked_ptr<detail::LdapClientState> s, detail::asn1 request, uint32_t response, const char* op) noexcept {
            auto p = co_await detail::ldap_exchange(s, request, {});
            if (!p) {
                co_return unexpected(p.error());
            }
            auto r = detail::ldap_outcome(**p, response, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<bool, io::error>> _co_compare(tracked_ptr<detail::LdapClientState> s, string dn, string attribute, string value) noexcept {
            using namespace detail;
            auto p = co_await ldap_exchange(s, ldap_app(op::compare_request, vector<asn1>{ldap_str(dn.view()), asn1::sequence({ldap_str(attribute.view()), ldap_str(value.view())})}), {});
            if (!p) {
                co_return unexpected(p.error());
            }
            auto r = ldap_outcome(**p, op::compare_response, "compare", true);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return r->code == 6;
        }

        static async::task<expected<pair<string, vector<byte>>, io::error>> _co_extended(tracked_ptr<detail::LdapClientState> s, string oid, vector<byte> value, const char* op,
                                                                                         bool handover = false) noexcept {
            using namespace detail;
            vector<asn1> parts{ldap_prim(asn1::tag_class::context_specific, 0, oid.view())};
            if (!value.empty()) {
                parts.push_back(ldap_prim(asn1::tag_class::context_specific, 1, std::string_view(reinterpret_cast<const char*>(value.data()), value.size())));
            }
            auto p = co_await ldap_exchange(s, ldap_app(op::extended_request, parts), {}, handover);
            if (!p) {
                co_return unexpected(p.error());
            }
            auto r = ldap_outcome(**p, op::extended_response, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            const asn1& last = (*p)->messages[(*p)->messages.size() - 1];
            pair<string, vector<byte>> out;
            for (auto e : last) {
                if (e.is_context(10)) {
                    auto c = e.content();
                    out.first = string(std::string_view(reinterpret_cast<const char*>(c.data()), c.size()));
                } else if (e.is_context(11)) {
                    auto c = e.content();
                    out.second = vector<byte>(c.begin(), c.end());
                }
            }
            co_return out;
        }

        static async::task<expected<string, io::error>> _co_who_am_i(tracked_ptr<detail::LdapClientState> s) noexcept {
            auto r = co_await _co_extended(s, string(detail::OidWhoAmI), vector<byte>(), "who_am_i");
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return string(std::string_view(reinterpret_cast<const char*>(r->second.data()), r->second.size()));
        }

        static async::task<expected<void, io::error>> _co_start_tls(tracked_ptr<detail::LdapClientState> s, net::tls::config c) noexcept {
            using namespace detail;
            if (s->tls) {
                co_return unexpected(ldap_error(result{1, string(), string("TLS already in place"), {}}, "ldap start_tls"));
            }
            {
                std::lock_guard g(s->lock);
                if (!s->pending.empty() || s->reading) {
                    co_return unexpected(ldap_error(result{1, string(), string("operations in flight"), {}}, "ldap start_tls"));
                }
            }
            // the exchange reads for itself and stops after the response:
            // what came past it is in s->buf, the connection StartTLS's
            auto r = co_await _co_extended(s, string(OidStartTls), vector<byte>(), "start_tls", true);
            auto release = [&] {
                std::lock_guard g(s->lock);
                s->handover = false;
            };
            if (!r) {
                release();
                ldap_pass_reading(s);
                co_return unexpected(r.error());
            }
            if (!s->buf.empty()) {
                release();
                ldap_end(*s, ldap_error(errc::malformed, "ldap start_tls", string("data after the response")));
                co_return unexpected(ldap_ended(*s));
            }
            if (c.server_name.empty()) {
                c.server_name = string(s->server.view().substr(0, s->server.view().rfind(':')));
            }
            s->conn.set_deadline(time_point());
            auto t = co_await net::tls::async_client(s->conn, c);
            if (!t) {
                release();
                ldap_end(*s, t.error());
                co_return unexpected(t.error());
            }
            s->conn = *t;
            s->tls = true;
            release();
            co_return expected<void, io::error>();
        }

        static async::task<expected<search_result, io::error>> _co_search(tracked_ptr<detail::LdapClientState> s, search_request q) noexcept {
            using namespace detail;
            asn1 filter = ldap_filter(q.filter.view());
            if (!filter) {
                co_return unexpected(ldap_error(errc::invalid_filter, "ldap search", q.filter));
            }
            vector<asn1> attrs;
            for (auto& a : q.attributes) {
                attrs.push_back(ldap_str(a.view()));
            }
            search_result out;
            std::string cookie;
            for (;;) {
                asn1 request = ldap_app(op::search_request,
                                        vector<asn1>{ldap_str(q.base.view()), asn1::enumerated(int(q.scope)), asn1::enumerated(int(q.deref)),
                                                     asn1::integer(int64_t(q.size_limit)), asn1::integer(int64_t(q.time_limit.milliseconds() / 1000)),
                                                     asn1::boolean(q.types_only), filter, asn1::sequence(attrs)});
                vector<asn1> controls;
                if (q.page_size) {
                    controls.push_back(ldap_control(OidPagedResults, false, asn1::sequence({asn1::integer(q.page_size), ldap_str(cookie)})));
                }
                auto p = co_await ldap_exchange(s, request, controls);
                if (!p) {
                    co_return unexpected(p.error());
                }
                auto& msgs = (*p)->messages;
                for (size_t i = 0; i + 1 < msgs.size(); ++i) {
                    if (msgs[i].tag() == op::search_entry) {
                        entry e;
                        if (!ldap_read_entry(msgs[i], e)) {
                            co_return unexpected(ldap_error(errc::malformed, "ldap search", string("an entry that does not read")));
                        }
                        out.entries.push_back(std::move(e));
                    } else if (msgs[i].tag() == op::search_reference) {
                        for (auto u : msgs[i]) {
                            out.referrals.push_back(string(ldap_text(u)));
                        }
                    }
                }
                auto r = ldap_outcome(**p, op::search_done, "search");
                if (!r) {
                    auto limit = result_of(r.error());
                    if (!limit || (limit->code != 3 && limit->code != 4)) {
                        co_return unexpected(r.error());
                    }
                    out.truncated = true;   // the entries until the limit: the search ends there
                    break;
                }
                cookie.clear();
                if (q.page_size) {
                    auto& last_controls = (*p)->controls[(*p)->controls.size() - 1];
                    for (auto& c : last_controls) {
                        if (ldap_text(c[0]) == OidPagedResults) {
                            asn1 value = c[c.size() - 1];
                            auto bytes = value.as_bytes();
                            if (bytes && ldap_definite(reinterpret_cast<const uint8_t*>(bytes->data()), bytes->size())) {
                                auto inner = asn1::parse(*bytes, asn1::ber);
                                if (inner && inner->is(asn1::type::sequence)) {
                                    cookie = ldap_text((*inner)[1]);
                                }
                            }
                        }
                    }
                }
                if (cookie.empty()) {
                    break;
                }
            }
            co_return out;
        }

        static async::task<expected<void, io::error>> _co_unbind(tracked_ptr<detail::LdapClientState> s) noexcept {
            using namespace detail;
            if (s->closed.load()) {
                co_return unexpected(ldap_ended(*s));
            }
            int32_t id;
            {
                std::lock_guard g(s->lock);
                id = ++s->next_id;
            }
            auto w = co_await ldap_write(s, ldap_message(id, ldap_prim(asn1::tag_class::application, op::unbind_request, std::string_view())));
            ldap_end(*s, io::error(io::errc::closed, "ldap", s->server));
            co_return w;
        }

        tracked_ptr<detail::LdapClientState> _s;
    };

    // A value escaped for a filter's text (RFC 4515 §3): "*", "(", ")", "\"
    // and NUL as \XX, so that a user's text matches as itself
    // ("a*b" is "a\2ab")
    inline string filter_escape(const string& value) {
        static constexpr char hex[] = "0123456789abcdef";
        std::string out;
        for (char c : value.view()) {
            if (c == '*' || c == '(' || c == ')' || c == '\\' || c == '\0') {
                out += '\\';
                out += hex[uint8_t(c) >> 4];
                out += hex[uint8_t(c) & 15];
            } else {
                out += c;
            }
        }
        return string(out);
    }
}
