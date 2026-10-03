//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "frame.h"
#include "hpack.h"
#include "../../../../core/aliases.h"
#include "../../../../core/expected.h"
#include "../../../../core/slice.h"
#include "../../../../core/vector.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// One HTTP/2 connection (RFC 9113) as a machine without I/O, as TLS's are:
// fed the bytes the transport read and the time, it reads the frames, keeps
// the streams (§5.1), the windows of flow control (§5.2, §6.9), SETTINGS
// (§6.5) and its acknowledgement, PING, GOAWAY and RST_STREAM, decodes each
// field block with HPACK, and tells its owner what happened through its
// Events; what it has to say to the peer it writes into output(), for the
// transport to send. The owner answers through it: send_headers() and
// send_data() frame what it writes within the windows and the peer's
// SETTINGS_MAX_FRAME_SIZE.
//
// Endpoint is what both roles share (a base by CRTP, nothing virtual); a
// role gives it these (members of Derived):
//
//   static constexpr bool reads_preface          the server reads the
//       client's 24 bytes before the first frame
//   bool _idle(uint32_t id) const                a stream not yet opened
//   bool _peer_opens(uint32_t id) const          HEADERS on an unknown,
//       idle id opens a stream of the peer's (the server: odd ids; the
//       client: never, it sends ENABLE_PUSH = 0)
//   uint32_t _last_peer_stream() const           for GOAWAY (§6.8)
//   expected<void, Error> _block_new(uint32_t id, Block&&, bool end_stream, bool self_dependent)
//   expected<void, Error> _block_on(Stream*, Block&&, bool end_stream, bool self_dependent)
//       a whole field block, decoded (always, whatever becomes of its
//       stream: the table is shared, §4.3), for a new stream or an open one
//   bool _accepts_data(const Stream&) const       DATA allowed now
//   bool _ignore_closed(uint32_t id) const       HEADERS or DATA on a closed
//       stream dropped quietly (else HEADERS is the connection's
//       STREAM_CLOSED, DATA the stream's): the server, a stream it reset
//       lately; the client, any stream of its own (a request given up
//       while its answer was on the way, as Go)
//   expected<void, Error> _peer_reset()          a RST_STREAM received
//   void _peer_goaway(uint32_t last, ErrorCode)  the peer's GOAWAY
//
// The Events every role's owner gives (called on the thread that feeds):
//
//   void on_data(uint32_t id, const uint8_t* p, size_t n, bool end_stream)
//       a body's bytes, a view of the bytes fed (valid during the call);
//       given back through consumed() once taken
//   void on_reset(uint32_t id, ErrorCode code)      a stream its owner knew
//       reset, by the peer or by the machine; after it (and after reset())
//       consumed() is not called for the stream: the bytes it had not
//       taken are given back to the connection's window by the machine
//   void on_window(uint32_t id)                     room to send opened on
//       the stream (0: on the connection, for every stream waiting)
//   void on_goaway(uint32_t last, ErrorCode code)   the peer is leaving
//   void on_ping_ack(const uint8_t* opaque)         our ping() answered
//
// The limits both roles keep against the known attacks:
//
// - CONTINUATION flood (CVE-2023-45288): the bytes of one field block are
//   limited before it is decoded, from its first fragment on,
//   max(2 × max_header_list_size, 64 KB) (as Go), past them GOAWAY
//   ENHANCE_YOUR_CALM;
// - control-frame flood (PING, SETTINGS, frames making us reset): more
//   than 10000 control frames waiting in output() unsent is GOAWAY
//   ENHANCE_YOUR_CALM (Go's maxQueuedControlFrames);
// - empty-frame flood (CVE-2019-9518: empty DATA, empty fragments,
//   PRIORITY, unknown types): more than max_empty_frames of them in a row,
//   with nothing of substance between, is GOAWAY ENHANCE_YOUR_CALM. The
//   1000 is ours: Go keeps no such count;
// - window zero: a stream that has had data to send and no window for
//   window_timeout is reset (CANCEL). The 30 s is ours: Go's nearest,
//   WriteByteTimeout, is off by default. tick() resets at most 16 such
//   streams a call, on purpose: the rest go on the next tick, a tick's
//   work bounded. SETTINGS unacknowledged for settings_timeout is
//   SETTINGS_TIMEOUT (§6.5.3).
//
// One thread at a time: the machine has no lock of its own. The transport
// serializes feed, tick, open_stream, send_headers, send_data, consumed,
// reset and the rest with one lock, and encodes a block with encoder()
// under the same lock as it hands the block to send_headers (the blocks go
// in the order they were encoded). An event may not call the machine: a
// verdict on a stream goes back as the result of on_request (on_response),
// anything else (a reset) after feed has returned.
//
// Where RFC 9113 and Go differ, the RFC is kept (h2spec checks the RFC):
// DATA past the connection's window is the connection's FLOW_CONTROL_ERROR,
// HEADERS on a closed stream the connection's STREAM_CLOSED.
namespace sgcl::net::http::detail::h2 {
    // What both roles set
    struct Limits {
        uint32_t initial_window = DefaultWindow;       // a stream's receive window we announce
        uint32_t connection_window = DefaultWindow;    // the connection's
        uint32_t max_frame_size = DefaultMaxFrameSize; // the largest frame we read
        uint32_t max_header_list_size = 32 * 1024;
        uint32_t header_table_size = 4096;
        uint32_t max_empty_frames = 1000;              // ours (see above)
        size_t max_control_frames = 10000;             // Go: maxQueuedControlFrames
        int64_t settings_timeout = 10'000'000'000;     // ns
        int64_t window_timeout = 30'000'000'000;       // ns, ours (see above)
    };

    // What the peer announced in its SETTINGS (§6.5.2 defaults)
    struct PeerSettings {
        uint32_t header_table_size = 4096;
        uint32_t max_concurrent_streams = 0xFFFFFFFFu;
        uint32_t initial_window = DefaultWindow;
        uint32_t max_frame_size = DefaultMaxFrameSize;
        uint32_t max_header_list_size = 0xFFFFFFFFu;
    };

    namespace connection_detail {
        // The open streams, a table of fixed size made in the constructor
        // (open addressing, deletion by backward shift): nothing allocated
        // for a stream
        struct Stream {
            uint32_t id = 0;              // 0: an empty slot
            bool remote_closed = false;   // END_STREAM received
            bool local_closed = false;    // END_STREAM sent
            bool known = false;           // given to the owner (on_request, open_stream)
            bool final_headers = false;   // the client: the final response's fields came
            int64_t send = 0;             // the window to send in (may go below 0, §6.9.2)
            int64_t recv = 0;             // what the peer may still send
            uint64_t unacked = 0;         // consumed, not yet given back by WINDOW_UPDATE
            uint64_t buffered = 0;        // given by on_data, not yet consumed
            int64_t blocked_since = 0;    // 0: not waiting for window
        };

        class StreamTable {
        public:
            explicit StreamTable(uint32_t most) noexcept {
                size_t n = 16;
                while (n < 2 * size_t(most) + 2) {
                    n *= 2;
                }
                _slots.resize(n);
            }

            Stream* find(uint32_t id) noexcept {
                for (size_t i = _home(id);; i = (i + 1) & _mask()) {
                    if (_slots[i].id == id) {
                        return &_slots[i];
                    }
                    if (_slots[i].id == 0) {
                        return nullptr;
                    }
                }
            }

            Stream* add(uint32_t id) noexcept {
                size_t i = _home(id);
                while (_slots[i].id != 0) {
                    i = (i + 1) & _mask();
                }
                _slots[i] = Stream();
                _slots[i].id = id;
                ++_size;
                return &_slots[i];
            }

            void remove(Stream* s) noexcept {
                size_t i = size_t(s - _slots.data());
                _slots[i] = Stream();
                --_size;
                for (size_t j = (i + 1) & _mask(); _slots[j].id != 0; j = (j + 1) & _mask()) {
                    const size_t h = _home(_slots[j].id);
                    // j's entry may move to i when its home is not in (i, j]
                    const bool stays = i <= j ? (i < h && h <= j) : (i < h || h <= j);
                    if (!stays) {
                        _slots[i] = _slots[j];
                        _slots[j] = Stream();
                        i = j;
                    }
                }
            }

            size_t size() const noexcept {
                return _size;
            }

            template<class F>
            void each(F&& f) noexcept(std::is_nothrow_invocable_v<F&, Stream&>) {
                for (auto& s : _slots) {
                    if (s.id) {
                        f(s);
                    }
                }
            }

        private:
            std::vector<Stream> _slots;
            size_t _size = 0;

            size_t _mask() const noexcept {
                return _slots.size() - 1;
            }

            size_t _home(uint32_t id) const noexcept {
                return size_t(uint64_t(id) * 0x9E3779B97F4A7C15ull >> 32) & _mask();
            }
        };

        // A literal field without indexing with the indexed name :status
        // (RFC 7541 §6.2.2, static index 8): a 431 without touching the
        // encoder's table
        inline constexpr uint8_t Status431[] = {0x08, 0x03, '4', '3', '1'};

        inline constexpr uint8_t DrainPing[8] = {'s', 'g', 'c', 'l', 'd', 'r', 'a', 'i'};

        inline Limits sane(Limits s) noexcept {
            s.initial_window = std::clamp(s.initial_window, DefaultWindow, LargestWindow);
            s.connection_window = std::clamp(s.connection_window, DefaultWindow, LargestWindow);
            s.max_frame_size = std::clamp(s.max_frame_size, DefaultMaxFrameSize, LargestMaxFrameSize);
            return s;
        }
    }

    template<class Derived, class Events>
    class Endpoint {
    public:
        using Stream = connection_detail::Stream;

        // The bytes read from the transport: every whole frame at their
        // front is handled; the result is how many bytes were taken (the
        // rest, a frame not yet whole, is to be given again with more). An
        // error is the connection's: GOAWAY with its code is in output(),
        // and nothing more is read
        [[nodiscard]] expected<size_t, Error> feed(const uint8_t* p, size_t n, int64_t now) {
            _now = now;
            if (_failed) {
                return unexpected(_error);
            }
            size_t at = 0;
            if (_phase == Phase::preface) {
                if (n == 0) {
                    return size_t(0);
                }
                const size_t k = std::min(n, PrefaceSize);
                if (std::memcmp(p, Preface, k) != 0) {
                    return _fail(connection_error(ErrorCode::protocol_error, "not the connection preface of HTTP/2"));
                }
                if (k < PrefaceSize) {
                    return size_t(0);
                }
                at = PrefaceSize;
                _phase = Phase::first_settings;
            }
            while (at < n) {
                auto r = parse_frame(p + at, n - at, _ours.max_frame_size);
                if (!r.has_value()) {
                    const Error e = r.error();
                    if (e.connection()) {
                        return _fail(e);
                    }
                    // a stream's error: the frame is whole (only the length
                    // is read early, and that is the connection's)
                    const FrameHeader h = read_frame_header(p + at);
                    if (_phase == Phase::first_settings) {
                        return _fail(connection_error(ErrorCode::protocol_error, "the first frame is not SETTINGS"));
                    }
                    if (auto f = _check_order(h); !f) {
                        return _fail(f.error());
                    }
                    if (h.type != uint8_t(FrameType::priority) && _role()._idle(h.stream)) {
                        // WINDOW_UPDATE of 0 on a stream never opened: an idle
                        // stream takes nothing but HEADERS and PRIORITY (§5.1)
                        return _fail(connection_error(ErrorCode::protocol_error, "a frame on an idle stream"));
                    }
                    _stream_error(e.stream, e.code);
                    at += FrameHeaderSize + h.length;
                    if (auto f = _check_flood(); !f) {
                        return _fail(f.error());
                    }
                    continue;
                }
                if (r->size == 0) {
                    break;
                }
                if (auto f = _frame(r->frame); !f) {
                    return _fail(f.error());
                }
                at += r->size;
                if (auto f = _check_flood(); !f) {
                    return _fail(f.error());
                }
            }
            return at;
        }

        // The time passing: SETTINGS unacknowledged too long, streams
        // starved of window too long (at most 16 a call, see above)
        [[nodiscard]] expected<void, Error> tick(int64_t now) {
            _now = now;
            if (_failed) {
                return unexpected(_error);
            }
            if (_settings_pending && now - _settings_sent_at > _ours.settings_timeout) {
                return _fail_void(connection_error(ErrorCode::settings_timeout, "our SETTINGS unacknowledged"));
            }
            uint32_t starved[16];
            size_t k = 0;
            _streams.each([&](Stream& s) {
                if (s.blocked_since && now - s.blocked_since > _ours.window_timeout && k < std::size(starved)) {
                    starved[k++] = s.id;
                }
            });
            for (size_t i = 0; i < k; ++i) {
                _stream_error(starved[i], ErrorCode::cancel);
            }
            return {};
        }

        // A field block encoded by HPACK for a stream, as HEADERS and as
        // many CONTINUATIONs as the peer's frame size needs; false when the
        // stream is gone or its side already ended
        bool send_headers(uint32_t id, const uint8_t* block, size_t n, bool end_stream) noexcept {
            Stream* s = _streams.find(id);
            if (!s || s->local_closed || _failed) {
                return false;
            }
            _write_block(id, block, n, end_stream);
            if (end_stream) {
                _end_local(s);
            }
            return true;
        }

        // A body's bytes: as many as the windows allow, in frames of the
        // peer's size; END_STREAM goes with the last byte, when all were
        // taken. The result is the bytes taken; fewer than n: the rest
        // waits for on_window
        size_t send_data(uint32_t id, const uint8_t* p, size_t n, bool end_stream) noexcept {
            return _send_data(id, n, end_stream, [&](FrameWriter& w, size_t at, size_t k, bool last) {
                w.data(id, p + at, k, last);
            });
        }

        // The same, the bytes not copied: each frame's header in output()
        // and its payload a piece of `data` in place, a slice of its owner
        // (managed memory is never named by a raw address: the collector
        // takes such a word for a pointer kept as data), whose bytes stay
        // unchanged until what take_output gives is written. Only for an
        // owner that takes its output with take_output (output() is then
        // not whole)
        size_t send_data_in_place(uint32_t id, const slice<const byte>& data, bool end_stream) noexcept {
            return _send_data(id, data.size(), end_stream, [&](FrameWriter& w, size_t at, size_t k, bool last) {
                w.header(uint32_t(k), FrameType::data, last ? flag::end_stream : 0, id);
                if (k) {
                    _pieces.push_back(OutPiece{_out.size(), data.last(data.size() - at).first(k)});
                }
            });
        }

        // A piece of output in place: after byte `at` of the output's own
        // bytes come the bytes of `piece`
        struct OutPiece {
            size_t at;
            slice<const byte> piece;
        };

        // Everything to be sent, taken whole: the output's own bytes into
        // `bytes` (swapped, no copy: the two strings keep their room) and
        // the pieces in place, in order, into `pieces`. What was taken is
        // then the owner's to write, as written() of all of it
        void take_output(std::string& bytes, vector<OutPiece>& pieces) noexcept {
            if (_out_at) {
                _out.erase(0, _out_at);
                for (auto& q : _pieces) {
                    q.at -= _out_at;
                }
                _out_at = 0;
            }
            bytes.clear();
            bytes.swap(_out);
            pieces.clear();
            pieces.swap(_pieces);
            _control = 0;
        }

        template<class Put>
        size_t _send_data(uint32_t id, size_t n, bool end_stream, Put&& put) noexcept {
            Stream* s = _streams.find(id);
            if (!s || s->local_closed || _failed) {
                return 0;
            }
            FrameWriter w(_out);
            size_t taken = 0;
            while (taken < n) {
                const int64_t room = std::min({s->send, _conn_send, int64_t(_peer.max_frame_size)});
                if (room <= 0) {
                    break;
                }
                const size_t k = std::min(n - taken, size_t(room));
                const bool last = end_stream && taken + k == n;
                put(w, taken, k, last);
                s->send -= int64_t(k);
                _conn_send -= int64_t(k);
                taken += k;
            }
            if (taken < n) {
                if (!s->blocked_since) {
                    s->blocked_since = _now ? _now : 1;
                }
                return taken;
            }
            s->blocked_since = 0;
            if (end_stream) {
                if (n == 0) {
                    w.data(id, nullptr, 0, true);
                }
                _end_local(s);
            }
            return taken;
        }

        // The owner took n bytes of a stream's body: given back to the
        // peer by WINDOW_UPDATE once half a window has gathered. A stream
        // ended by both sides is gone from the table: its bytes still go
        // back to the connection's window
        void consumed(uint32_t id, size_t n) noexcept {
            if (_failed || n == 0) {
                return;
            }
            _give_connection(n);
            if (Stream* s = _streams.find(id)) {
                s->buffered -= std::min<uint64_t>(s->buffered, n);
                if (!s->remote_closed) {
                    _give_stream(s, n);
                }
            }
        }

        // The owner resets a stream (a handler that failed, a body it will
        // not read, a request given up); what the stream had received and
        // the owner not taken goes back to the connection's window
        void reset(uint32_t id, ErrorCode code) noexcept {
            if (!_failed && _streams.find(id)) {
                _reset(id, code, false);
            }
        }

        // A PING of ours (liveness); its answer comes to on_ping_ack
        void ping(const uint8_t opaque[8]) noexcept {
            if (_failed) {
                return;
            }
            FrameWriter(_out).ping(opaque, false);
            ++_control;
        }

        // GOAWAY at once with the last stream of the peer's seen
        void goaway(ErrorCode code) noexcept {
            if (_failed || _goaway_sent) {
                return;
            }
            _final_goaway(code);
        }

        // What is to be sent; written(n) when n bytes of it are gone (an
        // owner that sends DATA in place takes it with take_output instead)
        slice<const byte> output() const noexcept {
            assert(_pieces.empty() && "output() is not whole with DATA in place: take_output");
            return slice<const byte>(reinterpret_cast<const byte*>(_out.data()) + _out_at, _out.size() - _out_at);
        }

        void written(size_t n) noexcept {
            _out_at += n;
            if (_out_at >= _out.size()) {
                _out.clear();
                _out_at = 0;
                _control = 0;
            }
        }

        const PeerSettings& peer() const noexcept {
            return _peer;
        }

        const Limits& limits() const noexcept {
            return _ours;
        }

        // Whether the stream is open for our HEADERS and DATA (open, or
        // half-closed by the peer): a block is encoded only for one that is,
        // the encoder's table being the peer's decoder's
        bool sendable(uint32_t id) noexcept {
            Stream* s = _streams.find(id);
            return s && !s->local_closed && !_failed;
        }

        // HPACK's encoder of this connection: its table follows the peer's
        // SETTINGS_HEADER_TABLE_SIZE by itself
        Encoder& encoder() noexcept {
            return _encoder;
        }

        bool peer_settings_received() const noexcept {
            return _phase == Phase::frames;
        }

        bool failed() const noexcept {
            return _failed;
        }

        // Every stream closed after the last GOAWAY: the transport may close
        bool finished() const noexcept {
            return _failed || (_goaway_sent && _streams.size() == 0);
        }

        size_t open_streams() const noexcept {
            return _streams.size();
        }

        int64_t connection_send_window() const noexcept {
            return _conn_send;
        }

        int64_t stream_send_window(uint32_t id) noexcept {
            Stream* s = _streams.find(id);
            return s ? s->send : 0;
        }

    protected:
        enum class Phase : uint8_t { preface, first_settings, frames };

        Events* _events;
        Limits _ours;
        PeerSettings _peer;
        Decoder _decoder;
        Encoder _encoder;
        connection_detail::StreamTable _streams;
        std::array<uint32_t, 64> _recent{};   // streams we reset lately, whose late frames are ignored (§5.1)
        size_t _recent_at = 0;
        std::string _out;
        size_t _out_at = 0;
        vector<OutPiece> _pieces;             // DATA payloads in place (send_data_in_place), by position in _out
        std::string _block;                   // a field block across CONTINUATIONs
        Phase _phase;
        bool _failed = false;
        Error _error;
        bool _settings_pending = false;
        int64_t _settings_sent_at = 0;
        int64_t _now = 0;
        int64_t _conn_send = DefaultWindow;
        int64_t _conn_recv = DefaultWindow;
        uint64_t _conn_unacked = 0;
        size_t _control = 0;                  // control frames in output() unsent
        uint32_t _empty = 0;                  // empty frames in a row
        bool _goaway_sent = false;
        uint32_t _goaway_last = 0x7FFFFFFFu;
        // the field block being gathered
        bool _hb_open = false;
        uint32_t _hb_stream = 0;
        bool _hb_end_stream = false;
        bool _hb_self_dependent = false;

        // table_streams: the most streams open at once the table holds
        Endpoint(Events& events, const Limits& limits, uint32_t table_streams) noexcept
        : _events(&events)
        , _ours(connection_detail::sane(limits))
        , _decoder(_ours.header_table_size)
        , _streams(table_streams)
        , _phase(Derived::reads_preface ? Phase::preface : Phase::first_settings) {
            _recent.fill(0);
        }

        // Our SETTINGS (the entries the role gives), the connection's
        // window raised from 65535 to ours
        void _start(int64_t now, const Setting* entries, size_t n) noexcept {
            _now = now;
            FrameWriter w(_out);
            w.settings(entries, n);
            _settings_sent_at = now;
            _settings_pending = true;
            if (_ours.connection_window > DefaultWindow) {
                w.window_update(0, _ours.connection_window - DefaultWindow);
            }
            _conn_recv = _ours.connection_window;
        }

        // The entries every role announces, appended to s from n
        size_t _common_settings(Setting* s, size_t n) const noexcept {
            s[n++] = {uint16_t(SettingId::initial_window_size), _ours.initial_window};
            s[n++] = {uint16_t(SettingId::max_header_list_size), _ours.max_header_list_size};
            if (_ours.max_frame_size != DefaultMaxFrameSize) {
                s[n++] = {uint16_t(SettingId::max_frame_size), _ours.max_frame_size};
            }
            if (_ours.header_table_size != 4096) {
                s[n++] = {uint16_t(SettingId::header_table_size), _ours.header_table_size};
            }
            return n;
        }

        Derived& _role() noexcept {
            return static_cast<Derived&>(*this);
        }

        const Derived& _role() const noexcept {
            return static_cast<const Derived&>(*this);
        }

        size_t _block_limit() const noexcept {
            return std::max<size_t>(2 * size_t(_ours.max_header_list_size), 64 * 1024);
        }

        expected<size_t, Error> _fail(const Error& e) noexcept {
            _fail_void(e);
            return unexpected(e);
        }

        expected<void, Error> _fail_void(const Error& e) noexcept {
            if (!_failed) {
                FrameWriter(_out).goaway(_role()._last_peer_stream(), e.code);
                _goaway_sent = true;
                _failed = true;
                _error = e;
            }
            return unexpected(e);
        }

        void _final_goaway(ErrorCode code) noexcept {
            _goaway_last = _role()._last_peer_stream();
            FrameWriter(_out).goaway(_goaway_last, code);
            ++_control;
            _goaway_sent = true;
        }

        expected<void, Error> _check_flood() noexcept {
            if (_control > _ours.max_control_frames) {
                return unexpected(connection_error(ErrorCode::enhance_your_calm, "control frames piling up unread"));
            }
            if (_empty > _ours.max_empty_frames) {
                return unexpected(connection_error(ErrorCode::enhance_your_calm, "a flood of empty frames"));
            }
            return {};
        }

        // §4.3, §6.10: a field block is a run of frames nothing may cut
        expected<void, Error> _check_order(const FrameHeader& h) noexcept {
            if (_hb_open && (h.type != uint8_t(FrameType::continuation) || h.stream != _hb_stream)) {
                return unexpected(connection_error(ErrorCode::protocol_error, "a frame inside a field block"));
            }
            return {};
        }

        bool _recently_reset(uint32_t id) const noexcept {
            return std::find(_recent.begin(), _recent.end(), id) != _recent.end();
        }

        void _remember_reset(uint32_t id) noexcept {
            _recent[_recent_at] = id;
            _recent_at = (_recent_at + 1) % _recent.size();
        }

        void _give_connection(size_t n) noexcept {
            _conn_unacked += n;
            if (_conn_unacked >= _ours.connection_window / 2) {
                FrameWriter(_out).window_update(0, uint32_t(_conn_unacked));
                ++_control;
                _conn_recv += int64_t(_conn_unacked);
                _conn_unacked = 0;
            }
        }

        void _give_stream(Stream* s, size_t n) noexcept {
            s->unacked += n;
            if (s->unacked >= _ours.initial_window / 2) {
                FrameWriter(_out).window_update(s->id, uint32_t(s->unacked));
                ++_control;
                s->recv += int64_t(s->unacked);
                s->unacked = 0;
            }
        }

        void _end_local(Stream* s) noexcept {
            s->local_closed = true;
            s->blocked_since = 0;
            if (s->remote_closed) {
                _streams.remove(s);
            }
        }

        void _end_remote(Stream* s) noexcept {
            s->remote_closed = true;
            if (s->local_closed) {
                _streams.remove(s);
            }
        }

        // A stream leaving the table reset: what it held unconsumed goes
        // back to the connection's window (its owner will not consume it)
        void _drop(Stream* s) noexcept {
            if (s->buffered) {
                _give_connection(s->buffered);
            }
            _streams.remove(s);
        }

        // RST_STREAM from us; the owner is told when it knew the stream
        void _reset(uint32_t id, ErrorCode code, bool tell) {
            FrameWriter(_out).rst_stream(id, code);
            ++_control;
            _remember_reset(id);
            if (Stream* s = _streams.find(id)) {
                const bool known = s->known;
                _drop(s);
                if (known && tell) {
                    _events->on_reset(id, code);
                }
            }
        }

        void _stream_error(uint32_t id, ErrorCode code) {
            _reset(id, code, true);
        }

        void _write_block(uint32_t id, const uint8_t* block, size_t n, bool end_stream) noexcept {
            FrameWriter w(_out);
            const size_t most = _peer.max_frame_size;
            size_t k = std::min(n, most);
            w.headers(id, block, k, end_stream, k == n);
            while (k < n) {
                const size_t m = std::min(n - k, most);
                w.continuation(id, block + k, m, k + m == n);
                k += m;
            }
        }

        expected<void, Error> _frame(const Frame& f) {
            if (_phase == Phase::first_settings) {
                if (f.type() != FrameType::settings || f.ack()) {
                    return unexpected(connection_error(ErrorCode::protocol_error, "the first frame is not SETTINGS"));
                }
                _phase = Phase::frames;
            }
            if (auto r = _check_order(f.header); !r) {
                return r;
            }
            if (!f.known()) {
                ++_empty;
                return {};   // §5.5
            }
            switch (f.type()) {
            case FrameType::data: return _data(f);
            case FrameType::headers: return _headers(f);
            case FrameType::continuation: return _continuation(f);
            case FrameType::priority:
                ++_empty;
                return {};
            case FrameType::rst_stream: return _rst_stream(f);
            case FrameType::settings: return _settings(f);
            case FrameType::push_promise:
                // the server takes none from a client; the client announced
                // ENABLE_PUSH = 0 (§8.4)
                return unexpected(connection_error(ErrorCode::protocol_error, "PUSH_PROMISE"));
            case FrameType::ping: return _ping(f);
            case FrameType::goaway:
                _role()._peer_goaway(f.last_stream, ErrorCode(f.error_code));
                return {};
            case FrameType::window_update: return _window_update(f);
            default: return {};
            }
        }

        // §6.1, §5.2
        expected<void, Error> _data(const Frame& f) {
            const uint32_t id = f.header.stream;
            const size_t length = f.header.length;
            if (_role()._idle(id)) {
                return unexpected(connection_error(ErrorCode::protocol_error, "DATA on an idle stream"));
            }
            if (int64_t(length) > _conn_recv) {
                return unexpected(connection_error(ErrorCode::flow_control_error, "DATA past the connection's window"));
            }
            _conn_recv -= int64_t(length);
            Stream* s = _streams.find(id);
            if (!s || s->remote_closed) {
                // not taken by anyone: its window given back at once
                _give_connection(length);
                if (!s && _role()._ignore_closed(id)) {
                    return {};
                }
                _stream_error(id, ErrorCode::stream_closed);
                return {};
            }
            if (int64_t(length) > s->recv) {
                _give_connection(length);
                _stream_error(id, ErrorCode::flow_control_error);
                return {};
            }
            if (!_role()._accepts_data(*s)) {
                _give_connection(length);
                _stream_error(id, ErrorCode::protocol_error);
                return {};
            }
            s->recv -= int64_t(length);
            // the padding is nobody's to consume: given back now
            const size_t padding = length - f.payload.size();
            if (padding) {
                _give_connection(padding);
                _give_stream(s, padding);
            }
            if (f.payload.size() || f.end_stream()) {
                _empty = 0;
            } else {
                ++_empty;
            }
            const bool end = f.end_stream();
            const bool known = s->known;
            s->buffered += f.payload.size();
            if (end) {
                _end_remote(s);
            }
            if (known) {
                _events->on_data(id, reinterpret_cast<const uint8_t*>(f.payload.data()), f.payload.size(), end);
            }
            return {};
        }

        // §6.2
        expected<void, Error> _headers(const Frame& f) {
            _hb_stream = f.header.stream;
            _hb_end_stream = f.end_stream();
            _hb_self_dependent = f.self_dependent();
            if (f.payload.size() > _block_limit()) {
                return unexpected(connection_error(ErrorCode::enhance_your_calm, "a field block past its limit"));
            }
            if (f.end_headers()) {
                return _block_done(reinterpret_cast<const uint8_t*>(f.payload.data()), f.payload.size());
            }
            if (f.payload.size() == 0) {
                ++_empty;
            }
            _hb_open = true;
            _block.assign(reinterpret_cast<const char*>(f.payload.data()), f.payload.size());
            return {};
        }

        // §6.10
        expected<void, Error> _continuation(const Frame& f) {
            if (!_hb_open) {
                return unexpected(connection_error(ErrorCode::protocol_error, "CONTINUATION without a field block"));
            }
            if (_block.size() + f.payload.size() > _block_limit()) {
                return unexpected(connection_error(ErrorCode::enhance_your_calm, "a field block past its limit"));
            }
            _block.append(reinterpret_cast<const char*>(f.payload.data()), f.payload.size());
            if (!f.end_headers()) {
                if (f.payload.size() == 0) {
                    ++_empty;
                }
                return {};
            }
            _hb_open = false;
            return _block_done(reinterpret_cast<const uint8_t*>(_block.data()), _block.size());
        }

        // A whole field block: decoded whatever becomes of its stream (the
        // table is shared, §4.3), then the role's
        expected<void, Error> _block_done(const uint8_t* block, size_t n) {
            _empty = 0;
            const uint32_t id = _hb_stream;
            auto decoded = _decoder.decode(slice<const byte>(reinterpret_cast<const byte*>(block), n), _ours.max_header_list_size);
            if (!decoded.has_value()) {
                return unexpected(decoded.error());
            }
            if (Stream* s = _streams.find(id)) {
                return _role()._block_on(s, std::move(*decoded), _hb_end_stream, _hb_self_dependent);
            }
            if (!_role()._idle(id)) {
                if (_role()._ignore_closed(id)) {
                    return {};
                }
                return unexpected(connection_error(ErrorCode::stream_closed, "HEADERS on a closed stream"));
            }
            if (!_role()._peer_opens(id)) {
                return unexpected(connection_error(ErrorCode::protocol_error, "HEADERS on a stream the peer may not open"));
            }
            return _role()._block_new(id, std::move(*decoded), _hb_end_stream, _hb_self_dependent);
        }

        // §6.4
        expected<void, Error> _rst_stream(const Frame& f) {
            const uint32_t id = f.header.stream;
            if (_role()._idle(id)) {
                return unexpected(connection_error(ErrorCode::protocol_error, "RST_STREAM on an idle stream"));
            }
            if (auto r = _role()._peer_reset(); !r) {
                return r;
            }
            if (Stream* s = _streams.find(id)) {
                const bool known = s->known;
                _drop(s);
                if (known) {
                    _events->on_reset(id, ErrorCode(f.error_code));
                }
            }
            return {};
        }

        // §6.5
        expected<void, Error> _settings(const Frame& f) {
            if (f.ack()) {
                if (!_settings_pending) {
                    return unexpected(connection_error(ErrorCode::protocol_error, "a SETTINGS acknowledgement for nothing sent"));
                }
                _settings_pending = false;
                _decoder.set_max_table_size(_ours.header_table_size);
                return {};
            }
            for (size_t i = 0; i < f.settings_count(); ++i) {
                const Setting s = f.setting(i);
                switch (SettingId(s.id)) {
                case SettingId::header_table_size:
                    _peer.header_table_size = s.value;
                    _encoder.set_peer_max_table_size(s.value);
                    break;
                case SettingId::max_concurrent_streams: _peer.max_concurrent_streams = s.value; break;
                case SettingId::max_frame_size: _peer.max_frame_size = s.value; break;
                case SettingId::max_header_list_size: _peer.max_header_list_size = s.value; break;
                case SettingId::initial_window_size: {
                    // §6.9.2: every stream's window moves by the difference
                    const int64_t delta = int64_t(s.value) - int64_t(_peer.initial_window);
                    _peer.initial_window = s.value;
                    bool overflow = false;
                    uint32_t opened[32];
                    size_t k = 0;
                    _streams.each([&](Stream& st) {
                        const bool was = st.send > 0;
                        st.send += delta;
                        overflow |= st.send > int64_t(LargestWindow);
                        if (!was && st.send > 0 && k < std::size(opened)) {
                            opened[k++] = st.id;
                        }
                    });
                    if (overflow) {
                        return unexpected(connection_error(ErrorCode::flow_control_error, "SETTINGS_INITIAL_WINDOW_SIZE overflows a window"));
                    }
                    for (size_t j = 0; j < k; ++j) {
                        if (Stream* st = _streams.find(opened[j])) {
                            st->blocked_since = 0;
                        }
                        _events->on_window(opened[j]);
                    }
                    if (k == std::size(opened)) {
                        _events->on_window(0);
                    }
                    break;
                }
                default: break;   // ENABLE_PUSH: nothing is pushed; unknown ids ignored
                }
            }
            FrameWriter(_out).settings_ack();
            ++_control;
            return {};
        }

        // §6.7
        expected<void, Error> _ping(const Frame& f) {
            const uint8_t* data = reinterpret_cast<const uint8_t*>(f.payload.data());
            if (!f.ack()) {
                FrameWriter(_out).ping(data, true);
                ++_control;
                return {};
            }
            if (!_role()._own_ping(data)) {
                _events->on_ping_ack(data);
            }
            return {};
        }

        // §6.9
        expected<void, Error> _window_update(const Frame& f) {
            const uint32_t id = f.header.stream;
            if (id == 0) {
                const bool was = _conn_send > 0;
                if (_conn_send + f.increment > int64_t(LargestWindow)) {
                    return unexpected(connection_error(ErrorCode::flow_control_error, "the connection's window past 2^31 - 1"));
                }
                _conn_send += f.increment;
                if (!was && _conn_send > 0) {
                    _events->on_window(0);
                }
                return {};
            }
            if (_role()._idle(id)) {
                return unexpected(connection_error(ErrorCode::protocol_error, "WINDOW_UPDATE on an idle stream"));
            }
            Stream* s = _streams.find(id);
            if (!s) {
                return {};
            }
            const bool was = s->send > 0;
            if (s->send + f.increment > int64_t(LargestWindow)) {
                _stream_error(id, ErrorCode::flow_control_error);
                return {};
            }
            s->send += f.increment;
            if (!was && s->send > 0) {
                s->blocked_since = 0;
                _events->on_window(id);
            }
            return {};
        }
    };

    // The server's settings (Go's numbers where Go has them)
    struct ServerSettings : Limits {
        uint32_t max_concurrent_streams = 250;   // Go: 250
        // RST_STREAM received: a bucket of max_resets, refilled at
        // resets_per_second (nghttp2's stream reset rate limit, 1000 and
        // 33/s), empty: GOAWAY ENHANCE_YOUR_CALM
        uint32_t max_resets = 1000;
        uint32_t resets_per_second = 33;

        ServerSettings() noexcept {
            initial_window = 1u << 20;      // Go: 1 MB
            connection_window = 1u << 20;   // Go: 1 MB
            max_header_list_size = 32 * 1024;   // the server's max_header_bytes
        }
    };

    // The server's side. Its Events add to the common ones:
    //
    //   ErrorCode on_request(uint32_t id, Block&& fields, bool end_stream, bool start)
    //       a new stream's request fields; `start`: a handler may run now,
    //       otherwise when on_start(id) comes. Anything but no_error refuses
    //       the stream with that code (a malformed request: PROTOCOL_ERROR)
    //   void on_start(uint32_t id)                      a queued request's turn
    //   void on_trailers(uint32_t id, Block&& fields)   the request's trailers
    //
    // Its limits against rapid reset (CVE-2023-44487), two in depth: Go's
    // handlers' queue — at most max_concurrent_streams handlers run, a
    // request past them waits its turn, more than 4 × max_concurrent_streams
    // waiting (streams the peer opened and reset while the handlers ran
    // count until their turn) is GOAWAY ENHANCE_YOUR_CALM, a request reset
    // while waiting never starts — and nghttp2's rate of RST_STREAM received
    // (ServerSettings::max_resets, resets_per_second). A list decoded past
    // max_header_list_size (an HPACK bomb) is answered 431 on its stream, the
    // connection lives.
    template<class Events>
    class ServerConnection : public Endpoint<ServerConnection<Events>, Events> {
        using Base = Endpoint<ServerConnection<Events>, Events>;
        friend Base;

    public:
        using Stream = connection_detail::Stream;

        static constexpr bool reads_preface = true;

        ServerConnection(Events& events, const ServerSettings& settings = ServerSettings()) noexcept
        : Base(events, settings, std::max(1u, settings.max_concurrent_streams))
        , _max_streams(std::max(1u, settings.max_concurrent_streams))
        , _waiting(4 * size_t(_max_streams) + 2)
        , _reset_bucket(double(settings.max_resets))
        , _reset_burst(double(settings.max_resets))
        , _reset_rate(double(settings.resets_per_second)) {
        }

        // The server's preface (§3.4): our SETTINGS, and the connection's
        // window raised from 65535 to ours
        void start(int64_t now) noexcept {
            Setting s[6];
            size_t n = 0;
            s[n++] = {uint16_t(SettingId::max_concurrent_streams), _max_streams};
            n = this->_common_settings(s, n);
            this->_start(now, s, n);
            _reset_at = now;
        }

        // A handler ended: its place goes to the next request waiting
        void release(uint32_t) {
            if (_handlers > 0) {
                --_handlers;
            }
            while (_handlers < _max_streams && _wait_size) {
                const uint32_t id = _waiting[_wait_head];
                _wait_head = (_wait_head + 1) % _waiting.size();
                --_wait_size;
                Stream* s = this->_streams.find(id);
                if (s && s->known) {   // reset while waiting: its turn is skipped
                    ++_handlers;
                    this->_events->on_start(id);
                }
            }
        }

        // Graceful shutdown (as Go): GOAWAY with the largest identifier and
        // a PING; when the PING comes back, GOAWAY with the last stream
        // really seen. Streams after it are not served
        void drain() noexcept {
            if (this->_failed || _draining || this->_goaway_sent) {
                return;
            }
            _draining = true;
            FrameWriter w(this->_out);
            w.goaway(0x7FFFFFFFu, ErrorCode::no_error);
            w.ping(connection_detail::DrainPing, false);
            this->_control += 2;
        }

        size_t handlers() const noexcept {
            return _handlers;
        }

        uint32_t max_concurrent_streams() const noexcept {
            return _max_streams;
        }

    private:
        uint32_t _max_streams;
        std::vector<uint32_t> _waiting;       // requests waiting for a handler, a ring
        size_t _wait_head = 0;
        size_t _wait_size = 0;
        uint32_t _max_client_id = 0;
        uint32_t _handlers = 0;
        bool _draining = false;
        double _reset_bucket;
        double _reset_burst;
        double _reset_rate;
        int64_t _reset_at = 0;

        // --- the hooks of Endpoint ---

        bool _idle(uint32_t id) const noexcept {
            return id > _max_client_id;
        }

        bool _peer_opens(uint32_t id) const noexcept {
            return (id & 1) != 0;
        }

        uint32_t _last_peer_stream() const noexcept {
            return _max_client_id;
        }

        bool _accepts_data(const Stream&) const noexcept {
            return true;
        }

        bool _ignore_closed(uint32_t id) const noexcept {
            return this->_recently_reset(id);
        }

        bool _own_ping(const uint8_t* data) noexcept {
            if (_draining && !this->_goaway_sent && std::memcmp(data, connection_detail::DrainPing, 8) == 0) {
                this->_final_goaway(ErrorCode::no_error);
                return true;
            }
            return false;
        }

        void _peer_goaway(uint32_t last, ErrorCode code) {
            this->_events->on_goaway(last, code);
        }

        // nghttp2's bucket: refilled by the time passed, one a RST_STREAM
        expected<void, Error> _peer_reset() noexcept {
            const int64_t now = this->_now;
            if (now > _reset_at) {
                _reset_bucket = std::min(_reset_burst, _reset_bucket + double(now - _reset_at) * _reset_rate / 1e9);
                _reset_at = now;
            }
            if (_reset_bucket < 1) {
                return unexpected(connection_error(ErrorCode::enhance_your_calm, "RST_STREAM past its rate"));
            }
            _reset_bucket -= 1;
            return {};
        }

        // trailers (§8.1)
        expected<void, Error> _block_on(Stream* s, Block&& block, bool end_stream, bool self_dependent) {
            const uint32_t id = s->id;
            if (s->remote_closed) {
                this->_stream_error(id, ErrorCode::stream_closed);
                return {};
            }
            if (!end_stream || self_dependent || block.truncated) {
                this->_stream_error(id, ErrorCode::protocol_error);
                return {};
            }
            const bool known = s->known;
            this->_end_remote(s);
            if (known) {
                this->_events->on_trailers(id, std::move(block));
            }
            return {};
        }

        // a new request
        expected<void, Error> _block_new(uint32_t id, Block&& block, bool end_stream, bool self_dependent) {
            _max_client_id = id;
            if (this->_goaway_sent && id > this->_goaway_last) {
                return {};   // after our last GOAWAY: not served, not answered (§6.8)
            }
            if (self_dependent) {
                this->_stream_error(id, ErrorCode::protocol_error);
                return {};
            }
            if (this->_streams.size() >= _max_streams) {
                this->_stream_error(id, ErrorCode::refused_stream);
                return {};
            }
            if (block.truncated) {
                // a list past max_header_list_size: 431 and the stream ends
                // here, the connection lives (§8.1: RST_STREAM NO_ERROR when
                // the request has not ended)
                FrameWriter(this->_out).headers(id, connection_detail::Status431, sizeof(connection_detail::Status431), true, true);
                if (!end_stream) {
                    FrameWriter(this->_out).rst_stream(id, ErrorCode::no_error);
                    ++this->_control;
                    this->_remember_reset(id);
                }
                return {};
            }
            Stream* s = this->_streams.add(id);
            s->send = this->_peer.initial_window;
            s->recv = this->_ours.initial_window;
            s->remote_closed = end_stream;
            const bool start = _handlers < _max_streams;
            if (!start && _wait_size > 4 * size_t(_max_streams)) {
                return unexpected(connection_error(ErrorCode::enhance_your_calm, "too many requests reset early"));
            }
            const ErrorCode verdict = this->_events->on_request(id, std::move(block), end_stream, start);
            if (verdict != ErrorCode::no_error) {
                this->_stream_error(id, verdict);
                return {};
            }
            if (Stream* again = this->_streams.find(id)) {
                again->known = true;
            }
            if (start) {
                ++_handlers;
            } else {
                _waiting[(_wait_head + _wait_size) % _waiting.size()] = id;
                ++_wait_size;
            }
            return {};
        }
    };
}
