//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../async/select.h"
#include "../async/signal.h"
#include "../async/stop_token.h"
#include "../compress/gzip.h"
#include "../core/aliases.h"
#include "../core/atomic.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../core/weak_ptr.h"
#include "../io/error.h"
#include "../io/file.h"
#include "../io/fs.h"
#include "../io/mixin/writer.h"
#include "../io/path.h"
#include "../time/cron.h"
#include "../time/datetime.h"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <string>
#include <cstdint>
#include <limits>
#include <string_view>

namespace sgcl::slog {
    // When and how a rotating_file rotates: a plain value of fields (it
    // holds a cron, whose zone and text are tracked words)
    struct rotation {
        uint64_t max_size = uint64_t(100) << 20;   // bytes in the file before a write that would pass them rotates it; 0: no limit
        optional<time::cron> at;                   // rotated at these times too, in the cron's zone: time::cron("@daily")
        size_t keep = 7;                           // rotated files kept, the oldest removed; 0: all
        bool compress = false;                     // a rotated file gzipped beside itself (name.log.gz), on the blocking pool
        bool reopen_on_sighup = false;             // the path opened again at SIGHUP, after an outside tool moved the file
    };

    class rotating_file;

    // A log file that rotates itself: an io writer for slog::options::out
    // (or any io::writer), a handle of one word. A write is one write(2)
    // to the file opened for appending, from the calling thread, no lock:
    // slog's "one record, one write". A write that would take the file
    // past max_size, or that comes after the cron's next time, rotates it
    // first: one thread wins a flag, renames the file to
    // name-YYYY-MM-DDTHH-MM-SS.mmm.ext (the local time; -1, -2 after it
    // for a name taken), opens the path anew and publishes it; the writes
    // of other threads in that moment land in the file renamed, through
    // the descriptor they hold, and none is lost. The old descriptor is
    // closed by the collector once no write holds it. The gzip of the
    // rotated file and the removal of those past `keep` run on the
    // blocking pool, so the thread that logs pays a rename and an open.
    namespace detail {
        struct RotatingState {
            string path;
            string stem;                 // the name without its extension, the directory before it
            string ext;                  // ".log", or empty
            rotation r;
            atomic<io::file> current;
            std::atomic<uint64_t> size{0};
            std::atomic<int64_t> next{std::numeric_limits<int64_t>::max()};   // the cron's next time, ns since 1970
            std::atomic<bool> rotating{false};
            std::atomic<bool> closed{false};
            std::atomic<bool> maintaining{false};   // a job of maintain() queued or running
            std::atomic<bool> dirty{false};         // a rotation since the job last looked
            async::stop_source watch;   // the SIGHUP watch's stop

            // The rotated files of this log in the directory, oldest first:
            // stem-<23 characters of time>[-n]ext[.gz]
            vector<string> rotated() const noexcept {
                vector<string> out;
                string dir = io::path::dir(path);
                string prefix = io::path::base(stem) + string("-");
                auto entries = io::read_dir(dir);
                if (!entries) {
                    return out;
                }
                for (auto& e : *entries) {
                    std::string_view n(e.name.data(), e.name.size());
                    std::string_view p(prefix.data(), prefix.size());
                    std::string_view x(ext.data(), ext.size());
                    if (n.size() < p.size() + 23 || n.substr(0, p.size()) != p) {
                        continue;
                    }
                    std::string_view rest = n.substr(p.size());
                    if (rest.size() >= 3 && rest.substr(rest.size() - 3) == ".gz") {
                        rest.remove_suffix(3);
                    }
                    if (rest.size() < 23 + x.size() || rest.substr(rest.size() - x.size()) != x) {
                        continue;
                    }
                    rest.remove_suffix(x.size());
                    if (rest[4] != '-' || rest[10] != 'T' || rest[19] != '.') {
                        continue;
                    }
                    out.push_back(e.path);
                }
                // by the time, then by the number after it ("-1" after none,
                // "-10" after "-9"): a name taken in one millisecond
                const size_t at = prefix.size();   // in the name, past stem-
                auto key = [at](const string& f) {
                    string base = io::path::base(f);
                    std::string_view n(base.data(), base.size());
                    std::string_view stamp = n.substr(at, 23);
                    uint64_t k = 0;
                    if (n.size() > at + 23 && n[at + 23] == '-') {
                        for (size_t i = at + 24; i < n.size() && n[i] >= '0' && n[i] <= '9'; ++i) {
                            k = k * 10 + uint64_t(n[i] - '0');
                        }
                    }
                    return pair<std::string, uint64_t>(std::string(stamp), k);
                };
                std::sort(out.begin(), out.end(), [&](const string& a, const string& b) {
                    return key(a) < key(b);
                });
                return out;
            }

            // The work after rotations, off the logging thread, one job at a
            // time: every rotated file not yet gzipped compressed (when the
            // rotation asks for it), then those past `keep` removed; again
            // while rotations came meanwhile
            void maintain() noexcept {
                for (;;) {
                    dirty.store(false, std::memory_order_release);
                    if (r.compress) {
                        for (auto& f : rotated()) {
                            std::string_view n(f.data(), f.size());
                            if (n.size() < 3 || n.substr(n.size() - 3) != ".gz") {
                                (void)compress::gzip::compress_file(f, {.keep = false});
                            }
                        }
                    }
                    prune();
                    if (dirty.load(std::memory_order_acquire)) {
                        continue;
                    }
                    maintaining.store(false, std::memory_order_release);
                    if (!dirty.load(std::memory_order_acquire) || maintaining.exchange(true, std::memory_order_acq_rel)) {
                        return;
                    }
                }
            }

            // The rotated files past `keep` removed, oldest first
            void prune() const noexcept {
                if (r.keep == 0) {
                    return;
                }
                vector<string> files = rotated();
                for (size_t i = 0; i + r.keep < files.size(); ++i) {
                    (void)io::remove(files[i]);
                }
            }

            int64_t next_time(int64_t now_ns) const noexcept {
                if (!r.at) {
                    return std::numeric_limits<int64_t>::max();
                }
                auto t = r.at->next(time::datetime::from_unix_nano(now_ns, r.at->zone()));
                return t ? t->unix_nano() : std::numeric_limits<int64_t>::max();
            }

            static expected<io::file, io::error> open_file(const string& path) noexcept {
                return io::open(path, io::open_flags::write | io::open_flags::create | io::open_flags::append, io::permissions(0644));
            }

            // The rotation, by the one thread that wins the flag, of the
            // file it saw (another rotation may have done it meanwhile)
            expected<void, io::error> rotate(const io::file& seen, bool forced) {
                if (rotating.exchange(true, std::memory_order_acquire)) {
                    return {};
                }
                struct Release {
                    std::atomic<bool>& flag;
                    ~Release() {
                        flag.store(false, std::memory_order_release);
                    }
                } release{rotating};
                if (closed.load(std::memory_order_acquire) || (!forced && current.load() != seen)) {
                    return {};
                }
                time::datetime now = time::now();
                char buf[40];
                std::snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d-%02d-%02d.%03d", now.year(), int(now.month()), now.day(), now.hour(), now.minute(), now.second(), now.nanosecond() / 1000000);
                string stamp(buf);
                string target = stem + string("-") + stamp + ext;
                for (int n = 1; io::exists(target) || io::exists(target + string(".gz")); ++n) {
                    target = stem + string("-") + stamp + string("-") + string(std::to_string(n)) + ext;
                }
                if (auto e = io::rename(path, target); !e) {
                    return unexpected(e.error());
                }
                auto f = open_file(path);
                if (!f) {
                    return unexpected(f.error());
                }
                current.store(*f);
                size.store(0, std::memory_order_relaxed);
                next.store(next_time(now.unix_nano()), std::memory_order_relaxed);
                dirty.store(true, std::memory_order_release);
                if (!maintaining.exchange(true, std::memory_order_acq_rel)) {   // one job at a time: a gzip never races a removal
                    tracked_ptr<RotatingState> self(this);
                    async::go_blocking([self] { self->maintain(); });
                }
                return {};
            }

            // The path opened again: the file an outside tool moved away
            // is let go of, the writes go to the new one
            expected<void, io::error> reopen() {
                if (closed.load(std::memory_order_acquire)) {
                    return unexpected(io::error(io::errc::closed, "reopen", path));
                }
                auto f = open_file(path);
                if (!f) {
                    return unexpected(f.error());
                }
                auto info = io::stat(path);
                current.store(*f);
                size.store(info ? info->size : 0, std::memory_order_relaxed);
                return {};
            }
        };

        // The SIGHUP watch: a reopen at every signal until the stop or
        // until the file is gone (a weak pointer: a file dropped without
        // close() is not kept open by its watch)
        inline async::task<> rotating_watch(weak_ptr<RotatingState> file, async::channel<int> hup, async::stop_token stop) {
            for (;;) {
                if (co_await async::select(hup.on_receive([](optional<int>) {}), stop.on_stop([] {})) == 1) {
                    co_return;
                }
                auto s = file.lock();
                if (!s) {
                    co_return;
                }
                (void)s->reopen();
            }
        }

        struct RotatingAccess;
    }

    class rotating_file : public io::mixin::writer<rotating_file> {
    public:
        using io::mixin::writer<rotating_file>::write;

        // The log file at path, opened for appending (made when absent), its
        // size the file's; an io::error when it cannot be opened
        static expected<rotating_file, io::error> open(const string& path, const rotation& r = {}) noexcept {
            tracked_ptr<detail::RotatingState> s = make_tracked<detail::RotatingState>();
            s->path = path;
            s->r = r;
            string e = io::path::ext(path);
            s->ext = e;
            s->stem = string(std::string_view(path.data(), path.size() - e.size()));
            auto f = detail::RotatingState::open_file(path);
            if (!f) {
                return unexpected(f.error());
            }
            s->current.store(*f);
            auto info = io::stat(path);
            s->size.store(info ? info->size : 0, std::memory_order_relaxed);
            s->next.store(s->next_time(time::now().unix_nano()), std::memory_order_relaxed);
            if (r.reopen_on_sighup) {
                async::channel<int> hup = async::signals({SIGHUP});   // the handler in place before open returns: a SIGHUP after it is the watch's
                async::go(detail::rotating_watch(weak_ptr<detail::RotatingState>(s), hup, s->watch.token()));
            }
            return rotating_file(std::move(s));
        }

        // The data appended by one write(2), the file rotated first when
        // the write would take it past max_size or comes after the cron's
        // next time
        expected<size_t, io::error> write(const slice<const byte>& data) const {
            detail::RotatingState& s = *_s;
            if (s.closed.load(std::memory_order_acquire)) [[unlikely]] {
                return unexpected(io::error(io::errc::closed, "write", s.path));
            }
            io::file f = s.current.load();
            uint64_t size = s.size.load(std::memory_order_relaxed);
            bool past = s.r.max_size != 0 && size != 0 && size + data.size() > s.r.max_size;
            if (!past && s.r.at) {
                past = time::now().unix_nano() >= s.next.load(std::memory_order_relaxed);
            }
            if (past) [[unlikely]] {
                if (auto e = s.rotate(f, false); !e) {
                    return unexpected(e.error());
                }
                f = s.current.load();
            }
            auto r = f.write(data);
            if (r) {
                s.size.fetch_add(*r, std::memory_order_relaxed);
            }
            return r;
        }

        // The file rotated now, whatever its size and the time
        expected<void, io::error> rotate() const {
            if (_s->closed.load(std::memory_order_acquire)) {
                return unexpected(io::error(io::errc::closed, "rotate", _s->path));
            }
            return _s->rotate(_s->current.load(), true);
        }

        // The path opened again, after an outside tool (logrotate) moved
        // the file away; what reopen_on_sighup does at the signal
        expected<void, io::error> reopen() const {
            return _s->reopen();
        }

        // The file closed and the SIGHUP watch stopped; a write after it is
        // errc::closed. A second close does nothing
        expected<void, io::error> close() const {
            if (_s->closed.exchange(true, std::memory_order_acq_rel)) {
                return {};
            }
            _s->watch.request_stop();
            return _s->current.load().close();
        }

        SGCL_INLINE_HOT string path() const noexcept {
            return _s->path;
        }

        // The bytes in the current file (the writes of other threads at
        // that moment counted as they end)
        SGCL_INLINE_HOT uint64_t size() const noexcept {
            return _s->size.load(std::memory_order_relaxed);
        }

        SGCL_INLINE_HOT friend bool operator==(const rotating_file& a, const rotating_file& b) noexcept {
            return a._s == b._s;
        }

    private:
        SGCL_INLINE_HOT explicit rotating_file(tracked_ptr<detail::RotatingState> s) noexcept
        : _s(std::move(s)) {
        }

        tracked_ptr<detail::RotatingState> _s;
    };
}
