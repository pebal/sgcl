//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "ip.h"
#include "socket.h"
#include "../async/coroutine.h"
#include "../async/promise.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../async/timer.h"
#include "../core/aliases.h"
#include "../core/clock.h"
#include "../core/duration.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "../core/string.h"
#include "../crypto/random.h"
#include "../io/error.h"
#include "../time/datetime.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

// SNTP (RFC 4330, the client's part of NTPv4, RFC 5905): one query of a
// server and what its answer says of this machine's clock
namespace sgcl::net::ntp {
    // What a server's answer may be refused for
    enum class errc {
        kiss_of_death = 1,     // stratum 0 with a code: DENY, RSTR (go away), RATE (ask less often); the code in the path
        unsynchronized,        // leap 3 or a stratum past 15: the server's clock is not set
        malformed,             // an answer that is not a server's to this query
    };

    namespace detail {
        class NtpCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "ntp";
            }

            std::string message(int c) const noexcept override {
                switch (static_cast<errc>(c)) {
                    case errc::kiss_of_death: return "kiss-o'-death";
                    case errc::unsynchronized: return "the server is not synchronized";
                    case errc::malformed: return "malformed NTP packet";
                }
                return "unknown ntp error";
            }
        };
    }

    // The category of errc, named "ntp"
    inline const std::error_category& category() noexcept {
        static const detail::NtpCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::net::ntp::errc> : std::true_type {};

namespace sgcl::net::ntp {
    // What a server answered (RFC 5905 §7.3), and what it says of this
    // machine's clock: offset, the server's clock less this one's
    // (θ = ((T2 - T1) + (T3 - T4)) / 2), and delay, the round trip less the
    // server's time (δ = (T4 - T1) - (T3 - T2))
    struct response {
        duration offset;                  // add it to this clock to have the server's
        duration delay;                   // the round trip, the server's own time taken out
        time::datetime time;              // the server's clock when it answered (T3), in UTC
        int stratum = 0;                  // 1: a reference clock of its own (GPS, an atomic clock); 2..15: that many servers away
        int leap = 0;                     // 0 none, 1 a 61-second minute, 2 a 59-second minute at the end of the day
        int version = 0;                  // the server's NTP version
        string reference_id;              // stratum 1: its clock's code ("GPS", "PPS"); above: the upstream server's IPv4 address
        int poll = 0;                     // log2 of the server's poll interval in seconds
        double precision = 0;             // its clock's precision in seconds
        duration root_delay;              // the round trip to its reference clock
        duration root_dispersion;         // the error it allows itself to its reference clock
        time::datetime reference_time;    // when its clock was last set, in UTC
    };

    // How a query is made
    struct options {
        duration timeout = std::chrono::seconds(5);   // the answer waited for at most; zero: none
        async::stop_token stop;                       // the wait ended with ECANCELED
    };

    namespace detail {
        // Seconds from 1900 (NTP's era 0) to 1970
        inline constexpr int64_t NtpUnixOffset = 2208988800;

        inline io::error ntp_error(errc e, const string& op, const string& what = {}) noexcept {
            return io::error(make_error_code(e), op, what);
        }

        // A 64-bit timestamp (32.32 since 1900) as nanoseconds since 1970;
        // the era (2036's rollover) taken as the one nearest now
        inline int64_t ntp_to_unix_nanos(uint64_t ts, int64_t now_nanos) noexcept {
            uint64_t secs = ts >> 32;
            uint64_t frac = ts & 0xFFFFFFFFu;
            int64_t nanos_frac = int64_t((frac * 1000000000ull) >> 32);
            int64_t now_secs = now_nanos / 1000000000 + NtpUnixOffset;   // now on NTP's scale
            int64_t era = now_secs >> 32;
            int64_t best = (era << 32) + int64_t(secs);
            for (int64_t e : {era - 1, era + 1}) {
                int64_t t = (e << 32) + int64_t(secs);
                if ((t > now_secs ? t - now_secs : now_secs - t) < (best > now_secs ? best - now_secs : now_secs - best)) {
                    best = t;
                }
            }
            return (best - NtpUnixOffset) * 1000000000 + nanos_frac;
        }

        inline uint64_t ntp_from_unix_nanos(int64_t nanos) noexcept {
            int64_t secs = nanos / 1000000000 + NtpUnixOffset;
            int64_t rest = nanos % 1000000000;
            if (rest < 0) {
                rest += 1000000000;
                --secs;
            }
            uint64_t frac = (uint64_t(rest) << 32) / 1000000000ull;
            return uint64_t(uint32_t(secs)) << 32 | frac;
        }

        inline uint32_t ntp_u32(const uint8_t* p) noexcept {
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        inline uint64_t ntp_u64(const uint8_t* p) noexcept {
            return uint64_t(ntp_u32(p)) << 32 | ntp_u32(p + 4);
        }

        inline void ntp_put64(uint8_t* p, uint64_t v) noexcept {
            for (int i = 0; i < 8; ++i) {
                p[i] = uint8_t(v >> (56 - 8 * i));
            }
        }

        // A short format (16.16 seconds) as a duration
        inline duration ntp_short(uint32_t v) noexcept {
            return std::chrono::nanoseconds(int64_t((uint64_t(v) * 1000000000ull) >> 16));
        }

        // The client's packet (RFC 4330 §5): LI 0, VN 4, mode 3, the
        // transmit timestamp the server echoes as originate
        inline void ntp_request(uint8_t* p, uint64_t transmit) noexcept {
            for (int i = 0; i < 48; ++i) {
                p[i] = 0;
            }
            p[0] = (0 << 6) | (4 << 3) | 3;
            ntp_put64(p + 40, transmit);
        }

        // A server's answer read (RFC 4330 §5, the checks of §5 and of RFC
        // 5905 §8): the response, given T1 (sent, as transmit), T4 (the
        // answer's arrival) in nanoseconds since 1970
        inline expected<response, io::error> ntp_read(const uint8_t* p, size_t n, uint64_t sent_transmit, int64_t t1, int64_t t4, const string& what) {
            if (n < 48) {
                return unexpected(ntp_error(errc::malformed, "ntp", string("a packet shorter than 48 bytes")));
            }
            int leap = p[0] >> 6;
            int version = (p[0] >> 3) & 7;
            int mode = p[0] & 7;
            int stratum = p[1];
            if (mode != 4 || version < 1 || version > 4) {
                return unexpected(ntp_error(errc::malformed, "ntp", string("not a server's answer")));
            }
            if (ntp_u64(p + 24) != sent_transmit) {
                return unexpected(ntp_error(errc::malformed, "ntp", string("an answer to another query")));   // originate is not our transmit: a stale or a forged packet
            }
            if (stratum == 0) {
                std::string code(reinterpret_cast<const char*>(p + 12), 4);
                while (!code.empty() && code.back() == '\0') {
                    code.pop_back();
                }
                return unexpected(ntp_error(errc::kiss_of_death, "ntp", string(code)));
            }
            uint64_t transmit = ntp_u64(p + 40);
            if (leap == 3 || stratum > 15 || transmit == 0) {
                return unexpected(ntp_error(errc::unsynchronized, "ntp", what));
            }
            int64_t t2 = ntp_to_unix_nanos(ntp_u64(p + 32), t1);
            int64_t t3 = ntp_to_unix_nanos(transmit, t1);
            response r;
            r.offset = std::chrono::nanoseconds(((t2 - t1) + (t3 - t4)) / 2);
            int64_t d = (t4 - t1) - (t3 - t2);
            r.delay = std::chrono::nanoseconds(d < 0 ? 0 : d);
            r.time = time::datetime::from_unix_nano(t3, time::zone::utc());
            r.stratum = stratum;
            r.leap = leap;
            r.version = version;
            r.poll = int(int8_t(p[2]));
            r.precision = 1.0;
            int8_t prec = int8_t(p[3]);
            for (int i = 0; i < (prec < 0 ? -prec : prec); ++i) {
                r.precision = prec < 0 ? r.precision / 2 : r.precision * 2;
            }
            r.root_delay = ntp_short(ntp_u32(p + 4));
            r.root_dispersion = ntp_short(ntp_u32(p + 8));
            if (stratum == 1) {
                std::string code(reinterpret_cast<const char*>(p + 12), 4);
                while (!code.empty() && code.back() == '\0') {
                    code.pop_back();
                }
                r.reference_id = string(code);
            } else {
                r.reference_id = net::ip_address::v4(p[12], p[13], p[14], p[15]).to_string();
            }
            uint64_t ref = ntp_u64(p + 16);
            r.reference_time = time::datetime::from_unix_nano(ref ? ntp_to_unix_nanos(ref, t1) : 0, time::zone::utc());
            return r;
        }

        inline async::task<expected<response, io::error>> ntp_query(string server, options o) noexcept {
            std::string address(server.view());
            auto hp = net::detail::split_host_port(address);
            if (!hp || hp->host.empty()) {
                address += ":123";
            }
            // an address: the socket made here (a UDP connect does not wait); a name: resolved by the task form
            auto hp2 = net::detail::split_host_port(address);
            bool literal = hp2 && net::ip_address::parse(string(hp2->host)).has_value();
            auto u = literal ? net::udp::connect(string(address)) : co_await net::udp::async_connect(string(address));
            if (!u) {
                co_return unexpected(u.error());
            }
            uint8_t packet[48];
            uint8_t in[512];
            // the transmit timestamp: the clock's seconds with random low bits of the fraction, so that an answer
            // cannot be guessed (RFC 5905 §9.1's advice), echoed back as originate
            int64_t t1 = time::detail::now_nanos();
            uint64_t transmit = ntp_from_unix_nanos(t1);
            uint8_t noise[2];
            crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(noise), 2));
            transmit = (transmit & ~uint64_t(0xFFFF)) | uint64_t(noise[0]) << 8 | noise[1];
            ntp_request(packet, transmit);
            if (o.timeout > duration::zero()) {
                u->set_read_deadline(sgcl::clock::now() + o.timeout);
            }
            if (auto w = co_await u->async_send(slice<const byte>(reinterpret_cast<const byte*>(packet), 48)); !w) {
                (void)u->close();
                co_return unexpected(w.error());
            }
            // a stop closes the socket, which ends the wait
            tracked_ptr<async::promise<bool>> done = make_tracked<async::promise<bool>>();
            std::atomic<bool> stopped = {false};
            async::task<> closer;
            if (o.stop.stop_possible()) {
                closer = async::spawn([](net::udp::socket u, async::stop_token stop, tracked_ptr<async::promise<bool>> done, std::atomic<bool>* stopped) -> async::task<> {
                    bool now = false;
                    co_await async::select(stop.on_stop([&] { now = true; }), done->on_done([] {}));
                    if (now) {
                        stopped->store(true);
                        (void)u.close();
                    }
                }(*u, o.stop, done, &stopped));
            }
            auto finish = [&]() -> async::task<> {
                done->set_value(true);
                if (o.stop.stop_possible()) {
                    co_await closer;
                }
            };
            for (;;) {
                expected<size_t, io::error> r = co_await u->async_receive(slice<byte>(reinterpret_cast<byte*>(in), sizeof in));
                int64_t t4 = time::detail::now_nanos();
                if (!r) {
                    (void)u->close();
                    co_await finish();
                    if (stopped.load()) {
                        co_return unexpected(io::error(error_code(ECANCELED, std::system_category()), "ntp", server));
                    }
                    co_return unexpected(r.error());
                }
                auto resp = ntp_read(in, *r, transmit, t1, t4, server);
                if (!resp && resp.error().code() == errc::malformed && resp.error().path().view() == "an answer to another query") {
                    continue;   // a stray datagram: the answer may still come
                }
                (void)u->close();
                co_await finish();
                co_return resp;
            }
        }
    }

    // One query of the server ("host[:123]"): its answer and what it says
    // of this machine's clock. pool.ntp.org when no server is named.
    // errc::kiss_of_death for a server that says to go away or ask less
    // often (its code in the path), errc::unsynchronized for one whose
    // clock is not set, ETIMEDOUT past the timeout.
    //
    //     auto r = net::ntp::query("time.cloudflare.com");
    //     println("off by {} ms", r->offset.milliseconds());
    // `query(...)` on this thread, `co_await async_query(...)` in a task
    inline expected<response, io::error> query(const string& server = string("pool.ntp.org"), const options& o = {}) {
        return detail::ntp_query(server, o).wait();
    }

    inline async::task<expected<response, io::error>> async_query(string server = string("pool.ntp.org"), options o = {}) noexcept {
        return detail::ntp_query(std::move(server), std::move(o));
    }
}
