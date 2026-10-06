//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../io/error.h"
#include "../io/os.h"
#include "../io/path.h"
#include "../io/stream.h"
#include "../time/datetime.h"
#include "detail/render.h"
#include "detail/text.h"
#include "level.h"
#include "record.h"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string_view>
#include <system_error>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace sgcl::slog {
    // syslog (RFC 5424, and RFC 3164 for the local daemons) and systemd's
    // journal as handlers of slog: a record is one message, its message
    // and slog's own text of its attributes after it, or the attributes
    // as RFC 5424's structured data, or as the journal's fields. The local
    // sockets are opened here with the system's calls; a remote server is
    // a connection of the program's given as an io::writer (a
    // net::connection is one: slog stands below net, whose HTTP server
    // logs through it).
    namespace detail {
        // syslog::facility, syslog::format and syslog::options, outside the
        // class: a default member initializer of a nested struct is not
        // usable in the enclosing class's default arguments
        enum class SyslogFacility : uint8_t {
            kern = 0,
            user = 1,
            mail = 2,
            daemon = 3,
            auth = 4,
            syslog = 5,
            lpr = 6,
            news = 7,
            uucp = 8,
            cron = 9,
            authpriv = 10,
            ftp = 11,
            local0 = 16,
            local1 = 17,
            local2 = 18,
            local3 = 19,
            local4 = 20,
            local5 = 21,
            local6 = 22,
            local7 = 23
        };

        enum class SyslogFormat : uint8_t {
            automatic,   // RFC 3164 for the local daemons (local), RFC 5424 for a writer
            rfc5424,     // <PRI>1 TIMESTAMP HOSTNAME APP-NAME PROCID MSGID SD MSG
            rfc3164      // <PRI>Mmm dd hh:mm:ss HOSTNAME TAG[PID]: MSG
        };

        struct SyslogOptions {
            SyslogFacility facility = SyslogFacility::user;
            string app_name;                        // empty: the program's name, the base of args()[0]
            string hostname;                        // empty: the system's; "-" when it has none
            SyslogFormat format = SyslogFormat::automatic;
            bool octet_counting = false;            // "LEN SP MSG" framing for a stream (RFC 6587, RFC 5425: TCP, TLS)
            string structured_data_id;              // non-empty: the attributes as SD-PARAMs under this SD-ID (RFC 5424 only)
        };
    }

    namespace detail {
        // The severity of a level: error and up 3, warn 4, info 6, below 7;
        // a level between named ones as the named one at or below it
        SGCL_INLINE_HOT int syslog_severity(slog::level l) noexcept {
            int v = int(l);
            return v >= int(slog::level::error) ? 3 : v >= int(slog::level::warn) ? 4 : v >= int(slog::level::info) ? 6 : 7;
        }

        // A datagram socket of the local machine, connected to the first
        // of the paths that takes it; -1 and the last error when none does
        inline int unix_datagram(const char* const* paths, size_t n, const char*& used) noexcept {
            int last = ENOENT;
            for (size_t i = 0; i < n; ++i) {
                int fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
                if (fd < 0) {
                    return -1;
                }
                sockaddr_un a{};
                a.sun_family = AF_UNIX;
                std::string_view p(paths[i]);
                if (p.size() >= sizeof a.sun_path) {
                    ::close(fd);
                    continue;
                }
                for (size_t k = 0; k < p.size(); ++k) {
                    a.sun_path[k] = p[k];
                }
                if (::connect(fd, reinterpret_cast<const sockaddr*>(&a), socklen_t(sizeof a)) == 0) {
                    used = paths[i];
                    return fd;
                }
                last = errno;
                ::close(fd);
            }
            errno = last;
            return -1;
        }

        // The state of a handler: the descriptor (local) or the writer
        // (remote, under a lock), the options, what failed
        struct SyslogState {
            int fd = -1;
            io::writer out;
            std::mutex out_lock;
            SyslogOptions o;   // the format resolved: never automatic
            string app;
            string host;
            int pid = 0;
            std::atomic<uint64_t> dropped{0};

            ~SyslogState() {
                if (fd >= 0) {
                    ::close(fd);
                }
            }
        };

        // An SD-PARAM's value escaped (RFC 5424 6.3.3): " \ ] after a backslash
        inline void sd_escaped(Buf& b, std::string_view v) noexcept {
            for (char c : v) {
                if (c == '"' || c == '\\' || c == ']') {
                    b.put('\\');
                }
                b.put(c);
            }
        }

        // An SD-NAME: printable US-ASCII but = space ] ", at most 32
        // characters; others written as _
        inline void sd_name(Buf& b, std::string_view prefix, std::string_view key) noexcept {
            size_t n = 0;
            auto put = [&](char c) {
                if (n++ < 32) {
                    b.put(c > 32 && c < 127 && c != '=' && c != ']' && c != '"' ? c : '_');
                }
            };
            for (char c : prefix) {
                put(c);
            }
            for (char c : key) {
                put(c);
            }
        }

        template<class Range>
        void sd_attrs(Buf& b, const Range& as, const std::string& prefix) {
            for (const slog::attr& a : as) {
                std::string_view key(a.key().data(), a.key().size());
                slog::value v = a.value();
                if (v.type() == slog::value::kind::group) {
                    sd_attrs(b, v.as_group(), key.empty() ? prefix : prefix + std::string(key) + ".");
                    continue;
                }
                b.put(' ');
                sd_name(b, prefix, key);
                b.put('=');
                b.put('"');
                string text = v.text();
                sd_escaped(b, std::string_view(text.data(), text.size()));
                b.put('"');
            }
        }

        // The message of a record: the header of the format, the message,
        // slog's text of the attributes (or the structured data)
        inline void syslog_message(Buf& b, Lines& w, const SyslogState& s, const record& r) {
            const RecordData& d = Access::data(r);
            int pri = int(s.o.facility) * 8 + syslog_severity(d.lvl);
            char head[96];
            time::datetime t = r.time();
            bool sd = !s.o.structured_data_id.empty() && s.o.format == SyslogFormat::rfc5424;
            if (s.o.format == SyslogFormat::rfc5424) {
                int off = int(t.offset().nanoseconds() / 60'000'000'000);
                char zone[8];
                if (off == 0 && d.utc) {
                    std::snprintf(zone, sizeof zone, "Z");
                } else {
                    std::snprintf(zone, sizeof zone, "%c%02d:%02d", off < 0 ? '-' : '+', (off < 0 ? -off : off) / 60, (off < 0 ? -off : off) % 60);
                }
                int n = std::snprintf(head, sizeof head, "<%d>1 %04d-%02d-%02dT%02d:%02d:%02d.%06d%s ", pri, t.year(), int(t.month()), t.day(), t.hour(), t.minute(),
                                      t.second(), t.nanosecond() / 1000, zone);
                b.put(head, size_t(n));
                b.put(s.host.data(), s.host.size());
                b.put(' ');
                b.put(s.app.data(), s.app.size());
                n = std::snprintf(head, sizeof head, " %d - ", s.pid);
                b.put(head, size_t(n));
                if (sd) {
                    b.put('[');
                    b.put(s.o.structured_data_id.data(), s.o.structured_data_id.size());
                    sd_attrs(b, r, std::string());
                    b.put(']');
                } else {
                    b.put('-');
                }
                b.put(' ');
            } else {
                static constexpr const char* Months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                int n = std::snprintf(head, sizeof head, "<%d>%s %2d %02d:%02d:%02d ", pri, Months[int(t.month()) - 1], t.day(), t.hour(), t.minute(), t.second());
                b.put(head, size_t(n));
                if (s.fd < 0) {   // a remote server: the hostname; the local daemons add theirs
                    b.put(s.host.data(), s.host.size());
                    b.put(' ');
                }
                b.put(s.app.data(), s.app.size());
                n = std::snprintf(head, sizeof head, "[%d]: ", s.pid);
                b.put(head, size_t(n));
            }
            b.put(d.msg, d.msg_n);
            if (!sd) {
                w.line.clear();
                w.prefix.clear();
                if (d.n) {
                    text_attrs(w, d.attrs, d.n, d.tail);
                } else {
                    text_attrs(w, d.tail.a, d.tail.n);
                }
                b.put(w.line.data(), w.line.size());
            }
        }

        struct SyslogAccess;
    }

    class syslog {
    public:
        using facility = detail::SyslogFacility;
        using format = detail::SyslogFormat;
        using options = detail::SyslogOptions;

        // The system's syslog: a datagram to /dev/log (Linux), then
        // /var/run/syslog (macOS) and /var/run/log (BSD); format::automatic
        // is RFC 3164 here, the format those daemons read (Go's log/syslog
        // writes it locally too), whatever else the options say
        static expected<syslog, io::error> local(const options& o = {}) noexcept {
            static constexpr const char* Paths[] = {"/dev/log", "/var/run/syslog", "/var/run/log"};
            return _local(Paths, 3, o);
        }

        // A remote server over a connection of the program's (a TCP or TLS
        // connection: octet_counting; a connected UDP socket: a datagram a
        // write), one message a write, under a lock of the handler's;
        // format::automatic is RFC 5424 here
        explicit syslog(const io::writer& out, const options& o = {})
        : _s(_state(o, format::rfc5424)) {
            _s->out = out;
        }

        // The record as one message; a write that fails counted in dropped()
        void handle(const record& r) const {
            thread_local detail::Lines w;
            thread_local detail::Buf b;
            b.clear();
            detail::syslog_message(b, w, *_s, r);
            if (_s->o.octet_counting) {
                char len[24];
                int n = std::snprintf(len, sizeof len, "%zu ", b.size());
                detail::Buf framed;
                framed.put(len, size_t(n));
                framed.put(b.data(), b.size());
                _send(framed);
            } else {
                _send(b);
            }
        }

        // The messages whose write failed
        SGCL_INLINE_HOT uint64_t dropped() const noexcept {
            return _s->dropped.load(std::memory_order_relaxed);
        }

        SGCL_INLINE_HOT friend bool operator==(const syslog& a, const syslog& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::SyslogAccess;

        // The datagram socket at the first of the paths that takes it
        static expected<syslog, io::error> _local(const char* const* paths, size_t n, const options& o) noexcept {
            const char* used = nullptr;
            int fd = detail::unix_datagram(paths, n, used);
            if (fd < 0) {
                return unexpected(io::error(error_code(errno, std::generic_category()), "syslog", string(paths[0])));
            }
            tracked_ptr<detail::SyslogState> s = _state(o, format::rfc3164);
            s->fd = fd;
            return syslog(std::move(s));
        }

        SGCL_INLINE_HOT explicit syslog(tracked_ptr<detail::SyslogState> s) noexcept
        : _s(std::move(s)) {
        }

        // The state, format::automatic resolved to the transport's own
        static tracked_ptr<detail::SyslogState> _state(const options& o, format automatic_as) {
            tracked_ptr<detail::SyslogState> s = make_tracked<detail::SyslogState>();
            s->o = o;
            if (o.format == format::automatic) {
                s->o.format = automatic_as;
            }
            s->pid = io::pid();
            if (!o.app_name.empty()) {
                s->app = o.app_name;
            } else {
                auto a = io::args();
                s->app = a.empty() ? string("sgcl") : io::path::base(a[0]);
            }
            if (!o.hostname.empty()) {
                s->host = o.hostname;
            } else {
                auto h = io::hostname();
                s->host = h && !h->empty() ? *h : string("-");
            }
            return s;
        }

        void _send(const detail::Buf& b) const {
            if (_s->fd >= 0) {
                if (::send(_s->fd, b.data(), b.size(), 0) < 0) {
                    _s->dropped.fetch_add(1, std::memory_order_relaxed);
                }
                return;
            }
            std::lock_guard g(_s->out_lock);
            auto r = _s->out.write(slice<const byte>(reinterpret_cast<const byte*>(b.data()), b.size()));
            if (!r || *r != b.size()) {
                _s->dropped.fetch_add(1, std::memory_order_relaxed);
            }
        }

        tracked_ptr<detail::SyslogState> _s;
    };

    namespace detail {
        // A journal field's name: upper-case letters, digits and _, not
        // starting with _ or a digit, at most 64 characters; a key's other
        // characters as _, a lower-case letter up
        inline void journal_name(Buf& b, std::string_view prefix, std::string_view key) noexcept {
            size_t n = 0;
            auto put = [&](char c) {
                if (n >= 64) {
                    return;
                }
                if (c >= 'a' && c <= 'z') {
                    c = char(c - 32);
                } else if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
                    c = '_';
                }
                if (n == 0 && (c == '_' || (c >= '0' && c <= '9'))) {
                    b.put('X');   // a name may not start so: X_ before it
                    ++n;
                }
                b.put(c);
                ++n;
            };
            for (char c : prefix) {
                put(c);
            }
            for (char c : key) {
                put(c);
            }
        }

        // One field: KEY=value and a newline, or, for a value with a
        // newline in it, the binary form: KEY, newline, the length in 8
        // bytes little-endian, the value, newline
        inline void journal_field(Buf& b, std::string_view name_prefix, std::string_view name, std::string_view value) noexcept {
            journal_name(b, name_prefix, name);
            if (value.find('\n') == std::string_view::npos) {
                b.put('=');
                b.put(value.data(), value.size());
            } else {
                b.put('\n');
                uint64_t n = value.size();
                for (int i = 0; i < 8; ++i) {
                    b.put(char(n >> (8 * i)));
                }
                b.put(value.data(), value.size());
            }
            b.put('\n');
        }

        template<class Range>
        void journal_attrs(Buf& b, const Range& as, const std::string& prefix) {
            for (const slog::attr& a : as) {
                std::string_view key(a.key().data(), a.key().size());
                slog::value v = a.value();
                if (v.type() == slog::value::kind::group) {
                    journal_attrs(b, v.as_group(), key.empty() ? prefix : prefix + std::string(key) + "_");
                    continue;
                }
                string text = v.text();
                journal_field(b, prefix, key, std::string_view(text.data(), text.size()));
            }
        }

        // The datagram of a record in the journal's native protocol
        inline void journal_message(Buf& b, const string& identifier, int pid, const record& r) {
            const RecordData& d = Access::data(r);
            char num[32];
            journal_field(b, {}, "MESSAGE", std::string_view(d.msg, d.msg_n));
            int n = std::snprintf(num, sizeof num, "%d", syslog_severity(d.lvl));
            journal_field(b, {}, "PRIORITY", std::string_view(num, size_t(n)));
            journal_field(b, {}, "SYSLOG_IDENTIFIER", std::string_view(identifier.data(), identifier.size()));
            n = std::snprintf(num, sizeof num, "%d", pid);
            journal_field(b, {}, "SYSLOG_PID", std::string_view(num, size_t(n)));
            if (d.has_source) {
                journal_field(b, {}, "CODE_FILE", d.where.file_name());
                n = std::snprintf(num, sizeof num, "%u", unsigned(d.where.line()));
                journal_field(b, {}, "CODE_LINE", std::string_view(num, size_t(n)));
                journal_field(b, {}, "CODE_FUNC", d.where.function_name());
            }
            journal_attrs(b, r, std::string());
        }

        struct JournalState {
            int fd = -1;
            string identifier;
            int pid = 0;
            std::atomic<uint64_t> dropped{0};

            ~JournalState() {
                if (fd >= 0) {
                    ::close(fd);
                }
            }
        };
    }

    // systemd's journal in its native protocol: a datagram of fields to
    // /run/systemd/journal/socket — MESSAGE, PRIORITY, SYSLOG_IDENTIFIER,
    // SYSLOG_PID, CODE_FILE, CODE_LINE and CODE_FUNC with options::source,
    // and the attributes as fields, their names upper-cased (a group's
    // name and _ before its keys). Linux only: elsewhere open() finds no
    // socket. A record past the datagram's limit (which journald takes
    // through a memfd) is dropped and counted
    class journald {
    public:
        static expected<journald, io::error> open(const string& identifier = {}) noexcept {
            return _open("/run/systemd/journal/socket", identifier);
        }

        void handle(const record& r) const {
            thread_local detail::Buf b;
            b.clear();
            detail::journal_message(b, _s->identifier, _s->pid, r);
            if (::send(_s->fd, b.data(), b.size(), 0) < 0) {
                _s->dropped.fetch_add(1, std::memory_order_relaxed);
            }
        }

        SGCL_INLINE_HOT uint64_t dropped() const noexcept {
            return _s->dropped.load(std::memory_order_relaxed);
        }

        SGCL_INLINE_HOT friend bool operator==(const journald& a, const journald& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::SyslogAccess;

        static expected<journald, io::error> _open(const char* path, const string& identifier) noexcept {
            const char* used = nullptr;
            int fd = detail::unix_datagram(&path, 1, used);
            if (fd < 0) {
                return unexpected(io::error(error_code(errno, std::generic_category()), "journald", string(path)));
            }
            tracked_ptr<detail::JournalState> s = make_tracked<detail::JournalState>();
            s->fd = fd;
            s->pid = io::pid();
            if (!identifier.empty()) {
                s->identifier = identifier;
            } else {
                auto a = io::args();
                s->identifier = a.empty() ? string("sgcl") : io::path::base(a[0]);
            }
            return journald(std::move(s));
        }

        SGCL_INLINE_HOT explicit journald(tracked_ptr<detail::JournalState> s) noexcept
        : _s(std::move(s)) {
        }

        tracked_ptr<detail::JournalState> _s;
    };

    namespace detail {
        // The local handlers at a socket of the program's choosing: for the
        // tests, which stand in for the daemons
        struct SyslogAccess {
            static expected<syslog, io::error> at(const char* path, const syslog::options& o) noexcept {
                return syslog::_local(&path, 1, o);
            }

            static expected<journald, io::error> journal_at(const char* path, const string& identifier) noexcept {
                return journald::_open(path, identifier);
            }
        };
    }
}
