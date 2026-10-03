//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "handler.h"
#include "level.h"
#include "record.h"
#include "detail/output.h"
#include "detail/owned.h"
#include "../core/aliases.h"
#include "../core/atomic.h"
#include "../core/detail/handle_word.h"
#include "../core/duration.h"
#include "../core/make_tracked.h"
#include "../core/rooted.h"
#include "../core/tracked_ptr.h"
#include "../io/os.h"
#include "../io/stream.h"
#include "../time/datetime.h"
#include "../time/zone.h"

#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace sgcl::slog {
    class logger;

    namespace detail {
        //----------------------------------------------------------------
        // The attributes a logger keeps: with() and group()
        //----------------------------------------------------------------
        // The logger's attributes by the group they were given in: level 0
        // before any group(), level k after the k-th. Copied, as a
        // record's clone is, into the context's own memory; rendered once
        // for both handlers (what slog calls preformatted attributes) and
        // kept as a tree for a handler of the program
        struct ContextLevel {
            const char* name = "";
            size_t name_n = 0;
            Attr* a = nullptr;
            size_t n = 0;
        };

        struct Context {
            Arena arena;
            std::vector<ContextLevel> levels;
            const Attr* tree = nullptr;     // levels as groups, a Splice where the call's go
            size_t tree_n = 0;
            std::string pre_text;           // " a=1 g.b=2"
            std::string prefix_text;        // "g.h." for the call's keys
            std::string pre_json;           // ",\"a\":1,\"g\":{\"b\":2"
            size_t opened = 0;              // the groups pre_json opened
            bool json_open = false;         // pre_json ends in an object just opened: no comma before the next
        };

        // The groups of levels 1..G as a tree whose innermost group ends in
        // the Splice, and the two renderings, from the copied levels (data
        // alone: a copy reads nothing of the program's)
        inline void build_context(Context& c) noexcept {
            const size_t g = c.levels.size() - 1;
            Attr* below = nullptr;
            size_t below_n = 0;
            for (size_t k = g + 1; k-- > 0;) {
                const ContextLevel& l = c.levels[k];
                Attr* list = c.arena.attrs(l.n + 1);
                for (size_t i = 0; i < l.n; ++i) {
                    list[i] = l.a[i];
                }
                Attr& last = list[l.n];
                if (k == g) {
                    last.value.kind = Kind::Splice;
                } else {
                    const ContextLevel& next = c.levels[k + 1];
                    last.key = next.name;
                    last.key_n = next.name_n;
                    last.value.kind = Kind::Group;
                    last.value.g = below;
                    last.value.count = uint32_t(below_n);
                }
                below = list;
                below_n = l.n + 1;
            }
            c.tree = below;
            c.tree_n = below_n;

            Lines w;      // the text, and the prefix of the groups
            Lines j;      // the JSON
            bool first = false;   // after "msg": a comma first
            c.opened = 0;
            for (size_t k = 0; k <= g; ++k) {
                const ContextLevel& l = c.levels[k];
                if (k) {
                    w.prefix.put(l.name, l.name_n);
                    w.prefix.put('.');
                }
                text_attrs(w, l.a, l.n);
                // JSON: the groups not yet opened, opened only when the
                // level writes something (slog's withAttrs)
                const size_t mark = j.line.size();
                bool f = first;
                for (size_t i = c.opened + 1; i <= k; ++i) {
                    if (!f) {
                        j.line.put(',');
                    }
                    json_string(j.line, c.levels[i].name, c.levels[i].name_n);
                    j.line.put(":{", 2);
                    f = true;
                }
                if (json_attrs(j, l.a, l.n, f)) {
                    c.opened = k;
                    first = f;
                } else {
                    j.line.resize_down(mark);
                }
            }
            c.pre_text.assign(w.line.data(), w.line.size());
            c.prefix_text.assign(w.prefix.data(), w.prefix.size());
            c.pre_json.assign(j.line.data(), j.line.size());
            c.json_open = first;
        }

        inline void copy_levels(Context& c, const Context* old, Lines& w) noexcept {
            if (!old) {
                c.levels.push_back(ContextLevel{});
                return;
            }
            for (const ContextLevel& l : old->levels) {
                ContextLevel k;
                k.name = c.arena.copy(l.name, l.name_n);
                k.name_n = l.name_n;
                k.a = copy_list(c.arena, w, l.a, l.n);
                k.n = l.n;
                c.levels.push_back(k);
            }
        }

        inline tracked_ptr<Context> context_with(const Context* old, const Attr* a, size_t n) {
            auto c = make_tracked<Context>();
            Lines w;
            copy_levels(*c, old, w);
            ContextLevel& last = c->levels.back();
            Attr* both = c->arena.attrs(last.n + n);
            for (size_t i = 0; i < last.n; ++i) {
                both[i] = last.a[i];
            }
            for (size_t i = 0; i < n; ++i) {
                copy_attr(c->arena, w, a[i], both[last.n + i], Tail{}, 0);
            }
            last.a = both;
            last.n += n;
            build_context(*c);
            return c;
        }

        inline tracked_ptr<Context> context_group(const Context* old, const char* name, size_t name_n) noexcept {
            auto c = make_tracked<Context>();
            Lines w;
            copy_levels(*c, old, w);
            ContextLevel k;
            k.name = c->arena.copy(name, name_n);
            k.name_n = name_n;
            c->levels.push_back(k);
            build_context(*c);
            return c;
        }

        //----------------------------------------------------------------
        // Sampling: the first records of a (level, message) in a window,
        // then every then-th
        //----------------------------------------------------------------
        struct SampleCount {
            int64_t window = INT64_MIN;
            uint64_t count = 0;
        };

        struct alignas(64) SampleSlot {
            static constexpr size_t Buckets = 256;

            SlotLock lock;
            SampleCount* counts = nullptr;   // Buckets of them, made at the slot's first record
            uint64_t dropped = 0;            // records left out in drop_window
            int64_t drop_window = INT64_MIN;
        };

        struct Sampler {
            Sampler(uint32_t f, uint32_t t, int64_t p) noexcept
            : first(f), then(t), per(p > 0 ? p : 1), slots(new SampleSlot[Slots]) {
            }

            Sampler(const Sampler&) = delete;
            Sampler& operator=(const Sampler&) = delete;

            ~Sampler() {
                for (unsigned i = 0; i < Slots; ++i) {
                    delete[] slots[i].counts;
                }
                delete[] slots;
            }

            // Whether the record goes on; `report` the records left out in
            // the thread's last window, when this is the first record
            // after it
            bool admit(slog::level l, const char* msg, size_t n, int64_t ns, uint64_t& report) noexcept {
                const int64_t window = ns >= 0 ? ns / per : -((-ns + per - 1) / per);
                uint64_t h = 1469598103934665603ull ^ uint64_t(uint8_t(l));
                for (size_t i = 0; i < n; ++i) {
                    h = (h ^ uint8_t(msg[i])) * 1099511628211ull;
                }
                SampleSlot& s = slots[Scheduler::worker_index()];
                s.lock.lock();
                if (!s.counts) [[unlikely]] {
                    s.counts = new (std::nothrow) SampleCount[SampleSlot::Buckets];
                    if (!s.counts) {
                        s.lock.unlock();
                        return true;
                    }
                }
                if (s.dropped && s.drop_window != window) {
                    report = s.dropped;
                    s.dropped = 0;
                }
                SampleCount& c = s.counts[h % SampleSlot::Buckets];
                if (c.window != window) {
                    c.window = window;
                    c.count = 0;
                }
                const uint64_t k = ++c.count;
                const bool in = k <= first || (then && (k - first) % then == 0);
                if (!in) {
                    if (s.drop_window != window) {
                        s.drop_window = window;
                    }
                    ++s.dropped;
                }
                s.lock.unlock();
                return in;
            }

            const uint32_t first;
            const uint32_t then;
            const int64_t per;
            SampleSlot* slots;
        };

        //----------------------------------------------------------------
        // The logger's state
        //----------------------------------------------------------------
        struct LoggerState {
            tracked_ptr<Output> out;
            tracked_ptr<Context> ctx;
            tracked_ptr<LevelVarState> var;
            tracked_ptr<Sampler> sampler;
            io::writer writer;            // text and JSON: what out writes to
            slog::handler custom;         // a handler of the program
            slog::level min = slog::level::info;
            Format format = Format::Text;
            bool source = false;
            bool utc = false;
            bool buffered = false;

            bool enabled(slog::level l) const {
                const int least = var ? int(var->value.load(std::memory_order_relaxed)) : int(min);
                if (int(l) < least) {
                    return false;
                }
                return format != Format::Custom || out->custom.enabled(l);
            }
        };

        // What a logger given an empty io::writer writes to: every write
        // fails, so its records are counted as lost and said once on
        // stderr, as a failed write's are
        struct NoWriter {
            expected<size_t, io::error> write(const slice<const byte>&) const noexcept {
                return unexpected(io::error(io::errc::closed, "write to an empty io::writer"));
            }
        };

        inline NoWriter no_writer;

        inline tracked_ptr<LoggerState> copy_state(const LoggerState& s) noexcept {
            return make_tracked<LoggerState>(s);
        }

        inline void set_output(LoggerState& s) noexcept {
            if (s.format == Format::Custom) {
                s.out = make_tracked<Output>(s.custom);
            } else {
                s.out = make_tracked<Output>(s.format, s.writer, s.buffered);
            }
        }

        inline tracked_ptr<LoggerState> default_state() noexcept {
            auto s = make_tracked<LoggerState>();
            s->writer = io::writer(io::stderr);
            set_output(*s);
            return s;
        }

        // The local offset of a second, remembered for the second: the
        // zone's rules are looked up once a second on each thread
        inline int32_t local_offset(int64_t ns) noexcept {
            struct Cache {
                int64_t second = INT64_MIN;
                int32_t offset = 0;
            };
            thread_local Cache cache;
            const int64_t s = floor_div(ns, 1000000000);
            if (s != cache.second) {
                cache.offset = time::detail::zone_access::offset_at(time::detail::local_data(), s);
                cache.second = s;
            }
            return cache.offset;
        }

        // The line a record is made in: the thread's, grown once to the
        // largest record; plain memory, no tracked word (condition 2 of
        // the auditor). A record made while one is being made on the same
        // thread (a to_text that logs) gets lines of its own, and so does a
        // record made after the thread's Scratch is gone: exit() destroys
        // the main thread's thread_locals before it runs the atexit
        // functions and the statics' destructors (macOS, glibc), where a record
        // may still come (the collector's lines drained at exit, a record
        // from a static's destructor); the flag outlives it, it has no
        // destructor
        inline bool& scratch_gone() noexcept {
            thread_local bool gone = false;
            return gone;
        }

        struct Scratch {
            Lines lines;
            bool busy = false;

            Scratch() noexcept = default;
            Scratch(const Scratch&) = delete;
            Scratch& operator=(const Scratch&) = delete;

            ~Scratch() {
                scratch_gone() = true;
            }
        };

        // The thread's Scratch; null once it is gone
        inline Scratch* scratch() noexcept {
            if (scratch_gone()) [[unlikely]] {
                return nullptr;
            }
            thread_local Scratch s;
            return &s;
        }

        inline void source_text(Lines& w, const std::source_location& where) noexcept {
            Buf& t = w.tmp;
            t.clear();
            t.put(where.file_name());
            t.put(':');
            put_uint(t, where.line());
            text_string(w.line, t.data(), t.size());
        }

        inline void render_text(Lines& w, const LoggerState& st, slog::level l, const message& m, int64_t ns, int32_t offset, const Attr* a, size_t n) {
            Buf& b = w.line;
            char* o = b.reserve(5 + TimeTextSize + 7 + 16);
            std::memcpy(o, "time=", 5);
            size_t k = 5 + time_text(o + 5, ns, offset);
            std::memcpy(o + k, " level=", 7);
            k += 7;
            k += level_text(o + k, l);
            b.commit(k);
            if (st.source) {
                b.put(" source=", 8);
                source_text(w, m.where());
            }
            b.put(" msg=", 5);
            text_string(b, Access::text(m), Access::size(m));
            w.prefix.clear();
            if (st.ctx) {
                b.put(st.ctx->pre_text);
                w.prefix.put(st.ctx->prefix_text);
            }
            text_attrs(w, a, n);
            b.put('\n');
        }

        inline void render_json(Lines& w, const LoggerState& st, slog::level l, const message& m, int64_t ns, int32_t offset, const Attr* a, size_t n) {
            Buf& b = w.line;
            char* o = b.reserve(9 + TimeTextSize + 12 + 16);
            std::memcpy(o, "{\"time\":\"", 9);
            size_t k = 9 + time_json(o + 9, ns, offset);
            std::memcpy(o + k, "\",\"level\":\"", 11);
            k += 11;
            k += level_text(o + k, l);
            o[k++] = '"';
            b.commit(k);
            if (st.source) {
                const std::source_location& s = m.where();
                b.put(",\"source\":{", 11);
                bool first = true;
                const size_t fn = std::strlen(s.function_name());
                if (fn) {
                    b.put("\"function\":", 11);
                    json_string(b, s.function_name(), fn);
                    first = false;
                }
                const size_t file = std::strlen(s.file_name());
                if (file) {
                    if (!first) {
                        b.put(',');
                    }
                    b.put("\"file\":", 7);
                    json_string(b, s.file_name(), file);
                    first = false;
                }
                if (s.line()) {
                    if (!first) {
                        b.put(',');
                    }
                    b.put("\"line\":", 7);
                    put_uint(b, s.line());
                }
                b.put('}');
            }
            b.put(",\"msg\":", 7);
            json_string(b, Access::text(m), Access::size(m));
            bool first = false;
            size_t opened = 0;
            const Context* c = st.ctx.get();
            if (c) {
                b.put(c->pre_json);
                opened = c->opened;
                first = c->json_open;
            }
            if (n) {
                const size_t mark = b.size();
                bool f = first;
                const size_t groups = c ? c->levels.size() - 1 : 0;
                for (size_t j = opened + 1; j <= groups; ++j) {
                    if (!f) {
                        b.put(',');
                    }
                    json_string(b, c->levels[j].name, c->levels[j].name_n);
                    b.put(":{", 2);
                    f = true;
                }
                if (json_attrs(w, a, n, f)) {
                    opened = groups;
                } else {
                    b.resize_down(mark);
                }
            }
            char* e = b.reserve(opened + 2);
            for (size_t i = 0; i < opened; ++i) {
                e[i] = '}';
            }
            e[opened] = '}';
            e[opened + 1] = '\n';
            b.commit(opened + 2);
        }

        inline void emit(const LoggerState& st, slog::level l, const message& m, const Attr* a, size_t n, bool sample = true);

        // The line of the records a sampler left out, before the first
        // record after their window
        inline void report_sampled(const LoggerState& st, uint64_t count) {
            Pack<char[6], uint64_t> pack("count", count);
            message m("records sampled out");
            emit(st, slog::level::warn, m, pack.attrs, 1, false);
        }

        SGCL_NOINLINE inline void emit(const LoggerState& st, slog::level l, const message& m, const Attr* a, size_t n, bool sample) {
            drain_collector_if_pending();   // the collector's lines first, when collector_log has some waiting
            const int64_t ns = time::detail::now_nanos();
            if (sample && st.sampler) {
                uint64_t report = 0;
                const bool in = st.sampler->admit(l, Access::text(m), Access::size(m), ns, report);
                if (report) {
                    report_sampled(st, report);
                }
                if (!in) {
                    return;
                }
            }
            const int32_t offset = st.utc ? 0 : local_offset(ns);
            if (st.format == Format::Custom) {
                RecordData d;
                d.ns = ns;
                d.offset = offset;
                d.utc = st.utc;
                d.has_source = st.source;
                d.lvl = l;
                d.msg = Access::text(m);
                d.msg_n = Access::size(m);
                d.where = m.where();
                if (st.ctx) {
                    d.attrs = st.ctx->tree;
                    d.n = st.ctx->tree_n;
                    d.tail = Tail{a, n};
                } else {
                    d.attrs = a;
                    d.n = n;
                }
                st.out->custom.handle(Access::make(d));
                return;
            }
            Scratch* s = scratch();
            Lines own;
            const bool nested = !s || s->busy;
            Lines& w = nested ? own : s->lines;
            if (!nested) {
                s->busy = true;
            }
            struct Release {
                Scratch* s;
                bool nested;
                ~Release() {
                    if (!nested) {
                        s->busy = false;
                    }
                }
            } release{s, nested};
            w.line.clear();
            if (st.format == Format::Json) {
                render_json(w, st, l, m, ns, offset, a, n);
            } else {
                render_text(w, st, l, m, ns, offset, a, n);
            }
            st.out->write(st.out, w.line.data(), w.line.size(), l);
        }
    }

    // What a logger is made with (slog.HandlerOptions and the choice of
    // handler): text lines on io::stderr at info, in the local time, by
    // default. A plain struct, filled by designated initializers:
    //
    //     slog::logger log(slog::options{.out = file, .level = slog::level::debug, .json = true});
    struct options {
        io::writer out = io::writer(io::stderr);   // where the text or JSON lines go
        slog::handler handler;                     // the records to a handler of the program instead, when set
        slog::level level = slog::level::info;     // the least level written
        optional<slog::level_var> level_var;       // the least level read from it at every record, in place of level
        bool json = false;                         // JSON lines (slog.JSONHandler), text lines otherwise (slog.TextHandler)
        bool source = false;                       // where the call is, file:line, as `source` (slog's AddSource)
        bool utc = false;                          // the time in UTC, not the local zone
        // Lines gathered per worker and written a batch at a time (32 KB):
        // when the batch is full, at a record of warn and up, when the
        // worker goes to sleep, at flush() and at exit. The order across
        // workers is kept only within each worker; every line has its
        // time. For a writer that must not take writes from many threads
        // (a buffered_writer), and where many lines a second go to a file
        bool buffered = false;
        // Of the records of one level and message in each span of
        // sample_per (zero: no sampling), the first sample_first and then
        // every sample_then-th (0: none more); counted per worker, without
        // a lock between them. The records left out are said once, in a
        // line before the first record after their span:
        // level=WARN msg="records sampled out" count=N
        uint32_t sample_first = 0;
        uint32_t sample_then = 0;
        sgcl::duration sample_per = {};
    };

    namespace detail {
        logger buffered_copy(const logger& l) noexcept;
    }

    // A logger (slog.Logger): what a record is written as — text or JSON
    // on a writer, or a handler of the program — from which level, with
    // which attributes of its own. A handle of one word: the copies and
    // the loggers made from one by with() and group() share its output
    // and its batches.
    //
    //     slog::logger log(io::stdout, slog::level::debug);
    //     auto api = log.with("service", "api");   // log as it was
    //     api.info("started", "port", 8080);
    class logger {
    public:
        // Text lines on io::stderr, level info, the local time
        logger() noexcept
        : _s(detail::default_state()) {
        }

        explicit logger(const options& o) noexcept
        : _s(make_tracked<detail::LoggerState>()) {
            detail::LoggerState& s = *_s;
            s.min = o.level;
            if (o.level_var) {
                s.var = detail::Access::state(*o.level_var);
            }
            s.source = o.source;
            s.utc = o.utc;
            s.buffered = o.buffered;
            if (o.sample_per.nanoseconds() > 0) {
                s.sampler = make_tracked<detail::Sampler>(o.sample_first, o.sample_then, o.sample_per.nanoseconds());
            }
            if (o.handler) {
                s.format = detail::Format::Custom;
                s.custom = o.handler;
            } else {
                s.format = o.json ? detail::Format::Json : detail::Format::Text;
                s.writer = o.out ? o.out : io::writer(detail::no_writer);
            }
            detail::set_output(s);
        }

        // Text lines on out from level l
        explicit logger(const io::writer& out, slog::level l = slog::level::info) noexcept
        : logger(options{.out = out, .level = l}) {
        }

        // The records to a handler of the program, from level l
        explicit logger(const slog::handler& h, slog::level l = slog::level::info) noexcept
        : logger(options{.handler = h, .level = l}) {
        }

        // A logger that writes these attributes in every record (slog's
        // With), rendered once, now
        template<class... A>
        logger with(const A&... kv) const {
            detail::Pack<A...> pack(kv...);
            auto s = detail::copy_state(*_s);
            s->ctx = detail::context_with(_s->ctx.get(), pack.attrs, pack.Count);
            return logger(std::move(s));
        }

        // A logger whose later attributes, its with()'s and its records',
        // are in a group of this name (slog's WithGroup); an empty name
        // is no group
        logger group(const char* name) const noexcept {
            auto s = detail::copy_state(*_s);
            const size_t n = name ? std::strlen(name) : 0;
            if (n) {
                s->ctx = detail::context_group(_s->ctx.get(), name, n);
            }
            return logger(std::move(s));
        }

        template<class... A>
        void debug(message m, const A&... kv) const {
            _log(slog::level::debug, m, kv...);
        }

        template<class... A>
        void info(message m, const A&... kv) const {
            _log(slog::level::info, m, kv...);
        }

        template<class... A>
        void warn(message m, const A&... kv) const {
            _log(slog::level::warn, m, kv...);
        }

        template<class... A>
        void error(message m, const A&... kv) const {
            _log(slog::level::error, m, kv...);
        }

        template<class... A>
        void log(slog::level l, message m, const A&... kv) const {
            _log(l, m, kv...);
        }

        // Whether a record of level l is written: for an argument that
        // costs to compute, if (log.enabled(slog::level::debug)) ...
        bool enabled(slog::level l) const {
            return _s->enabled(l);
        }

        // The batches written now (a buffered logger)
        void flush() const {
            detail::drain_collector_if_pending();
            _s->out->flush();
        }

        // The records whose write failed
        uint64_t dropped() const noexcept {
            return _s->out->dropped.load(std::memory_order_relaxed);
        }

    private:
        friend struct detail::Access;
        friend struct sgcl::detail::HandleWord;
        friend logger detail::buffered_copy(const logger& l) noexcept;

        explicit logger(tracked_ptr<detail::LoggerState> s) noexcept
        : _s(std::move(s)) {
        }

        logger(sgcl::detail::FromWord, const tracked_ptr<detail::LoggerState>& w) noexcept
        : _s(w) {
        }

        tracked_ptr<detail::LoggerState>& _handle_word() noexcept {
            return _s;
        }

        const tracked_ptr<detail::LoggerState>& _handle_word() const noexcept {
            return _s;
        }

        template<class... A>
        void _log(slog::level l, const message& m, const A&... kv) const {
            const detail::LoggerState& s = *_s;
            if (!s.enabled(l)) {
                return;
            }
            detail::Pack<A...> pack(kv...);
            detail::emit(s, l, m, pack.attrs, pack.Count);
        }

        tracked_ptr<detail::LoggerState> _s;
    };

    namespace detail {
        // The logger as it is, its lines gathered per worker (options::
        // buffered): the server's access log over the default logger
        inline logger buffered_copy(const logger& l) noexcept {
            auto s = copy_state(*l._s);
            s->buffered = true;
            if (s->format != Format::Custom) {
                set_output(*s);
            }
            return logger(std::move(s));
        }

        // The default logger: a handle in an atomic of its word
        // (core/atomic.h over handle_word.h), kept by a rooted made once
        // and never destroyed: a record may come from a static's
        // destructor, after a static rooted would be gone
        inline sgcl::atomic<logger>& default_word() noexcept {
            static rooted<sgcl::atomic<logger>>* word = new rooted<sgcl::atomic<logger>>(std::in_place);
            return **word;
        }
    }

    // The logger the free functions write through: text on io::stderr at
    // info, until set_default. A copy: it shares its output
    inline logger default_logger() noexcept {
        return detail::default_word().load(std::memory_order_acquire);
    }

    // The default logger from now on, for every thread; atomic
    inline void set_default(const logger& l) noexcept {
        detail::default_word().store(l, std::memory_order_release);
    }

    template<class... A>
    void debug(message m, const A&... kv) {
        default_logger().debug(m, kv...);
    }

    template<class... A>
    void info(message m, const A&... kv) {
        default_logger().info(m, kv...);
    }

    template<class... A>
    void warn(message m, const A&... kv) {
        default_logger().warn(m, kv...);
    }

    template<class... A>
    void error(message m, const A&... kv) {
        default_logger().error(m, kv...);
    }
}
