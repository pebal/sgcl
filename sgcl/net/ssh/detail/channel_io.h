//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "conn.h"
#include "../../connection.h"
#include "../../ip.h"
#include "../../../core/function.h"
#include "../../../io/stream.h"

#include <string>
#include <string_view>

// A channel as the rest of the library reads and writes it: one of a
// session's streams as an io stream (its standard input to write, its
// output and error to read), and a forwarded channel (direct-tcpip,
// forwarded-tcpip, an agent's) as a net::connection, with deadlines,
// close_write as the channel's EOF and close as its CLOSE.
namespace sgcl::net::ssh::detail {
    // One stream of a channel: 0 the data, 1 stderr's extended data; read,
    // or written (the writer's close sends EOF)
    class ChannelStream {
    public:
        SGCL_INLINE_HOT ChannelStream(const tracked_ptr<ChannelImpl>& c, int stream) noexcept
        : _c(c)
        , _stream(stream) {
        }

        SGCL_INLINE_HOT expected<size_t, io::error> read(const slice<byte>& b) {
            return SshConn::co_channel_read(_c->conn, _c, _stream, b).wait();
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_read(const slice<byte>& b) noexcept {
            return SshConn::co_channel_read(_c->conn, _c, _stream, b);
        }

        SGCL_INLINE_HOT expected<size_t, io::error> write(const slice<const byte>& d) {
            return SshConn::co_channel_write(_c->conn, _c, _stream, d).wait();
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_write(const slice<const byte>& d) noexcept {
            return SshConn::co_channel_write(_c->conn, _c, _stream, d);
        }

        // A writer's close: the channel's EOF (a reader's: nothing)
        SGCL_INLINE_HOT expected<void, io::error> close() {
            if (_stream != 0) {
                return {};
            }
            return SshConn::co_channel_eof(_c->conn, _c).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_close() noexcept {
            return _co_close(_c, _stream);
        }

    private:
        static async::task<expected<void, io::error>> _co_close(tracked_ptr<ChannelImpl> c, int stream) noexcept {
            if (stream != 0) {
                co_return expected<void, io::error>();
            }
            co_return co_await SshConn::co_channel_eof(c->conn, c);
        }

        tracked_ptr<ChannelImpl> _c;
        int _stream;
    };

    SGCL_INLINE_HOT io::reader reader_of(const tracked_ptr<ChannelImpl>& c, int stream) {
        return io::reader(tracked_ptr<ChannelStream>(make_tracked<ChannelStream>(c, stream)));
    }

    SGCL_INLINE_HOT io::writer writer_of(const tracked_ptr<ChannelImpl>& c, int stream) {
        return io::writer(tracked_ptr<ChannelStream>(make_tracked<ChannelStream>(c, stream)));
    }

    // A forwarded channel as a connection: the data stream both ways
    class ChannelConn final : public net::detail::ConnImpl {
    public:
        SGCL_INLINE_HOT ChannelConn(const tracked_ptr<ChannelImpl>& c, endpoint local, endpoint remote, const string& what) noexcept
        : _c(c)
        , _local(local)
        , _remote(remote)
        , _what(what) {
        }

        expected<size_t, io::error> raw_read(const slice<byte>& b) override {
            return SshConn::co_channel_read(_c->conn, _c, 0, b).wait();
        }

        async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte> b) noexcept override {
            return SshConn::co_channel_read(_c->conn, _c, 0, b);
        }

        expected<size_t, io::error> raw_write(const slice<const byte>& d) override {
            return SshConn::co_channel_write(_c->conn, _c, 0, d).wait();
        }

        async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> d) noexcept override {
            return SshConn::co_channel_write(_c->conn, _c, 0, d);
        }

        expected<void, io::error> close() noexcept override {
            _c->conn->close_channel(_c);
            return {};
        }

        bool is_closed() const noexcept override {
            std::lock_guard<std::mutex> g(_c->m);
            return _c->close_out || _c->conn_gone;
        }

        // Our EOF: the peer reads the end, this side still reads
        expected<void, io::error> close_write() override {
            if (is_closed()) {
                return fail(io::error(io::errc::closed, "close_write", describe()));
            }
            _c->conn->eof_soon(_c);   // never waits: a task's close_write too
            return {};
        }

        void set_deadline(int dir, time_point t) noexcept override {
            {
                std::lock_guard<std::mutex> g(_c->m);
                _c->deadlines[dir == net::detail::Descriptor::Read ? 0 : 1] = t;
            }
            _c->wake();
        }

        time_point deadline(int dir) const noexcept override {
            std::lock_guard<std::mutex> g(_c->m);
            return _c->deadlines[dir == net::detail::Descriptor::Read ? 0 : 1];
        }

        endpoint local_endpoint() const noexcept override {
            return _local;
        }

        endpoint remote_endpoint() const noexcept override {
            return _remote;
        }

        string describe() const noexcept override {
            return _what;
        }

        SGCL_INLINE_HOT const tracked_ptr<ChannelImpl>& channel() const noexcept {
            return _c;
        }

    private:
        tracked_ptr<ChannelImpl> _c;
        endpoint _local;
        endpoint _remote;
        string _what;
    };

    // An endpoint of "host" and a port when the host is an address; the
    // empty one otherwise
    inline endpoint endpoint_of(std::string_view host, uint32_t port) noexcept {
        auto a = ip_address::parse(string(host));
        if (!a || port > 65535) {
            return endpoint();
        }
        return endpoint(*a, uint16_t(port));
    }

    inline net::connection connection_of(const tracked_ptr<ChannelImpl>& c, endpoint local, endpoint remote, const string& what) {
        return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(make_tracked<ChannelConn>(c, local, remote, what)));
    }

    // Two connections copied into each other until both ends are done:
    // what a forwarded channel and the socket it stands for do (each
    // direction's end the other's close_write, both closed at the last)
    inline async::task<void> co_pipe_one(net::connection from, net::connection to) noexcept {
        (void)co_await from.async_copy_to(to);
        (void)to.close_write();
    }

    inline async::task<void> co_pipe(net::connection a, net::connection b) noexcept {
        auto t = async::spawn(co_pipe_one(b, a));
        co_await co_pipe_one(a, b);
        co_await t;
        (void)a.close();
        (void)b.close();
    }

    // A listener whose connections are channels the peer opens to an
    // address of ours (tcpip-forward's forwarded-tcpip, RFC 4254 §7.1); its
    // close cancels the forwarding
    class ForwardListener final : public net::detail::ListenerImpl {
    public:
        ForwardListener(const tracked_ptr<SshConn>& conn, std::string_view address, uint32_t port, endpoint local) noexcept
        : conn(conn)
        , address(address)
        , port(port)
        , _local(local)
        , _ready(16) {
        }

        tracked_ptr<SshConn> conn;
        const std::string address;   // as asked, the server's name for it
        const uint32_t port;         // as the server bound it

        expected<net::connection, io::error> _block_accept() override {
            auto c = _ready.receive().wait();
            if (!c) {
                return fail(net::detail::closed_error("accept", describe()));
            }
            return *c;
        }

        async::task<expected<net::connection, io::error>> _co_accept() noexcept override {
            auto c = co_await _ready.receive();
            if (!c) {
                co_return fail(net::detail::closed_error("accept", describe()));
            }
            co_return *c;
        }

        // A connection the server forwarded; false when the listener is
        // closed or its queue full (the channel is then closed)
        bool offer(const net::connection& c) {
            return _ready.try_send(c);
        }

        expected<void, io::error> close() noexcept override {
            if (_closed.exchange(true)) {
                return {};
            }
            _ready.close();
            while (auto c = _ready.try_receive()) {
                (void)c->close();
            }
            if (on_close) {
                on_close();
            }
            return {};
        }

        bool is_closed() const noexcept override {
            return _closed.load();
        }

        endpoint local_endpoint() const noexcept override {
            return _local;
        }

        string describe() const noexcept override {
            return string("ssh " + conn->describe() + " forward " + address + ":" + std::to_string(port));
        }

        // The forwarding cancelled at the server (set by the client)
        function<void()> on_close;

    private:
        endpoint _local;
        async::channel<net::connection> _ready;
        std::atomic<bool> _closed{false};
    };
}
