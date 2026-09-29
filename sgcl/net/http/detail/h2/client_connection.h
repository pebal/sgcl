//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "connection.h"

#include <algorithm>
#include <cstring>
#include <vector>

// The client's side of one HTTP/2 connection (RFC 9113), the other role of
// Endpoint (connection.h): what both roles share is there, flow control,
// SETTINGS, PING, the field blocks and the limits against floods; here is
// what only a client does. It opens the streams (odd identifiers, rising in
// the order their HEADERS go out, §5.1.1), announces ENABLE_PUSH = 0 (so
// the server opens none: PUSH_PROMISE and HEADERS on an even stream are the
// connection's PROTOCOL_ERROR), keeps to the server's
// SETTINGS_MAX_CONCURRENT_STREAMS, reads a response as interim 1xx blocks,
// one final block and trailers (§8.1), and on the server's GOAWAY tells
// which of its streams the server never processed (§6.8).
//
// Its Events add to the common ones (connection.h):
//
//   ErrorCode on_response(uint32_t id, Block&& fields, bool end_stream, bool informational)
//       a response's field block: `informational` for a 1xx (never 101;
//       there may be several, none with END_STREAM), then once the final
//       one. Anything but no_error resets the stream with that code (a
//       response without :status: PROTOCOL_ERROR; fields.truncated, a list
//       past max_header_list_size: the transport's call)
//   void on_trailers(uint32_t id, Block&& fields)
//       the trailers (with END_STREAM, no pseudo-field); fields.truncated
//       when past max_header_list_size
//   void on_unprocessed(uint32_t id)
//       a stream of ours above the last one of the server's GOAWAY: the
//       server never processed it and it is gone from the machine (§6.8;
//       the transport retries only what may be retried after GOAWAY). A
//       stream the server refused by RST_STREAM comes to on_reset with
//       REFUSED_STREAM (§8.7)
//
// Each request's field block is encoded with encoder() under the lock the
// transport holds for open_stream(), in the same section (the blocks go in
// the order they were encoded, and the identifiers in the order their
// HEADERS are written).
namespace sgcl::net::http::detail::h2 {
    // The client's settings. Windows smaller than Go's transport (4 MB a
    // stream, 1 GB the connection): a stream's buffer grows to its window
    // while nobody reads its body, and memory comes first; 1 MB at 100 ms of
    // RTT is some 80 Mb/s a stream, a faster link raises it
    struct ClientSettings : Limits {
        // the most streams of ours open at once, whatever the server
        // announces (Go: defaultMaxConcurrentStreams, 1000); the size of
        // the stream table, made in the constructor
        uint32_t max_concurrent_streams = 1000;
        // before the server's SETTINGS come (Go: initialMaxConcurrentStreams)
        uint32_t initial_concurrent_streams = 100;

        ClientSettings() {
            initial_window = 1u << 20;         // 1 MB
            connection_window = 16u << 20;     // 16 MB
            max_header_list_size = 1u << 20;   // the client's max_response_header_bytes
        }
    };

    template<class Events>
    class ClientConnection : public Endpoint<ClientConnection<Events>, Events> {
        using Base = Endpoint<ClientConnection<Events>, Events>;
        friend Base;

    public:
        using Stream = connection_detail::Stream;

        static constexpr bool reads_preface = false;

        ClientConnection(Events& events, const ClientSettings& settings = ClientSettings())
        : Base(events, settings, std::max(1u, settings.max_concurrent_streams))
        , _max_streams(std::max(1u, settings.max_concurrent_streams))
        , _initial_streams(std::max(1u, std::min(settings.initial_concurrent_streams, _max_streams))) {
            _unprocessed.reserve(_max_streams);
        }

        // The client's preface (§3.4): the 24 bytes, our SETTINGS
        // (ENABLE_PUSH = 0 first), the connection's window raised to ours
        void start(int64_t now) {
            this->_out.append(Preface, PrefaceSize);
            Setting s[6];
            size_t n = 0;
            s[n++] = {uint16_t(SettingId::enable_push), 0};
            n = this->_common_settings(s, n);
            this->_start(now, s, n);
        }

        // Whether open_stream() would take a stream now
        bool can_open() const noexcept {
            return !this->_failed && !_goaway_received && !this->_goaway_sent && _next_id <= LargestStreamId
                && this->_streams.size() < stream_limit();
        }

        // A new stream with its request's field block (encoded just before,
        // in the same section under the transport's lock), as HEADERS and
        // CONTINUATIONs; END_STREAM when the request has no body. The result
        // is its identifier, or 0: refused (the server's GOAWAY came, as
        // many streams open as the server allows, the identifiers used up,
        // the connection failed). A refused block was not written: the
        // encoder's table has gone on without the peer's, so the transport
        // encodes only once it knows can_open()
        uint32_t open_stream(const uint8_t* block, size_t n, bool end_stream) {
            if (!can_open()) {
                return 0;
            }
            const uint32_t id = _next_id;
            _next_id += 2;
            Stream* s = this->_streams.add(id);
            s->send = this->_peer.initial_window;
            s->recv = this->_ours.initial_window;
            s->known = true;
            this->_write_block(id, block, n, end_stream);
            if (end_stream) {
                s->local_closed = true;
            }
            return id;
        }

        // The most streams open at once now: the server's
        // SETTINGS_MAX_CONCURRENT_STREAMS (initial_concurrent_streams until
        // its SETTINGS come), never more than ours
        uint32_t stream_limit() const noexcept {
            const uint32_t peer = this->peer_settings_received() ? this->_peer.max_concurrent_streams : _initial_streams;
            return std::min(peer, _max_streams);
        }

        bool goaway_received() const noexcept {
            return _goaway_received;
        }

        // The last stream the server's GOAWAY said it may process
        uint32_t goaway_last() const noexcept {
            return _goaway_last_peer;
        }

    private:
        static constexpr uint32_t LargestStreamId = 0x7FFFFFFFu;

        uint32_t _max_streams;
        uint32_t _initial_streams;
        uint32_t _next_id = 1;
        bool _goaway_received = false;
        uint32_t _goaway_last_peer = LargestStreamId;
        std::vector<uint32_t> _unprocessed;   // scratch for GOAWAY, capacity made in the constructor

        // --- the hooks of Endpoint ---

        // The server opens nothing (ENABLE_PUSH = 0): every even stream is
        // idle; an odd one until we open it
        bool _idle(uint32_t id) const noexcept {
            return (id & 1) == 0 || id >= _next_id;
        }

        bool _peer_opens(uint32_t) const noexcept {
            return false;
        }

        // GOAWAY from us names the last stream the server opened: none
        uint32_t _last_peer_stream() const noexcept {
            return 0;
        }

        // DATA only after the final response's fields (§8.1)
        bool _accepts_data(const Stream& s) const noexcept {
            return s.final_headers;
        }

        // A frame on a stream of ours already closed (called only for one
        // not idle: odd, below the next): a request given up while its
        // answer was on the way, however many were given up; dropped (Go
        // too). Its DATA's window goes back to the connection
        bool _ignore_closed(uint32_t id) const noexcept {
            return (id & 1) && id < _next_id;
        }

        bool _own_ping(const uint8_t*) const noexcept {
            return false;
        }

        expected<void, Error> _peer_reset() const noexcept {
            return {};
        }

        // §6.8: our streams above `last` were never processed; they leave
        // the machine (what they held goes back to the connection's window)
        // and the transport hears of each. A later GOAWAY may lower `last`,
        // never raise it
        void _peer_goaway(uint32_t last, ErrorCode code) {
            _goaway_received = true;
            if (last < _goaway_last_peer) {
                _goaway_last_peer = last;
            }
            _unprocessed.clear();
            this->_streams.each([&](Stream& s) {
                if (s.id > _goaway_last_peer) {
                    _unprocessed.push_back(s.id);
                }
            });
            std::sort(_unprocessed.begin(), _unprocessed.end());   // told in the order they were opened
            for (uint32_t id : _unprocessed) {
                if (Stream* s = this->_streams.find(id)) {
                    // not remembered as reset: the server sends nothing on
                    // a stream it never processed, and the ring of recent
                    // resets is kept for ours
                    this->_drop(s);
                    this->_events->on_unprocessed(id);
                }
            }
            this->_events->on_goaway(last, code);
        }

        // The server opens no stream: never called (_peer_opens refuses first)
        expected<void, Error> _block_new(uint32_t, Block&&, bool, bool) {
            return unexpected(connection_error(ErrorCode::protocol_error, "HEADERS on a stream the server may not open"));
        }

        // The response's field blocks on a stream of ours (§8.1): 1xx, the
        // final, the trailers
        expected<void, Error> _block_on(Stream* s, Block&& block, bool end_stream, bool self_dependent) {
            const uint32_t id = s->id;
            if (s->remote_closed) {
                this->_stream_error(id, ErrorCode::stream_closed);
                return {};
            }
            if (self_dependent) {
                this->_stream_error(id, ErrorCode::protocol_error);
                return {};
            }
            if (s->final_headers) {
                // trailers: END_STREAM, and no pseudo-field
                if (!end_stream || _pseudo(block)) {
                    this->_stream_error(id, ErrorCode::protocol_error);
                    return {};
                }
                this->_end_remote(s);
                this->_events->on_trailers(id, std::move(block));
                return {};
            }
            const int status = _status(block);
            if (status == 101) {
                this->_stream_error(id, ErrorCode::protocol_error);   // no Upgrade in HTTP/2 (§8.6)
                return {};
            }
            const bool informational = status >= 100 && status < 200;
            if (informational && end_stream) {
                this->_stream_error(id, ErrorCode::protocol_error);   // a 1xx ends nothing (§8.1)
                return {};
            }
            if (!informational) {
                s->final_headers = true;
                if (end_stream) {
                    this->_end_remote(s);   // s may be gone from here on
                }
            }
            const ErrorCode verdict = this->_events->on_response(id, std::move(block), end_stream, informational);
            if (verdict != ErrorCode::no_error && this->_streams.find(id)) {
                this->_stream_error(id, verdict);
            }
            return {};
        }

        // :status when it leads the block and is three digits, else 0 (the
        // transport's verdict then)
        static int _status(const Block& block) {
            auto& fields = HeadersAccess::fields(block.fields);
            if (fields.empty() || fields[0].first.view() != ":status") {
                return 0;
            }
            const auto v = fields[0].second.view();
            if (v.size() != 3) {
                return 0;
            }
            int n = 0;
            for (char c : v) {
                if (c < '0' || c > '9') {
                    return 0;
                }
                n = n * 10 + (c - '0');
            }
            return n;
        }

        static bool _pseudo(const Block& block) {
            for (auto& f : HeadersAccess::fields(block.fields)) {
                if (!f.first.view().empty() && f.first.view()[0] == ':') {
                    return true;
                }
            }
            return false;
        }
    };
}
