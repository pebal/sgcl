//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"
#include "../../connection.h"
#include "../../error.h"
#include "../../socket.h"
#include "../../url.h"
#include "../../http/detail/parser.h"
#include "../../http/detail/wire.h"
#include "../../../async/coroutine.h"
#include "../../../core/make_tracked.h"
#include "../../../core/root_ptr.h"
#include "../../../core/tracked_ptr.h"
#include "../../../core/vector.h"
#include "../../../crypto/x509.h"
#include "../../../crypto/sha256.h"
#include "../../../crypto/x509_revocation.h"

#include <cstdint>
#include <mutex>
#include <string>

// The revocation of a peer's chain as a TLS connection checks it after
// verification (tls::config::revocation): the leaf's OCSP staple, the CRLs
// of the program's, and online the OCSP responders of each certificate's
// AIA and the CRLs of its distribution points, fetched over plain HTTP
// (RFC 6960 Appendix A, RFC 5280 §4.2.1.13: http URIs; an https one would
// need TLS for the check of TLS) with HTTP/1.1's wire of net::http (its
// parser and its body framing: the http::client itself is built over TLS,
// which includes this). What was fetched is kept in a cache until its
// nextUpdate. A server's staple is fetched the same way (stapler below).
namespace sgcl::net::tls::detail {
    // Where a connection's revocation status came from
    enum class RevocationSource : uint8_t {
        none,
        staple,
        ocsp,
        crl,
    };

    // The policy (tls::revocation_mode's values)
    enum class RevocationMode : uint8_t {
        off,
        staple_only,
        soft_fail,
        hard_fail,
    };

    // What the online checks fetched: OCSP statuses by certificate, CRLs by
    // URL, each until its nextUpdate (an hour for one without), at most
    // `capacity`, the oldest dropped first; safe from many threads
    struct RevocationCacheState {
        struct Entry {
            string key;
            int64_t expires = 0;                                  // Unix seconds
            crypto::x509::revocation_status status = crypto::x509::revocation_status::unknown;   // an OCSP entry's
            optional<crypto::x509::revocation_list> crl;          // a CRL entry's
        };

        SGCL_INLINE_HOT explicit RevocationCacheState(size_t c) noexcept
        : capacity(c) {
        }

        // The entry of the key while it is fresh; a stale one dropped
        optional<Entry> find(const string& key, int64_t now) noexcept {
            std::lock_guard<std::mutex> g(_lock);
            for (size_t i = 0; i < _entries.size(); ++i) {
                if (_entries[i].key != key) {
                    continue;
                }
                if (_entries[i].expires < now) {
                    _entries.erase(_entries.begin() + std::ptrdiff_t(i));
                    return nullopt;
                }
                return _entries[i];
            }
            return nullopt;
        }

        void put(const Entry& e) noexcept {
            std::lock_guard<std::mutex> g(_lock);
            if (capacity == 0) {
                return;
            }
            for (size_t i = 0; i < _entries.size(); ++i) {
                if (_entries[i].key == e.key) {
                    _entries.erase(_entries.begin() + std::ptrdiff_t(i));
                    break;
                }
            }
            if (_entries.size() >= capacity) {
                _entries.erase(_entries.begin());
            }
            _entries.push_back(e);
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            std::lock_guard<std::mutex> g(_lock);
            return _entries.size();
        }

        SGCL_INLINE_HOT void clear() noexcept {
            std::lock_guard<std::mutex> g(_lock);
            _entries.clear();
        }

        const size_t capacity;

    private:
        mutable std::mutex _lock;
        vector<Entry> _entries;
    };

    // The process's cache, for a config without one of its own: made at the
    // first use, never destroyed (nothing of it runs at exit)
    inline const tracked_ptr<RevocationCacheState>& process_revocation_cache() {
        static const root_ptr<RevocationCacheState>* cache = new root_ptr<RevocationCacheState>(make_tracked<RevocationCacheState>(1024));
        return cache->ptr();
    }

    // The most a fetched response may be: an OCSP response's bound, and a
    // CRL's (a large CA's is tens of megabytes)
    inline constexpr size_t MaxOcspFetch = size_t(1) << 20;
    inline constexpr size_t MaxCrlFetch = size_t(64) << 20;

    // A GET (or, with a body, a POST of application/ocsp-request) of an
    // http:// URL over a connection of its own, its response's body when
    // the status is 200; ETIMEDOUT at the deadline, body_too_large past the
    // limit, unsupported_scheme for another scheme
    inline async::task<expected<vector<byte>, io::error>> http_fetch(string address, vector<byte> post, time_point deadline, size_t limit) noexcept {
        auto u = net::url::parse(address);
        if (!u) {
            co_return unexpected(u.error());
        }
        if (u->scheme() != "http") {
            co_return unexpected(net::detail::net_error(net::errc::unsupported_scheme, "revocation", address));
        }
        const auto left = deadline - sgcl::clock::now();
        if (left <= time_point::duration::zero()) {
            co_return unexpected(io::error(std::make_error_code(std::errc::timed_out), "revocation", address));
        }
        std::string host(u->hostname().view());
        std::string target = host.find(':') != std::string::npos ? "[" + host + "]" : host;
        target += ":" + std::to_string(u->effective_port());
        auto c = co_await net::tcp::async_connect(string(target), duration(left));
        if (!c) {
            co_return unexpected(c.error());
        }
        c->set_deadline(deadline);
        std::string head = std::string(post.empty() ? "GET " : "POST ") + std::string(u->request_target().view()) + " HTTP/1.1\r\nHost: " + std::string(u->host().view())
                         + "\r\nUser-Agent: sgcl\r\nAccept: */*\r\nConnection: close\r\n";
        if (!post.empty()) {
            head += "Content-Type: application/ocsp-request\r\nContent-Length: " + std::to_string(post.size()) + "\r\n";
        }
        head += "\r\n";
        string request(head);
        if (auto w = co_await c->async_write(request.as_slice()); !w) {
            (void)c->close();
            co_return unexpected(w.error());
        }
        if (!post.empty()) {
            if (auto w = co_await c->async_write(post.as_slice()); !w) {
                (void)c->close();
                co_return unexpected(w.error());
            }
        }
        tracked_ptr<net::http::detail::Wire> wire = make_tracked<net::http::detail::Wire>(*c);
        auto got = co_await wire->read_head(16384);
        if (!got || !*got) {
            (void)c->close();
            co_return unexpected(got ? io::error(io::errc::unexpected_eof, "revocation", address) : got.error());
        }
        net::http::detail::StatusLine line;
        net::http::headers h;
        net::http::detail::BodyFraming framing;
        if (net::http::detail::parse_response_head(**got, line, h) != 0 || !net::http::detail::response_framing(h, line.status, false, framing)) {
            (void)c->close();
            co_return unexpected(net::detail::net_error(net::errc::malformed_response, "revocation", address));
        }
        if (line.status != 200) {
            (void)c->close();
            co_return unexpected(net::detail::net_error(net::errc::http_status, "revocation", string(std::string(address.view()) + " answered " + std::to_string(line.status))));
        }
        net::http::detail::Body body(wire, framing, limit, true);
        auto all = co_await body.read_everything();
        (void)c->close();
        co_return all;
    }

    // What a check of a chain is given: everything by value (a task is lazy)
    struct RevocationCheck {
        crypto::x509::chain chain;                       // as verification built it, the leaf to a root
        vector<byte> staple;                             // the leaf's OCSP response the peer sent; empty: none
        RevocationMode mode = RevocationMode::off;
        vector<crypto::x509::revocation_list> crls;      // the program's
        bool fetch_crls = true;
        tracked_ptr<RevocationCacheState> cache;
        time_point deadline;
    };

    // What it found: the leaf's status (nullopt: not checked; unknown: no
    // source said) and where it came from — the certificates above it
    // checked too, a revoked one failing, an unknown one failing hard_fail;
    // or the failure, with the alert to send and the reason
    struct RevocationOutcome {
        bool ok = true;
        optional<crypto::x509::revocation_status> status;
        RevocationSource source = RevocationSource::none;
        AlertDescription alert = AlertDescription::internal_error;
        crypto::x509::reason reason = crypto::x509::reason::none;
        string what;
    };

    SGCL_INLINE_HOT std::string hex_of(const slice<const byte>& b) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string s;
        s.reserve(b.size() * 2);
        for (auto x : b) {
            s += digits[uint8_t(x) >> 4];
            s += digits[uint8_t(x) & 15];
        }
        return s;
    }

    // The cache's key of a certificate's OCSP status: its issuer's subject
    // and key, its serial
    inline string ocsp_cache_key(const crypto::x509::certificate& c, const crypto::x509::certificate& issuer) {
        return string("ocsp:" + hex_of(issuer.raw_subject()) + ":" + hex_of(issuer.raw_subject_public_key_info()) + ":" + hex_of(c.serial_number().as_slice()));
    }

    SGCL_INLINE_HOT std::string describe_certificate(const crypto::x509::certificate& c) {
        std::string s(c.subject().to_string().view());
        return "\"" + s + "\"";
    }

    // The OCSP status of a certificate from its responders (AIA): the
    // first that answers with a response that verifies; nullopt when none
    // does (unreachable, unknown, broken)
    inline async::task<optional<crypto::x509::revocation_status>> co_ocsp_online(crypto::x509::certificate c, crypto::x509::certificate issuer, tracked_ptr<RevocationCacheState> cache,
                                                                                    time_point deadline) noexcept {
        const int64_t now = time::now().unix();
        const string key = ocsp_cache_key(c, issuer);
        if (auto hit = cache->find(key, now)) {
            co_return hit->status;
        }
        for (const auto& server : c.ocsp_servers()) {
            if (sgcl::clock::now() >= deadline) {
                break;
            }
            auto req = crypto::x509::ocsp_request::make(c, issuer);
            if (!req) {
                co_return nullopt;
            }
            // GET when its URL is short enough to be cached on the way (RFC
            // 5019 §5), else POST
            string get = req->url(server);
            vector<byte> post;
            if (get.size() > 255) {
                post = vector<byte>(req->raw().data(), req->raw().data() + req->raw().size());
            }
            auto body = co_await http_fetch(post.empty() ? get : server, post, deadline, MaxOcspFetch);
            if (!body) {
                continue;
            }
            auto resp = crypto::x509::ocsp_response::parse(body->as_slice());
            if (!resp) {
                continue;
            }
            auto single = resp->verify(c, issuer);
            if (!single || single->status == crypto::x509::revocation_status::unknown) {
                continue;
            }
            RevocationCacheState::Entry e;
            e.key = key;
            e.status = single->status;
            e.expires = single->next_update ? single->next_update->unix() : now + 3600;
            cache->put(e);
            co_return single->status;
        }
        co_return nullopt;
    }

    // The status of a certificate by the CRLs of its distribution points,
    // fetched (or cached by URL); nullopt when none says
    inline async::task<optional<crypto::x509::revocation_status>> co_crl_online(crypto::x509::certificate c, crypto::x509::certificate issuer, tracked_ptr<RevocationCacheState> cache,
                                                                                   time_point deadline) noexcept {
        const int64_t now = time::now().unix();
        for (const auto& point : c.crl_distribution_points()) {
            if (sgcl::clock::now() >= deadline) {
                break;
            }
            const string key = "crl:" + point;
            optional<crypto::x509::revocation_list> crl;
            if (auto hit = cache->find(key, now)) {
                crl = hit->crl;
            } else {
                auto body = co_await http_fetch(point, vector<byte>(), deadline, MaxCrlFetch);
                if (!body) {
                    continue;
                }
                auto parsed = crypto::x509::revocation_list::parse(body->as_slice());
                if (!parsed) {
                    // a CRL served as PEM
                    const char* p = reinterpret_cast<const char*>(body->data());
                    parsed = crypto::x509::revocation_list::from_pem(string(std::string(p, body->size())));
                }
                if (!parsed) {
                    continue;
                }
                crl = *parsed;
                RevocationCacheState::Entry e;
                e.key = key;
                e.crl = crl;
                auto next = crl->next_update();
                e.expires = next ? next->unix() : now + 3600;
                cache->put(e);
            }
            auto s = crl->status_of(c, issuer);
            if (s) {
                co_return *s;
            }
        }
        co_return nullopt;
    }

    // The status by the program's CRLs: a complete list of the
    // certificate's issuer that says, with a delta of it when there is one
    inline optional<crypto::x509::revocation_status> crl_offline(const crypto::x509::certificate& c, const crypto::x509::certificate& issuer,
                                                                 const vector<crypto::x509::revocation_list>& crls) noexcept {
        for (const auto& base : crls) {
            if (base.is_delta()) {
                continue;
            }
            for (const auto& delta : crls) {
                if (delta.is_delta()) {
                    if (auto s = base.status_of(c, issuer, delta)) {
                        return *s;
                    }
                }
            }
            if (auto s = base.status_of(c, issuer)) {
                return *s;
            }
        }
        return nullopt;
    }

    SGCL_INLINE_HOT RevocationOutcome revocation_failure(AlertDescription a, crypto::x509::reason r, const std::string& what) {
        RevocationOutcome o;
        o.ok = false;
        o.alert = a;
        o.reason = r;
        o.what = string(what);
        return o;
    }

    // The check of a chain by the policy (tls::config::revocation): each
    // certificate but the root, its issuer the next. The leaf: its staple,
    // verified (a staple that does not verify fails, whatever the policy:
    // the peer sent it); Must-Staple without one fails. Then the program's
    // CRLs; then, for soft_fail and hard_fail, OCSP online and the CRLs of
    // the distribution points (fetch_crls). Revoked fails always; a status
    // unknown fails hard_fail alone
    inline async::task<RevocationOutcome> co_check_revocation(RevocationCheck in) noexcept {
        using crypto::x509::revocation_status;
        RevocationOutcome out;
        if (in.mode == RevocationMode::off) {
            co_return out;
        }
        if (in.chain.size() < 2) {
            out.status = revocation_status::good;   // a root alone: nothing to revoke
            co_return out;
        }
        const bool online = in.mode == RevocationMode::soft_fail || in.mode == RevocationMode::hard_fail;
        for (size_t i = 0; i + 1 < in.chain.size(); ++i) {
            const auto& c = in.chain[i];
            const auto& issuer = in.chain[i + 1];
            optional<revocation_status> status;
            RevocationSource source = RevocationSource::none;
            if (i == 0) {
                bool stapled = false;
                // a staple verified before, for this leaf of this issuer and
                // not past its nextUpdate: kept in the cache by its bytes, so a
                // server's staple costs its signatures once, not every
                // handshake (the same answer, signed once by its responder)
                string staple_key;
                if (!in.staple.empty()) {
                    crypto::sha256 h;
                    h.update(in.staple.as_slice());
                    h.update(c.raw());
                    h.update(issuer.raw());
                    staple_key = string("staple:" + hex_of(h.value()));
                    if (auto hit = in.cache->find(staple_key, time::now().unix())) {
                        stapled = true;
                        if (hit->status != revocation_status::unknown) {
                            status = hit->status;
                            source = RevocationSource::staple;
                        }
                    }
                }
                if (!in.staple.empty() && !stapled) {
                    auto resp = crypto::x509::ocsp_response::parse(in.staple.as_slice());
                    if (!resp) {
                        co_return revocation_failure(AlertDescription::bad_certificate_status_response, crypto::x509::reason::revocation_unknown,
                                                     "the stapled OCSP response does not parse: " + std::string(resp.error().message().view()));
                    }
                    auto single = resp->verify(c, issuer);
                    if (!single) {
                        co_return revocation_failure(AlertDescription::bad_certificate_status_response, crypto::x509::reason::revocation_unknown,
                                                     "the stapled OCSP response does not verify: " + std::string(single.error().message().view()));
                    }
                    stapled = true;
                    if (single->status != revocation_status::unknown) {
                        status = single->status;
                        source = RevocationSource::staple;
                    }
                    RevocationCacheState::Entry e;
                    e.key = staple_key;
                    e.status = single->status;
                    // until its nextUpdate and the skew verify allows, or the
                    // age verify takes for a response without one
                    e.expires = (single->next_update ? single->next_update->unix() : single->this_update.unix() + 7 * 86400) + 300;
                    in.cache->put(e);
                }
                if (c.must_staple() && !stapled) {
                    co_return revocation_failure(AlertDescription::bad_certificate_status_response, crypto::x509::reason::revocation_unknown,
                                                 "the certificate " + describe_certificate(c) + " requires an OCSP staple (Must-Staple, RFC 7633) and none came");
                }
            }
            if (!status && !in.crls.empty()) {
                if ((status = crl_offline(c, issuer, in.crls))) {
                    source = RevocationSource::crl;
                }
            }
            if (!status && online) {
                if ((status = co_await co_ocsp_online(c, issuer, in.cache, in.deadline))) {
                    source = RevocationSource::ocsp;
                } else if (in.fetch_crls && (status = co_await co_crl_online(c, issuer, in.cache, in.deadline))) {
                    source = RevocationSource::crl;
                }
            }
            if (status == revocation_status::revoked) {
                co_return revocation_failure(AlertDescription::certificate_revoked, crypto::x509::reason::revoked, "the certificate " + describe_certificate(c) + " is revoked");
            }
            if (!status && in.mode == RevocationMode::hard_fail) {
                co_return revocation_failure(AlertDescription::certificate_unknown, crypto::x509::reason::revocation_unknown,
                                             "the revocation status of the certificate " + describe_certificate(c) + " could not be established");
            }
            if (i == 0) {
                out.status = status ? *status : revocation_status::unknown;
                out.source = source;
            }
        }
        co_return out;
    }

    // --- a server's staple ------------------------------------------------------

    // The OCSP response of a leaf fetched from its responders (AIA) and
    // verified under its issuer: its DER, its thisUpdate and nextUpdate;
    // nullopt when no responder gave one
    struct FetchedStaple {
        vector<byte> der;
        int64_t this_update = 0;
        int64_t next_update = 0;
    };

    inline async::task<optional<FetchedStaple>> co_fetch_staple(crypto::x509::certificate leaf, crypto::x509::certificate issuer, time_point deadline) noexcept {
        for (const auto& server : leaf.ocsp_servers()) {
            auto req = crypto::x509::ocsp_request::make(leaf, issuer);
            if (!req) {
                co_return nullopt;
            }
            string get = req->url(server);
            vector<byte> post;
            if (get.size() > 255) {
                post = vector<byte>(req->raw().data(), req->raw().data() + req->raw().size());
            }
            auto body = co_await http_fetch(post.empty() ? get : server, post, deadline, MaxOcspFetch);
            if (!body) {
                continue;
            }
            auto resp = crypto::x509::ocsp_response::parse(body->as_slice());
            if (!resp) {
                continue;
            }
            auto single = resp->verify(leaf, issuer);
            if (!single) {
                continue;
            }
            FetchedStaple f;
            f.der = std::move(*body);
            f.this_update = single->this_update.unix();
            f.next_update = single->next_update ? single->next_update->unix() : 0;
            co_return f;
        }
        co_return nullopt;
    }
}
