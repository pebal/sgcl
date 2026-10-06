//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::jsonrpc: the methods on the examples of the specification (JSON-RPC
// 2.0 §7), peers over TCP (both framings, both directions, batches,
// cancellation, timeouts) and over a WebSocket, the client over HTTP against
// the methods as an HTTP handler, and a peer of Python's (json and socket
// alone, python/peer.py) as an implementation of its own of LSP's framing.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http.h"
#include "sgcl/net/jsonrpc.h"

#include <cstdio>
#include <cstdlib>

namespace {
    namespace rpc = sgcl::net::jsonrpc;
    namespace http = sgcl::net::http;
    using encoding::json;

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    json parse(const char* text) {
        return json::parse(sgcl::string(text)).value();
    }

    // The specification's methods: subtract (by position or by name), update and foobar notifications
    rpc::methods spec_methods() {
        rpc::methods m;
        m.add("subtract", [](const json& p) -> expected<json, rpc::error> {
            if (p.is_array()) {
                return json(p[0].as_int(0) - p[1].as_int(0));
            }
            if (p.is_object()) {
                return json(p["minuend"].as_int(0) - p["subtrahend"].as_int(0));
            }
            return unexpected(rpc::error(rpc::errc::invalid_params, "Invalid params"));
        });
        m.add("sum", [](const json& p) -> expected<json, rpc::error> {
            int64_t s = 0;
            for (auto& v : p.elements()) {
                s += v.as_int(0);
            }
            return json(s);
        });
        m.add("get_data", [](const json&) -> expected<json, rpc::error> { return json::array({json("hello"), json(5)}); });
        m.add_notification("update", [](const json&) {});
        m.add_notification("notify_hello", [](const json&) {});
        return m;
    }

    std::string handle(const rpc::methods& m, const char* text) {
        auto r = m.handle(sgcl::string(text));
        return r ? str(*r) : std::string("(none)");
    }

    // Two connections of one TCP connection over the loopback
    std::pair<net::connection, net::connection> socket_pair() {
        auto l = net::tcp::listen("127.0.0.1:0").value();
        auto accepted = async::spawn([](net::listener l) -> async::task<expected<net::connection, io::error>> { co_return co_await l.async_accept(); }(l));
        auto a = net::tcp::connect(l.local_endpoint().to_string()).value();
        auto b = accepted.wait().value();
        (void)l.close();
        return {a, b};
    }

    std::string run_command(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }
}

TEST(JsonRpcMethods, SpecificationExamples) {
    auto m = spec_methods();
    // §7, by position and by name
    EXPECT_EQ(handle(m, R"({"jsonrpc": "2.0", "method": "subtract", "params": [42, 23], "id": 1})"), R"({"jsonrpc":"2.0","id":1,"result":19})");
    EXPECT_EQ(handle(m, R"({"jsonrpc": "2.0", "method": "subtract", "params": [23, 42], "id": 2})"), R"({"jsonrpc":"2.0","id":2,"result":-19})");
    EXPECT_EQ(handle(m, R"({"jsonrpc": "2.0", "method": "subtract", "params": {"subtrahend": 23, "minuend": 42}, "id": 3})"),
              R"({"jsonrpc":"2.0","id":3,"result":19})");
    // a notification, and a method that is not there
    EXPECT_EQ(handle(m, R"({"jsonrpc": "2.0", "method": "update", "params": [1,2,3,4,5]})"), "(none)");
    EXPECT_EQ(handle(m, R"({"jsonrpc": "2.0", "method": "foobar", "id": "1"})"),
              R"({"jsonrpc":"2.0","id":"1","error":{"code":-32601,"message":"Method not found"}})");
    // invalid JSON, an invalid request object
    EXPECT_EQ(handle(m, R"({"jsonrpc": "2.0", "method": "foobar, "params": "bar", "baz])"),
              R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}})");
    EXPECT_EQ(handle(m, R"({"jsonrpc": "2.0", "method": 1, "params": "bar"})"),
              R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}})");
    // batches: invalid JSON, empty, not empty but invalid, invalid, mixed, notifications only
    EXPECT_EQ(handle(m, R"([{"jsonrpc": "2.0", "method": "sum", "params": [1,2,4], "id": "1"}, {"jsonrpc": "2.0", "method"])"),
              R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}})");
    EXPECT_EQ(handle(m, "[]"), R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}})");
    EXPECT_EQ(handle(m, "[1]"), R"([{"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}}])");
    EXPECT_EQ(handle(m, "[1,2,3]"),
              R"([{"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}},)"
              R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}},)"
              R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}}])");
    std::string mixed = handle(m, R"([
        {"jsonrpc": "2.0", "method": "sum", "params": [1,2,4], "id": "1"},
        {"jsonrpc": "2.0", "method": "notify_hello", "params": [7]},
        {"jsonrpc": "2.0", "method": "subtract", "params": [42,23], "id": "2"},
        {"foo": "boo"},
        {"jsonrpc": "2.0", "method": "foo.get", "params": {"name": "myself"}, "id": "5"},
        {"jsonrpc": "2.0", "method": "get_data", "id": "9"}
    ])");
    EXPECT_EQ(mixed, R"([{"jsonrpc":"2.0","id":"1","result":7},{"jsonrpc":"2.0","id":"2","result":19},)"
                     R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}},)"
                     R"({"jsonrpc":"2.0","id":"5","error":{"code":-32601,"message":"Method not found"}},)"
                     R"({"jsonrpc":"2.0","id":"9","result":["hello",5]}])");
    EXPECT_EQ(handle(m, R"([{"jsonrpc": "2.0", "method": "notify_sum", "params": [1,2,4]}, {"jsonrpc": "2.0", "method": "notify_hello", "params": [7]}])"),
              "(none)");
    // params that are neither an array nor an object; an id that is an object; a handler's error with data
    EXPECT_EQ(handle(m, R"({"jsonrpc": "2.0", "method": "sum", "params": 5, "id": 1})"),
              R"({"jsonrpc":"2.0","id":1,"error":{"code":-32602,"message":"Invalid params"}})");
    EXPECT_EQ(handle(m, R"({"jsonrpc": "2.0", "method": "sum", "id": {}})"),
              R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Invalid Request"}})");
    m.add("fail", [](const json&) -> expected<json, rpc::error> { return unexpected(rpc::error(-32001, "Busy", json::object({{"retry", json(3)}}))); });
    EXPECT_EQ(handle(m, R"({"jsonrpc":"2.0","method":"fail","id":7})"),
              R"({"jsonrpc":"2.0","id":7,"error":{"code":-32001,"message":"Busy","data":{"retry":3}}})");
    m.remove("fail");
    EXPECT_NE(handle(m, R"({"jsonrpc":"2.0","method":"fail","id":7})").find("-32601"), std::string::npos);
    // error_of reads an error object back
    auto e = rpc::detail::rpc_error(rpc::error(-32001, "Busy", json(3)), "x");
    auto back = rpc::error_of(e);
    ASSERT_TRUE(back);
    EXPECT_EQ(back->code, -32001);
    EXPECT_EQ(str(back->message), "Busy");
    EXPECT_EQ(back->data->as_int(0), 3);
    EXPECT_FALSE(rpc::error_of(io::error(io::errc::closed, "x", sgcl::string())));
    error_code c = rpc::errc::method_not_found;
    EXPECT_EQ(std::string(c.category().name()), "jsonrpc");
    EXPECT_EQ(c.message(), "method not found");
}

TEST(JsonRpcPeer, BothWaysOverTcp) {
    for (auto f : {rpc::framing::content_length, rpc::framing::line}) {
        auto [a, b] = socket_pair();
        rpc::methods server_side;
        rpc::peer::options so;
        so.framing = f;
        so.methods = server_side;
        auto server = rpc::peer::connect(b, so);
        std::atomic<int> notes{0};
        server_side.add("add", [](const json& p) -> expected<json, rpc::error> { return json(p[0].as_int(0) + p[1].as_int(0)); });
        server_side.add_notification("bump", [&](const json&) { ++notes; });
        // a method of the server's that calls back the client while it serves
        server_side.add_task("ask", [server](json p, async::stop_token) -> async::task<expected<json, rpc::error>> {
            auto r = co_await server.async_call(sgcl::string("upper"), p);
            if (!r) {
                co_return unexpected(rpc::error(rpc::errc::internal_error, str(r.error().message()).c_str()));
            }
            co_return *r;
        });
        rpc::methods client_side;
        client_side.add("upper", [](const json& p) -> expected<json, rpc::error> {
            std::string s = str(p[0].as_string(sgcl::string()));
            for (char& ch : s) {
                ch = char(std::toupper(uint8_t(ch)));
            }
            return json(sgcl::string(s));
        });
        rpc::peer::options co;
        co.framing = f;
        co.methods = client_side;
        auto client = rpc::peer::connect(a, co);
        EXPECT_EQ(client.call("add", json::array({json(2), json(3)})).value().as_int(0), 5);
        EXPECT_EQ(str(client.call("ask", json::array({json("shout")})).value().as_string(sgcl::string())), "SHOUT");
        ASSERT_TRUE(client.notify("bump"));
        ASSERT_TRUE(client.notify("bump", json::array({})));
        auto missing = client.call("nope");
        ASSERT_FALSE(missing);
        EXPECT_EQ(missing.error().code(), rpc::errc::method_not_found);
        // a batch: calls and a notification, outcomes in the order of the calls
        auto r = client.batch({{"add", json::array({json(1), json(1)}), false}, {"bump", json(), true}, {"nope", json(), false}, {"add", json::array({json(5), json(5)}), false}})
                     .value();
        ASSERT_EQ(r.size(), 3u);
        EXPECT_EQ(r[0]->as_int(0), 2);
        EXPECT_EQ(r[1].error().code(), rpc::errc::method_not_found);
        EXPECT_EQ(r[2]->as_int(0), 10);
        for (int i = 0; i < 100 && notes.load() < 3; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        EXPECT_EQ(notes.load(), 3);
        // many at once from many tasks
        vector<async::task<bool>> all;
        for (int i = 0; i < 64; ++i) {
            all.push_back(async::spawn([](rpc::peer p, int i) -> async::task<bool> {
                auto r = co_await p.async_call(sgcl::string("add"), json::array({json(i), json(i)}));
                co_return r && r->as_int(0) == 2 * i;
            }(client, i)));
        }
        for (auto& t : all) {
            EXPECT_TRUE(t.wait());
        }
        ASSERT_TRUE(client.close());
        EXPECT_TRUE(server.wait());   // the other side's end: a close, no error
        EXPECT_FALSE(client.call("add", json::array({json(1), json(1)})));
    }
}

TEST(JsonRpcPeer, CancellationAndTimeout) {
    auto [a, b] = socket_pair();
    rpc::methods m;
    std::atomic<bool> saw_stop{false};
    m.add_task("slow", [&saw_stop](json, async::stop_token stop) -> async::task<expected<json, rpc::error>> {
        bool stopped = false;
        co_await async::select(stop.on_stop([&] { stopped = true; }), async::timeout(std::chrono::seconds(5), [] {}));
        saw_stop = stopped;
        if (stopped) {
            co_return unexpected(rpc::error(rpc::errc::request_cancelled, "cancelled"));
        }
        co_return json("done");
    });
    m.add_task("late", [](json, async::stop_token) -> async::task<expected<json, rpc::error>> {
        co_await async::sleep_until(sgcl::clock::now() + std::chrono::milliseconds(300));
        co_return json("late");
    });
    rpc::peer::options so;
    so.methods = m;
    auto server = rpc::peer::connect(b, so);
    auto client = rpc::peer::connect(a);
    async::stop_source cancel;
    auto pending = async::spawn(client.async_call(sgcl::string("slow"), json(), {std::chrono::seconds(10), cancel.token()}));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cancel.request_stop();
    auto r = pending.wait();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), std::errc::operation_canceled);
    for (int i = 0; i < 200 && !saw_stop.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_TRUE(saw_stop.load());   // the server's handler was stopped by $/cancelRequest
    auto t = client.call("late", json(), {std::chrono::milliseconds(50), {}});
    ASSERT_FALSE(t);
    EXPECT_TRUE(t.error().is_timeout());
    // the connection goes on after both
    EXPECT_EQ(str(client.call("late").value().as_string(sgcl::string())), "late");
    client.close();
    server.close();
}

TEST(JsonRpcPeer, WebSocketAndHttp) {
    rpc::methods m;
    m.add("hello", [](const json& p) -> expected<json, rpc::error> { return json(sgcl::string("hello " + str(p["name"].as_string(sgcl::string("?"))))); });
    m.add_notification("ping", [](const json&) {});
    http::server srv;
    srv.route("/rpc", [m](http::request r, http::response_writer w) { return m.async_serve(r, w); });
    srv.route("GET /ws", [m](http::request r, http::response_writer w) -> async::task<> {
        auto ws = co_await http::websocket::async_accept(r, w);
        if (!ws) {
            co_return;
        }
        rpc::peer::options o;
        o.methods = m;
        auto p = rpc::peer::connect(*ws, o);
        (void)co_await p.async_wait();
    });
    auto l = net::tcp::listen("127.0.0.1:0").value();
    auto serving = async::spawn(srv.async_serve(l));
    std::string base = "127.0.0.1:" + std::to_string(l.local_endpoint().port());
    // over HTTP
    rpc::client c(sgcl::string("http://" + base + "/rpc"));
    EXPECT_EQ(str(c.call("hello", json::object({{"name", json("http")}})).value().as_string(sgcl::string())), "hello http");
    ASSERT_TRUE(c.notify("ping"));
    auto missing = c.call("nope");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), rpc::errc::method_not_found);
    auto b = c.batch({{"hello", json::object({{"name", json("a")}}), false}, {"ping", json(), true}, {"hello", json::object({{"name", json("b")}}), false}}).value();
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(str(b[1]->as_string(sgcl::string())), "hello b");
    EXPECT_TRUE(c.batch({{"ping", json(), true}}).value().empty());   // notifications only: 204
    auto get = http::client().get(sgcl::string("http://" + base + "/rpc")).value();
    EXPECT_EQ(get.status(), 405);
    rpc::client nowhere(sgcl::string("http://" + base + "/missing"));
    EXPECT_EQ(nowhere.call("hello").error().code(), net::errc::http_status);
    // over a WebSocket
    auto ws = http::websocket::connect(sgcl::string("ws://" + base + "/ws")).value();
    auto p = rpc::peer::connect(ws);
    EXPECT_EQ(str(p.call("hello", json::object({{"name", json("ws")}})).value().as_string(sgcl::string())), "hello ws");
    p.close();
    srv.close();
    serving.wait();
}

TEST(JsonRpcPeer, PythonPeerOverContentLength) {
    if (std::system("command -v python3 > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no python3";
    }
    auto l = net::tcp::listen("127.0.0.1:0").value();
    std::string script = (source_root() / "tests/net/jsonrpc/python/peer.py").string();
    auto py = async::spawn_blocking([script, port = l.local_endpoint().port()] { return run_command("python3 " + script + " " + std::to_string(port) + " 2>&1"); });
    auto c = l.accept().value();
    rpc::methods m;
    std::atomic<int> logs{0};
    m.add("upper", [](const json& p) -> expected<json, rpc::error> {
        std::string s = str(p["text"].as_string(sgcl::string()));
        for (char& ch : s) {
            ch = char(std::toupper(uint8_t(ch)));
        }
        return json(sgcl::string(s));
    });
    m.add_notification("log", [&](const json& p) { logs += p["message"].as_string(sgcl::string()) == "hi" ? 1 : 0; });
    rpc::peer::options o;
    o.methods = m;
    auto p = rpc::peer::connect(c, o);
    EXPECT_EQ(p.call("add", json::array({json(40), json(2)})).value().as_int(0), 42);
    EXPECT_EQ(p.call("echo", json::object({{"k", json("v")}})).value().to_string(), "{\"k\":\"v\"}");
    EXPECT_EQ(str(p.call("ask_upper", json::object({{"text", json("lsp")}})).value().as_string(sgcl::string())), "LSP");
    EXPECT_TRUE(p.call("say", json::object({{"text", json("hi")}})).value().as_bool(false));
    for (int i = 0; i < 100 && logs.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(logs.load(), 1);
    auto missing = p.call("nope");
    ASSERT_FALSE(missing);
    EXPECT_EQ(rpc::error_of(missing.error())->data->as_string(sgcl::string()), "nope");
    auto b = p.batch({{"x", json::array({json(1)}), false}, {"y", json::array({json(2)}), false}}).value();
    EXPECT_EQ(b[1]->to_string(), "[2]");
    EXPECT_TRUE(p.call("exit").has_value());
    EXPECT_NE(py.wait().find("ok"), std::string::npos);
    p.close();
}
