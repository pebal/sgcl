//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../http/request.h"
#include "../http/response_writer.h"
#include "../../async/coroutine.h"
#include "../../async/stop_token.h"
#include "../../core/aliases.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../encoding/json.h"

#include <mutex>
#include <string>
#include <string_view>

// The methods of JSON-RPC 2.0 that a peer or an HTTP route serves, and the
// dispatch of a request, a notification or a batch to them
namespace sgcl::net::jsonrpc {
    class methods;

    namespace detail {
        struct RpcMethodsAccess;

        using RpcSync = function<expected<json, error>(const json&)>;
        using RpcAsync = function<async::task<expected<json, error>>(json, async::stop_token)>;
        using RpcNote = function<void(const json&)>;

        struct RpcMethod {
            RpcSync sync;
            RpcAsync wait;
            RpcNote note;
        };

        struct RpcMethodsState {
            std::mutex lock;
            map<string, RpcMethod> table;
        };

        inline json rpc_error_object(const error& e) {
            json o = json::object({{string("code"), json(int64_t(e.code))}, {string("message"), json(e.message)}});
            if (e.data) {
                o = o.set(string("data"), *e.data);
            }
            return o;
        }

        inline json rpc_response_error(const json& id, const error& e) {
            return json::object({{string("jsonrpc"), json("2.0")}, {string("id"), id}, {string("error"), rpc_error_object(e)}});
        }

        inline json rpc_response(const json& id, const json& result) {
            return json::object({{string("jsonrpc"), json("2.0")}, {string("id"), id}, {string("result"), result}});
        }

        // Whether a value may be a request's id: a string, a number, null
        inline bool rpc_id_valid(const json& id) noexcept {
            return id.is_string() || id.is_number() || id.is_null();
        }

        // A request checked (§4): its id, its method's entry and params; the
        // error response when it is not a request, or none for a
        // notification that needs no answer
        struct RpcCall {
            bool has_id = false;
            json id;
            json params;
            RpcMethod entry;
            bool found = false;
            optional<json> answer;   // set: the request is answered with this (or, for a notification, done)
            bool done = false;
        };

        inline RpcCall rpc_check(RpcMethodsState& m, const json& msg) {
            RpcCall c;
            if (!msg.is_object()) {
                c.answer = rpc_response_error(json(nullptr), error(errc::invalid_request, string("Invalid Request")));
                c.done = true;
                return c;
            }
            c.has_id = msg.contains(string("id"));
            c.id = c.has_id ? msg["id"] : json(nullptr);
            if (c.has_id && !rpc_id_valid(c.id)) {
                c.answer = rpc_response_error(json(nullptr), error(errc::invalid_request, string("Invalid Request")));
                c.done = true;
                return c;
            }
            const json& version = msg["jsonrpc"];
            const json& method = msg["method"];
            if (version.as_string(string()) != "2.0" || !method.is_string()) {
                c.answer = rpc_response_error(c.id, error(errc::invalid_request, string("Invalid Request")));
                c.done = true;
                return c;
            }
            bool has_params = msg.contains(string("params"));
            c.params = has_params ? msg["params"] : json(nullptr);
            if (has_params && !c.params.is_array() && !c.params.is_object()) {
                if (c.has_id) {
                    c.answer = rpc_response_error(c.id, error(errc::invalid_params, string("Invalid params")));
                }
                c.done = true;
                return c;
            }
            {
                std::lock_guard g(m.lock);
                auto it = m.table.find(*method.as_string());
                if (it != m.table.end()) {
                    c.entry = it->second;
                    c.found = true;
                }
            }
            if (c.has_id && (!c.found || (!c.entry.sync && !c.entry.wait))) {
                c.answer = rpc_response_error(c.id, error(errc::method_not_found, string("Method not found")));
                c.done = true;
            } else if (!c.has_id && (!c.found || c.entry.note)) {
                if (c.found) {
                    c.entry.note(c.params);
                }
                c.done = true;
            } else if (c.entry.sync) {
                // a handler that answers at once: answered here, no task
                expected<json, error> r = c.entry.sync(c.params);
                if (c.has_id) {
                    c.answer = r ? rpc_response(c.id, *r) : rpc_response_error(c.id, r.error());
                }
                c.done = true;
            }
            return c;
        }

        // One request or notification (an element of a batch, or the
        // message itself): its response, none for a notification. stop is
        // the request's own (stopped when the other side cancels it)
        inline async::task<optional<json>> rpc_dispatch(tracked_ptr<RpcMethodsState> m, json msg, async::stop_token stop) noexcept {
            RpcCall c = rpc_check(*m, msg);
            if (c.done) {
                co_return c.answer;
            }
            expected<json, error> r = co_await c.entry.wait(c.params, stop);   // a method called as a notification runs, its result unsent (§4.1)
            if (!c.has_id) {
                co_return nullopt;
            }
            if (!r) {
                co_return rpc_response_error(c.id, r.error());
            }
            co_return rpc_response(c.id, *r);
        }

        // A message's text answered without a task when every method it
        // calls answers at once: the answer (an empty optional inside for
        // nothing to answer), or none when a method waits
        inline optional<optional<string>> rpc_handle_sync(RpcMethodsState& m, const string& text) {
            auto msg = json::parse(text);
            if (!msg) {
                return optional<string>(rpc_response_error(json(nullptr), error(errc::parse_error, string("Parse error"))).to_string());
            }
            if (!msg->is_array()) {
                RpcCall c = rpc_check(m, *msg);
                if (!c.done) {
                    return nullopt;
                }
                return c.answer ? optional<string>(c.answer->to_string()) : optional<string>();
            }
            if (msg->empty()) {
                return optional<string>(rpc_response_error(json(nullptr), error(errc::invalid_request, string("Invalid Request"))).to_string());
            }
            for (auto& e : msg->elements()) {   // a method that waits anywhere in the batch: the whole batch by tasks
                if (e.is_object() && e["method"].is_string()) {
                    std::lock_guard g(m.lock);
                    auto it = m.table.find(*e["method"].as_string());
                    if (it != m.table.end() && it->second.wait) {
                        return nullopt;
                    }
                }
            }
            json out = json::array({});
            for (auto& e : msg->elements()) {
                RpcCall c = rpc_check(m, e);
                if (c.answer) {
                    out = out.push_back(*c.answer);
                }
            }
            if (out.empty()) {
                return optional<string>();
            }
            return optional<string>(out.to_string());
        }

        // A whole message's text: a request, a notification or a batch;
        // the response's text, none when there is nothing to answer
        inline async::task<optional<string>> rpc_handle_text(tracked_ptr<RpcMethodsState> m, string text, async::stop_token stop) noexcept {
            auto msg = json::parse(text);
            if (!msg) {
                co_return rpc_response_error(json(nullptr), error(errc::parse_error, string("Parse error"))).to_string();
            }
            if (!msg->is_array()) {
                auto r = co_await rpc_dispatch(m, *msg, stop);
                if (!r) {
                    co_return nullopt;
                }
                co_return r->to_string();
            }
            if (msg->empty()) {
                co_return rpc_response_error(json(nullptr), error(errc::invalid_request, string("Invalid Request"))).to_string();
            }
            // a batch: its elements at once, their responses gathered in the order of the requests
            vector<async::task<optional<json>>> running;
            for (auto& e : msg->elements()) {
                running.push_back(async::spawn(rpc_dispatch(m, e, stop)));
            }
            json out = json::array({});
            for (auto& t : running) {
                if (auto r = co_await t) {
                    out = out.push_back(*r);
                }
            }
            if (out.empty()) {
                co_return nullopt;   // notifications only: nothing at all (§6)
            }
            co_return out.to_string();
        }
    }

    // The methods a peer or an HTTP route serves, by name: a handler that
    // answers at once, one that waits (a task, given the call's stop token,
    // stopped when the caller cancels), or one of a notification. A handle
    // of one word: copies share the table, which may change while it serves.
    //
    //     net::jsonrpc::methods m;
    //     m.add("sum", [](const json& p) -> expected<json, net::jsonrpc::error> {
    //         return json(p[0].as_int(0) + p[1].as_int(0));
    //     });
    class methods {
    public:
        methods()
        : _s(make_tracked<detail::RpcMethodsState>()) {
        }

        // A method that answers at once: its params (an array, an object,
        // or null when the call has none), its result or an error object
        methods& add(const string& name, function<expected<json, error>(const json& params)> f) {
            std::lock_guard g(_s->lock);
            _s->table[name] = detail::RpcMethod{std::move(f), {}, {}};
            return *this;
        }

        // A method that waits: a task of the params and the call's stop token
        methods& add_task(const string& name, function<async::task<expected<json, error>>(json params, async::stop_token stop)> f) {
            std::lock_guard g(_s->lock);
            _s->table[name] = detail::RpcMethod{{}, std::move(f), {}};
            return *this;
        }

        // A notification: its params, nothing answered
        methods& add_notification(const string& name, function<void(const json& params)> f) {
            std::lock_guard g(_s->lock);
            _s->table[name] = detail::RpcMethod{{}, {}, std::move(f)};
            return *this;
        }

        // The method of the name taken out
        methods& remove(const string& name) {
            std::lock_guard g(_s->lock);
            _s->table.erase(name);
            return *this;
        }

        // A message's text (a request, a notification or a batch) and the
        // response's text; none when there is nothing to answer: the
        // methods on a transport of the program's own
        // `handle(...)` on this thread, `co_await async_handle(...)` in a task
        optional<string> handle(const string& text) const {
            if (auto r = detail::rpc_handle_sync(*_s, text)) {   // every method it calls answers at once: no task
                return *r;
            }
            return async_handle(text).wait();
        }

        async::task<optional<string>> async_handle(string text) const noexcept {
            return detail::rpc_handle_text(_s, std::move(text), async::stop_token());
        }

        // The methods as an HTTP handler: a POST's body handled, its response
        // the reply (application/json), 204 for notifications only, 405 for
        // another method
        //
        //     srv.route("POST /rpc", [m](http::request r, http::response_writer w) { return m.async_serve(r, w); });
        async::task<> async_serve(http::request r, http::response_writer w) const noexcept {
            return _co_serve(_s, std::move(r), std::move(w));
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const methods& a, const methods& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::RpcMethodsAccess;

        static async::task<> _co_serve(tracked_ptr<detail::RpcMethodsState> s, http::request r, http::response_writer w) noexcept {
            if (r.method() != "POST") {
                w.set_status(405).set_header(string("Allow"), string("POST"));
                co_return;
            }
            auto body = co_await r.async_text();
            if (!body) {
                w.set_status(400);
                co_return;
            }
            optional<string> out;
            if (auto now = detail::rpc_handle_sync(*s, *body)) {   // every method it calls answers at once: no task
                out = *now;
            } else {
                out = co_await detail::rpc_handle_text(s, *body, r.stop());
            }
            if (!out) {
                w.set_status(204);
                co_return;
            }
            w.set_header(string("Content-Type"), string("application/json"));
            w.write(*out);
        }

        tracked_ptr<detail::RpcMethodsState> _s;
    };

    namespace detail {
        struct RpcMethodsAccess {
            static const tracked_ptr<RpcMethodsState>& state(const methods& m) noexcept {
                return m._s;
            }
        };
    }
}
