//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../types.h"
#include "../../../core/aliases.h"
#include "../../../core/array.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"
#include "../../../crypto/ed25519.h"
#include "../../../encoding/base32.h"
#include "../../../encoding/base64.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

// The client protocol of NATS (docs.nats.io, "Client Protocol"): the
// lines, the header block of NATS/1.0, the subjects, the nkeys
namespace sgcl::net::nats::detail {
    // A subject's tokens: not empty, no white space; wildcards ("*" a token,
    // ">" the last) only where a subscription may have them
    inline bool nats_subject_valid(std::string_view s, bool wildcards) noexcept {
        if (s.empty() || s.size() > 65535) {
            return false;
        }
        size_t start = 0;
        for (size_t i = 0; i <= s.size(); ++i) {
            if (i == s.size() || s[i] == '.') {
                std::string_view t = s.substr(start, i - start);
                if (t.empty()) {
                    return false;
                }
                if (t.find_first_of("*>") != std::string_view::npos) {
                    if (!wildcards || t.size() != 1 || (t[0] == '>' && i != s.size())) {
                        return false;
                    }
                }
                start = i + 1;
                continue;
            }
            uint8_t c = uint8_t(s[i]);
            if (c <= ' ' || c == 0x7F) {
                return false;
            }
        }
        return true;
    }

    // Whether a subscription's subject matches a message's
    inline bool nats_match(std::string_view filter, std::string_view subject) noexcept {
        size_t f = 0, s = 0;
        for (;;) {
            size_t fe = filter.find('.', f), se = subject.find('.', s);
            std::string_view ft = filter.substr(f, fe == std::string_view::npos ? std::string_view::npos : fe - f);
            std::string_view st = subject.substr(s, se == std::string_view::npos ? std::string_view::npos : se - s);
            if (ft == ">") {
                return s <= subject.size() && !st.empty();
            }
            if (ft != "*" && ft != st) {
                return false;
            }
            if (fe == std::string_view::npos || se == std::string_view::npos) {
                return fe == std::string_view::npos && se == std::string_view::npos;
            }
            f = fe + 1;
            s = se + 1;
        }
    }

    inline bool nats_number(std::string_view t, uint64_t& out) noexcept {
        if (t.empty() || t.size() > 19) {
            return false;
        }
        auto r = std::from_chars(t.data(), t.data() + t.size(), out);
        return r.ec == std::errc() && r.ptr == t.data() + t.size();
    }

    // A line's words split by spaces and tabs
    inline size_t nats_words(std::string_view line, std::string_view* out, size_t max) noexcept {
        size_t n = 0, i = 0;
        while (i < line.size() && n < max) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                ++i;
            }
            size_t start = i;
            while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
                ++i;
            }
            if (i > start) {
                out[n++] = line.substr(start, i - start);
            }
        }
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        return i < line.size() ? max + 1 : n;   // words left over: more than max
    }

    // The header block of HPUB and HMSG: "NATS/1.0[ status[ text]]\r\n",
    // then "Name: value\r\n" lines, then an empty line
    inline bool nats_read_headers(std::string_view b, message& m) {
        constexpr std::string_view Version = "NATS/1.0";
        if (b.size() < Version.size() + 4 || b.substr(0, Version.size()) != Version || b.substr(b.size() - 4) != "\r\n\r\n") {
            return false;
        }
        size_t eol = b.find("\r\n");
        std::string_view first = b.substr(Version.size(), eol - Version.size());
        while (!first.empty() && first[0] == ' ') {
            first.remove_prefix(1);
        }
        if (!first.empty()) {
            size_t sp = first.find(' ');
            uint64_t status = 0;
            if (!nats_number(first.substr(0, sp), status) || status > 999) {
                return false;
            }
            m.status = int(status);
            if (sp != std::string_view::npos) {
                m.description = string(first.substr(sp + 1));
            }
        }
        size_t at = eol + 2;
        while (at < b.size() - 2) {
            size_t e = b.find("\r\n", at);
            std::string_view line = b.substr(at, e - at);
            size_t colon = line.find(':');
            if (colon == std::string_view::npos || colon == 0) {
                return false;
            }
            std::string_view value = line.substr(colon + 1);
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                value.remove_prefix(1);
            }
            m.headers.push_back({string(line.substr(0, colon)), string(value)});
            at = e + 2;
        }
        return true;
    }

    inline void nats_write_headers(std::string& out, const message& m) {
        out += "NATS/1.0";
        if (m.status) {
            out += ' ';
            out += std::to_string(m.status);
            if (!m.description.empty()) {
                out += ' ';
                out.append(m.description.view());
            }
        }
        out += "\r\n";
        for (auto& [n, v] : m.headers) {
            out.append(n.view());
            out += ": ";
            out.append(v.view());
            out += "\r\n";
        }
        out += "\r\n";
    }

    // A header's name or value fit to be sent: no CR or LF, a name without ':'
    inline bool nats_header_valid(const message& m) noexcept {
        for (auto& [n, v] : m.headers) {
            if (n.empty() || n.view().find_first_of(":\r\n") != std::string_view::npos || v.view().find_first_of("\r\n") != std::string_view::npos) {
                return false;
            }
        }
        return m.description.view().find_first_of("\r\n") == std::string_view::npos;
    }

    // PUB or HPUB of a message
    inline void nats_write_pub(std::string& out, const message& m) {
        bool headers = !m.headers.empty() || m.status;
        if (!headers) {
            out += "PUB ";
            out.append(m.subject.view());
            if (!m.reply.empty()) {
                out += ' ';
                out.append(m.reply.view());
            }
            out += ' ';
            out += std::to_string(m.data.size());
            out += "\r\n";
        } else {
            std::string block;
            nats_write_headers(block, m);
            out += "HPUB ";
            out.append(m.subject.view());
            if (!m.reply.empty()) {
                out += ' ';
                out.append(m.reply.view());
            }
            out += ' ';
            out += std::to_string(block.size());
            out += ' ';
            out += std::to_string(block.size() + m.data.size());
            out += "\r\n";
            out += block;
        }
        out.append(m.data.view());
        out += "\r\n";
    }

    // One unit of what a server sends: a message (MSG, HMSG), PING, PONG,
    // +OK, -ERR with its text, INFO with its JSON
    struct NatsFrame {
        enum class kind : uint8_t { msg, ping, pong, ok, err, info } k = kind::ok;
        uint64_t sid = 0;
        message m;
        std::string_view text;   // -ERR's text, INFO's JSON: a view of the buffer
    };

    inline constexpr size_t NatsLineMax = 64 * 1024 + 256;

    // The unit at the buffer's front: the bytes it took; 0 when it is not
    // whole yet; -1 for bytes that break the protocol (why says how)
    inline long nats_parse(std::string_view b, size_t max_payload, NatsFrame& f, const char*& why) {
        size_t eol = b.find("\r\n");
        if (eol == std::string_view::npos) {
            if (b.size() > NatsLineMax) {
                why = "a line past its limit";
                return -1;
            }
            return 0;
        }
        std::string_view line = b.substr(0, eol);
        auto is = [&](std::string_view w) {
            if (line.size() < w.size()) {
                return false;
            }
            for (size_t i = 0; i < w.size(); ++i) {
                if ((line[i] | 0x20) != (w[i] | 0x20)) {
                    return false;
                }
            }
            return line.size() == w.size() || line[w.size()] == ' ' || line[w.size()] == '\t';
        };
        bool msg = is("MSG"), hmsg = !msg && is("HMSG");
        if (msg || hmsg) {
            std::string_view words[6];
            size_t n = nats_words(line, words, 6);
            uint64_t total = 0, sid = 0, hlen = 0;
            bool ok = msg ? (n == 4 || n == 5) : (n == 5 || n == 6);
            if (!ok || !nats_number(words[n - 1], total) || !nats_number(words[2], sid) || total > max_payload + 65536) {
                why = "a MSG line that does not read";
                return -1;
            }
            if (hmsg && (!nats_number(words[n - 2], hlen) || hlen > total)) {
                why = "an HMSG line that does not read";
                return -1;
            }
            size_t want = eol + 2 + size_t(total) + 2;
            if (b.size() < want) {
                return 0;
            }
            if (b.substr(want - 2, 2) != "\r\n") {
                why = "a payload without its CRLF";
                return -1;
            }
            std::string_view payload = b.substr(eol + 2, size_t(total));
            f = NatsFrame();
            f.k = NatsFrame::kind::msg;
            f.sid = sid;
            f.m.subject = string(words[1]);
            if (msg ? n == 5 : n == 6) {
                f.m.reply = string(words[3]);
            }
            if (hmsg) {
                if (!nats_read_headers(payload.substr(0, size_t(hlen)), f.m)) {
                    why = "an HMSG header block that does not read";
                    return -1;
                }
                f.m.data = string(payload.substr(size_t(hlen)));
            } else {
                f.m.data = string(payload);
            }
            return long(want);
        }
        f = NatsFrame();
        if (is("PING")) {
            f.k = NatsFrame::kind::ping;
        } else if (is("PONG")) {
            f.k = NatsFrame::kind::pong;
        } else if (is("+OK")) {
            f.k = NatsFrame::kind::ok;
        } else if (is("-ERR")) {
            f.k = NatsFrame::kind::err;
            f.text = line.substr(line.size() > 4 ? 5 : 4);
        } else if (is("INFO")) {
            f.k = NatsFrame::kind::info;
            f.text = line.substr(line.size() > 4 ? 5 : 4);
        } else {
            why = "an unknown line";
            return -1;
        }
        return long(eol + 2);
    }

    // nkeys (NATS's Ed25519 keys as text): base32 of a prefix byte, the 32
    // bytes and a CRC-16 (XMODEM) of them, little-endian
    inline uint16_t nats_crc16(const uint8_t* p, size_t n) noexcept {
        uint16_t crc = 0;
        for (size_t i = 0; i < n; ++i) {
            crc ^= uint16_t(p[i]) << 8;
            for (int b = 0; b < 8; ++b) {
                crc = uint16_t(crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1);
            }
        }
        return crc;
    }

    inline constexpr uint8_t NkeySeedPrefix = 18 << 3;   // 'S'
    inline constexpr uint8_t NkeyUserPrefix = 20 << 3;   // 'U'

    inline constexpr encoding::base32 NkeyBase32 = encoding::base32::standard.without_padding();

    // A user seed ("SU...") read: its 32 bytes of Ed25519 seed; false for
    // a seed of another kind or a CRC that does not match
    inline bool nats_seed(std::string_view text, array<byte, 32>& seed, uint8_t& kind) {
        auto raw = NkeyBase32.decode(string(text));
        if (!raw || raw->size() != 36) {
            return false;
        }
        auto b = reinterpret_cast<const uint8_t*>(raw->data());
        uint16_t crc = uint16_t(b[34] | b[35] << 8);
        if (crc != nats_crc16(b, 34) || (b[0] & 0xF8) != NkeySeedPrefix) {
            return false;
        }
        kind = uint8_t((b[0] & 7) << 5 | (b[1] & 0xF8) >> 3);
        for (size_t i = 0; i < 32; ++i) {
            seed[i] = byte(b[2 + i]);
        }
        return true;
    }

    // A public key of the kind as text: "U..." for a user
    inline string nats_public_key(uint8_t kind, const slice<const byte>& key) {
        uint8_t raw[35];
        raw[0] = kind;
        for (size_t i = 0; i < 32; ++i) {
            raw[1 + i] = uint8_t(key[i]);
        }
        uint16_t crc = nats_crc16(raw, 33);
        raw[33] = uint8_t(crc);
        raw[34] = uint8_t(crc >> 8);
        return NkeyBase32.encode(slice<const byte>(reinterpret_cast<const byte*>(raw), 35));
    }

    // The nonce of INFO signed with the seed: the public key and the
    // signature (base64url without padding), as CONNECT sends them
    inline bool nats_sign_nonce(std::string_view seed_text, std::string_view nonce, string& public_key, string& signature) {
        array<byte, 32> seed;
        uint8_t kind = 0;
        if (!nats_seed(seed_text, seed, kind)) {
            return false;
        }
        auto key = crypto::ed25519::private_key::from_seed(slice<const byte>(seed.data(), seed.size()));
        if (!key) {
            return false;
        }
        auto pk = key->public_key();   // the bytes are a view of it: it lives until they are read
        auto pub = pk.bytes();
        public_key = nats_public_key(kind, slice<const byte>(pub.data(), pub.size()));
        auto sig = key->sign(slice<const byte>(reinterpret_cast<const byte*>(nonce.data()), nonce.size()));
        signature = encoding::base64::raw_url.encode(slice<const byte>(sig.data(), sig.size()));
        return true;
    }
}
