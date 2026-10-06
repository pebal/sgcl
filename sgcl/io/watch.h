//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "fs.h"
#include "path.h"
#include "../async/channel.h"
#include "../async/coroutine.h"
#include "../async/reactor.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../async/timer.h"
#include "../core/aliases.h"
#include "../core/duration.h"
#include "../core/string.h"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include "detail/apple_fsevents.h"
#elif defined(__linux__)
#include <map>
#include <sys/inotify.h>
#endif

namespace sgcl::io {
    // The changes of the file system as they come (Go has none in its
    // standard library; fsnotify is a module of its own): a file, or the
    // entries of a directory (its whole tree when recursive), watched from
    // now on, the events a channel that a task co_awaits, a thread receives
    // on, a select takes as a case, as async::signals and size_changes give
    // theirs. macOS: FSEvents (CoreServices), the stream of file events, its
    // latency the coalescing interval; a file is watched through its
    // directory. Linux: inotify, a watch per directory, the new ones of a
    // recursive watch added as they come (written to the pattern, not run
    // here). Windows: ReadDirectoryChangesW (detail/win_watch.h, compile-
    // checked only). The system's side touches nothing of the library's: it
    // leaves plain records under a lock and wakes a task of the library on
    // the reactor, which merges a path's records within the coalescing
    // interval, gives each path back under the path the program named, and
    // sends the events.

    // What happened to a path; an event may carry several (macOS merges the
    // operations of a path that come close together into one event)
    enum class watch_op : uint8_t {
        created = 1,
        modified = 2,
        removed = 4,
        renamed = 8,      // either name of a rename: a stat tells the new from the old
        attribute = 16,   // the permissions, the owner, the times, the extended attributes
        overflow = 32,    // events were lost (the system's queue, or the records waiting): rescan what matters
    };

    SGCL_INLINE_HOT constexpr watch_op operator|(watch_op a, watch_op b) noexcept {
        return static_cast<watch_op>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
    }

    SGCL_INLINE_HOT constexpr bool operator&(watch_op a, watch_op b) noexcept {
        return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
    }

    // One change: the path, what happened to it, whether it is a directory
    struct watch_event {
        string path;                       // the watched path as given, joined with the part below it; empty for an overflow
        watch_op ops = watch_op(0);        // what happened, or-ed
        bool is_directory = false;
    };

    // How a path is watched
    struct watch_options {
        bool recursive = false;                       // a directory's whole tree, not only its entries
        duration coalesce = 50 * millisecond;         // the events of a path within it merged into one; zero: none
    };

    namespace detail {
        // A change as the system's side leaves it: the real path, the ops,
        // whether a directory
        struct WatchRecord {
            std::string path;
            uint8_t ops = 0;
            bool dir = false;
        };

        inline constexpr size_t WatchMaxRecords = 65536;

        // What the system's side and the task share: plain memory, made by
        // watch() and deleted by the task when the source is stopped
        class WatchSource {
        public:
            std::string root;      // the real path of the directory watched (a file's directory)
            std::string target;    // a file watched: its real path; empty for a directory
            bool recursive = false;
            double latency = 0;    // FSEvents' latency, the coalescing interval in seconds

            WatchSource() = default;
            WatchSource(const WatchSource&) = delete;
            WatchSource& operator=(const WatchSource&) = delete;

            ~WatchSource() {
                stop();
            }

            // A record left by the system's side, and the task woken
            void push(std::string path, uint8_t ops, bool dir) {
                std::lock_guard g(_m);
                if (_records.size() >= WatchMaxRecords) {
                    _overflow = true;
                    return;
                }
                _records.push_back(WatchRecord{std::move(path), ops, dir});
            }

            void lost() noexcept {
                std::lock_guard g(_m);
                _overflow = true;
            }

            // The records left since the last call, and whether some were lost
            std::pair<std::vector<WatchRecord>, bool> take() {
                drain();
                std::lock_guard g(_m);
                std::vector<WatchRecord> out;
                out.swap(_records);
                bool lost = _overflow;
                _overflow = false;
                return {std::move(out), lost};
            }

            // The descriptor the task waits on: the pipe the system's side
            // writes to (macOS), the inotify descriptor (Linux)
            int wait_fd() const noexcept {
#if defined(__linux__)
                return _inotify;
#else
                return _pipe[0];
#endif
            }

            expected<void, error> start(const string& given) noexcept;
            void stop() noexcept;

        private:
            void drain();

            std::mutex _m;
            std::vector<WatchRecord> _records;
            bool _overflow = false;
            bool _started = false;
#if defined(__APPLE__)
            int _pipe[2] = {-1, -1};
            apple::Stream _stream = nullptr;
            apple::Queue _queue = nullptr;

            void _wake() noexcept {
                char b = 1;
                (void)!::write(_pipe[1], &b, 1);   // a full pipe has a wake in it already
            }

            static uint8_t _ops_of(apple::EventFlags f) noexcept {
                uint8_t ops = 0;
                ops |= (f & apple::ItemCreated) ? uint8_t(watch_op::created) : 0;
                ops |= (f & apple::ItemModified) ? uint8_t(watch_op::modified) : 0;
                ops |= (f & apple::ItemRemoved) ? uint8_t(watch_op::removed) : 0;
                ops |= (f & apple::ItemRenamed) ? uint8_t(watch_op::renamed) : 0;
                ops |= (f & (apple::ItemInodeMetaMod | apple::ItemChangeOwner | apple::ItemXattrMod | apple::ItemFinderInfoMod)) ? uint8_t(watch_op::attribute) : 0;
                return ops;
            }

            // FSEvents' callback, on the dispatch queue's thread: plain
            // records under the lock, a byte to the pipe; nothing managed
            static void _callback(const apple::__FSEventStream*, void* info, size_t count, void* paths, const apple::EventFlags flags[], const apple::EventId[]) {
                auto* s = static_cast<WatchSource*>(info);
                char** p = static_cast<char**>(paths);
                for (size_t i = 0; i < count; ++i) {
                    const apple::EventFlags f = flags[i];
                    if (f & (apple::MustScanSubDirs | apple::UserDropped | apple::KernelDropped)) {
                        s->lost();
                    }
                    if (f & apple::RootChanged) {
                        s->push(s->target.empty() ? s->root : s->target, uint8_t(watch_op::removed), s->target.empty());
                        continue;
                    }
                    const uint8_t ops = _ops_of(f);
                    if (ops) {
                        s->push(std::string(p[i]), ops, (f & apple::ItemIsDir) != 0);
                    }
                }
                s->_wake();
            }
#elif defined(__linux__)
            int _inotify = -1;
            std::map<int, std::string> _dirs;   // the watch descriptors and the directories they watch (the task's alone)

            static constexpr uint32_t _Mask = IN_CREATE | IN_MODIFY | IN_CLOSE_WRITE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF | IN_EXCL_UNLINK | IN_ONLYDIR;

            // A directory watched, and its tree for a recursive watch
            void _add(const std::string& dir, bool tree) {
                int wd = ::inotify_add_watch(_inotify, dir.c_str(), _Mask);
                if (wd < 0) {
                    return;
                }
                _dirs[wd] = dir;
                if (!tree) {
                    return;
                }
                auto entries = detail::_block_read_dir(string(dir));
                if (!entries) {
                    return;
                }
                for (auto& e : *entries) {
                    if (e.type == file_type::directory) {
                        _add(dir + "/" + std::string(e.name.view()), true);
                    }
                }
            }
#endif
        };

#if defined(__APPLE__)
        inline expected<void, error> WatchSource::start(const string& given) noexcept {
            if (::pipe(_pipe) != 0) {
                _pipe[0] = _pipe[1] = -1;
                return detail::fail(last_error("watch", given));
            }
            for (int fd : _pipe) {
                ::fcntl(fd, F_SETFD, FD_CLOEXEC);
                ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
            }
            (void)::fcntl(_pipe[1], F_SETNOSIGPIPE, 1);
            _queue = apple::queue_create("sgcl.io.watch", nullptr);
            apple::Ref dir = apple::string_create(nullptr, root.c_str(), apple::Utf8);
            const void* one[1] = {dir};
            apple::Ref paths = apple::array_create(nullptr, one, 1, apple::type_array_callbacks);
            apple::StreamContext context = {0, this, nullptr, nullptr, nullptr};
            _stream = apple::stream_create(nullptr, &_callback, &context, paths, apple::SinceNow, latency, apple::CreateFileEvents | apple::CreateWatchRoot | (latency == 0 ? apple::CreateNoDefer : 0));
            apple::release(paths);
            apple::release(dir);
            if (!_stream) {
                stop();
                return detail::fail(error(std::make_error_code(std::errc::not_supported), "watch", given));
            }
            apple::stream_set_dispatch_queue(_stream, _queue);
            if (!apple::stream_start(_stream)) {
                stop();
                return detail::fail(error(std::make_error_code(std::errc::not_supported), "watch", given));
            }
            _started = true;
            return {};
        }

        inline void WatchSource::stop() noexcept {
            if (_stream) {
                if (_started) {
                    apple::stream_stop(_stream);
                }
                apple::stream_invalidate(_stream);
                apple::sync_f(_queue, nullptr, [](void*) {});   // a callback still on the queue has run
                apple::stream_release(_stream);
                _stream = nullptr;
                _started = false;
            }
            if (_queue) {
                apple::queue_release(_queue);
                _queue = nullptr;
            }
            for (int& fd : _pipe) {
                if (fd >= 0) {
                    async::cancel_waits(fd);
                    ::close(fd);
                    fd = -1;
                }
            }
        }

        inline void WatchSource::drain() {
            char buf[256];
            while (::read(_pipe[0], buf, sizeof buf) > 0) {
            }
        }
#elif defined(__linux__)
        inline expected<void, error> WatchSource::start(const string& given) noexcept {
            _inotify = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
            if (_inotify < 0) {
                return detail::fail(last_error("watch", given));
            }
            int wd = ::inotify_add_watch(_inotify, root.c_str(), _Mask);
            if (wd < 0) {
                error e = last_error("watch", given);
                stop();
                return detail::fail(e);
            }
            _dirs[wd] = root;
            if (recursive) {
                auto entries = detail::_block_read_dir(string(root));
                if (entries) {
                    for (auto& e : *entries) {
                        if (e.type == file_type::directory) {
                            _add(root + "/" + std::string(e.name.view()), true);
                        }
                    }
                }
            }
            _started = true;
            return {};
        }

        inline void WatchSource::stop() noexcept {
            if (_inotify >= 0) {
                async::cancel_waits(_inotify);
                ::close(_inotify);
                _inotify = -1;
            }
            _dirs.clear();
            _started = false;
        }

        // The events read off the inotify descriptor into records: the
        // directory of the watch joined with the name; a directory made
        // in a recursive watch watched in its turn
        inline void WatchSource::drain() {
            alignas(struct inotify_event) char buf[16384];
            for (;;) {
                ssize_t n = ::read(_inotify, buf, sizeof buf);
                if (n <= 0) {
                    return;
                }
                for (char* at = buf; at < buf + n;) {
                    auto* e = reinterpret_cast<struct inotify_event*>(at);
                    at += sizeof(struct inotify_event) + e->len;
                    if (e->mask & IN_Q_OVERFLOW) {
                        lost();
                        continue;
                    }
                    auto dir = _dirs.find(e->wd);
                    if (dir == _dirs.end()) {
                        continue;
                    }
                    if (e->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                        if (dir->second == root) {
                            push(target.empty() ? root : target, uint8_t(watch_op::removed), target.empty());
                        }
                        continue;
                    }
                    if (e->mask & IN_IGNORED) {
                        _dirs.erase(dir);
                        continue;
                    }
                    std::string path = e->len ? dir->second + "/" + e->name : dir->second;
                    const bool is_dir = (e->mask & IN_ISDIR) != 0;
                    uint8_t ops = 0;
                    ops |= (e->mask & IN_CREATE) ? uint8_t(watch_op::created) : 0;
                    ops |= (e->mask & (IN_MODIFY | IN_CLOSE_WRITE)) ? uint8_t(watch_op::modified) : 0;
                    ops |= (e->mask & IN_DELETE) ? uint8_t(watch_op::removed) : 0;
                    ops |= (e->mask & (IN_MOVED_FROM | IN_MOVED_TO)) ? uint8_t(watch_op::renamed) : 0;
                    ops |= (e->mask & IN_ATTRIB) ? uint8_t(watch_op::attribute) : 0;
                    if (recursive && is_dir && (e->mask & (IN_CREATE | IN_MOVED_TO))) {
                        _add(path, true);
                    }
                    if (ops) {
                        push(std::move(path), ops, is_dir);
                    }
                }
            }
        }
#else
        inline expected<void, error> WatchSource::start(const string& given) noexcept {
            return detail::fail(error(std::make_error_code(std::errc::not_supported), "watch", given));
        }

        inline void WatchSource::stop() noexcept {
        }

        inline void WatchSource::drain() {
        }
#endif

        // The path of a record under the path the program named: a file's
        // own, an entry of the directory (any entry of its tree when
        // recursive), the directory itself for its removal; empty for one
        // outside what is watched
        inline std::string watch_path_of(const WatchSource& s, const std::string& real, std::string_view given, uint8_t ops) {
            if (!s.target.empty()) {
                return real == s.target ? std::string(given) : std::string();
            }
            if (real == s.root) {
                return (ops & uint8_t(watch_op::removed | watch_op::renamed)) ? std::string(given) : std::string();
            }
            if (real.size() <= s.root.size() + 1 || real.compare(0, s.root.size(), s.root) != 0 || real[s.root.size()] != '/') {
                return {};
            }
            std::string_view below = std::string_view(real).substr(s.root.size() + 1);
            if (!s.recursive && below.find('/') != std::string_view::npos) {
                return {};
            }
            std::string out(given);
            if (out.empty() || out.back() != '/') {
                out += '/';
            }
            out.append(below);
            return out;
        }

        // The task between the system's side and the program: a wait on the
        // reactor (or the stop), the coalescing interval where the system
        // does not coalesce, the records of a path merged in the order they
        // came, sent; the end stops the source and closes the channel
        inline async::task<> follow_watch(WatchSource* raw, string given, async::channel<watch_event> out, async::stop_token stop, duration coalesce) noexcept {
            std::unique_ptr<WatchSource> source(raw);
#if defined(__APPLE__)
            const bool system_coalesces = true;   // FSEvents' latency is the interval
#else
            const bool system_coalesces = false;
#endif
            bool ended = false;
            while (!ended) {
                const int fd = source->wait_fd();
                bool stopped = false;
                if (stop.stop_possible()) {
                    co_await async::select(async::readable(fd).on_set([] {}), stop.on_stop([&] { stopped = true; }));
                } else {
                    co_await async::readable(fd);
                }
                if (stopped || out.closed()) {
                    break;
                }
                if (!system_coalesces && coalesce > duration::zero()) {
                    co_await async::sleep(coalesce);
                }
                std::vector<WatchRecord> records;
                bool lost = false;
                try {
                    auto taken = source->take();
                    records = std::move(taken.first);
                    lost = taken.second;
                } catch (...) {
                    lost = true;
                }
                // a path's records merged, in the order of their first
                std::vector<std::pair<std::string, size_t>> order;   // path, index in merged
                std::vector<WatchRecord> merged;
                for (auto& r : records) {
                    std::string path = watch_path_of(*source, r.path, given.view(), r.ops);
                    if (path.empty()) {
                        continue;
                    }
                    size_t k = 0;
                    for (; k < order.size() && order[k].first != path; ++k) {
                    }
                    if (k == order.size() || coalesce == duration::zero()) {
                        order.push_back({path, merged.size()});
                        merged.push_back(WatchRecord{std::move(path), r.ops, r.dir});
                    } else {
                        merged[order[k].second].ops |= r.ops;
                        merged[order[k].second].dir = merged[order[k].second].dir || r.dir;
                    }
                }
                for (auto& m : merged) {
                    watch_event e;
                    e.path = string(m.path);
                    e.ops = watch_op(m.ops);
                    e.is_directory = m.dir;
                    if (!co_await out.send(std::move(e))) {
                        ended = true;
                        break;
                    }
                }
                if (lost && !ended) {
                    watch_event e;
                    e.ops = watch_op::overflow;
                    ended = !co_await out.send(std::move(e));
                }
            }
            source->stop();
            out.close();
        }
    }

    // The changes of a file, or of a directory's entries (its tree when
    // recursive), from now on, as a channel of 64 events: a slow receiver
    // holds the events back, past 65536 records waiting they are dropped and
    // one `overflow` is sent. The channel ends, closed, when the stop is
    // requested, or at the next change after the program closed it. Paths
    // come back under the path the program named (the system's real path
    // replaced by it). A path that is not there is an error now
    // (is_not_found()); a system with no watch of its own, ENOTSUP;
    // std::system_error when the reactor's thread cannot be made
    inline expected<async::channel<watch_event>, error> watch(const string& path, const watch_options& options = {}, async::stop_token stop = {}) {
        struct ::stat st;
        if (::stat(path.c_str(), &st) != 0) {
            return detail::fail(last_error("watch", path));
        }
        char real[PATH_MAX];
        if (!::realpath(path.c_str(), real)) {
            return detail::fail(last_error("watch", path));
        }
        auto source = std::make_unique<detail::WatchSource>();
        if (S_ISDIR(st.st_mode)) {
            source->root = real;
            source->recursive = options.recursive;
        } else {
            source->target = real;
            source->root = io::path::dir(string(real)).str();
        }
        source->latency = options.coalesce > duration::zero() ? double(options.coalesce.nanoseconds()) / 1e9 : 0.0;
        if (auto r = source->start(path); !r) {
            return detail::fail(r);
        }
        async::channel<watch_event> out(64);
        string given = io::path::clean(path);
        async::go(detail::follow_watch(source.release(), given, out, stop, options.coalesce));
        return out;
    }
}
