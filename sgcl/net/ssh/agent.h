//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "keys.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../../async/mutex.h"
#include "../../core/detail/handle_word.h"
#include "../../io/os.h"

#include <string>
#include <string_view>

// The client of an SSH agent (draft-miller-ssh-agent), ssh-agent's own
// protocol over a unix socket: the keys it holds listed, a signature by
// one of them asked for, keys added (for a time or for good) and removed.
// The private keys stay in the agent; what comes back are public keys and
// signatures. A client's authentication uses the agent at $SSH_AUTH_SOCK
// by itself (client::options::agent).
namespace sgcl::net::ssh {
    namespace detail {
        enum : uint8_t {
            AgentFailure = 5,
            AgentSuccess = 6,
            AgentRequestIdentities = 11,
            AgentIdentitiesAnswer = 12,
            AgentSignRequest = 13,
            AgentSignResponse = 14,
            AgentAddIdentity = 17,
            AgentRemoveIdentity = 18,
            AgentRemoveAllIdentities = 19,
            AgentAddIdConstrained = 25,
        };

        inline constexpr uint8_t AgentConstrainLifetime = 1;
        inline constexpr uint32_t AgentRsaSha256 = 2;
        inline constexpr uint32_t AgentRsaSha512 = 4;
        inline constexpr uint32_t AgentMaxMessage = 256 * 1024;

        struct AgentState {
            net::connection c;
            async::mutex lock;   // one request and its answer at a time
            string path;
        };

        inline io::error agent_error(net::errc code, const string& path, std::string_view what) noexcept {
            return io::error(net::make_error_code(code), "ssh agent", string(std::string(path.view()) + ": " + std::string(what)));
        }

        // A request sent and its answer read (its type first)
        inline async::task<expected<Bytes, io::error>> co_agent_call(tracked_ptr<AgentState> s, Bytes request) noexcept {
            auto g = co_await s->lock.scoped_lock();
            Bytes framed(4);
            store32(framed.data(), uint32_t(request.size()));
            framed.insert(framed.end(), request.begin(), request.end());
            wipe(request);
            auto w = co_await s->c.async_write(slice<const byte>(reinterpret_cast<const byte*>(framed.data()), framed.size()));
            wipe(framed);
            if (!w) {
                co_return fail(w);
            }
            uint8_t len[4];
            auto r = co_await s->c.async_read_full(mutable_bytes_of(len, 4));
            if (!r) {
                co_return fail(r);
            }
            if (*r == 0) {
                co_return fail(agent_error(net::errc::ssh_disconnected, s->path, "the agent closed the connection"));
            }
            uint32_t n = load32(len);
            if (n == 0 || n > AgentMaxMessage) {
                co_return fail(agent_error(net::errc::ssh_protocol, s->path, "an answer of a length out of range"));
            }
            Bytes reply(n);
            auto b = co_await s->c.async_read_full(mutable_bytes_of(reply.data(), n));
            if (!b) {
                co_return fail(b);
            }
            if (*b != n) {
                co_return fail(agent_error(net::errc::ssh_protocol, s->path, "a truncated answer"));
            }
            co_return reply;
        }

        // The answer of a request with no data back: success, or the
        // agent's refusal (ssh_request_refused)
        inline async::task<expected<void, io::error>> co_agent_simple(tracked_ptr<AgentState> s, Bytes request, const char* what) noexcept {
            auto r = co_await co_agent_call(s, std::move(request));
            if (!r) {
                co_return fail(r);
            }
            if (r->size() != 1 || (*r)[0] != AgentSuccess) {
                co_return fail(agent_error(net::errc::ssh_request_refused, s->path, what));
            }
            co_return expected<void, io::error>();
        }

        // An IDENTITIES_ANSWER read: the keys of kinds read here, with their
        // comments; false for one that breaks the protocol
        inline bool read_identities(const uint8_t* p, size_t n, vector<public_key>& out) {
            Reader rd(p, n);
            if (rd.u8() != AgentIdentitiesAnswer) {
                return false;
            }
            uint32_t count = rd.u32();
            for (uint32_t i = 0; i < count && rd.ok(); ++i) {
                Span blob = rd.string();
                Span comment = rd.string();
                if (!rd.ok()) {
                    break;
                }
                auto k = public_key::from_bytes(blob.bytes());
                if (k) {   // a kind not read here (a security key's) passed over
                    out.push_back(k->with_comment(string(printable(comment.view()))));
                }
            }
            return rd.ok();
        }

        // A SIGN_RESPONSE read: the signature blob; false for anything else
        inline bool read_sign_response(const uint8_t* p, size_t n, Bytes& sig) {
            Reader rd(p, n);
            if (rd.u8() != AgentSignResponse) {
                return false;
            }
            Span s = rd.string();
            if (!rd.done()) {
                return false;
            }
            sig.assign(s.p, s.p + s.n);
            return true;
        }

        // The agent's keys: their blobs with their comments
        inline async::task<expected<vector<public_key>, io::error>> co_agent_list(tracked_ptr<AgentState> s) noexcept {
            auto r = co_await co_agent_call(s, Bytes{AgentRequestIdentities});
            if (!r) {
                co_return fail(r);
            }
            vector<public_key> out;
            if (!read_identities(r->data(), r->size(), out)) {
                co_return fail(agent_error(r->empty() || (*r)[0] != AgentIdentitiesAnswer ? net::errc::ssh_request_refused : net::errc::ssh_protocol, s->path,
                                           "the keys were not listed"));
            }
            co_return out;
        }

        // A signature by the agent's key of a blob, with an algorithm's
        // flags (RSA: rsa-sha2-256 or -512)
        inline async::task<expected<Bytes, io::error>> co_agent_sign(tracked_ptr<AgentState> s, string blob, Bytes data, uint32_t flags) noexcept {
            Bytes req;
            Writer w(req);
            w.u8(AgentSignRequest).string(blob.view()).string(data).u32(flags);
            auto r = co_await co_agent_call(s, std::move(req));
            if (!r) {
                co_return fail(r);
            }
            Bytes sig;
            if (!read_sign_response(r->data(), r->size(), sig)) {
                co_return fail(agent_error(net::errc::ssh_request_refused, s->path, "the agent did not sign"));
            }
            co_return sig;
        }

        inline async::task<expected<tracked_ptr<AgentState>, io::error>> co_agent_connect(string path) noexcept {
            if (path.empty()) {
                auto env = io::getenv(string("SSH_AUTH_SOCK"));
                if (!env || env->empty()) {
                    co_return fail(io::error(io::errc::not_found, "ssh agent", string("SSH_AUTH_SOCK is not set")));
                }
                path = *env;
            }
            auto c = co_await net::unix_domain::async_connect(path);
            if (!c) {
                co_return fail(c);
            }
            tracked_ptr s = make_tracked<AgentState>();
            s->c = *c;
            s->path = path;
            co_return s;
        }

        struct AgentAccess;
    }

    // A connection to an SSH agent: a handle of one word, its copies the
    // same connection, a request and its answer at a time
    class agent {
    public:
        agent() noexcept = default;

        // The agent at a unix socket's path; empty: $SSH_AUTH_SOCK's
        // (io::errc::not_found when it is not set)
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<agent, io::error> connect(const string& path = {}) {
            return async_connect(path).wait();
        }

        static async::task<expected<agent, io::error>> async_connect(string path = {}) noexcept {
            auto s = co_await detail::co_agent_connect(std::move(path));
            if (!s) {
                co_return unexpected(s.error());
            }
            co_return agent(*s);
        }

        // The keys the agent holds, each with its comment
        // `list(...)` on this thread, `co_await async_list(...)` in a task
        expected<vector<public_key>, io::error> list() const {
            return detail::co_agent_list(_s).wait();
        }

        async::task<expected<vector<public_key>, io::error>> async_list() const noexcept {
            return detail::co_agent_list(_s);
        }

        // The signature blob of data by the agent's key (an RSA key's by
        // rsa-sha2-512)
        // `sign(...)` on this thread, `co_await async_sign(...)` in a task
        expected<vector<byte>, io::error> sign(const public_key& key, const slice<const byte>& data) const {
            return async_sign(key, data).wait();
        }

        async::task<expected<vector<byte>, io::error>> async_sign(const public_key& key, const slice<const byte>& data) const noexcept {
            return _co_sign(_s, key, detail::Bytes(reinterpret_cast<const uint8_t*>(data.data()), reinterpret_cast<const uint8_t*>(data.data()) + data.size()));
        }

        // The key added with its comment, held for `lifetime` (zero: until
        // removed or the agent ends)
        // `add(...)` on this thread, `co_await async_add(...)` in a task
        expected<void, io::error> add(const private_key& key, duration lifetime = duration::zero()) const {
            return async_add(key, lifetime).wait();
        }

        async::task<expected<void, io::error>> async_add(const private_key& key, duration lifetime = duration::zero()) const noexcept {
            detail::Bytes req;
            detail::Writer w(req);
            const detail::KeyPair& k = detail::PrivateKeyAccess::key(key);
            w.u8(lifetime > duration::zero() ? detail::AgentAddIdConstrained : detail::AgentAddIdentity);
            detail::write_key_fields(w, k);
            w.string(k.comment);
            if (lifetime > duration::zero()) {
                w.u8(detail::AgentConstrainLifetime).u32(uint32_t(std::max<int64_t>(1, int64_t(lifetime.seconds()))));
            }
            return detail::co_agent_simple(_s, std::move(req), "the key was not added");
        }

        // The key taken out of the agent
        // `remove(...)` on this thread, `co_await async_remove(...)` in a task
        expected<void, io::error> remove(const public_key& key) const {
            return async_remove(key).wait();
        }

        async::task<expected<void, io::error>> async_remove(const public_key& key) const noexcept {
            detail::Bytes req;
            detail::Writer w(req);
            w.u8(detail::AgentRemoveIdentity).string(detail::PublicKeyAccess::blob(key).view());
            return detail::co_agent_simple(_s, std::move(req), "the key was not removed");
        }

        // Every key taken out of the agent
        // `remove_all(...)` on this thread, `co_await async_remove_all(...)` in a task
        expected<void, io::error> remove_all() const {
            return async_remove_all().wait();
        }

        async::task<expected<void, io::error>> async_remove_all() const noexcept {
            return detail::co_agent_simple(_s, detail::Bytes{detail::AgentRemoveAllIdentities}, "the keys were not removed");
        }

        // The connection to the agent closed
        expected<void, io::error> close() const noexcept {
            return _s->c.close();
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_s;
        }

    private:
        friend struct detail::AgentAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit agent(tracked_ptr<detail::AgentState> s) noexcept
        : _s(std::move(s)) {
        }

        SGCL_INLINE_HOT agent(sgcl::detail::FromWord, const tracked_ptr<detail::AgentState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::AgentState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::AgentState>& _handle_word() const noexcept {
            return _s;
        }

        static async::task<expected<vector<byte>, io::error>> _co_sign(tracked_ptr<detail::AgentState> s, public_key key, detail::Bytes data) noexcept {
            const uint32_t flags = key.type() == key_type::rsa ? detail::AgentRsaSha512 : 0;
            auto r = co_await detail::co_agent_sign(s, detail::PublicKeyAccess::blob(key), std::move(data), flags);
            if (!r) {
                co_return unexpected(r.error());
            }
            vector<byte> out(r->size());
            sgcl::detail::copy_bytes(out.data(), r->data(), r->size());
            co_return out;
        }

        tracked_ptr<detail::AgentState> _s;
    };

    namespace detail {
        struct AgentAccess {
            SGCL_INLINE_HOT static const tracked_ptr<AgentState>& state(const agent& a) noexcept {
                return a._s;
            }

            SGCL_INLINE_HOT static agent make(const tracked_ptr<AgentState>& s) noexcept {
                return agent(s);
            }
        };
    }
}
