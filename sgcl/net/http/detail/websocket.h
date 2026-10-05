//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "parser.h"
#include "../headers.h"
#include "../../../compress/detail/deflate.h"
#include "../../../compress/detail/inflate.h"
#include "../../../core/aliases.h"
#include "../../../core/detail/bytes.h"
#include "../../../core/string.h"
#include "../../../crypto/random.h"
#include "../../../crypto/sha1.h"
#include "../../../encoding/base64.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// The parts of WebSocket (RFC 6455) that do no I/O: a frame's header read
// and written, the mask, UTF-8 checked as it comes, the close frame's
// payload, the opening handshake's key, its checks on either side, and
// permessage-deflate (RFC 7692) over the compress module's DEFLATE. The
// connection (websocket.h) drives them.
namespace sgcl::net::http::detail {
    using namespace sgcl::detail;

    // RFC 6455 §5.2
    enum class WsOpcode : uint8_t { continuation = 0x0, text = 0x1, binary = 0x2, close = 0x8, ping = 0x9, pong = 0xA };

    // A frame's header: the first two bytes, the extended length, the mask
    struct WsFrame {
        bool fin = false;
        bool rsv1 = false;            // permessage-deflate: the message is compressed (its first frame)
        WsOpcode opcode = WsOpcode::continuation;
        bool masked = false;
        uint32_t mask = 0;            // the key's four bytes, the first in the low byte
        uint64_t length = 0;
        size_t header = 0;            // the header's bytes, 2 to 14

        SGCL_INLINE_HOT bool control() const noexcept {
            return (uint8_t(opcode) & 0x8) != 0;
        }
    };

    // What a header's read came to: more bytes wanted, the header, or a
    // breach of the protocol (the close code to answer it with: 1002)
    enum class WsParse : uint8_t { more, frame, invalid };

    // The header at p[0, n): its fields, and what of the rules a frame
    // breaks (§5.2, §5.5): a reserved bit not negotiated, a reserved
    // opcode, a control frame fragmented or longer than 125 bytes, a mask
    // where none belongs or none where one must be (a server's frames
    // unmasked, a client's masked), a length whose top bit is set
    inline WsParse parse_ws_frame(const uint8_t* p, size_t n, bool expect_mask, bool deflate, WsFrame& f, const char*& why) noexcept {
        if (n < 2) {
            return WsParse::more;
        }
        const uint8_t b0 = p[0], b1 = p[1];
        f.fin = (b0 & 0x80) != 0;
        f.rsv1 = (b0 & 0x40) != 0;
        f.opcode = WsOpcode(b0 & 0x0F);
        f.masked = (b1 & 0x80) != 0;
        if ((b0 & 0x30) || (f.rsv1 && !deflate)) {
            why = "a reserved bit set";
            return WsParse::invalid;
        }
        switch (f.opcode) {
            case WsOpcode::continuation:
            case WsOpcode::text:
            case WsOpcode::binary:
            case WsOpcode::close:
            case WsOpcode::ping:
            case WsOpcode::pong:
                break;
            default:
                why = "a reserved opcode";
                return WsParse::invalid;
        }
        if (f.masked != expect_mask) {
            why = expect_mask ? "a client's frame unmasked" : "a server's frame masked";
            return WsParse::invalid;
        }
        uint64_t len = b1 & 0x7F;
        size_t h = 2;
        if (len == 126) {
            if (n < 4) {
                return WsParse::more;
            }
            len = (uint64_t(p[2]) << 8) | p[3];
            h = 4;
        } else if (len == 127) {
            if (n < 10) {
                return WsParse::more;
            }
            len = 0;
            for (int i = 0; i < 8; ++i) {
                len = (len << 8) | p[2 + i];
            }
            if (len >> 63) {
                why = "a length with its top bit set";
                return WsParse::invalid;
            }
            h = 10;
        }
        if (f.control()) {
            if (!f.fin) {
                why = "a control frame fragmented";
                return WsParse::invalid;
            }
            if (len > 125) {
                why = "a control frame longer than 125 bytes";
                return WsParse::invalid;
            }
            if (f.rsv1) {
                why = "a control frame compressed";
                return WsParse::invalid;
            }
        }
        if (f.masked) {
            if (n < h + 4) {
                return WsParse::more;
            }
            f.mask = uint32_t(p[h]) | (uint32_t(p[h + 1]) << 8) | (uint32_t(p[h + 2]) << 16) | (uint32_t(p[h + 3]) << 24);
            h += 4;
        } else {
            f.mask = 0;
        }
        f.length = len;
        f.header = h;
        return WsParse::frame;
    }

    // A frame's header into out (at least 14 bytes): FIN, RSV1, the opcode,
    // the length in the fewest bytes (§5.2), the mask when there is one;
    // its size
    SGCL_INLINE_HOT size_t write_ws_header(uint8_t* out, bool fin, bool rsv1, WsOpcode op, uint64_t length, bool masked, uint32_t mask) noexcept {
        out[0] = uint8_t((fin ? 0x80 : 0) | (rsv1 ? 0x40 : 0) | uint8_t(op));
        const uint8_t m = masked ? 0x80 : 0;
        size_t h;
        if (length < 126) {
            out[1] = uint8_t(m | length);
            h = 2;
        } else if (length <= 0xFFFF) {
            out[1] = uint8_t(m | 126);
            out[2] = uint8_t(length >> 8);
            out[3] = uint8_t(length);
            h = 4;
        } else {
            out[1] = uint8_t(m | 127);
            for (int i = 0; i < 8; ++i) {
                out[2 + i] = uint8_t(length >> (56 - 8 * i));
            }
            h = 10;
        }
        if (masked) {
            out[h] = uint8_t(mask);
            out[h + 1] = uint8_t(mask >> 8);
            out[h + 2] = uint8_t(mask >> 16);
            out[h + 3] = uint8_t(mask >> 24);
            h += 4;
        }
        return h;
    }

    // The mask (§5.3) over p[0, n), the payload's byte `offset` the first:
    // eight bytes at a time; the same call unmasks
    inline void ws_mask(uint8_t* p, size_t n, uint32_t mask, uint64_t offset) noexcept {
        const unsigned r = unsigned(offset & 3) * 8;
        uint32_t m = r ? (mask >> r) | (mask << (32 - r)) : mask;
        size_t i = 0;
        // to an 8-byte boundary of p, the key turned with each byte
        while (i < n && (reinterpret_cast<uintptr_t>(p + i) & 7)) {
            p[i++] ^= uint8_t(m);
            m = (m >> 8) | (m << 24);
        }
        const uint64_t m64 = uint64_t(m) | (uint64_t(m) << 32);
        for (; i + 8 <= n; i += 8) {
            uint64_t w;
            std::memcpy(&w, p + i, 8);
            w ^= m64;
            std::memcpy(p + i, &w, 8);
        }
        for (; i < n; ++i) {
            p[i] ^= uint8_t(m);
            m = (m >> 8) | (m << 24);
        }
    }

    // UTF-8 (RFC 3629) checked as it comes, a fragment at a time: a byte
    // that cannot come where it does fails at once (the fail-fast of
    // §8.1), and a message must end with no sequence open
    class Utf8Check {
    public:
        // false at the first byte that makes the text invalid
        bool feed(const uint8_t* p, size_t n) noexcept {
            size_t i = 0;
            while (i < n) {
                if (_need == 0) {
                    // ASCII, eight bytes at a time
                    while (i + 8 <= n) {
                        uint64_t w;
                        std::memcpy(&w, p + i, 8);
                        if (w & 0x8080808080808080ull) {
                            break;
                        }
                        i += 8;
                    }
                    if (i == n) {
                        break;
                    }
                    const uint8_t c = p[i++];
                    if (c < 0x80) {
                        continue;
                    }
                    if (c >= 0xC2 && c <= 0xDF) {
                        _need = 1;
                        _lo = 0x80;
                        _hi = 0xBF;
                    } else if (c >= 0xE0 && c <= 0xEF) {
                        _need = 2;
                        _lo = c == 0xE0 ? 0xA0 : 0x80;   // no overlong form
                        _hi = c == 0xED ? 0x9F : 0xBF;   // no surrogate
                    } else if (c >= 0xF0 && c <= 0xF4) {
                        _need = 3;
                        _lo = c == 0xF0 ? 0x90 : 0x80;   // no overlong form
                        _hi = c == 0xF4 ? 0x8F : 0xBF;   // nothing past U+10FFFF
                    } else {
                        return false;
                    }
                    continue;
                }
                const uint8_t c = p[i++];
                if (c < _lo || c > _hi) {
                    return false;
                }
                _lo = 0x80;
                _hi = 0xBF;
                --_need;
            }
            return true;
        }

        // whether the text so far ends where a character does
        SGCL_INLINE_HOT bool complete() const noexcept {
            return _need == 0;
        }

        SGCL_INLINE_HOT void reset() noexcept {
            _need = 0;
        }

    private:
        uint8_t _need = 0;
        uint8_t _lo = 0x80, _hi = 0xBF;
    };

    SGCL_INLINE_HOT bool valid_utf8(const uint8_t* p, size_t n) noexcept {
        Utf8Check c;
        return c.feed(p, n) && c.complete();
    }

    // A close code a frame may carry (§7.4): 1000-1003, 1007-1014 of the
    // registry, 3000-4999 for libraries and applications; never 1004-1006
    // and 1015, which are not sent
    SGCL_INLINE_HOT bool sendable_close_code(uint16_t code) noexcept {
        return (code >= 1000 && code <= 1003) || (code >= 1007 && code <= 1014) || (code >= 3000 && code <= 4999);
    }

    // A close frame's payload (§5.5.1): nothing, or a code and a reason in
    // UTF-8; the code 1005 when there is none. False for one byte, a code
    // that may not be sent, a reason that is not UTF-8
    inline bool parse_ws_close(const uint8_t* p, size_t n, uint16_t& code, std::string& reason) noexcept {
        reason.clear();
        if (n == 0) {
            code = 1005;
            return true;
        }
        if (n == 1) {
            return false;
        }
        code = uint16_t((p[0] << 8) | p[1]);
        if (!sendable_close_code(code) || !valid_utf8(p + 2, n - 2)) {
            return false;
        }
        reason.assign(reinterpret_cast<const char*>(p + 2), n - 2);
        return true;
    }

    // §1.3: the accept of a key, base64(SHA-1(key + the GUID))
    inline string ws_accept(std::string_view key) noexcept {
        crypto::sha1 h;
        h.update(slice<const byte>(reinterpret_cast<const byte*>(key.data()), key.size()));
        static constexpr char guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        h.update(slice<const byte>(reinterpret_cast<const byte*>(guid), sizeof(guid) - 1));
        auto d = h.value();
        return encoding::base64::standard.encode(slice<const byte>(d.data(), d.size()));
    }

    // A client's key: 16 random bytes in base64 (§4.1)
    inline string ws_key() noexcept {
        array<byte, 16> r;
        crypto::random::fill(slice<byte>(r.data(), r.size()));
        return encoding::base64::standard.encode(slice<const byte>(r.data(), r.size()));
    }

    // Whether a comma-separated field holds the token (without case)
    inline bool ws_has_token(const headers& h, std::string_view name, std::string_view token) noexcept {
        return HeadersAccess::has_token(h, name, token);
    }

    // The tokens of every field of the name, in order: "a, b", "c" gives
    // a, b, c
    inline std::vector<std::string> ws_tokens(const headers& h, std::string_view name) noexcept {
        std::vector<std::string> out;
        for (auto& f : HeadersAccess::fields(h)) {
            if (!iequal(f.first.view(), name)) {
                continue;
            }
            std::string_view v = f.second.view();
            while (!v.empty()) {
                auto comma = v.find(',');
                auto item = trim_ows(v.substr(0, comma));
                if (!item.empty()) {
                    out.emplace_back(item);
                }
                if (comma == std::string_view::npos) {
                    break;
                }
                v.remove_prefix(comma + 1);
            }
        }
        return out;
    }

    // permessage-deflate's parameters (RFC 7692 §7.1), of an offer or of
    // the answer to one
    struct WsDeflateParams {
        bool server_no_context_takeover = false;
        bool client_no_context_takeover = false;
        int server_max_window_bits = 15;   // 0: given without a value (a client's)
        int client_max_window_bits = 15;   // 0: given without a value (a client's offer: "I can take one")
        bool client_max_window_bits_given = false;
        bool server_max_window_bits_given = false;
    };

    // One extension of a Sec-WebSocket-Extensions list ("permessage-deflate;
    // client_max_window_bits"): its parameters, when it is permessage-deflate
    // and every parameter is one RFC 7692 has, once, with a value it allows
    inline bool parse_ws_deflate(std::string_view ext, WsDeflateParams& p) noexcept {
        p = WsDeflateParams();
        auto semi = ext.find(';');
        if (!iequal(trim_ows(ext.substr(0, semi)), "permessage-deflate")) {
            return false;
        }
        bool seen[4] = {};
        while (semi != std::string_view::npos) {
            ext.remove_prefix(semi + 1);
            semi = ext.find(';');
            std::string_view param = trim_ows(ext.substr(0, semi));
            auto eq = param.find('=');
            std::string_view key = trim_ows(param.substr(0, eq));
            std::string_view value = eq == std::string_view::npos ? std::string_view() : trim_ows(param.substr(eq + 1));
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            auto bits = [&](int& out) {
                if (value.empty()) {
                    out = 0;
                    return true;
                }
                if (value.size() > 2 || value[0] < '0' || value[0] > '9' || (value.size() == 2 && (value[1] < '0' || value[1] > '9')) || value[0] == '0') {
                    return false;
                }
                int v = value.size() == 2 ? (value[0] - '0') * 10 + (value[1] - '0') : value[0] - '0';
                if (v < 8 || v > 15) {
                    return false;
                }
                out = v;
                return true;
            };
            int index;
            if (iequal(key, "server_no_context_takeover") && value.empty() && eq == std::string_view::npos) {
                index = 0;
                p.server_no_context_takeover = true;
            } else if (iequal(key, "client_no_context_takeover") && value.empty() && eq == std::string_view::npos) {
                index = 1;
                p.client_no_context_takeover = true;
            } else if (iequal(key, "server_max_window_bits")) {
                index = 2;
                if (!bits(p.server_max_window_bits) || p.server_max_window_bits == 0) {
                    return false;   // a value it must have
                }
                p.server_max_window_bits_given = true;
            } else if (iequal(key, "client_max_window_bits")) {
                index = 3;
                if (!bits(p.client_max_window_bits)) {
                    return false;
                }
                p.client_max_window_bits_given = true;
            } else {
                return false;
            }
            if (seen[index]) {
                return false;
            }
            seen[index] = true;
        }
        return true;
    }

    // The server's answer to a client's offers (each extension of the
    // list in order, RFC 7692 §5): the first permessage-deflate offer it
    // can take. Its compressor uses a 32 KB window, so an offer asking the
    // server for less (server_max_window_bits below 15) is declined; the
    // client's window may be any, since a decoder of 32 KB reads every
    // one. The answer: the offer's no_context_takeover parameters echoed
    inline optional<WsDeflateParams> choose_ws_deflate(const std::vector<std::string>& offers) noexcept {
        for (const auto& o : offers) {
            WsDeflateParams p;
            if (!parse_ws_deflate(o, p)) {
                continue;
            }
            if (p.server_max_window_bits_given && p.server_max_window_bits < 15) {
                continue;
            }
            return p;
        }
        return nullopt;
    }

    // The answer's text of the parameters taken
    inline std::string ws_deflate_answer(const WsDeflateParams& p) noexcept {
        std::string s = "permessage-deflate";
        if (p.server_no_context_takeover) {
            s += "; server_no_context_takeover";
        }
        if (p.client_no_context_takeover) {
            s += "; client_no_context_takeover";
        }
        return s;
    }

    // permessage-deflate's two directions (RFC 7692 §7.2): a message
    // compressed by a sync flush with its last four bytes (00 00 FF FF)
    // taken off, and a message decompressed with them put back, each
    // direction's window kept from one message to the next unless its
    // no_context_takeover was agreed. The compressor at level 1, as Go's
    // and the browsers' are by default: speed over the last bytes
    class WsDeflate {
    public:
        SGCL_INLINE_HOT WsDeflate(bool reset_out, bool reset_in) noexcept
        : _deflater(std::make_unique<compress::detail::Deflater>(1))
        , _state(std::make_unique<compress::detail::InflateState>())
        , _window(2 * compress::detail::WindowSize + compress::detail::MaxMatch + 8)
        , _reset_out(reset_out)
        , _reset_in(reset_in) {
            _state->reset();
        }

        // The message's compressed bytes
        void compress(const uint8_t* data, size_t n, std::vector<uint8_t>& out) noexcept {
            out.clear();
            _deflater->write(data, n, out);
            _deflater->flush(out);
            out.resize(out.size() - 4);   // the flush's empty stored block, 00 00 FF FF
            if (_reset_out) {
                _deflater->reset();
            }
        }

        // The message's bytes appended to out, at most `limit` of them in
        // all: false for data that is not DEFLATE (corrupt), or past the
        // limit (`too_large` set)
        bool decompress(const uint8_t* in, size_t n, vector<byte>& out, size_t limit, bool& too_large) noexcept {
            too_large = false;
            static constexpr uint8_t tail[4] = {0, 0, 0xFF, 0xFF};
            if (!_feed(in, n, out, limit, too_large)) {
                return false;
            }
            if (!_ended && !_feed(tail, 4, out, limit, too_large)) {
                return false;
            }
            if (_ended || _reset_in) {
                // a last block (BFINAL) ended the stream, as RFC 7692
                // §7.2.3.4 allows: the next message starts another, over the
                // same window unless its context is not kept
                _state->reset();
                _ended = false;
            }
            if (_reset_in) {
                _pos = 0;
            }
            return true;
        }

    private:
        // in decoded into the window and appended to out; the input past a
        // last block's end is not read
        bool _feed(const uint8_t* in, size_t n, vector<byte>& out, size_t limit, bool& too_large) noexcept {
            using namespace compress::detail;
            const uint8_t* end = in + n;
            const size_t window_bytes = 2 * WindowSize;
            for (;;) {
                if (_pos + MaxMatch + 8 > window_bytes) {
                    size_t keep = std::min<size_t>(_pos, WindowSize);
                    sgcl::detail::move_bytes(_window.data(), _window.data() + _pos - keep, keep);
                    _pos = keep;
                }
                size_t before = _pos;
                auto st = inflate(*_state, in, end, _window.data(), _pos, window_bytes);
                size_t made = _pos - before;
                if (made) {
                    if (out.size() + made > limit) {
                        too_large = true;
                        return false;
                    }
                    size_t at = out.size();
                    out.resize(at + made);
                    sgcl::detail::copy_bytes(out.data() + at, _window.data() + before, made);
                }
                if (st == InflateStatus::failed) {
                    return false;
                }
                if (st == InflateStatus::done) {
                    _ended = true;
                    return true;
                }
                if (st == InflateStatus::need_input && in == end) {
                    return true;
                }
            }
        }

        std::unique_ptr<compress::detail::Deflater> _deflater;
        std::unique_ptr<compress::detail::InflateState> _state;
        std::vector<uint8_t> _window;
        size_t _pos = 0;
        bool _ended = false;
        bool _reset_out;
        bool _reset_in;
    };

    // What the client's handshake checks of the server's answer (§4.1):
    // 101, Upgrade: websocket, Connection: upgrade, the accept of its key,
    // a subprotocol it offered (or none), an extension it offered; the
    // fault, or none
    struct WsAnswer {
        string subprotocol;
        optional<WsDeflateParams> deflate;
    };

    inline const char* check_ws_answer(int status, const headers& h, std::string_view key, const std::vector<std::string>& offered_protocols, bool offered_deflate,
                                       WsAnswer& out) noexcept {
        if (status != 101) {
            return "the status is not 101";
        }
        if (!ws_has_token(h, "upgrade", "websocket")) {
            return "no Upgrade: websocket";
        }
        if (!ws_has_token(h, "connection", "upgrade")) {
            return "no Connection: upgrade";
        }
        auto accept = HeadersAccess::find(h, "sec-websocket-accept");
        if (!accept || trim_ows(*accept) != ws_accept(key).view()) {
            return "a Sec-WebSocket-Accept not of the key";
        }
        auto protocols = ws_tokens(h, "sec-websocket-protocol");
        if (protocols.size() > 1) {
            return "more than one subprotocol";
        }
        if (protocols.size() == 1) {
            bool found = false;
            for (auto& o : offered_protocols) {
                found |= o == protocols[0];
            }
            if (!found) {
                return "a subprotocol not offered";
            }
            out.subprotocol = string(std::string_view(protocols[0]));
        }
        auto extensions = ws_tokens(h, "sec-websocket-extensions");
        for (auto& e : extensions) {
            WsDeflateParams p;
            if (!offered_deflate || out.deflate || !parse_ws_deflate(e, p)) {
                return "an extension not offered";
            }
            if (p.client_max_window_bits_given && p.client_max_window_bits != 15) {
                return "client_max_window_bits below 15";   // the client's compressor uses 32 KB
            }
            out.deflate = p;
        }
        return nullptr;
    }

    // What the server checks of a client's opening request (§4.2.1): GET,
    // HTTP/1.1, Upgrade: websocket, Connection: upgrade, a key of 16 bytes
    // in base64, version 13. The status to refuse it with (400, or 426
    // for another version) and why, or 0
    inline int check_ws_request(std::string_view method, int minor, const headers& h, const char*& why) noexcept {
        if (method != "GET") {
            why = "not a GET";
            return 405;
        }
        if (minor != 1) {
            why = "not HTTP/1.1";
            return 400;
        }
        if (!ws_has_token(h, "upgrade", "websocket")) {
            why = "no Upgrade: websocket";
            return 400;
        }
        if (!ws_has_token(h, "connection", "upgrade")) {
            why = "no Connection: upgrade";
            return 400;
        }
        auto version = HeadersAccess::find(h, "sec-websocket-version");
        if (!version || trim_ows(*version) != "13") {
            why = "not version 13";
            return 426;
        }
        auto key = HeadersAccess::find(h, "sec-websocket-key");
        if (!key) {
            why = "no Sec-WebSocket-Key";
            return 400;
        }
        auto k = trim_ows(*key);
        auto decoded = encoding::base64::standard.decode(string(k));
        if (!decoded || decoded->size() != 16) {
            why = "a Sec-WebSocket-Key not of 16 bytes";
            return 400;
        }
        return 0;
    }

    // Whether the Origin of a request is one the server takes: none (not
    // a browser), the request's own host, or one of `allowed` (a host, or
    // "*" for every one), as Go's gorilla checks it by default
    inline bool ws_origin_allowed(const headers& h, const std::vector<std::string>& allowed) noexcept {
        auto origin = HeadersAccess::find(h, "origin");
        if (!origin) {
            return true;
        }
        std::string_view o = trim_ows(*origin);
        auto scheme = o.find("://");
        std::string_view host = scheme == std::string_view::npos ? o : o.substr(scheme + 3);
        auto slash = host.find('/');
        if (slash != std::string_view::npos) {
            host = host.substr(0, slash);
        }
        auto own = HeadersAccess::find(h, "host");
        if (own && iequal(trim_ows(*own), host)) {
            return true;
        }
        for (auto& a : allowed) {
            if (a == "*" || iequal(a, host) || iequal(a, o)) {
                return true;
            }
        }
        return false;
    }
}
