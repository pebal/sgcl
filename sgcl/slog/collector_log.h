//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "logger.h"
#include "detail/output.h"
#include "../core/atomic.h"
#include "../core/detail/diagnostics.h"
#include "../core/rooted.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The collector's own lines (SGCL_LOG_PRINT_LEVEL, core/config.h) as
// records of a logger instead of lines on std::cout (DESIGN 283, Q7).
// Nothing changes until collector_log is called: the lines stay on
// std::cout byte for byte, and a program that includes slog without the
// call (a benchmark that parses them) sees them there as before.
//
// A line comes from the collector's thread or from a thread's
// registration, where nothing managed may be touched. The sink copies it
// into a queue of plain memory and flags it; a thread that may log takes
// the queue: a worker on its way to sleep, the next record of any logger,
// flush(), the exit. The level of the macro that asked for the line is the
// attribute `verbosity` (1 to 3); the line of a cycle (level 2) is its
// numbers as attributes, every other line its text.
namespace sgcl::slog {
    namespace detail {
        struct CollectorLine {
            int level;
            std::string text;
        };

        struct CollectorQueue {
            static constexpr size_t Most = 4096;   // lines kept while no thread takes them; past it counted and dropped

            std::mutex lock;
            std::vector<CollectorLine> lines;
            uint64_t dropped = 0;
        };

        // Made once and never destroyed: a line may come while the
        // program's statics go
        inline CollectorQueue& collector_queue() {
            static CollectorQueue* q = new CollectorQueue;
            return *q;
        }

        // The logger the lines go to
        inline sgcl::atomic<logger>& collector_target() {
            static rooted<sgcl::atomic<logger>> word(std::in_place);
            return *word;
        }

        inline void collector_sink(int level, const char* text, size_t n) noexcept {
            try {
                CollectorQueue& q = collector_queue();
                std::lock_guard<std::mutex> g(q.lock);
                if (q.lines.size() >= CollectorQueue::Most) {
                    ++q.dropped;
                } else {
                    q.lines.push_back(CollectorLine{level, std::string(text, n)});
                }
            } catch (...) {
                return;   // a line lost rather than a collector stopped
            }
            collector_pending.store(true, std::memory_order_release);
        }

        // The numbers of a cycle's line ("[sgcl] mem allocs:  12,    mem
        // removed: ..."): false when the line is not one
        struct CycleLine {
            uint64_t mem_allocs = 0, mem_removed = 0, total_mem = 0;
            uint64_t objects_created = 0, objects_removed = 0, live_objects = 0;
            std::string_view cycle;
            bool helpers = false;
            uint64_t helpers_used = 0;
            double time_ms = 0, total_time_ms = 0;
        };

        inline std::string_view trimmed(std::string_view s) noexcept {
            while (!s.empty() && s.front() == ' ') {
                s.remove_prefix(1);
            }
            while (!s.empty() && s.back() == ' ') {
                s.remove_suffix(1);
            }
            return s;
        }

        inline bool cycle_line(std::string_view line, CycleLine& c) {
            constexpr std::string_view Prefix = "[sgcl] ";
            if (line.substr(0, Prefix.size()) != Prefix) {
                return false;
            }
            line.remove_prefix(Prefix.size());
            if (line.substr(0, 11) != "mem allocs:") {
                return false;
            }
            int found = 0;
            while (!line.empty()) {
                const size_t comma = line.find(',');
                std::string_view part = line.substr(0, comma);
                line = comma == std::string_view::npos ? std::string_view() : line.substr(comma + 1);
                const size_t colon = part.find(':');
                if (colon == std::string_view::npos) {
                    return false;
                }
                const std::string_view key = trimmed(part.substr(0, colon));
                std::string_view value = trimmed(part.substr(colon + 1));
                const std::string text(value);
                const uint64_t u = std::strtoull(text.c_str(), nullptr, 10);
                if (key == "mem allocs") {
                    c.mem_allocs = u;
                } else if (key == "mem removed") {
                    c.mem_removed = u;
                } else if (key == "total mem") {
                    c.total_mem = u;
                } else if (key == "objects created") {
                    c.objects_created = u;
                } else if (key == "objects removed") {
                    c.objects_removed = u;
                } else if (key == "live objects") {
                    c.live_objects = u;
                } else if (key == "cycle") {
                    c.cycle = value;
                } else if (key == "helpers") {   // "on  used: 3"
                    c.helpers = value.substr(0, 2) == "on";
                    const size_t used = value.find("used:");
                    if (used != std::string_view::npos) {
                        c.helpers_used = std::strtoull(std::string(value.substr(used + 5)).c_str(), nullptr, 10);
                    }
                } else if (key == "time") {
                    c.time_ms = std::strtod(text.c_str(), nullptr);
                } else if (key == "total time") {
                    c.total_time_ms = std::strtod(text.c_str(), nullptr);
                } else {
                    return false;
                }
                ++found;
            }
            return found == 10;   // ten fields between the commas: the helpers' "used:" is inside theirs
        }

        inline void log_collector_line(const logger& log, const CollectorLine& l) {
            CycleLine c;
            if (l.level == 2 && cycle_line(l.text, c)) {
                log.info("collector", "verbosity", l.level, "mem_allocs", c.mem_allocs, "mem_removed", c.mem_removed, "total_mem", c.total_mem,
                         "objects_created", c.objects_created, "objects_removed", c.objects_removed, "live_objects", c.live_objects,
                         "cycle", c.cycle, "helpers", c.helpers, "helpers_used", c.helpers_used, "time_ms", c.time_ms, "total_time_ms", c.total_time_ms);
                return;
            }
            std::string_view text = l.text;
            if (text.substr(0, 7) == "[sgcl] ") {
                text.remove_prefix(7);
            }
            log.info("collector", "verbosity", l.level, "line", text);
        }

        // The waiting lines logged, on a thread that may log; not again
        // from inside itself (a record of its own that finds lines waiting)
        inline void drain_collector() {
            thread_local bool draining = false;
            if (draining) {
                return;
            }
            draining = true;
            std::vector<CollectorLine> taken;
            uint64_t dropped = 0;
            {
                CollectorQueue& q = collector_queue();
                std::lock_guard<std::mutex> g(q.lock);
                taken.swap(q.lines);
                dropped = std::exchange(q.dropped, 0);
                collector_pending.store(false, std::memory_order_relaxed);
            }
            if (!taken.empty() || dropped) {
                try {
                    const logger log = collector_target().load(std::memory_order_acquire);
                    for (const auto& l : taken) {
                        log_collector_line(log, l);
                    }
                    if (dropped) {
                        log.warn("collector lines dropped", "count", dropped);
                    }
                } catch (...) {
                }
            }
            draining = false;
        }
    }

    // The collector's lines (SGCL_LOG_PRINT_LEVEL above 0) as records of
    // log from now on, at info, `msg=collector`: a line of a cycle as its
    // numbers, any other as its text. Without the macro the collector says
    // nothing and neither does this. Called again: to that logger instead
    inline void collector_log(const logger& log) {
        detail::collector_target().store(log, std::memory_order_release);
        detail::collector_drain.store(&detail::drain_collector, std::memory_order_release);
        static std::once_flag once;
        std::call_once(once, [] {
            (void)detail::collector_queue();
            async::detail::worker_idle_hook.store(&detail::worker_idle, std::memory_order_relaxed);
            std::atexit([] { detail::drain_collector(); });
            std::at_quick_exit([] { detail::drain_collector(); });
        });
        sgcl::detail::diagnostic_sink.store(&detail::collector_sink, std::memory_order_release);
    }

    // Through the default logger
    inline void collector_log() {
        collector_log(default_logger());
    }
}
