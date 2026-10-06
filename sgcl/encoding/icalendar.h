//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "content_line.h"
#include "error.h"
#include "recurrence.h"
#include "detail/content_reader.h"
#include "detail/files.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../io/functions.h"
#include "../io/stream.h"
#include "../time/date.h"
#include "../time/datetime.h"
#include "../time/zone.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        struct IcalNode;
        struct IcalendarAccess;
    }

    // One component of iCalendar (RFC 5545): a VCALENDAR, or a VEVENT, a
    // VTODO, a VJOURNAL, a VFREEBUSY, a VTIMEZONE (with its STANDARD and
    // DAYLIGHT), a VALARM, any other inside it; its properties and the
    // components inside it, in order. Immutable, one word shared by
    // copying. A calendar reads the times of its components with its own
    // VTIMEZONEs (datetime_of) and expands their recurrences (occurrences).
    class icalendar {
    public:
        using error = encoding::error;
        using property = content_line;

        // What a parse accepts
        struct options {
            uint32_t max_depth = 64;              // components inside one another
            size_t max_size = size_t(64) << 20;   // the text's bytes
        };

        // A VCALENDAR of VERSION:2.0 and a PRODID, nothing else
        icalendar() noexcept;

        // An empty component of the name: icalendar("VEVENT")
        explicit icalendar(const string& name) noexcept;

        // A text of exactly one VCALENDAR
        static expected<icalendar, error> parse(const string& text) noexcept;
        static expected<icalendar, error> parse(const string& text, const options& o) noexcept;
        static expected<icalendar, error> parse(const io::reader& in);
        static expected<icalendar, error> parse(const io::reader& in, const options& o);
        static async::task<expected<icalendar, error>> async_parse(io::reader in) noexcept;
        static async::task<expected<icalendar, error>> async_parse(io::reader in, options o) noexcept;

        // Every VCALENDAR of a text (a stream of several)
        static expected<vector<icalendar>, error> parse_all(const string& text) noexcept;
        static expected<vector<icalendar>, error> parse_all(const string& text, const options& o) noexcept;

        // The calendar of a file: icalendar::load("team.ics")
        static expected<icalendar, error> load(const string& path);
        static async::task<expected<icalendar, error>> async_load(string path) noexcept;

        // to_string into a file, made or written over
        expected<void, error> save(const string& path) const;
        async::task<expected<void, error>> async_save(string path) const noexcept;

        // BEGIN:name, the properties, the components, END:name, each line
        // folded at 75 octets and ended by CRLF. invalid_argument for a name
        // that is none
        string to_string() const;

        // VCALENDAR, VEVENT, ...: upper-cased
        string name() const noexcept;

        slice<const property> properties() const noexcept;

        // The first property of the name (in any case), every one
        optional<property> property_of(const string& name) const noexcept;
        vector<property> properties_of(const string& name) const noexcept;

        // The first property's text() (SUMMARY, LOCATION), or the fallback
        string text(const string& name, const string& fallback) const noexcept;

        slice<const icalendar> components() const noexcept;

        // The components of the name: events() is components_of("VEVENT")
        vector<icalendar> components_of(const string& name) const noexcept;

        // On a calendar, the instant of a DATE-TIME of one of its
        // components: UTC, a TZID of one of the calendar's VTIMEZONEs (in the
        // fixed zone of its offset then), else a TZID the system knows; a
        // floating one, and a DATE at its midnight, in the zone given (the
        // system's local zone without one). nullopt for a value that is no
        // DATE or DATE-TIME, an unknown TZID, a year outside time's
        optional<time::datetime> datetime_of(const property& p) const noexcept;
        optional<time::datetime> datetime_of(const property& p, const time::zone& floating) const noexcept;

        // On a calendar, the starts of a component of it: DTSTART, the times
        // of its RRULEs and RDATEs, less its EXDATEs; those at or after from
        // and before to, in order, at most limit (100 000 without it), in
        // DTSTART's zone. Floating and DATE starts in the zone given (the
        // system's local zone without one). Empty without a DTSTART
        vector<time::datetime> occurrences(const icalendar& component, const time::datetime& from, const time::datetime& to) const;
        vector<time::datetime> occurrences(const icalendar& component, const time::datetime& from, const time::datetime& to, size_t limit) const;
        vector<time::datetime> occurrences(const icalendar& component, const time::datetime& from, const time::datetime& to, size_t limit,
                                           const time::zone& floating) const;

        // New versions: a property or a component added at the end, every
        // property of a line's name replaced by it, every property of a
        // name taken out
        icalendar add(const property& p) const noexcept;
        icalendar add(const icalendar& component) const noexcept;
        icalendar set(const property& p) const noexcept;
        icalendar erase(const string& name) const noexcept;

        // The same name, properties and components, in the same order
        friend bool operator==(const icalendar& a, const icalendar& b) noexcept;

    private:
        friend struct detail::IcalendarAccess;

        tracked_ptr<const detail::IcalNode> _node;

        const detail::IcalNode& _n() const noexcept;
    };

    namespace detail {
        struct IcalNode {
            string name;
            vector<content_line> properties;
            vector<icalendar> components;
        };

        struct IcalendarAccess {
            static icalendar make(string name, vector<content_line> properties, vector<icalendar> components) noexcept {
                auto n = make_tracked<IcalNode>();
                n->name = std::move(name);
                n->properties = std::move(properties);
                n->components = std::move(components);
                icalendar c(string("X"));
                c._node = std::move(n);
                return c;
            }

            static icalendar build(const ContentTree& t, size_t index) noexcept {
                const ContentNode& n = t.nodes[index];
                vector<icalendar> children;
                for (size_t c : n.children) {
                    children.push_back(build(t, c));
                }
                return make(n.name, n.lines, children);
            }

            static const IcalNode& node(const icalendar& c) noexcept {
                return c._n();
            }
        };

        // A VTIMEZONE's offsets: its observances' onsets, from the instant
        // each begins the offset it goes to
        class IcalZone {
        public:
            // The onsets up to the wall clock until (a year past what is asked)
            IcalZone(const icalendar& vtimezone, int64_t until) {
                struct Observance {
                    int64_t start_wall;
                    int64_t from, to;
                };
                int64_t earliest = INT64_MAX;
                for (const icalendar& o : vtimezone.components()) {
                    if (o.name().view() != "STANDARD" && o.name().view() != "DAYLIGHT") {
                        continue;
                    }
                    auto start = o.property_of("DTSTART");
                    auto from = o.property_of("TZOFFSETFROM");
                    auto to = o.property_of("TZOFFSETTO");
                    RecMoment m;
                    if (!start || !from || !to || !rec_moment(start->value().view(), m)) {
                        continue;
                    }
                    auto f = from->as_utc_offset();
                    auto t = to->as_utc_offset();
                    if (!f || !t) {
                        continue;
                    }
                    int64_t fs = f->nanoseconds() / 1000000000, ts = t->nanoseconds() / 1000000000;
                    auto add = [&](int64_t wall) {
                        _onsets.push_back({wall - fs, fs, ts});
                        if (wall - fs < earliest) {
                            earliest = wall - fs;
                            _first = fs;
                        }
                    };
                    add(m.wall);
                    for (const content_line& r : o.properties_of("RRULE")) {
                        auto rule = r.as_recurrence();
                        if (!rule) {
                            continue;
                        }
                        RecResolve resolve = [fs](int64_t w) { return w - fs; };
                        // onsets to the end of time's range: a few hundred a rule
                        auto walls = rec_expand(RecurrenceAccess::data(*rule), m.wall, false, resolve, INT64_MIN, until, 100000);
                        for (size_t i = 1; i < walls.size(); ++i) {
                            add(walls[i]);
                        }
                    }
                    for (const content_line& r : o.properties_of("RDATE")) {
                        for (const string& v : r.list()) {
                            RecMoment rm;
                            if (rec_moment(v.view(), rm)) {
                                add(rm.wall);
                            }
                        }
                    }
                }
                std::sort(_onsets.begin(), _onsets.end(), [](const Onset& a, const Onset& b) { return a.at < b.at; });
                _offsets.clear();
                for (const Onset& o : _onsets) {
                    _offsets.push_back(o.from);
                    _offsets.push_back(o.to);
                }
                std::sort(_offsets.begin(), _offsets.end());
                _offsets.erase(std::unique(_offsets.begin(), _offsets.end()), _offsets.end());
            }

            bool empty() const noexcept {
                return _onsets.empty();
            }

            // The offset at an instant
            int64_t offset_at(int64_t utc) const noexcept {
                auto it = std::upper_bound(_onsets.begin(), _onsets.end(), utc, [](int64_t t, const Onset& o) { return t < o.at; });
                return it == _onsets.begin() ? _first : std::prev(it)->to;
            }

            // A wall clock's instant: of the offsets the clock may be in, the
            // largest that is in effect then (the first of a time shown
            // twice); a time skipped moved on by the skip
            int64_t resolve(int64_t wall) const noexcept {
                for (auto it = _offsets.rbegin(); it != _offsets.rend(); ++it) {
                    if (offset_at(wall - *it) == *it) {
                        return wall - *it;
                    }
                }
                for (const Onset& o : _onsets) {
                    if (o.to > o.from && wall >= o.at + o.from && wall < o.at + o.to) {
                        return wall - o.from;
                    }
                }
                return wall - _first;
            }

        private:
            struct Onset {
                int64_t at;
                int64_t from, to;
            };

            std::vector<Onset> _onsets;
            std::vector<int64_t> _offsets;
            int64_t _first = 0;
        };

        // How a DATE-TIME of a calendar becomes instants: its wall clock's
        // resolver and the zone its results are shown in
        struct IcalClock {
            enum class kind { utc, system, vtimezone, floating } k = kind::floating;
            time::zone zone;              // system, floating
            std::shared_ptr<IcalZone> vz;  // vtimezone

            int64_t resolve(int64_t wall) const noexcept {
                switch (k) {
                    case kind::utc: return wall;
                    case kind::vtimezone: return vz->resolve(wall);
                    default: return cl_at(wall, zone).unix();
                }
            }

            time::datetime show(int64_t utc, int64_t nanos = 0) const noexcept {
                switch (k) {
                    case kind::utc: return time::datetime::from_unix_nano(utc * 1000000000 + nanos, time::zone::utc());
                    case kind::vtimezone: return time::datetime::from_unix_nano(utc * 1000000000 + nanos, time::zone::fixed(duration(std::chrono::seconds(vz->offset_at(utc)))));
                    default: return time::datetime::from_unix_nano(utc * 1000000000 + nanos, zone);
                }
            }
        };
    }

    inline icalendar::icalendar() noexcept
    : icalendar(string("VCALENDAR")) {
        vector<content_line> props;
        props.push_back(content_line("VERSION", "2.0"));
        props.push_back(content_line("PRODID", "-//sgcl//sgcl//EN"));
        *this = detail::IcalendarAccess::make(string("VCALENDAR"), props, {});
    }

    inline icalendar::icalendar(const string& name) noexcept
    : _node([&] {
          auto n = make_tracked<detail::IcalNode>();
          n->name = string(detail::cl_upper(name.view()));
          return n;
      }()) {
    }

    inline const detail::IcalNode& icalendar::_n() const noexcept {
        return *_node;
    }

    inline string icalendar::name() const noexcept {
        return _n().name;
    }

    inline slice<const icalendar::property> icalendar::properties() const noexcept {
        return _n().properties.as_slice();
    }

    inline optional<icalendar::property> icalendar::property_of(const string& name) const noexcept {
        std::string u = detail::cl_upper(name.view());
        for (const property& p : _n().properties) {
            if (p.name().view() == u) {
                return p;
            }
        }
        return nullopt;
    }

    inline vector<icalendar::property> icalendar::properties_of(const string& name) const noexcept {
        std::string u = detail::cl_upper(name.view());
        vector<property> out;
        for (const property& p : _n().properties) {
            if (p.name().view() == u) {
                out.push_back(p);
            }
        }
        return out;
    }

    inline string icalendar::text(const string& name, const string& fallback) const noexcept {
        auto p = property_of(name);
        return p ? p->text() : fallback;
    }

    inline slice<const icalendar> icalendar::components() const noexcept {
        return _n().components.as_slice();
    }

    inline vector<icalendar> icalendar::components_of(const string& name) const noexcept {
        std::string u = detail::cl_upper(name.view());
        vector<icalendar> out;
        for (const icalendar& c : _n().components) {
            if (c.name().view() == u) {
                out.push_back(c);
            }
        }
        return out;
    }

    inline icalendar icalendar::add(const property& p) const noexcept {
        vector<property> props(_n().properties.begin(), _n().properties.end());
        props.push_back(p);
        return detail::IcalendarAccess::make(_n().name, props, _n().components);
    }

    inline icalendar icalendar::add(const icalendar& component) const noexcept {
        vector<icalendar> cs(_n().components.begin(), _n().components.end());
        cs.push_back(component);
        return detail::IcalendarAccess::make(_n().name, _n().properties, cs);
    }

    inline icalendar icalendar::set(const property& p) const noexcept {
        vector<property> props;
        bool placed = false;
        for (const property& q : _n().properties) {
            if (q.name() == p.name()) {
                if (!placed) {
                    props.push_back(p);
                    placed = true;
                }
            } else {
                props.push_back(q);
            }
        }
        if (!placed) {
            props.push_back(p);
        }
        return detail::IcalendarAccess::make(_n().name, props, _n().components);
    }

    inline icalendar icalendar::erase(const string& name) const noexcept {
        std::string u = detail::cl_upper(name.view());
        vector<property> props;
        for (const property& q : _n().properties) {
            if (q.name().view() != u) {
                props.push_back(q);
            }
        }
        return detail::IcalendarAccess::make(_n().name, props, _n().components);
    }

    inline bool operator==(const icalendar& a, const icalendar& b) noexcept {
        const auto& x = a._n();
        const auto& y = b._n();
        if (x.name != y.name || x.properties.size() != y.properties.size() || x.components.size() != y.components.size()) {
            return false;
        }
        for (size_t i = 0; i < x.properties.size(); ++i) {
            if (!(x.properties[i] == y.properties[i])) {
                return false;
            }
        }
        for (size_t i = 0; i < x.components.size(); ++i) {
            if (!(x.components[i] == y.components[i])) {
                return false;
            }
        }
        return true;
    }

    inline expected<icalendar, icalendar::error> icalendar::parse(const string& text) noexcept {
        return parse(text, options());
    }

    inline expected<icalendar, icalendar::error> icalendar::parse(const string& text, const options& o) noexcept {
        auto all = parse_all(text, o);
        if (!all) {
            return unexpected<error>(std::move(all.error()));
        }
        if (all->size() != 1) {
            error e(errc::syntax, 0, string(all->empty() ? "no VCALENDAR" : "more than one VCALENDAR"));
            return unexpected<error>(std::move(e.locate(text)));
        }
        return (*all)[0];
    }

    inline expected<vector<icalendar>, icalendar::error> icalendar::parse_all(const string& text) noexcept {
        return parse_all(text, options());
    }

    inline expected<vector<icalendar>, icalendar::error> icalendar::parse_all(const string& text, const options& o) noexcept {
        detail::ContentLimits limits;
        limits.max_depth = o.max_depth;
        limits.max_size = o.max_size;
        auto tree = detail::content_read(text, false, limits);
        if (!tree) {
            return unexpected<error>(std::move(tree.error()));
        }
        vector<icalendar> out;
        for (size_t r : tree->roots) {
            if (tree->nodes[r].name.view() != "VCALENDAR") {
                error e(errc::syntax, 0, string("a top component that is no VCALENDAR: " + std::string(tree->nodes[r].name.view())));
                return unexpected<error>(std::move(e.locate(text)));
            }
            out.push_back(detail::IcalendarAccess::build(*tree, r));
        }
        return out;
    }

    inline expected<icalendar, icalendar::error> icalendar::parse(const io::reader& in) {
        return parse(in, options());
    }

    inline expected<icalendar, icalendar::error> icalendar::parse(const io::reader& in, const options& o) {
        auto all = io::read_all(in);
        if (!all) {
            error e(all.error(), 0);
            return unexpected<error>(std::move(e));
        }
        return parse(string(*all), o);
    }

    inline async::task<expected<icalendar, icalendar::error>> icalendar::async_parse(io::reader in) noexcept {
        return async_parse(std::move(in), options());
    }

    inline async::task<expected<icalendar, icalendar::error>> icalendar::async_parse(io::reader in, options o) noexcept {
        auto all = co_await io::async_read_all(in);
        if (!all) {
            error e(all.error(), 0);
            co_return unexpected<error>(std::move(e));
        }
        co_return parse(string(*all), o);
    }

    inline string icalendar::to_string() const {
        std::string out;
        struct Frame {
            const icalendar* c;
            size_t next;
        };
        std::vector<Frame> stack{{this, 0}};
        auto begin = [&](const icalendar& c) {
            if (!detail::cl_valid_name(c.name().view())) {
                throw invalid_argument("sgcl::encoding::icalendar::to_string: a component's name that is none");
            }
            out += "BEGIN:";
            out.append(c.name().view());
            out += "\r\n";
            for (const property& p : c.properties()) {
                out.append(p.to_string().view());
            }
        };
        begin(*this);
        while (!stack.empty()) {
            Frame& f = stack.back();
            auto cs = f.c->components();
            if (f.next == cs.size()) {
                out += "END:";
                out.append(f.c->name().view());
                out += "\r\n";
                stack.pop_back();
                continue;
            }
            const icalendar* child = &cs[f.next++];
            begin(*child);
            stack.push_back({child, 0});
        }
        return string(out);
    }

    namespace detail {
        // The clock of a property of a calendar: UTC, a VTIMEZONE of it, a
        // zone of the system, or floating
        inline bool ical_clock(const icalendar& cal, const content_line& p, const time::zone& floating, const RecMoment& m, IcalClock& clock,
                               int64_t until) {
            if (m.utc) {
                clock.k = IcalClock::kind::utc;
                return true;
            }
            auto tzid = p.param("TZID");
            if (!tzid || m.date) {
                clock.k = IcalClock::kind::floating;
                clock.zone = floating;
                return true;
            }
            for (const icalendar& c : cal.components()) {
                if (c.name().view() == "VTIMEZONE" && c.text("TZID", string()) == *tzid) {
                    auto z = std::make_shared<IcalZone>(c, until);
                    if (!z->empty()) {
                        clock.k = IcalClock::kind::vtimezone;
                        clock.vz = std::move(z);
                        return true;
                    }
                }
            }
            auto z = time::zone::load(*tzid);
            if (!z) {
                return false;
            }
            clock.k = IcalClock::kind::system;
            clock.zone = *z;
            return true;
        }
    }

    inline optional<time::datetime> icalendar::datetime_of(const property& p) const noexcept {
        return datetime_of(p, time::zone::local());
    }

    inline optional<time::datetime> icalendar::datetime_of(const property& p, const time::zone& floating) const noexcept {
        detail::RecMoment m;
        if (!detail::rec_moment(p.value().view(), m) || !detail::cl_in_range(m.wall)) {
            return nullopt;
        }
        detail::IcalClock clock;
        if (!detail::ical_clock(*this, p, floating, m, clock, m.wall + 366 * 86400)) {
            return nullopt;
        }
        return clock.show(clock.resolve(m.wall));
    }

    inline vector<time::datetime> icalendar::occurrences(const icalendar& component, const time::datetime& from, const time::datetime& to) const {
        return occurrences(component, from, to, 100000, time::zone::local());
    }

    inline vector<time::datetime> icalendar::occurrences(const icalendar& component, const time::datetime& from, const time::datetime& to, size_t limit) const {
        return occurrences(component, from, to, limit, time::zone::local());
    }

    inline vector<time::datetime> icalendar::occurrences(const icalendar& component, const time::datetime& from, const time::datetime& to, size_t limit,
                                                         const time::zone& floating) const {
        vector<time::datetime> result;
        auto start = component.property_of("DTSTART");
        detail::RecMoment m;
        if (!start || !detail::rec_moment(start->value().view(), m) || !detail::cl_in_range(m.wall)) {
            return result;
        }
        detail::IcalClock clock;
        if (!detail::ical_clock(*this, *start, floating, m, clock, std::max(m.wall, to.unix()) + 2 * 366 * 86400)) {
            return result;
        }
        detail::RecResolve resolve = [&clock](int64_t w) { return clock.resolve(w); };
        int64_t lo = from.unix() - 1, hi = to.unix() + 1;
        std::vector<int64_t> instants;
        auto rules = component.properties_of("RRULE");
        if (rules.empty()) {
            int64_t at = clock.resolve(m.wall);
            if (at >= lo && at < hi) {
                instants.push_back(at);
            }
        }
        for (const content_line& r : rules) {
            auto rule = r.as_recurrence();
            if (!rule) {
                continue;
            }
            for (int64_t w : detail::rec_expand(detail::RecurrenceAccess::data(*rule), m.wall, m.date, resolve, lo, hi, limit > SIZE_MAX - 4 ? limit : limit + 4)) {
                instants.push_back(clock.resolve(w));
            }
        }
        // RDATE: DATE-TIMEs, DATEs or PERIODs (their starts), in their own TZID or DTSTART's clock
        for (const content_line& r : component.properties_of("RDATE")) {
            for (const string& v : r.list()) {
                std::string_view sv = v.view();
                sv = sv.substr(0, sv.find('/'));
                detail::RecMoment rm;
                detail::IcalClock rc;
                if (!detail::rec_moment(sv, rm) || !detail::cl_in_range(rm.wall)) {
                    continue;
                }
                if (rm.utc) {
                    rc.k = detail::IcalClock::kind::utc;
                } else if (r.param("TZID") && r.param("TZID") != start->param("TZID")) {
                    if (!detail::ical_clock(*this, r, floating, rm, rc, rm.wall + 366 * 86400)) {
                        continue;
                    }
                } else {
                    rc = clock;
                }
                int64_t at = rc.resolve(rm.wall);
                if (at >= lo && at < hi) {
                    instants.push_back(at);
                }
            }
        }
        std::sort(instants.begin(), instants.end());
        instants.erase(std::unique(instants.begin(), instants.end()), instants.end());
        // EXDATE: DATE-TIMEs by their instants, DATEs by DTSTART's date
        std::vector<int64_t> ex_instants, ex_days;
        for (const content_line& r : component.properties_of("EXDATE")) {
            for (const string& v : r.list()) {
                detail::RecMoment em;
                detail::IcalClock ec;
                if (!detail::rec_moment(v.view(), em) || !detail::cl_in_range(em.wall)) {
                    continue;
                }
                if (em.date) {
                    ex_days.push_back(detail::rec_floor_div(em.wall, 86400));
                    continue;
                }
                if (em.utc) {
                    ec.k = detail::IcalClock::kind::utc;
                } else if (r.param("TZID") && r.param("TZID") != start->param("TZID")) {
                    if (!detail::ical_clock(*this, r, floating, em, ec, em.wall + 366 * 86400)) {
                        continue;
                    }
                } else {
                    ec = clock;
                }
                ex_instants.push_back(ec.resolve(em.wall));
            }
        }
        for (int64_t at : instants) {
            if (std::find(ex_instants.begin(), ex_instants.end(), at) != ex_instants.end()) {
                continue;
            }
            if (!ex_days.empty()) {
                time::datetime shown = clock.show(at);
                int64_t day = std::chrono::sys_days(shown.date()).time_since_epoch().count();
                if (std::find(ex_days.begin(), ex_days.end(), day) != ex_days.end()) {
                    continue;
                }
            }
            time::datetime t = clock.show(at);
            if (t >= from && t < to && result.size() < limit) {
                result.push_back(t);
            }
        }
        return result;
    }

    namespace detail {
        inline async::task<expected<void, icalendar::error>> icalendar_save_task(string path, icalendar value) noexcept {
            co_return co_await async::spawn_blocking([path, value] { return value.save(path); });
        }
    }

    inline expected<icalendar, icalendar::error> icalendar::load(const string& path) {
        return detail::with_file(path, [](const io::reader& in) { return icalendar::parse(in); });
    }

    inline async::task<expected<icalendar, icalendar::error>> icalendar::async_load(string path) noexcept {
        co_return co_await async::spawn_blocking([path] { return icalendar::load(path); });
    }

    inline expected<void, icalendar::error> icalendar::save(const string& path) const {
        return detail::save_document(path, to_string());
    }

    inline async::task<expected<void, icalendar::error>> icalendar::async_save(string path) const noexcept {
        return detail::icalendar_save_task(std::move(path), *this);
    }
}
