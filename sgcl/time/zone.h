//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../concurrent/map.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/duration.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/root_ptr.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../core/weak_ptr.h"
#include "detail/files.h"
#include "detail/posix_rule.h"
#include "detail/tzif.h"
#include "error.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <string_view>

namespace sgcl::time {
    class datetime;
    class zone;

    namespace detail {
        using namespace sgcl::detail;

        struct zone_access;

        // A local time type of a zone: the offset from UTC in seconds
        // east, whether it is daylight saving time, the abbreviation
        struct zone_type {
            int32_t offset = 0;
            bool dst = false;
            string abbreviation;
        };

        // The part of a POSIX TZ rule the lookups need, the names turned
        // into two types of the zone
        struct zone_rule {
            int32_t std_offset = 0;
            int32_t dst_offset = 0;
            bool has_dst = false;
            posix_date start, end;
            int32_t start_time = 0;
            int32_t end_time = 0;
            uint16_t std_type = 0;
            uint16_t dst_type = 0;
        };

        // What a zone is made of, built once and never changed. A managed
        // object that the registry below holds for the rest of the
        // program, so that a zone may point at it with a raw pointer and a
        // datetime stays a value of sixteen bytes that any memory may
        // hold, as std::chrono::tzdb hands out a const time_zone*.
        //
        // The table is the file's transitions with those that change
        // nothing dropped (every entry left changes the offset, the flag
        // of daylight saving time or the abbreviation), and then the
        // changes of the POSIX rule through the year 2100 appended to it,
        // so that every time up to then is a binary search; a later time
        // asks the rule, which is what the rule is there for.
        // The tag of the constructors of zone and datetime that the
        // library uses to make one in place (in the expected it returns)
        struct made_in_place {};

        struct zone_data {
            bool fixed = false;             // a fixed offset: fixed_offset and nothing else
            int32_t fixed_offset = 0;
            string name;
            vector<int64_t> at;             // seconds since 1970 UTC, ascending
            vector<uint16_t> type_of;       // the type each entry goes to
            vector<zone_type> types;        // type 0 is the time before the first entry
            bool has_rule = false;          // the rule governs after the last entry
            bool rule_everywhere = false;   // and before the first too: a zone of a rule alone
            zone_rule rule;

            // Whether the rule has daylight saving time at t
            bool rule_dst(int64_t t) const noexcept {
                int64_t y = year_of(t, rule.std_offset);
                // The changes of the year before, this one and the next,
                // in order; the last at or before t says which time it is
                // (the year before settles one early in January in the
                // south, where the time is still the summer's)
                bool dst = false;
                int64_t best = INT64_MIN;
                for (int64_t k = y - 1; k <= y + 1; ++k) {
                    posix_year c = posix_changes(rule, k);
                    // Of two changes at the same moment the later of the
                    // year's order wins: the end of one year and the start
                    // of the next, both at New Year in a zone of daylight
                    // saving time all year, leave it on
                    int64_t first = std::min(c.start, c.end);
                    int64_t second = std::max(c.start, c.end);
                    bool first_dst = c.start <= c.end;
                    if (first <= t && first >= best) {
                        best = first;
                        dst = first_dst;
                    }
                    if (second <= t && second >= best) {
                        best = second;
                        dst = !first_dst;
                    }
                }
                return dst;
            }

            uint16_t rule_type(int64_t t) const noexcept {
                if (!rule.has_dst) {
                    return rule.std_type;
                }
                return rule_dst(t) ? rule.dst_type : rule.std_type;
            }

            // The type in force at t (seconds since 1970 UTC)
            uint16_t type_at(int64_t t) const noexcept {
                size_t n = at.size();
                if (n == 0 || t < at[0]) {
                    return (rule_everywhere || (n == 0 && has_rule)) ? rule_type(t) : 0;
                }
                const int64_t* first = at.data();
                size_t i = size_t(std::upper_bound(first, first + n, t) - first) - 1;
                if (i == n - 1 && has_rule) {
                    return rule_type(t);
                }
                return type_of[i];
            }

            // The rule's changes after t, or before it, that change
            // something (the end of one year of daylight saving time all
            // year and the start of the next are not one), looked for
            // over a few years at most
            optional<int64_t> rule_next(int64_t t) const noexcept {
                if (!rule.has_dst) {
                    return nullopt;
                }
                int64_t y = year_of(t, rule.std_offset);
                int64_t best = INT64_MAX;
                for (int64_t k = y - 1; k <= y + 2; ++k) {
                    posix_year c = posix_changes(rule, k);
                    for (int64_t e : {c.start, c.end}) {
                        if (e > t && e < best && rule_dst(e - 1) != rule_dst(e)) {
                            best = e;
                        }
                    }
                }
                return best == INT64_MAX ? nullopt : optional<int64_t>(best);
            }

            optional<int64_t> rule_previous(int64_t t) const noexcept {
                if (!rule.has_dst) {
                    return nullopt;
                }
                int64_t y = year_of(t, rule.std_offset);
                int64_t best = INT64_MIN;
                for (int64_t k = y - 2; k <= y + 1; ++k) {
                    posix_year c = posix_changes(rule, k);
                    for (int64_t e : {c.start, c.end}) {
                        if (e < t && e > best && rule_dst(e - 1) != rule_dst(e)) {
                            best = e;
                        }
                    }
                }
                return best == INT64_MIN ? nullopt : optional<int64_t>(best);
            }

            // The first change after t
            optional<int64_t> next_change(int64_t t) const noexcept {
                size_t n = at.size();
                const int64_t* first = at.data();
                if (n == 0 || (rule_everywhere && t < at[0])) {
                    if (has_rule) {
                        auto r = rule_next(t);
                        if (r && (n == 0 || *r < at[0])) {
                            return r;
                        }
                    }
                    if (n == 0) {
                        return nullopt;
                    }
                }
                size_t j = size_t(std::upper_bound(first, first + n, t) - first);
                if (j < n) {
                    return at[j];
                }
                return has_rule ? rule_next(t) : nullopt;
            }

            // The last change before t
            optional<int64_t> previous_change(int64_t t) const noexcept {
                size_t n = at.size();
                const int64_t* first = at.data();
                if (n != 0 && has_rule && t > at[n - 1]) {
                    auto r = rule_previous(t);
                    if (r && *r > at[n - 1]) {
                        return r;
                    }
                    return at[n - 1];
                }
                size_t j = size_t(std::lower_bound(first, first + n, t) - first);
                if (j > 0) {
                    return at[j - 1];
                }
                if (has_rule && (n == 0 || rule_everywhere)) {
                    return rule_previous(t);
                }
                return nullopt;
            }
        };

        // The last year the table carries the rule to
        inline constexpr int64_t ExpandedUntil = 2100;

        inline uint16_t find_or_add_type(vector<zone_type>& types, int32_t offset, bool dst, std::string_view abbreviation) {
            for (size_t i = 0; i < types.size(); ++i) {
                if (types[i].offset == offset && types[i].dst == dst && std::string_view(types[i].abbreviation) == abbreviation) {
                    return uint16_t(i);
                }
            }
            types.push_back(zone_type{offset, dst, string(abbreviation)});
            return uint16_t(types.size() - 1);
        }

        inline bool same_type(const zone_type& a, const zone_type& b) noexcept {
            return a.offset == b.offset && a.dst == b.dst && a.abbreviation == b.abbreviation;
        }

        // The zone built from what a file or a rule gave: the entries that
        // change nothing dropped, the rule's changes appended to 2100
        inline tracked_ptr<zone_data> build_zone(const string& name, const tzif_data& file, const optional<posix_rule>& rule) {
            auto z = make_tracked<zone_data>();
            z->name = name;
            for (auto& t : file.types) {
                z->types.push_back(zone_type{t.offset, t.dst, string(t.abbreviation)});
            }
            uint16_t current = 0;
            for (size_t i = 0; i < file.at.size(); ++i) {
                uint16_t type = file.type_of[i];
                if (same_type(z->types[type], z->types[current])) {
                    continue;
                }
                z->at.push_back(file.at[i]);
                z->type_of.push_back(type);
                current = type;
            }
            if (!rule) {
                return z;
            }
            z->has_rule = true;
            z->rule_everywhere = file.at.empty();
            zone_rule& r = z->rule;
            r.std_offset = rule->std_offset;
            r.dst_offset = rule->dst_offset;
            r.has_dst = rule->has_dst;
            r.start = rule->start;
            r.end = rule->end;
            r.start_time = rule->start_time;
            r.end_time = rule->end_time;
            r.std_type = find_or_add_type(z->types, rule->std_offset, false, rule->std_name);
            r.dst_type = rule->has_dst ? find_or_add_type(z->types, rule->dst_offset, true, rule->dst_name) : r.std_type;
            // RFC 9636: the rule gives the time from the file's last
            // transition on — the last of the file, not of the entries kept
            // — and where the file disagrees with it there, the rule wins
            bool from_file = !file.at.empty();
            int64_t last = from_file ? file.at.back() : INT64_MIN;
            if (from_file) {
                uint16_t there = z->rule_type(last);
                if (!z->at.empty() && z->at.back() == last) {
                    z->type_of.back() = there;
                    uint16_t before = z->at.size() > 1 ? z->type_of[z->at.size() - 2] : 0;
                    if (same_type(z->types[before], z->types[there])) {
                        z->at.pop_back();
                        z->type_of.pop_back();
                    }
                } else if (!same_type(z->types[there], z->types[current])) {
                    z->at.push_back(last);
                    z->type_of.push_back(there);
                }
                current = there;
            }
            if (!rule->has_dst) {
                return z;
            }
            // The rule's changes appended, from the year of the file's last
            // transition (or from 1900 for a zone of a rule alone, whose
            // time before then the rule still gives) through 2100
            int64_t from = from_file ? year_of(last, r.std_offset) : 1900;
            if (!from_file) {
                // Before the first entry the rule answers (rule_everywhere),
                // so the first entry is the first change of the rule
                current = z->rule_type(days_from_civil(from, 1) * 86400 - r.std_offset);
            }
            for (int64_t y = from; y <= ExpandedUntil; ++y) {
                posix_year c = posix_changes(z->rule, y);
                int64_t first = std::min(c.start, c.end);
                int64_t second = std::max(c.start, c.end);
                for (int64_t e : {first, second}) {
                    if (e <= last) {
                        continue;
                    }
                    uint16_t type = z->rule_type(e);
                    if (same_type(z->types[type], z->types[current])) {
                        continue;
                    }
                    z->at.push_back(e);
                    z->type_of.push_back(type);
                    current = type;
                    last = e;
                }
            }
            return z;
        }
    }

    namespace detail {
        // The zones of the program. Held for the rest of it: the ones of
        // the system's database by name, and the fixed offsets — finitely
        // many of both — and the local zone. Not held: a zone made from
        // bytes or a TZ string that came from anywhere (from_tzif,
        // from_posix), which lives while a zone or a datetime has it and
        // is found again by what it was made of while it does (the same
        // bytes under the same name are the same zone); its entry dies
        // with it and is swept, so input from outside does not grow this.
        struct zone_registry {
            concurrent::map<string, tracked_ptr<zone_data>> by_name;
            concurrent::map<string, weak_ptr<zone_data>> by_content;
            std::atomic<size_t> content_inserts{0};        // since the last sweep of by_content
            std::atomic<bool> sweeping{false};
            array<tracked_ptr<zone_data>, 193> quarters;   // the offsets of whole quarters of an hour, -24h to +24h
            concurrent::map<int32_t, tracked_ptr<zone_data>> by_offset;
            tracked_ptr<zone_data> local;                  // the local zone's data, whatever it was made from
        };

        // The text of an offset: "+05:30", "-03:30", "+05:30:15"
        inline string offset_text(int32_t offset) {
            char text[16];
            size_t n = 0;
            text[n++] = offset < 0 ? '-' : '+';
            int32_t a = offset < 0 ? -offset : offset;
            auto two = [&](int v) {
                text[n++] = char('0' + v / 10);
                text[n++] = char('0' + v % 10);
            };
            two(a / 3600);
            text[n++] = ':';
            two(a / 60 % 60);
            if (a % 60) {
                text[n++] = ':';
                two(a % 60);
            }
            return string(text, n);
        }

        // A fixed offset as a zone of one type and no changes: every
        // lookup takes the same road as for any zone (type_at is 0 at once)
        inline tracked_ptr<zone_data> make_fixed_zone(int32_t offset, const string& name) {
            auto d = make_tracked<zone_data>();
            d->fixed = true;
            d->fixed_offset = offset;
            d->name = name;
            d->types.push_back(zone_type{offset, false, name});
            return d;
        }

        inline zone_registry& registry() {
            // A root_ptr, as stencil's table of functions: the registry
            // holds tracked pointers and lives as long as the program does.
            // Never destroyed: a zone held by a static of the program, or
            // used from an atexit handler, points into it after every
            // destructor of statics has run
            static root_ptr<zone_registry>* r = [] {
                auto* p = new root_ptr<zone_registry>(make_tracked<zone_registry>());
                for (int i = 0; i < 193; ++i) {
                    int32_t offset = (i - 96) * 900;
                    (*p)->quarters[size_t(i)] = make_fixed_zone(offset, i == 96 ? string("UTC") : offset_text(offset));   // offset 0 is UTC's own data
                }
                return p;
            }();
            return **r;
        }

        // The registry's zones of whole quarters of an hour as plain
        // pointers, taken once (the registry holds them for the program)
        inline const std::array<const zone_data*, 193>& quarter_zones() {
            static const std::array<const zone_data*, 193> quarters = [] {
                std::array<const zone_data*, 193> t{};
                auto& r = registry();
                for (size_t i = 0; i < t.size(); ++i) {
                    t[i] = r.quarters[i].get();
                }
                return t;
            }();
            return quarters;
        }

        // UTC's data: a fixed offset of 0 named "UTC", so that every lookup
        // has an object to refer to
        inline const zone_data& utc_data() {
            return *quarter_zones()[96];
        }

        // The data of a fixed offset: one object per offset, for the
        // program's life (there are finitely many)
        inline const zone_data& fixed_zone(int32_t seconds) {
            if (seconds % 900 == 0) {
                return *quarter_zones()[size_t(seconds / 900 + 96)];
            }
            auto& r = registry();
            auto found = r.by_offset.find(seconds);
            if (found != r.by_offset.end()) {
                return *found->second.get();
            }
            auto [it, inserted] = r.by_offset.try_emplace(seconds, make_fixed_zone(seconds, offset_text(seconds)));
            return *it->second.get();
        }

        // Where the systems keep the database: Linux and macOS (whose
        // /usr/share/zoneinfo is a link to the second), then Solaris and
        // the rarer places Go also looks in
        inline constexpr const char* ZoneDirectories[] = {
            "/usr/share/zoneinfo/",
            "/var/db/timezone/zoneinfo/",
            "/usr/share/lib/zoneinfo/",
            "/usr/lib/locale/TZ/",
            "/etc/zoneinfo/",
        };

        // A name of the database, and not a path out of it: no "..", no
        // leading slash, nothing that is not a character of a name there
        inline bool zone_name_ok(std::string_view name) noexcept {
            if (name.empty() || name.size() > 255 || name[0] == '/' || name[0] == '.' || name.back() == '/') {
                return false;
            }
            // One name for one zone: no empty part and no "." part, which
            // would read the same file under another name
            if (name.find("//") != std::string_view::npos || name.find("/./") != std::string_view::npos
                || name.find("/.") != std::string_view::npos) {
                return false;
            }
            // The copy of the database that counts leap seconds in its
            // times is not POSIX time, which a datetime is
            if (name.substr(0, 6) == "right/") {
                return false;
            }
            for (size_t i = 0; i < name.size(); ++i) {
                char c = name[i];
                bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                       || c == '/' || c == '_' || c == '-' || c == '+' || c == '.';
                if (!ok || (c == '.' && i + 1 < name.size() && name[i + 1] == '.')) {
                    return false;
                }
            }
            return true;
        }

        inline slice<const byte> bytes_of(const std::string& s) noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
        }

        // A zone of the database from its bytes: the file read, the
        // footer read as a rule
        inline expected<tracked_ptr<zone_data>, error> zone_from_bytes(const slice<const byte>& bytes, const string& name) {
            auto file = read_tzif(bytes);
            if (!file) {
                return unexpected(file.error());
            }
            optional<posix_rule> rule;
            if (!file->footer.empty()) {
                auto r = read_posix(file->footer);
                if (!r) {
                    // The offset of the footer's character in the file
                    size_t at = bytes.size() - 1 - file->footer.size() + r.error().offset();
                    return unexpected(error(string("TZif: the footer is not a POSIX TZ string: " + std::string(r.error().message())), at));
                }
                rule = std::move(*r);
            }
            return build_zone(name, *file, rule);
        }

        inline expected<tracked_ptr<zone_data>, error> zone_from_rule(const string& text) {
            auto r = read_posix(std::string_view(text));
            if (!r) {
                return unexpected(r.error());
            }
            tzif_data file;
            file.version = 3;
            file.types.push_back(tzif_type{r->std_offset, false, r->std_name});
            return build_zone(text, file, *r);
        }
    }

    // A time zone: the rules that give the offset from UTC of a place's
    // clock at every moment, with the abbreviation and whether it is
    // daylight saving time. UTC, a fixed offset, a zone of the system's
    // tz database ("Europe/Warsaw"), one read from the bytes of a TZif
    // file of any origin, or one from a POSIX TZ string.
    //
    // A value of one word, copied freely and kept anywhere: UTC and a
    // fixed offset are the word itself, nothing allocated; any other zone
    // points at data that the library keeps for the rest of the program,
    // read once — the second load of a name reads nothing. Two zones are
    // equal when they are the same zone: the same name read from the same
    // place, the same offset. Nothing here waits once a zone is loaded.
    class zone {
    public:
        // UTC
        zone() noexcept = default;

        // For the library (detail): the zone of this data, made where it goes
        zone(detail::made_in_place, const detail::zone_data& data) noexcept
        : _ptr(&data) {
        }

        static zone utc() noexcept {
            return zone();
        }

        // A zone that is always `offset` east of UTC, named for it
        // ("+05:30"); whole seconds, less than a day. zone::fixed(0) is UTC
        static zone fixed(duration offset) {
            int64_t seconds = offset.nanoseconds() / 1000000000;
            if (seconds <= -86400 || seconds >= 86400) {
                throw invalid_argument("sgcl::time::zone::fixed: an offset of a day or more");
            }
            if (seconds == 0) {
                return zone();
            }
            return zone(detail::made_in_place(), detail::fixed_zone(int32_t(seconds)));
        }

        // A zone of the system's database by its name: "Europe/Warsaw",
        // "America/New_York", "UTC". Read from /usr/share/zoneinfo (and
        // the other places systems keep it) the first time, from memory
        // every time after. An unknown name, a name that is not one
        // ("../x"), or a file that is not a TZif file is an error
        static expected<zone, error> load(const string& name);

        // A zone from the bytes of a TZif file (RFC 9636, versions 1 to
        // 4), which may come from anywhere: a database of one's own, the
        // network. The same bytes under the same name give the same zone
        static expected<zone, error> from_tzif(const slice<const byte>& data, const string& name);

        // A zone from a POSIX TZ string alone: "CET-1CEST,M3.5.0,M10.5.0/3";
        // named by it
        static expected<zone, error> from_posix(const string& rule);

        // The zone of this computer: the TZ variable of the environment
        // (":Europe/Warsaw", "Europe/Warsaw", a path to a TZif file, or a
        // POSIX TZ string; empty means UTC), else the zone /etc/localtime
        // is, else UTC. Settled once, the first time it is asked for
        static zone local();

        // The names of the zones of the system's database, sorted
        static vector<string> available();

        // What the zone is at an instant: the offset from UTC (+2h in
        // Warsaw in summer), the abbreviation ("CEST"; for UTC "UTC", for
        // a fixed offset its name), whether it is daylight saving time
        duration offset_at(const datetime& t) const;
        string abbreviation_at(const datetime& t) const;
        bool is_dst_at(const datetime& t) const;

        // The first change of the zone after t and the last before it
        // (strictly: a change at t itself is neither), in this zone; a
        // change is one of the offset, of daylight saving time or of the
        // abbreviation. Nothing for UTC and a fixed offset, and nothing
        // past the last change of a zone that stopped changing
        optional<datetime> next_transition(const datetime& t) const;
        optional<datetime> previous_transition(const datetime& t) const;

        // "Europe/Warsaw", "UTC", "+05:30", the TZ string of a zone made
        // from one; a zone read from /etc/localtime that is not a link
        // into the database is "Local"
        string name() const;

        friend bool operator==(const zone& a, const zone& b) noexcept {
            const detail::zone_data* pa = a._ptr.get();   // null and UTC's data are both UTC
            const detail::zone_data* pb = b._ptr.get();
            return (pa ? pa : &detail::utc_data()) == (pb ? pb : &detail::utc_data());
        }

    private:
        friend struct detail::zone_access;

        // null: UTC (zone() allocates nothing); otherwise the zone's data,
        // a fixed offset's too. Read once by zone_access::data, and the
        // data passed on by reference
        tracked_ptr<const detail::zone_data> _ptr;
    };

    namespace detail {
        // What the zone is at one moment
        struct zone_state {
            int32_t offset = 0;
            bool dst = false;
            const string* abbreviation = nullptr;   // null for UTC and a fixed offset
        };

        // The lookups the zone's members and datetime are made of, in
        // seconds since 1970 UTC
        struct zone_access {
            // The zone's data, its pointer read once: the public call that
            // holds the zone asks this, and passes the data on by reference
            static const zone_data& data(const zone& z) noexcept {
                auto p = sgcl::detail::load_plain(z._ptr);   // a value's own word, which no other thread writes while this one reads it
                return p ? *p : utc_data();
            }

            static zone_state state_at(const zone_data& d, int64_t t) noexcept {
                if (d.fixed) {
                    return {d.fixed_offset, false, nullptr};
                }
                const zone_type& type = d.types[d.type_at(t)];
                return {type.offset, type.dst, &type.abbreviation};
            }

            static int32_t offset_at(const zone_data& d, int64_t t) noexcept {
                return d.fixed ? d.fixed_offset : d.types[d.type_at(t)].offset;
            }

            // The offset asked of the zone itself, its word read once: UTC's
            // is 0 with no data to look at (datetime's fields ask this)
            static int32_t offset_at(const zone& z, int64_t t) noexcept {
                auto p = sgcl::detail::load_plain(z._ptr);
                return p ? offset_at(*p, t) : 0;
            }

            static optional<int64_t> next_change(const zone_data& d, int64_t t) noexcept {
                return d.fixed ? nullopt : d.next_change(t);
            }

            static optional<int64_t> previous_change(const zone_data& d, int64_t t) noexcept {
                return d.fixed ? nullopt : d.previous_change(t);
            }

            // The abbreviation of a state of the zone; a fixed offset's and
            // UTC's are their names ("+05:30", "UTC")
            static string abbreviation(const zone_data& d, const zone_state& s) {
                return s.abbreviation ? *s.abbreviation : d.name;
            }
        };

        // The entries of by_content whose zones are gone, dropped with their
        // keys (the bytes of a file)
        inline void sweep_dead(zone_registry& r) {
            for (auto it = r.by_content.begin(); it != r.by_content.end();) {
                if (it->second.expired()) {
                    it = r.by_content.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // sweep_dead by one thread at a time, once the map has had half as
        // many insertions as it has entries (64 at least): the cost of a
        // sweep spread over the insertions that made the dead entries
        inline void sweep_content(zone_registry& r) {
            size_t n = r.content_inserts.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n < std::max<size_t>(64, r.by_content.size() / 2) || r.sweeping.exchange(true, std::memory_order_acquire)) {
                return;
            }
            r.content_inserts.store(0, std::memory_order_relaxed);
            sweep_dead(r);
            r.sweeping.store(false, std::memory_order_release);
        }

        // The zone made from `key` (the bytes of a file and a name, or a TZ
        // string) while it lives, made by `make` otherwise; two threads
        // that make it at once both keep the one that got in first
        template<class Make>
        expected<zone, error> intern_foreign(const string& key, Make&& make) {
            auto& r = registry();
            for (;;) {
                auto found = r.by_content.find(key);
                if (found != r.by_content.end()) {
                    if (auto held = found->second.lock()) {
                        return expected<zone, error>(std::in_place, made_in_place(), *held);
                    }
                    r.by_content.erase(found);   // its zone is gone: made again below
                    continue;
                }
                expected<tracked_ptr<zone_data>, error> made = make();
                if (!made) {
                    return unexpected(made.error());
                }
                auto [it, inserted] = r.by_content.try_emplace(key, weak_ptr<zone_data>(*made));
                if (inserted) {
                    sweep_content(r);
                    return expected<zone, error>(std::in_place, made_in_place(), **made);
                }
                if (auto held = it->second.lock()) {
                    return expected<zone, error>(std::in_place, made_in_place(), *held);
                }
                r.by_content.erase(it);   // the one that got in first is gone already
            }
        }

        // The zone kept under `key` in `table` for the program, made by
        // `make` if nobody has made it yet; two threads that make it at
        // once both keep the one that got in first
        template<class Make>
        expected<zone, error> intern_zone(concurrent::map<string, tracked_ptr<zone_data>>& table, const string& key, Make&& make) {
            auto found = table.find(key);
            if (found != table.end()) {
                return expected<zone, error>(std::in_place, made_in_place(), *found->second.get());
            }
            expected<tracked_ptr<zone_data>, error> made = make();
            if (!made) {
                return unexpected(made.error());
            }
            auto [it, inserted] = table.try_emplace(key, *made);
            return expected<zone, error>(std::in_place, made_in_place(), *it->second.get());
        }

        // The zone a TZ variable names, or nullopt when it names none
        inline optional<zone> zone_from_tz(const string& tz) {
            std::string_view v(tz);
            if (v.empty()) {
                return zone::utc();
            }
            std::string_view name = v[0] == ':' ? v.substr(1) : v;
            if (!name.empty() && name[0] == '/') {
                auto bytes = read_small_file(std::string(name));
                if (bytes.error) {
                    return nullopt;
                }
                string zone_name("Local");
                if (size_t k = name.rfind("zoneinfo/"); k != std::string_view::npos && zone_name_ok(name.substr(k + 9))) {
                    zone_name = string(name.substr(k + 9));
                }
                auto z = zone::from_tzif(bytes_of(bytes.bytes), zone_name);
                return z ? optional<zone>(*z) : nullopt;
            }
            if (auto z = zone::load(string(name))) {
                return *z;
            }
            if (auto z = zone::from_posix(string(v))) {
                return *z;
            }
            return nullopt;
        }

        // The zone /etc/localtime is: the one of the database its link
        // points at, or the file's bytes under the name "Local"
        inline optional<zone> zone_from_localtime(const std::string& path) {
            if (auto target = link_target(path)) {
                std::string_view t(*target);
                if (size_t k = t.rfind("zoneinfo/"); k != std::string_view::npos) {
                    if (auto z = zone::load(string(t.substr(k + 9)))) {
                        return *z;
                    }
                }
            }
            auto bytes = read_small_file(path);
            if (bytes.error) {
                return nullopt;
            }
            auto z = zone::from_tzif(bytes_of(bytes.bytes), string("Local"));
            return z ? optional<zone>(*z) : nullopt;
        }

        inline zone find_local() {
            if (const char* tz = std::getenv("TZ")) {
                if (auto z = zone_from_tz(string(tz))) {
                    return *z;
                }
                return zone::utc();
            }
            if (auto z = zone_from_localtime("/etc/localtime")) {
                return *z;
            }
            return zone::utc();
        }
    }

    inline expected<zone, error> zone::load(const string& name) {
        std::string_view v(name);
        if (v == "UTC") {
            return zone::utc();
        }
        if (!detail::zone_name_ok(v)) {
            return unexpected(error(string("not a name of a time zone: \"" + std::string(v) + "\"")));
        }
        return detail::intern_zone(detail::registry().by_name, name, [&]() -> expected<tracked_ptr<detail::zone_data>, error> {
            for (const char* dir : detail::ZoneDirectories) {
                auto bytes = detail::read_small_file(std::string(dir) + std::string(v));
                if (bytes.error == ENOENT || bytes.error == ENOTDIR) {
                    continue;
                }
                if (bytes.error) {
                    return unexpected(error(string("time zone \"" + std::string(v) + "\": " + detail::error_text(bytes.error))));
                }
                auto z = detail::zone_from_bytes(detail::bytes_of(bytes.bytes), name);
                if (!z) {
                    return unexpected(error(string("time zone \"" + std::string(v) + "\": " + std::string(z.error().message())), z.error().offset()));
                }
                return *z;
            }
            return unexpected(error(string("unknown time zone \"" + std::string(v) + "\"")));
        });
    }

    inline expected<zone, error> zone::from_tzif(const slice<const byte>& data, const string& name) {
        std::string key = "T";
        key.append(name.data(), name.size());
        key += '\0';
        key.append(reinterpret_cast<const char*>(data.data()), data.size());
        return detail::intern_foreign(string(key), [&] {
            return detail::zone_from_bytes(data, name);
        });
    }

    inline expected<zone, error> zone::from_posix(const string& rule) {
        return detail::intern_foreign(string("P" + std::string(rule)), [&] {
            return detail::zone_from_rule(rule);
        });
    }

    namespace detail {
        // The data of the local zone (null: UTC), settled once; held by
        // the registry
        inline const zone_data& local_data() {
            static const zone_data* d = [] {
                zone z = find_local();
                const zone_data& data = zone_access::data(z);
                registry().local = tracked_ptr<zone_data>(const_cast<zone_data*>(&data));   // held for the program: it may have come from a TZ string or a file
                return &data;
            }();
            return *d;
        }
    }

    inline zone zone::local() {
        return zone(detail::made_in_place(), detail::local_data());
    }

    inline vector<string> zone::available() {
        for (const char* dir : detail::ZoneDirectories) {
            error_code ec;
            if (!std::filesystem::is_directory(dir, ec)) {
                continue;
            }
            // The files that start with "TZif" are zones; the tables and
            // the copies of the whole database for other uses (posix/,
            // right/) are not
            std::vector<std::string> names;
            for (auto& n : detail::files_under(dir, {"posix", "right"})) {
                std::string_view base(n);
                base = base.substr(base.rfind('/') == std::string_view::npos ? 0 : base.rfind('/') + 1);
                if (base == "posixrules" || base == "localtime" || !detail::zone_name_ok(n)) {
                    continue;
                }
                if (detail::starts_with(std::string(dir) + n, "TZif")) {
                    names.push_back(n);
                }
            }
            std::sort(names.begin(), names.end());
            vector<string> out;
            out.reserve(names.size());
            for (auto& n : names) {
                out.push_back(string(n));
            }
            return out;
        }
        return {};
    }

    inline string zone::name() const {
        return detail::zone_access::data(*this).name;
    }
}
