//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "client.h"
#include "error.h"
#include "key.h"
#include "types.h"
#include "../http/request.h"
#include "../http/response_writer.h"
#include "../tls.h"
#include "../../async/blocking.h"
#include "../../async/event.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../async/timer.h"
#include "../../core/function.h"
#include "../../core/map.h"
#include "../../crypto/random.h"
#include "../../crypto/read_secret.h"
#include "../../io/file.h"
#include "../../io/fs.h"
#include "../../io/path.h"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>

// Certificates obtained and renewed by themselves for a TLS server, Go's
// golang.org/x/crypto/acme/autocert: a manager of the names a server
// answers to (or a policy that decides), its tls_config() given to the
// server. The first handshake for a name obtains its certificate (one order
// however many handshakes wait for it), by tls-alpn-01 on the same port
// (the server answers the CA's acme-tls/1 hello with the challenge's
// certificate), by http-01 through http_handler() on port 80, or by dns-01
// through the program's own publisher of TXT records (a wildcard's only
// way); the key and the chain go into the cache directory (the key 0600)
// and are read from it on the next start; each certificate is renewed
// before it expires, when the CA's renewal information (RFC 9773) suggests,
// else at two thirds of its lifetime, in the background, the old one served
// until the new one is there.
namespace sgcl::net::acme {
    // Let's Encrypt's directories, production and staging (whose
    // certificates browsers do not trust, under rate limits far higher)
    inline constexpr char lets_encrypt_url[] = "https://acme-v02.api.letsencrypt.org/directory";
    inline constexpr char lets_encrypt_staging_url[] = "https://acme-staging-v02.api.letsencrypt.org/directory";

    namespace detail {
        struct ManagerState;
    }

    class manager {
    public:
        // Where certificates come from and how
        struct options {
            string directory_url = string(lets_encrypt_url);
            string cache;                                       // a directory for the account key, the keys and the chains; empty: memory alone
            vector<string> contact;                             // the account's ("mailto:admin@example.com")
            bool accept_terms = false;                          // the CA's terms of service agreed to (Let's Encrypt requires it)
            optional<acme::external_account> external_account;  // for a CA that requires an external account binding
            optional<account_key> key;                          // the account's key; none: the cache's, else a new P-256 key
            function<bool(const string&)> host_policy;          // whether a name not among the manager's names may have a certificate; empty: none may
            vector<string> challenges = {string("tls-alpn-01"), string("http-01")};   // the types solved, in order of preference
            function<async::task<expected<void, io::error>>(const string&, const string&)> dns_publish;   // dns-01: the TXT record (name, value) set
            function<async::task<expected<void, io::error>>(const string&, const string&)> dns_cleanup;   // dns-01: the record removed afterwards; empty: left
            duration renew_before = duration::zero();           // renewal this long before expiry; zero: ARI where the CA has it, else at 2/3 of the lifetime
            key_algorithm certificate_key = key_algorithm::es256;   // the kind of the certificates' keys
            string default_name;                                // the name of a hello without SNI (a client of an IP address); empty: refused
            acme::client::options client;                       // the ACME client's: its HTTP client, timeouts
        };

        // A manager of the names: exact names, and wildcards
        // ("*.example.com", served to the names one label under it, obtained
        // by dns-01), from Let's Encrypt
        explicit manager(const vector<string>& names);

        manager(const vector<string>& names, const options& o);

        // A manager of what options::host_policy allows
        explicit manager(const options& o);

        // A server's TLS config: the identity of each hello's name from the
        // manager (identity_for), ALPN h2, http/1.1 and acme-tls/1 (what
        // tls-alpn-01 is validated with)
        tls::config tls_config() const;

        // A handler for port 80 that answers http-01's requests and redirects
        // every other GET or HEAD to https (400 for another method): Go's
        // HTTPHandler(nil). `server.route("/", m.http_handler())`
        function<void(http::request, http::response_writer)> http_handler() const;

        // The identity of a name: from memory, the cache, or the CA
        // (errc::host_not_allowed for a name neither the names nor the
        // policy allow); what the TLS config's handshakes get
        // `certificate(...)` on this thread, `co_await async_certificate(...)` in a task
        expected<tls::identity, io::error> certificate(const string& name) const;

        async::task<expected<tls::identity, io::error>> async_certificate(string name) const noexcept;

        // The ACME client of the account (registered on first use), for what
        // the manager does not do itself: a revocation, an order of its own
        expected<acme::client, io::error> client() const;

        async::task<expected<acme::client, io::error>> async_client() const noexcept;

        // The renewals stopped: the certificates already obtained are still
        // served, none is obtained or renewed any more
        void close() const;

    private:
        tracked_ptr<detail::ManagerState> _s;
    };

    namespace detail {
        // One certificate the manager holds (by a name, or a wildcard's
        // "*.example.com"): its identity, its times, the order under way
        // for it and what the last attempt left
        struct ManagedCert {
            optional<tls::identity> identity;
            int64_t not_before = 0, not_after = 0;      // seconds since 1970
            optional<async::event> pending;             // an order under way: set when it ends
            optional<io::error> error;                  // the last attempt's failure
            time_point retry_at;                        // no new attempt before (a failure's backoff)
            duration backoff = duration::zero();
            uint64_t generation = 0;                    // the renewal loop that owns it
        };

        struct ManagerState {
            manager::options o;
            vector<string> names;
            std::mutex lock;
            map<string, ManagedCert> certs;
            map<string, string> http_tokens;            // http-01: token -> key authorization
            map<string, optional<tls::identity>> alpn_identities;   // tls-alpn-01: the validator's SNI -> the challenge's identity
            optional<acme::client> acme;                // made on first use (the account's key read or made, the account registered)
            optional<async::event> acme_pending;
            optional<io::error> acme_error;
            async::stop_source stop;
            std::atomic<bool> closed = false;
        };

        // A name as the manager keys it: lower case, no trailing dot
        inline string normalized(const string& name) {
            std::string out(name.view());
            if (!out.empty() && out.back() == '.') {
                out.pop_back();
            }
            for (char& c : out) {
                if (c >= 'A' && c <= 'Z') {
                    c = char(c - 'A' + 'a');
                }
            }
            return string(out);
        }

        // The certificate a name is served from: itself among the names, the
        // wildcard among them one label above it, itself when the policy
        // allows it; nullopt for a name nothing allows
        inline optional<string> certificate_name(ManagerState& s, const string& host) {
            string n = normalized(host);
            if (n.empty()) {
                return nullopt;
            }
            for (auto& x : s.names) {
                if (x == n) {
                    return n;
                }
            }
            std::string_view v = n.view();
            size_t dot = v.find('.');
            if (dot != std::string_view::npos) {
                string wildcard = string::concat("*", v.substr(dot));
                for (auto& x : s.names) {
                    if (x == wildcard) {
                        return wildcard;
                    }
                }
            }
            if (s.o.host_policy && s.o.host_policy(n)) {
                return n;
            }
            return nullopt;
        }

        // The cache's file of a certificate: the name, "*" as "_" (Go keeps
        // the star)
        inline string cache_file(const ManagerState& s, const string& name) {
            std::string f(name.view());
            for (char& c : f) {
                if (c == '*') {
                    c = '_';
                } else if (c == '/' || c == ':') {
                    c = '_';
                }
            }
            return io::path::join(s.o.cache, string(f));
        }

        // The file written whole or not at all: path + ".tmp" at 0600, then
        // renamed (on the blocking pool)
        inline async::task<expected<void, io::error>> write_private(string path, std::shared_ptr<const crypto::secret_bytes> key, string rest) noexcept {
            co_return co_await async::spawn_blocking([path, key, rest]() -> expected<void, io::error> {
                crypto::secret_bytes all(key->size() + rest.size());
                auto out = all.as_slice();
                auto k = key->as_slice();
                if (k.size()) {
                    sgcl::detail::copy_bytes(out.data(), k.data(), k.size());
                }
                if (rest.size()) {
                    sgcl::detail::copy_bytes(out.data() + k.size(), rest.data(), rest.size());
                }
                string part = string::concat(path, ".tmp");
                auto w = io::write_file(part, slice<const byte>(all.as_slice()), io::permissions(0600));
                if (!w) {
                    return w;
                }
                return io::rename(part, path);
            });
        }

        // A file's bytes read into a secret_bytes on the blocking pool
        inline async::task<expected<std::shared_ptr<crypto::secret_bytes>, io::error>> read_private(string path) noexcept {
            co_return co_await async::spawn_blocking([path]() -> expected<std::shared_ptr<crypto::secret_bytes>, io::error> {
                auto r = crypto::read_secret(path);
                if (!r) {
                    return unexpected(r.error());
                }
                return std::make_shared<crypto::secret_bytes>(std::move(*r));
            });
        }

        // The identity of a file of the cache: the key's PEM (read where it
        // lies) and the chain after it
        inline expected<tls::identity, io::error> identity_of_file(const crypto::secret_bytes& file) {
            auto b = file.as_slice();
            std::string_view v(reinterpret_cast<const char*>(b.data()), b.size());
            size_t at = v.find("-----BEGIN CERTIFICATE-----");
            if (at == std::string_view::npos) {
                return unexpected(acme_error(errc::malformed, "acme cache", string("a cached certificate without its chain")));
            }
            return tls::identity::from_pem(string(v.substr(at)), b);
        }

        // The account's client: its key from the options, the cache, or made
        // (and kept in the cache), the account registered; once, the others
        // waiting for it
        inline async::task<expected<acme::client, io::error>> co_manager_client(tracked_ptr<ManagerState> s) noexcept {
            for (;;) {
                optional<async::event> wait;
                {
                    std::lock_guard<std::mutex> g(s->lock);
                    if (s->acme) {
                        co_return *s->acme;
                    }
                    if (s->acme_pending) {
                        wait = s->acme_pending;
                    } else {
                        s->acme_pending = async::event();
                        s->acme_error.reset();
                    }
                }
                if (wait) {
                    co_await *wait;
                    std::lock_guard<std::mutex> g(s->lock);
                    if (s->acme) {
                        co_return *s->acme;
                    }
                    if (s->acme_error) {
                        co_return unexpected(*s->acme_error);
                    }
                    continue;
                }
                auto made = co_await [](tracked_ptr<ManagerState> s) -> async::task<expected<acme::client, io::error>> {
                    optional<account_key> key = s->o.key;
                    if (!key && !s->o.cache.empty()) {
                        string path = io::path::join(s->o.cache, string("acme_account+key"));
                        auto file = co_await read_private(path);
                        if (file) {
                            auto k = account_key::from_pem(**file);
                            if (!k) {
                                co_return unexpected(k.error());
                            }
                            key = *k;
                        } else if (!file.error().is_not_found()) {
                            co_return unexpected(file.error());
                        }
                    }
                    if (!key) {
                        key = account_key();
                        if (!s->o.cache.empty()) {
                            string dir = s->o.cache;
                            auto m = co_await async::spawn_blocking([dir] { return io::mkdir_all(dir, io::permissions(0700)); });
                            if (!m) {
                                co_return unexpected(m.error());
                            }
                            auto pem = std::make_shared<const crypto::secret_bytes>(key->to_pem());
                            auto w = co_await write_private(io::path::join(s->o.cache, string("acme_account+key")), pem, string());
                            if (!w) {
                                co_return unexpected(w.error());
                            }
                        }
                    }
                    acme::client c(s->o.directory_url, *key, s->o.client);
                    account_options a;
                    a.contact = s->o.contact;
                    a.terms_agreed = s->o.accept_terms;
                    a.external_account = s->o.external_account;
                    auto acct = co_await c.async_register_account(a);
                    if (!acct) {
                        co_return unexpected(acct.error());
                    }
                    co_return c;
                }(s);
                std::lock_guard<std::mutex> g(s->lock);
                if (made) {
                    s->acme = *made;
                } else {
                    s->acme_error = made.error();
                }
                auto ev = *s->acme_pending;
                s->acme_pending.reset();
                ev.set();
                if (!made) {
                    co_return unexpected(made.error());
                }
                co_return *made;
            }
        }

        // The certificate of a name obtained: an order, each authorization
        // by the first type of the preference the CA offers, solved and
        // answered (on failure the next type, in a new order), a CSR of a
        // new key, the chain; `replaces` the ARI id of the certificate it
        // renews
        inline async::task<expected<tls::identity, io::error>> co_obtain(tracked_ptr<ManagerState> s, string name, string replaces) noexcept {
            auto c = co_await co_manager_client(s);
            if (!c) {
                co_return unexpected(c.error());
            }
            const char* op = "acme obtain";
            optional<io::error> last;
            vector<string> types = s->o.challenges;
            const bool wildcard = name.view().substr(0, 2) == "*.";
            if (wildcard) {
                types = {string("dns-01")};
            }
            for (size_t attempt = 0; attempt < types.size(); ++attempt) {
                order_options oo;
                oo.replaces = replaces;
                auto o = co_await c->async_new_order({name}, oo);
                if (!o && !replaces.empty() && (o.error().code() == errc::already_replaced || o.error().code() == errc::unsupported)) {
                    oo.replaces = string();   // a CA that will not take the replacement: an order of its own
                    o = co_await c->async_new_order({name}, oo);
                }
                if (!o) {
                    co_return unexpected(o.error());
                }
                bool failed = false;
                for (auto& url : o->authorizations) {
                    auto az = co_await c->async_authorization(url);
                    if (!az) {
                        co_return unexpected(az.error());
                    }
                    if (az->status == status::valid) {
                        continue;
                    }
                    // the first type of the preference, from this attempt on, the CA offers
                    optional<acme::challenge> chosen;
                    for (size_t t = attempt; t < types.size() && !chosen; ++t) {
                        for (auto& ch : az->challenges) {
                            if (ch.type == types[t] && (ch.type != "dns-01" || s->o.dns_publish)) {
                                chosen = ch;
                                attempt = t;
                                break;
                            }
                        }
                    }
                    if (!chosen) {
                        co_return unexpected(acme_error(errc::no_challenge, op, string::concat(az->identifier.value, ": no challenge of the types the manager solves")));
                    }
                    string ka = c->key_authorization(chosen->token);
                    string sni = az->identifier.value;
                    string record = client::dns01_name(az->identifier.value);
                    if (chosen->type == "http-01") {
                        std::lock_guard<std::mutex> g(s->lock);
                        s->http_tokens[chosen->token] = ka;
                    } else if (chosen->type == "tls-alpn-01") {
                        if (az->identifier.type == "ip") {
                            sni = reverse_name(net::ip_address::parse(az->identifier.value).value());
                        }
                        auto id = c->tls_alpn01_identity(chosen->token, az->identifier.value);
                        if (!id) {
                            co_return unexpected(id.error());
                        }
                        std::lock_guard<std::mutex> g(s->lock);
                        s->alpn_identities[sni] = *id;
                    } else {
                        auto p = co_await s->o.dns_publish(record, c->dns01_value(chosen->token));
                        if (!p) {
                            co_return unexpected(p.error());
                        }
                    }
                    auto accepted = co_await c->async_accept(*chosen);
                    expected<acme::authorization, io::error> done = accepted ? co_await c->async_wait_authorization(url) : unexpected(accepted.error());
                    // the challenge's traces removed, whatever happened
                    if (chosen->type == "http-01") {
                        std::lock_guard<std::mutex> g(s->lock);
                        s->http_tokens.erase(chosen->token);
                    } else if (chosen->type == "tls-alpn-01") {
                        std::lock_guard<std::mutex> g(s->lock);
                        s->alpn_identities.erase(sni);
                    } else if (s->o.dns_cleanup) {
                        (void)co_await s->o.dns_cleanup(record, c->dns01_value(chosen->token));
                    }
                    if (!done) {
                        last = done.error();
                        if (done.error().code() != errc::authorization_invalid) {
                            co_return unexpected(done.error());
                        }
                        failed = true;
                        break;
                    }
                }
                if (failed) {
                    continue;   // the next type of the preference, in a new order
                }
                auto ready = co_await c->async_wait_order(o->url);
                if (!ready) {
                    co_return unexpected(ready.error());
                }
                // a new key and its CSR
                std::unique_ptr<tls::detail::IdentityKey> k = std::make_unique<tls::detail::IdentityKey>();
                crypto::x509::certificate_request_template t;
                if (auto ip = net::ip_address::parse(name)) {
                    crypto::x509::ip_address a;
                    auto b = ip->bytes();
                    size_t from = ip->is_v4() ? 12 : 0;
                    a.size = uint8_t(16 - from);
                    for (size_t i = from; i < 16; ++i) {
                        a.bytes[i - from] = byte(b[i]);
                    }
                    t.ip_addresses.push_back(a);
                } else {
                    t.dns_names.push_back(name);
                    t.common_name = name;
                }
                optional<crypto::x509::certificate_request> csr;
                crypto::secret_bytes key_pem;
                switch (s->o.certificate_key) {
                    case key_algorithm::es384: {
                        auto key = crypto::p384::private_key::generate();
                        csr = crypto::x509::create_certificate_request(t, key);
                        key_pem = key.to_pem();
                        break;
                    }
                    case key_algorithm::eddsa: {
                        auto key = crypto::ed25519::private_key::generate();
                        csr = crypto::x509::create_certificate_request(t, key);
                        key_pem = key.to_pem();
                        break;
                    }
                    case key_algorithm::rs256: {
                        auto key = crypto::rsa::private_key::generate(2048);
                        csr = crypto::x509::create_certificate_request(t, key);
                        key_pem = key.to_pem();
                        break;
                    }
                    default: {
                        auto key = crypto::p256::private_key::generate();
                        csr = crypto::x509::create_certificate_request(t, key);
                        key_pem = key.to_pem();
                        break;
                    }
                }
                auto fin = co_await c->async_finalize(*ready, *csr);
                if (!fin) {
                    co_return unexpected(fin.error());
                }
                auto chain = co_await c->async_certificate(fin->certificate);
                if (!chain) {
                    co_return unexpected(chain.error());
                }
                auto id = tls::identity::from_pem(chain->pem, key_pem.as_slice());
                if (!id) {
                    co_return unexpected(id.error());
                }
                if (!s->o.cache.empty()) {
                    string dir = s->o.cache;
                    auto m = co_await async::spawn_blocking([dir] { return io::mkdir_all(dir, io::permissions(0700)); });
                    if (m) {
                        auto key = std::make_shared<const crypto::secret_bytes>(std::move(key_pem));
                        (void)co_await write_private(cache_file(*s, name), key, chain->pem);   // a cache that cannot be written leaves the certificate in memory
                    }
                }
                co_return *id;
            }
            co_return unexpected(last ? *last : acme_error(errc::no_challenge, op, string::concat(name, ": no challenge of the types the manager solves")));
        }

        // The point in [start, end) a renewal goes at (RFC 9773 §4.2: a
        // random one, the load of a CA's clients spread)
        inline int64_t random_point(int64_t start, int64_t end) noexcept {
            if (end <= start) {
                return start;
            }
            auto r = crypto::random::bytes(8);
            uint64_t v = 0;
            for (auto b : r) {
                v = v << 8 | uint64_t(b);
            }
            return start + int64_t(v % uint64_t(end - start));
        }

        inline void schedule_renewal(tracked_ptr<ManagerState> s, string name);

        // The identity of a certificate's name stored and its renewal
        // started (the lock not held)
        inline void keep_identity(tracked_ptr<ManagerState> s, const string& name, const tls::identity& id) {
            const auto& leaf = id.certificates()[0];
            {
                std::lock_guard<std::mutex> g(s->lock);
                auto& e = s->certs[name];
                e.identity = id;
                e.not_before = leaf.not_before().unix();
                e.not_after = leaf.not_after().unix();
                e.error.reset();
                e.backoff = duration::zero();
                ++e.generation;
            }
            schedule_renewal(s, name);
        }

        // The identity of a certificate name: in memory and not expired, the
        // cache's, else obtained; one attempt at a time per name, the other
        // callers waiting for it; a failure's backoff (a minute, doubling
        // to a day) before the next attempt
        inline async::task<expected<tls::identity, io::error>> co_identity(tracked_ptr<ManagerState> s, string name) noexcept {
            for (;;) {
                optional<async::event> wait;
                {
                    std::lock_guard<std::mutex> g(s->lock);
                    auto& e = s->certs[name];
                    const int64_t now = time::now().unix();
                    if (e.identity && now < e.not_after) {
                        co_return *e.identity;
                    }
                    if (e.pending) {
                        wait = e.pending;
                    } else if (e.error && sgcl::clock::now() < e.retry_at) {
                        co_return unexpected(*e.error);
                    } else if (s->closed.load()) {
                        co_return unexpected(io::error(io::errc::closed, "acme obtain", name));
                    } else {
                        e.pending = async::event();
                    }
                }
                if (wait) {
                    co_await *wait;
                    std::lock_guard<std::mutex> g(s->lock);
                    auto& e = s->certs[name];
                    if (e.identity && time::now().unix() < e.not_after) {
                        co_return *e.identity;
                    }
                    if (e.error) {
                        co_return unexpected(*e.error);
                    }
                    continue;
                }
                // this caller's attempt: the cache first
                optional<tls::identity> got;
                if (!s->o.cache.empty()) {
                    auto file = co_await read_private(cache_file(*s, name));
                    if (file) {
                        auto id = identity_of_file(**file);
                        // taken while not past two thirds of its life (else renewed now)
                        if (id) {
                            const auto& leaf = id->certificates()[0];
                            int64_t nb = leaf.not_before().unix(), na = leaf.not_after().unix(), now = time::now().unix();
                            if (now < na && now < nb + (na - nb) * 2 / 3 + 1) {
                                got = *id;
                            }
                        }
                    }
                }
                expected<tls::identity, io::error> r = got ? expected<tls::identity, io::error>(*got) : co_await co_obtain(s, name, string());
                if (r) {
                    keep_identity(s, name, *r);
                }
                std::lock_guard<std::mutex> g(s->lock);
                auto& e = s->certs[name];
                if (!r) {
                    e.error = r.error();
                    e.backoff = e.backoff == duration::zero() ? duration(60 * second) : (e.backoff * 2 > duration(24 * hour) ? duration(24 * hour) : e.backoff * 2);
                    e.retry_at = sgcl::clock::now() + e.backoff;
                }
                auto ev = *e.pending;
                e.pending.reset();
                ev.set();
                co_return r;
            }
        }

        // Waits until the stop or the time; true when the time came
        inline async::task<bool> pause_until(tracked_ptr<ManagerState> s, int64_t unix_seconds) noexcept {
            int64_t now = time::now().unix();
            if (unix_seconds <= now) {
                co_return !s->closed.load();
            }
            bool stopped = false;
            auto token = s->stop.token();
            co_await async::select(token.on_stop([&] { stopped = true; }), async::timeout(duration((unix_seconds - now) * second), [] {}));
            co_return !stopped && !s->closed.load();
        }

        // The renewal of one certificate, a loop of its own for as long as
        // its generation is the certificate's: the time chosen (renew_before;
        // the CA's window, asked again at its Retry-After; two thirds of the
        // lifetime), the wait, the new certificate (replacing the old one by
        // ARI); a failure tried again after a backoff, the old certificate
        // served meanwhile
        inline async::task<> co_renew(tracked_ptr<ManagerState> s, string name, uint64_t generation) noexcept {
            duration backoff = 60 * second;
            for (;;) {
                optional<tls::identity> id;
                {
                    std::lock_guard<std::mutex> g(s->lock);
                    auto& e = s->certs[name];
                    if (e.generation != generation || !e.identity) {
                        co_return;
                    }
                    id = e.identity;
                }
                const auto& leaf = id->certificates()[0];
                const int64_t nb = leaf.not_before().unix(), na = leaf.not_after().unix();
                int64_t when = nb + (na - nb) * 2 / 3;
                int64_t ask_again = 0;
                string replaces;
                if (s->o.renew_before > duration::zero()) {
                    when = na - whole_seconds(s->o.renew_before);
                } else if (auto c = co_await co_manager_client(s)) {
                    auto info = co_await c->async_renewal_info(leaf);
                    if (info) {
                        when = random_point(info->start.unix(), info->end.unix());
                        if (info->retry_after > duration::zero()) {
                            ask_again = time::now().unix() + whole_seconds(info->retry_after);
                        }
                        if (auto rid = client::renewal_id(leaf)) {
                            replaces = *rid;
                        }
                    }
                }
                if (ask_again && ask_again < when) {
                    if (!co_await pause_until(s, ask_again)) {
                        co_return;
                    }
                    continue;   // the window asked for again
                }
                if (!co_await pause_until(s, when)) {
                    co_return;
                }
                {
                    std::lock_guard<std::mutex> g(s->lock);
                    if (s->certs[name].generation != generation) {
                        co_return;
                    }
                }
                auto r = co_await co_obtain(s, name, replaces);
                if (r) {
                    keep_identity(s, name, *r);   // a new generation: this loop ends
                    co_return;
                }
                {
                    std::lock_guard<std::mutex> g(s->lock);
                    s->certs[name].error = r.error();
                }
                if (!co_await pause_until(s, time::now().unix() + whole_seconds(backoff))) {
                    co_return;
                }
                backoff = backoff * 2 > duration(24 * hour) ? duration(24 * hour) : backoff * 2;
            }
        }

        inline void schedule_renewal(tracked_ptr<ManagerState> s, string name) {
            uint64_t generation;
            {
                std::lock_guard<std::mutex> g(s->lock);
                generation = s->certs[name].generation;
            }
            if (s->closed.load()) {
                return;
            }
            async::go(co_renew(s, std::move(name), generation));
        }

        inline async::task<expected<tls::identity, io::error>> co_hello(tracked_ptr<ManagerState> s, tls::client_hello h) noexcept {
            // the CA's hello of tls-alpn-01: acme-tls/1 alone
            if (h.alpn.size() == 1 && h.alpn[0] == "acme-tls/1") {
                std::lock_guard<std::mutex> g(s->lock);
                auto it = s->alpn_identities.find(normalized(h.server_name));
                if (it == s->alpn_identities.end()) {
                    co_return unexpected(acme_error(errc::tls, "acme tls-alpn-01", string::concat("no challenge for ", h.server_name)));
                }
                co_return *it->second;
            }
            string host = h.server_name.empty() ? s->o.default_name : h.server_name;
            if (host.empty()) {
                co_return unexpected(acme_error(errc::host_not_allowed, "acme", string("a hello without a server name")));
            }
            auto name = certificate_name(*s, host);
            if (!name) {
                co_return unexpected(acme_error(errc::host_not_allowed, "acme", host));
            }
            co_return co_await co_identity(s, *name);
        }

        inline vector<string> normalized_names(const vector<string>& names) {
            vector<string> out;
            for (auto& n : names) {
                out.push_back(normalized(n));
            }
            return out;
        }
    }

    inline manager::manager(const vector<string>& names)
    : manager(names, options()) {
    }

    inline manager::manager(const vector<string>& names, const options& o)
    : _s(make_tracked<detail::ManagerState>()) {
        _s->o = o;
        _s->names = detail::normalized_names(names);
    }

    inline manager::manager(const options& o)
    : manager(vector<string>(), o) {
    }

    inline tls::config manager::tls_config() const {
        tls::config c;
        c.alpn = {string("h2"), string("http/1.1"), string("acme-tls/1")};
        tracked_ptr<detail::ManagerState> s = _s;
        c.identity_for = [s](const tls::client_hello& h) {
            return detail::co_hello(s, h);
        };
        return c;
    }

    inline function<void(http::request, http::response_writer)> manager::http_handler() const {
        tracked_ptr<detail::ManagerState> s = _s;
        return [s](http::request req, http::response_writer w) {
            std::string_view path = req.url().path().view();
            constexpr std::string_view prefix = "/.well-known/acme-challenge/";
            if (path.substr(0, prefix.size()) == prefix) {
                string token(path.substr(prefix.size()));
                string ka;
                {
                    std::lock_guard<std::mutex> g(s->lock);
                    auto it = s->http_tokens.find(token);
                    if (it != s->http_tokens.end()) {
                        ka = it->second;
                    }
                }
                if (ka.empty()) {
                    w.error(404);
                    return;
                }
                w.set_header(string("Content-Type"), string("text/plain"));
                w.write(ka);
                return;
            }
            if (req.method() != "GET" && req.method() != "HEAD") {
                w.error(400, string("Use HTTPS"));
                return;
            }
            // to https on the default port, the path and the query kept
            auto u = req.url();
            string q = u.query();
            string h = u.hostname();
            if (h.view().find(':') != std::string_view::npos) {
                h = string::concat("[", h, "]");
            }
            string target = string::concat("https://", h, u.path(), q.empty() ? string() : string::concat("?", q));
            w.redirect(target, 302);
        };
    }

    inline expected<tls::identity, io::error> manager::certificate(const string& name) const {
        return async_certificate(name).wait();
    }

    inline async::task<expected<tls::identity, io::error>> manager::async_certificate(string name) const noexcept {
        tracked_ptr<detail::ManagerState> s = _s;
        return [](tracked_ptr<detail::ManagerState> s, string name) -> async::task<expected<tls::identity, io::error>> {
            auto n = detail::certificate_name(*s, name);
            if (!n) {
                co_return unexpected(detail::acme_error(errc::host_not_allowed, "acme", name));
            }
            co_return co_await detail::co_identity(s, *n);
        }(s, std::move(name));
    }

    inline expected<acme::client, io::error> manager::client() const {
        return async_client().wait();
    }

    inline async::task<expected<acme::client, io::error>> manager::async_client() const noexcept {
        return detail::co_manager_client(_s);
    }

    inline void manager::close() const {
        _s->closed.store(true);
        _s->stop.request_stop();
    }
}
