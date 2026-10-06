//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "peer.h"
#include "../http/client.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../encoding/json.h"

#include <atomic>
#include <string>

// JSON-RPC 2.0 over HTTP: each call a POST of application/json
namespace sgcl::net::jsonrpc {
    namespace detail {
        struct RpcClientState {
            string url;
            http::client http;
            std::atomic<int64_t> next_id = {0};
        };
    }

    // A client of a JSON-RPC service over HTTP: a call is a POST of the
    // request to the URL, its response the reply; a batch one POST of all
    // of them. A handle of one word: copies share the HTTP client.
    //
    //     net::jsonrpc::client rpc("http://localhost:8080/rpc");
    //     auto sum = rpc.call("sum", json::array({1, 2}));
    class client {
    public:
        client() noexcept = default;

        // A client of the URL through an HTTP client of the default settings
        explicit client(const string& url)
        : client(url, http::client()) {
        }

        // The same through the program's HTTP client (its TLS, its proxy,
        // its timeouts, its pool)
        client(const string& url, const http::client& h)
        : _s(make_tracked<detail::RpcClientState>()) {
            _s->url = url;
            _s->http = h;
        }

        // A call and its result, as peer::call: the error object of a
        // refusal as an error of the category "jsonrpc"; an HTTP status
        // other than 200 as net::errc::http_status
        // `call(...)` on this thread, `co_await async_call(...)` in a task
        expected<json, io::error> call(const string& method, const json& params = json()) const {
            return async_call(method, params).wait();
        }

        async::task<expected<json, io::error>> async_call(string method, json params = json()) const noexcept {
            return _co_call(_s, std::move(method), std::move(params));
        }

        // A notification: posted, the service's 204 (or 200) taken
        // `notify(...)` on this thread, `co_await async_notify(...)` in a task
        expected<void, io::error> notify(const string& method, const json& params = json()) const {
            return async_notify(method, params).wait();
        }

        async::task<expected<void, io::error>> async_notify(string method, json params = json()) const noexcept {
            return _co_notify(_s, std::move(method), std::move(params));
        }

        // A batch in one POST: the outcomes of its calls in their order
        // `batch(...)` on this thread, `co_await async_batch(...)` in a task
        expected<vector<expected<json, io::error>>, io::error> batch(const vector<batch_entry>& entries) const {
            return async_batch(entries).wait();
        }

        async::task<expected<vector<expected<json, io::error>>, io::error>> async_batch(vector<batch_entry> entries) const noexcept {
            return _co_batch(_s, std::move(entries));
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const client& a, const client& b) noexcept {
            return a._s == b._s;
        }

    private:
        static async::task<expected<string, io::error>> _post(tracked_ptr<detail::RpcClientState> s, string body, string op, bool empty_ok) noexcept {
            auto r = co_await s->http.async_post(s->url, string("application/json"), body);
            if (!r) {
                co_return unexpected(r.error());
            }
            int status = r->status();
            auto text = co_await r->async_text();
            if (!text) {
                co_return unexpected(text.error());
            }
            if (status == 204 && empty_ok) {
                co_return string();
            }
            if (status != 200) {
                co_return unexpected(io::error(net::make_error_code(net::errc::http_status), op, string(std::to_string(status))));
            }
            co_return *text;
        }

        static async::task<expected<json, io::error>> _co_call(tracked_ptr<detail::RpcClientState> s, string method, json params) noexcept {
            using namespace detail;
            int64_t id = ++s->next_id;
            string op = string::concat("jsonrpc ", method);
            auto text = co_await _post(s, rpc_request(id, method, params, false).to_string(), op, false);
            if (!text) {
                co_return unexpected(text.error());
            }
            auto msg = json::parse(*text);
            if (!msg || !msg->is_object() || (*msg)["id"].as_int() != id) {
                co_return unexpected(rpc_error(errc::malformed, op, string("a response that is not the call's")));
            }
            co_return rpc_outcome(*msg, method);
        }

        static async::task<expected<void, io::error>> _co_notify(tracked_ptr<detail::RpcClientState> s, string method, json params) noexcept {
            auto text = co_await _post(s, detail::rpc_request(0, method, params, true).to_string(), string::concat("jsonrpc ", method), true);
            if (!text) {
                co_return unexpected(text.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<vector<expected<json, io::error>>, io::error>> _co_batch(tracked_ptr<detail::RpcClientState> s, vector<batch_entry> entries) noexcept {
            using namespace detail;
            if (entries.empty()) {
                co_return vector<expected<json, io::error>>();
            }
            json all = json::array({});
            vector<int64_t> ids;
            for (auto& e : entries) {
                int64_t id = e.notification ? 0 : ++s->next_id;
                ids.push_back(id);
                all = all.push_back(rpc_request(id, e.method, e.params, e.notification));
            }
            bool calls = false;
            for (auto& e : entries) {
                calls |= !e.notification;
            }
            auto text = co_await _post(s, all.to_string(), string("jsonrpc batch"), !calls);
            if (!text) {
                co_return unexpected(text.error());
            }
            vector<expected<json, io::error>> out;
            if (!calls) {
                co_return out;
            }
            auto msg = json::parse(*text);
            if (!msg || !msg->is_array()) {
                co_return unexpected(rpc_error(errc::malformed, string("jsonrpc batch"), string("a response that is no array")));
            }
            map<int64_t, json> by_id;
            for (auto& r : msg->elements()) {
                if (auto id = r["id"].as_int()) {
                    by_id[*id] = r;
                }
            }
            for (size_t i = 0; i < entries.size(); ++i) {
                if (entries[i].notification) {
                    continue;
                }
                auto it = by_id.find(ids[i]);
                if (it == by_id.end()) {
                    out.push_back(unexpected(rpc_error(errc::malformed, string::concat("jsonrpc ", entries[i].method), string("no response of the call"))));
                } else {
                    out.push_back(rpc_outcome(it->second, entries[i].method));
                }
            }
            co_return out;
        }

        tracked_ptr<detail::RpcClientState> _s;
    };
}
