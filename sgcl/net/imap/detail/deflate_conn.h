//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../connection.h"
#include "../../../compress/detail/deflate.h"
#include "../../../compress/detail/inflate.h"
#include "../../../core/detail/bytes.h"
#include "../../../core/make_tracked.h"

#include <memory>
#include <string>
#include <vector>

// COMPRESS=DEFLATE (RFC 4978): a connection over another whose bytes go
// as one raw DEFLATE stream each way (RFC 1951, no zlib header), every
// write ended by a sync flush so that the peer can read all of it at
// once. Over the compress module's deflater and inflater.
namespace sgcl::net::imap::detail {
    class DeflateConn final : public net::detail::ConnImpl {
    public:
        static constexpr size_t History = compress::detail::WindowSize;
        static constexpr size_t Room = size_t(64) << 10;
        static constexpr size_t InputSize = size_t(16) << 10;

        // over the connection given; `pending` the bytes already read
        // from it after the command (compressed already)
        DeflateConn(const net::connection& inner, std::string_view pending)
        : _inner(inner)
        , _deflater(6)
        , _state(std::make_unique<compress::detail::InflateState>())
        , _window(History + Room)
        , _in(pending.begin(), pending.end()) {
            _state->reset();
        }

        expected<size_t, io::error> raw_read(const slice<byte>& buffer) override {
            for (;;) {
                if (auto n = _take(buffer)) {
                    return n;
                }
                auto step = _inflate();
                if (!step) {
                    return unexpected(step.error());
                }
                if (*step) {
                    continue;
                }
                if (_ended) {
                    return size_t(0);
                }
                _in.resize(InputSize);
                _in_pos = 0;
                auto r = _inner.read(slice<byte>(reinterpret_cast<byte*>(_in.data()), _in.size()));
                if (!r) {
                    _in.clear();
                    return unexpected(r.error());
                }
                _in.resize(*r);
                if (*r == 0) {
                    return size_t(0);
                }
            }
        }

        async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte> buffer) noexcept override {
            for (;;) {
                if (auto n = _take(buffer)) {
                    co_return n;
                }
                auto step = _inflate();
                if (!step) {
                    co_return unexpected(step.error());
                }
                if (*step) {
                    continue;
                }
                if (_ended) {
                    co_return size_t(0);
                }
                _in.resize(InputSize);
                _in_pos = 0;
                auto r = co_await _inner.async_read(slice<byte>(reinterpret_cast<byte*>(_in.data()), _in.size()));
                if (!r) {
                    _in.clear();
                    co_return unexpected(r.error());
                }
                _in.resize(*r);
                if (*r == 0) {
                    co_return size_t(0);
                }
            }
        }

        expected<size_t, io::error> raw_write(const slice<const byte>& data) override {
            _compress(data);
            auto r = _inner.write(slice<const byte>(reinterpret_cast<const byte*>(_out.data()), _out.size()));
            if (!r) {
                return unexpected(r.error());
            }
            return data.size();
        }

        async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> data) noexcept override {
            _compress(data);
            auto r = co_await _inner.async_write(slice<const byte>(reinterpret_cast<const byte*>(_out.data()), _out.size()));
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return data.size();
        }

        expected<void, io::error> close() noexcept override {
            return _inner.close();
        }

        async::task<expected<void, io::error>> async_close() noexcept override {
            co_return co_await _inner.async_close();
        }

        bool is_closed() const noexcept override {
            return _inner.is_closed();
        }

        expected<void, io::error> close_write() override {
            return _inner.close_write();
        }

        void set_deadline(int dir, time_point t) noexcept override {
            net::detail::ConnectionAccess::impl(_inner).set_deadline(dir, t);
        }

        time_point deadline(int dir) const noexcept override {
            return net::detail::ConnectionAccess::impl(_inner).deadline(dir);
        }

        endpoint local_endpoint() const noexcept override {
            return _inner.local_endpoint();
        }

        endpoint remote_endpoint() const noexcept override {
            return _inner.remote_endpoint();
        }

        string path() const noexcept override {
            return _inner.path();
        }

        string describe() const noexcept override {
            return string("deflate ") + net::detail::ConnectionAccess::impl(_inner).describe();
        }

    private:
        // What the window holds and the caller has not taken
        size_t _take(const slice<byte>& buffer) noexcept {
            const size_t n = std::min(buffer.size(), _produced - _given);
            if (n) {
                sgcl::detail::copy_bytes(buffer.data(), _window.data() + _given, n);
                _given += n;
            }
            return n;
        }

        // The input inflated into the window: whether bytes came
        expected<bool, io::error> _inflate() noexcept {
            if (_in_pos >= _in.size() || _ended) {
                return false;
            }
            if (_produced > History + Room - (compress::detail::MaxMatch + 16)) {
                // keep the history a back reference may reach
                std::memmove(_window.data(), _window.data() + _produced - History, History);
                _produced = History;
                _given = History;
            }
            const uint8_t* in = _in.data() + _in_pos;
            const uint8_t* end = _in.data() + _in.size();
            size_t pos = _produced;
            auto st = compress::detail::inflate_blocks<false>(*_state, in, end, _window.data(), pos, _window.size());
            _in_pos = size_t(in - _in.data());
            const bool came = pos > _produced;
            _produced = pos;
            if (st == compress::detail::InflateStatus::failed) {
                return unexpected(io::error(std::make_error_code(std::errc::illegal_byte_sequence), "read", describe()));
            }
            if (st == compress::detail::InflateStatus::done) {
                _ended = true;
            }
            return came || st == compress::detail::InflateStatus::need_room;
        }

        void _compress(const slice<const byte>& data) noexcept {
            _out.clear();
            _deflater.write(reinterpret_cast<const uint8_t*>(data.data()), data.size(), _out);
            _deflater.flush(_out);
        }

        net::connection _inner;
        compress::detail::Deflater _deflater;
        std::vector<uint8_t> _out;
        std::unique_ptr<compress::detail::InflateState> _state;
        std::vector<uint8_t> _window;
        size_t _produced = 0;
        size_t _given = 0;
        std::vector<uint8_t> _in;
        size_t _in_pos = 0;
        bool _ended = false;
    };

    inline net::connection deflate_connection(const net::connection& inner, std::string_view pending) {
        return net::detail::ConnectionAccess::make(tracked_ptr<net::detail::ConnImpl>(make_tracked<DeflateConn>(inner, pending)));
    }
}
