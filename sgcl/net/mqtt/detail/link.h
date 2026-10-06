//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "codec.h"
#include "../../connection.h"
#include "../../error.h"
#include "../../http/websocket.h"
#include "../../../async/coroutine.h"
#include "../../../core/aliases.h"
#include "../../../core/array.h"
#include "../../../core/make_tracked.h"
#include "../../../core/tracked_ptr.h"

#include <string>

// What MQTT is carried over: a stream connection (TCP, TLS) or a
// WebSocket's binary messages (MQTT 5 §6, subprotocol "mqtt"); bytes in
// to a buffer, whole packets out
namespace sgcl::net::mqtt::detail {
    using MqttBlock = array<std::byte, 16384>;

    class MqttLink {
    public:
        virtual ~MqttLink() = default;

        // More bytes appended to buf; 0 at the end
        virtual async::task<expected<size_t, io::error>> fill(std::string& buf) noexcept = 0;

        // Packets written whole, in order (the caller holds the write lock)
        virtual async::task<expected<void, io::error>> send(const std::string& data) noexcept = 0;

        virtual expected<void, io::error> close() noexcept = 0;

        virtual void read_deadline(time_point t) noexcept = 0;

        virtual endpoint remote() const noexcept = 0;
    };

    class MqttStreamLink final : public MqttLink {
    public:
        explicit MqttStreamLink(net::connection c) noexcept
        : _c(std::move(c)) {
        }

        async::task<expected<size_t, io::error>> fill(std::string& buf) noexcept override {
            auto& impl = net::detail::ConnectionAccess::impl(_c);
            for (;;) {   // the bytes there now without a wait (net's try_read), the readiness waited for
                bool slow = false;
                auto t = impl.try_read(slice<byte>(_block->data(), _block->size()), slow);
                if (slow) {
                    auto r = co_await _c.async_read(slice<byte>(_block->data(), _block->size()));
                    if (r && *r) {
                        buf.append(reinterpret_cast<const char*>(_block->data()), *r);
                    }
                    co_return r;
                }
                if (!t) {
                    co_return net::detail::fail(t);
                }
                if (*t) {
                    buf.append(reinterpret_cast<const char*>(_block->data()), **t);
                    co_return **t;
                }
                if (auto ready = co_await impl.raw_readable(); !ready) {
                    co_return net::detail::fail(ready);
                }
            }
        }

        async::task<expected<void, io::error>> send(const std::string& data) noexcept override {
            auto st = net::detail::ConnectionAccess::impl(_c).start_write(slice<const byte>(reinterpret_cast<const byte*>(data.data()), data.size()));
            expected<size_t, io::error> w = std::move(st.done);
            if (st.rest) {
                w = co_await std::move(*st.rest);
            }
            if (!w) {
                co_return net::detail::fail(w);
            }
            co_return expected<void, io::error>();
        }

        expected<void, io::error> close() noexcept override {
            return _c.close();
        }

        void read_deadline(time_point t) noexcept override {
            _c.set_read_deadline(t);
        }

        endpoint remote() const noexcept override {
            return _c.remote_endpoint();
        }

    private:
        net::connection _c;
        tracked_ptr<MqttBlock> _block = make_tracked<MqttBlock>();
    };

    class MqttWsLink final : public MqttLink {
    public:
        explicit MqttWsLink(http::websocket ws) noexcept
        : _ws(std::move(ws)) {
        }

        async::task<expected<size_t, io::error>> fill(std::string& buf) noexcept override {
            for (;;) {   // an empty message carries nothing: the next one
                auto m = co_await _ws.async_receive();
                if (!m) {
                    if (m.error().code() == net::errc::websocket_closed) {
                        co_return size_t(0);
                    }
                    co_return net::detail::fail(m);
                }
                if (!m->binary) {
                    co_return unexpected(mqtt_error(errc::malformed_packet, "mqtt", string("a text frame over WebSocket")));
                }
                if (!m->data.empty()) {
                    buf.append(reinterpret_cast<const char*>(m->data.data()), m->data.size());
                    co_return m->data.size();
                }
            }
        }

        async::task<expected<void, io::error>> send(const std::string& data) noexcept override {
            co_return co_await _ws.async_send(slice<const byte>(reinterpret_cast<const byte*>(data.data()), data.size()));
        }

        expected<void, io::error> close() noexcept override {
            return _ws.connection().close();
        }

        void read_deadline(time_point t) noexcept override {
            _ws.connection().set_read_deadline(t);
        }

        endpoint remote() const noexcept override {
            return _ws.connection().remote_endpoint();
        }

    private:
        http::websocket _ws;
    };
}
