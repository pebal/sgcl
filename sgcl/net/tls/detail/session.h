//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "key_share.h"
#include "messages.h"
#include "record.h"
#include "schedule.h"
#include "../../../core/make_tracked.h"
#include "../../../core/string.h"
#include "../../../core/tracked_ptr.h"
#include "../../../core/vector.h"
#include "../../../crypto/gcm.h"
#include "../../../crypto/random.h"
#include "../../../crypto/secure_zero.h"
#include "../../../crypto/x509.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

// Session resumption of TLS 1.3 (RFC 8446 §2.2, §4.2.11, §4.6.1), psk_dhe_ke
// only, no early data: what each side keeps between connections; and the
// client's sessions of TLS 1.2 (RFC 5246 §7.3, RFC 5077), in the same cache.
//
// The client keeps the sessions its servers' NewSessionTickets gave it in a
// SessionCacheState (the state of tls::session_cache): each a Session, the
// ticket as the server sealed it, the resumption PSK made from the
// ticket's nonce, the suite, when it came and for how long it holds, the
// server's chain of the handshake that made it (a 1.2 session: its master
// secret in the PSK's place, its ticket or session id). A session is offered
// once (§C.4: a ticket used again links the connections): take() removes the
// newest fresh one of a server; a server whose tickets the cache holds
// already holds at most PerServer of them, the cache at most its capacity,
// the oldest dropped first. A 1.2 session the server resumed is put back
// after the abbreviated handshake, its new ticket in place of the old when
// the server sent one (RFC 5077 §3.3: a ticket is used until another
// replaces it; a session id names the server's session as long as it
// keeps it), its lifetime counted from the full handshake that made it.
//
// The server keeps nothing of a session: its ticket is the session itself,
// sealed under a key of its own (TicketKeys, the state of
// tls::ticket_keys) with AES-256-GCM: the key's name, a random nonce, then
// the content sealed with the name as its additional data. The content is
// the suite, the time the ticket was issued, its lifetime and age_add, the
// PSK, and the client's chain when it sent one (mTLS: a resumed session is
// as authenticated as the handshake that made it). Two keys are kept, the
// current one and the previous one: the current one seals, either opens,
// and the current one becomes the previous when it is older than the
// tickets' lifetime (at the next seal), so a ticket opens for exactly its
// lifetime and no key opens tickets for more than two.
//
// Secrets: a Session's PSK lives in an unmanaged block of its own, zeroed
// when the session is taken, dropped or collected; the ticket keys in the
// unmanaged TicketKeys, zeroed when they are replaced and when it goes; a
// ticket's content is made and read in unmanaged buffers zeroed after use.
namespace sgcl::net::tls::detail {
    // The largest ticket a client keeps and a server issues: room for a
    // client's chain in it, and the ClientHello that offers it far from its
    // 2^16 bytes of extensions
    inline constexpr size_t MaxTicket = 32768;

    // A session the client may resume: TLS 1.3's (a ticket and its PSK) or
    // TLS 1.2's (the master secret, and a ticket of RFC 5077 or the
    // server's session id; always one of the extended master secret, the
    // only 1.2 handshake the client completes)
    struct Session {
        uint16_t version = Tls13;                    // the version of the handshake that made it
        uint16_t cipher = 0;                         // its suite: 1.3's hash of the PSK, 1.2's suite resumed
        vector<byte> ticket;                         // the server's, opaque (1.2: empty for a session id's session)
        std::unique_ptr<Secret> psk;                 // unmanaged, zeroed when it goes; 1.2: the master secret
        int64_t received_ms = 0;                     // when the ticket came, Unix milliseconds of the client's clock
        uint32_t lifetime = 0;                       // seconds (at most 604800)
        uint32_t age_add = 0;
        uint8_t session_id[32] = {};                 // 1.2: the server's session id (RFC 5246 §7.4.1.3), 0 to 32 bytes
        uint8_t session_id_size = 0;
        uint16_t group = 0;                          // 1.2: the curve of the handshake that made it (a resumed state's)
        crypto::x509::chain peer_certificates;       // the server's, of the handshake that made it

        Session() noexcept = default;
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        // Whether it may be offered at now_ms: its PSK still there, its
        // lifetime not over, no time before it came
        SGCL_INLINE_HOT bool fresh(int64_t now_ms) const noexcept {
            return psk && !psk->empty() && now_ms >= received_ms && now_ms - received_ms < int64_t(lifetime) * 1000;
        }

        SGCL_INLINE_HOT void wipe() noexcept {
            if (psk) {
                psk->wipe();
            }
        }
    };

    // The client's sessions (tls::session_cache), by a key of the server's
    // name, port and protocols offered; safe from many threads
    struct SessionCacheState {
        static constexpr size_t PerServer = 4;

        struct Entry {
            string key;
            tracked_ptr<Session> session;
        };

        SGCL_INLINE_HOT explicit SessionCacheState(size_t c) noexcept
        : capacity(c) {
        }

        // A session the server gave; the oldest of its key dropped past
        // PerServer, the oldest of all past the capacity
        void put(const string& key, const tracked_ptr<Session>& s) noexcept {
            std::lock_guard<std::mutex> g(_lock);
            if (capacity == 0) {
                s->wipe();
                return;
            }
            size_t mine = 0;
            for (auto& e : _entries) {
                mine += e.key == key;
            }
            if (mine >= PerServer) {
                for (size_t i = 0; i < _entries.size(); ++i) {
                    if (_entries[i].key == key) {
                        _drop(i);
                        break;
                    }
                }
            }
            if (_entries.size() >= capacity) {
                _drop(0);
            }
            _entries.push_back(Entry{key, s});
        }

        // The newest fresh session of the key, removed (a ticket is offered
        // once); the stale ones of the key dropped on the way; null for none
        tracked_ptr<Session> take(const string& key, int64_t now_ms) noexcept {
            std::lock_guard<std::mutex> g(_lock);
            tracked_ptr<Session> found;
            for (size_t i = _entries.size(); i-- > 0;) {
                if (_entries[i].key != key) {
                    continue;
                }
                if (!found && _entries[i].session->fresh(now_ms)) {
                    found = _entries[i].session;
                    _entries.erase(_entries.begin() + std::ptrdiff_t(i));
                } else if (!_entries[i].session->fresh(now_ms)) {
                    _drop(i);
                }
            }
            return found;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            std::lock_guard<std::mutex> g(_lock);
            return _entries.size();
        }

        void clear() noexcept {
            std::lock_guard<std::mutex> g(_lock);
            while (!_entries.empty()) {
                _drop(_entries.size() - 1);
            }
        }

        const size_t capacity;

    private:
        SGCL_INLINE_HOT void _drop(size_t i) noexcept {
            _entries[i].session->wipe();
            _entries.erase(_entries.begin() + std::ptrdiff_t(i));
        }

        mutable std::mutex _lock;
        vector<Entry> _entries;   // the oldest first
    };

    // The key of a server in the cache: its name, its port and the
    // protocols offered (a session of one protocol is not resumed for
    // another)
    inline string session_key(const string& server_name, uint16_t port, const vector<string>& alpn) noexcept {
        std::string k(server_name.view());
        k += ':';
        k += std::to_string(port);
        for (const string& p : alpn) {
            k += '\n';
            k += p.view();
        }
        return string(std::string_view(k));
    }

    // --- the server's tickets ------------------------------------------------

    // What a ticket's content holds, read from the opened bytes (views into
    // them)
    struct TicketContent {
        uint16_t cipher = 0;
        int64_t issued_ms = 0;
        uint32_t lifetime = 0;
        uint32_t age_add = 0;
        Bytes psk;
        Bytes certificates;          // the client's chain: entries of a 24-bit length and the DER, the leaf first
    };

    inline constexpr uint8_t TicketVersion = 1;

    // The content: version, suite, issued, lifetime, age_add, PSK<32..48>,
    // certificates<0..2^24-1> (the client's chain, the leaf first)
    inline void write_ticket_content(Builder& w, uint16_t cipher, int64_t issued_ms, uint32_t lifetime, uint32_t age_add, const Secret& psk, const crypto::x509::chain& chain) noexcept {
        w.u8(TicketVersion);
        w.u16(cipher);
        w.u32(uint32_t(uint64_t(issued_ms) >> 32));
        w.u32(uint32_t(uint64_t(issued_ms)));
        w.u32(lifetime);
        w.u32(age_add);
        {
            auto p = w.block8();
            w.bytes(psk.view());
        }
        auto list = w.block24();
        for (const crypto::x509::certificate& c : chain) {
            auto one = w.block24();
            w.bytes(c.raw());
        }
    }

    inline optional<TicketContent> read_ticket_content(const Bytes& plain) noexcept {
        Reader r(plain);
        TicketContent t;
        uint8_t version;
        uint32_t hi, lo;
        if (!r.u8(version) || version != TicketVersion || !r.u16(t.cipher) || !r.u32(hi) || !r.u32(lo) || !r.u32(t.lifetime) || !r.u32(t.age_add)
            || !r.vec8(t.psk, 32, MaxHashSize) || !r.vec24(t.certificates, 0, 0xFFFFFF) || !r.end()) {
            return nullopt;
        }
        t.issued_ms = int64_t(uint64_t(hi) << 32 | lo);
        Reader list(t.certificates);
        while (!list.empty()) {
            Bytes der;
            if (!list.vec24(der, 1, 0xFFFFFF)) {
                return nullopt;
            }
        }
        return t;
    }

    // The keys a server seals its tickets under: unmanaged, zeroed when
    // replaced and when the object goes; safe from many threads
    class TicketKeys {
    public:
        static constexpr size_t NameSize = 8;
        static constexpr size_t KeySize = 32;
        static constexpr size_t Overhead = NameSize + crypto::aes_gcm::nonce_size + crypto::aes_gcm::tag_size;

        // (the first key made by the first seal: a client's config, which
        // never seals, costs no key)
        TicketKeys() noexcept = default;

        // A current key given (the tests' and the fuzzers': tickets that
        // open in another process)
        TicketKeys(const uint8_t* name, const uint8_t* key) noexcept {
            std::memcpy(_current.name, name, NameSize);
            _current.aead.emplace(bytes_of(key, KeySize));
        }

        TicketKeys(const TicketKeys&) = delete;
        TicketKeys& operator=(const TicketKeys&) = delete;

        // A new current key; the current one becomes the previous, and the
        // previous is gone (the tickets sealed under it no longer open)
        void rotate() noexcept {
            std::lock_guard<std::mutex> g(_lock);
            _rotate();
        }

        // The content sealed into a ticket appended to out (Overhead bytes
        // more than the content); the current key replaced first when it
        // is older than lifetime_ms (its first seal stamps its age)
        void seal(std::vector<byte>& out, const Bytes& content, int64_t now_ms, int64_t lifetime_ms, const Entropy& entropy) noexcept {
            std::lock_guard<std::mutex> g(_lock);
            if (!_current.aead) {
                _make(_current);
            }
            if (_current.created_ms < 0) {
                _current.created_ms = now_ms;
            } else if (now_ms - _current.created_ms >= lifetime_ms || now_ms < _current.created_ms) {
                _rotate();
                _current.created_ms = now_ms;
            }
            const size_t at = out.size();
            out.resize(at + Overhead + content.size());
            uint8_t* p = reinterpret_cast<uint8_t*>(out.data()) + at;
            std::memcpy(p, _current.name, NameSize);
            entropy(p + NameSize, crypto::aes_gcm::nonce_size);
            uint8_t* sealed = p + NameSize + crypto::aes_gcm::nonce_size;
            _current.aead->seal_to(room_of(sealed, content.size() + crypto::aes_gcm::tag_size), bytes_of(p + NameSize, crypto::aes_gcm::nonce_size), content,
                                   bytes_of(p, NameSize));
        }

        // A ticket opened into out (ticket.size() - Overhead bytes of room):
        // the content's length, or nullopt for a ticket of no key held or
        // one that does not open (out zeroed then)
        optional<size_t> open(const Bytes& ticket, uint8_t* out) const noexcept {
            if (ticket.size() < Overhead) {
                return nullopt;
            }
            std::lock_guard<std::mutex> g(_lock);
            const Key* k = nullptr;
            for (const Key* c : {&_current, &_previous}) {
                if (c->aead && std::memcmp(c->name, ticket.data(), NameSize) == 0) {
                    k = c;
                }
            }
            if (!k) {
                return nullopt;
            }
            const size_t n = ticket.size() - Overhead;
            auto r = k->aead->open_to(room_of(out, n), Bytes(ticket.data() + NameSize, crypto::aes_gcm::nonce_size),
                                      Bytes(ticket.data() + NameSize + crypto::aes_gcm::nonce_size, n + crypto::aes_gcm::tag_size), Bytes(ticket.data(), NameSize));
            if (!r) {
                return nullopt;
            }
            return *r;
        }

    private:
        struct Key {
            uint8_t name[NameSize] = {};
            optional<crypto::aes_gcm> aead;        // zeroes its key schedule when it goes
            int64_t created_ms = -1;               // stamped by its first seal

            SGCL_INLINE_HOT ~Key() {
                crypto::detail::secure_zero(name, sizeof name);
            }
        };

        static void _make(Key& k) noexcept {
            uint8_t key[KeySize];
            crypto::random::fill(room_of(k.name, NameSize));
            crypto::random::fill(room_of(key, KeySize));
            ZeroingProbe::on_secret(key, KeySize);
            k.aead.emplace(bytes_of(key, KeySize));
            crypto::detail::secure_zero(key, sizeof key);
            k.created_ms = -1;
        }

        void _rotate() noexcept {
            std::memcpy(_previous.name, _current.name, NameSize);
            _previous.aead = std::move(_current.aead);
            _previous.created_ms = _current.created_ms;
            _make(_current);
        }

        mutable std::mutex _lock;
        Key _current, _previous;
    };
}
