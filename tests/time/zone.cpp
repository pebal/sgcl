//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// time::zone: the reader of TZif (RFC 9636), the POSIX TZ string, the
// registry, fixed offsets and the local zone. Against Go
// (tools/time_oracle.go zones): every zone of the system's database and a
// list of TZ strings, each change from 1800 to 2100 and instants to 2400;
// against files built here byte by byte for what a file may get wrong.
#include "sgcl/time/time.h"
#include "tests/time/zone_cases.h"
#include "tests/types.h"

#include <atomic>
#include <cstring>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    using time::zone;
    using access = time::detail::zone_access;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // The zone at t as the oracle writes it
    struct State {
        int32_t offset;
        bool dst;
        std::string abbreviation;

        bool operator==(const State&) const = default;
    };

    std::ostream& operator<<(std::ostream& os, const State& s) {
        return os << "{" << s.offset << ", " << s.dst << ", " << s.abbreviation << "}";
    }

    State state(const zone& z, int64_t t) {
        auto s = access::state_at(access::data(z), t);
        return {s.offset, s.dst, text(access::abbreviation(access::data(z), s))};
    }

    State expected_state(const oracle::ZoneState& s) {
        return {s.offset, s.dst, oracle::ZoneAbbreviations[s.abbreviation]};
    }

    std::string system_version() {
        std::ifstream f("/usr/share/zoneinfo/+VERSION");
        std::string v;
        std::getline(f, v);
        return v;
    }

    // A TZif file built from its parts, version 1 (one block of 32-bit
    // times) or 2 and later (the same block, then 64-bit times and the
    // footer); what a test wants broken it breaks in the bytes after
    struct Tzif {
        char version = '2';
        std::vector<int64_t> at;
        std::vector<uint8_t> type_of;
        struct Type {
            int32_t offset;
            uint8_t dst;
            uint8_t index;
        };
        std::vector<Type> types{{0, 0, 0}};
        std::string chars = std::string("UTC", 4);
        std::vector<std::pair<int64_t, int32_t>> leaps;
        std::vector<uint8_t> isstd, isut;
        std::string footer = "UTC0";

        std::string bytes() const {
            std::string b;
            auto u32 = [&](uint32_t v) {
                b += char(v >> 24);
                b += char(v >> 16);
                b += char(v >> 8);
                b += char(v);
            };
            auto header = [&](char v) {
                b += "TZif";
                b += v;
                b += std::string(15, '\0');
                u32(uint32_t(isut.size()));
                u32(uint32_t(isstd.size()));
                u32(uint32_t(leaps.size()));
                u32(uint32_t(at.size()));
                u32(uint32_t(types.size()));
                u32(uint32_t(chars.size()));
            };
            auto block = [&](int width) {
                for (int64_t t : at) {
                    if (width == 8) {
                        u32(uint32_t(uint64_t(t) >> 32));
                    }
                    u32(uint32_t(t));
                }
                for (uint8_t i : type_of) {
                    b += char(i);
                }
                for (auto& t : types) {
                    u32(uint32_t(t.offset));
                    b += char(t.dst);
                    b += char(t.index);
                }
                b += chars;
                for (auto& l : leaps) {
                    if (width == 8) {
                        u32(uint32_t(uint64_t(l.first) >> 32));
                    }
                    u32(uint32_t(l.first));
                    u32(uint32_t(l.second));
                }
                for (uint8_t v : isstd) {
                    b += char(v);
                }
                for (uint8_t v : isut) {
                    b += char(v);
                }
            };
            header(version == '1' ? '\0' : version);
            block(4);
            if (version != '1') {
                header(version);
                block(8);
                b += '\n';
                b += footer;
                b += '\n';
            }
            return b;
        }
    };

    slice<const byte> bytes_of(const std::string& s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    expected<time::detail::tzif_data, time::error> read(const std::string& s) {
        return time::detail::read_tzif(bytes_of(s));
    }

    std::string file_of(const std::string& name) {
        std::ifstream f("/usr/share/zoneinfo/" + name, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    // Where RFC 9636 and Go part before the first transition: the RFC
    // says type 0, Go's lookupFirstZone takes the first standard type
    // when type 0 is daylight saving time or is used by a transition.
    // With zic's output of today type 0 is the local mean time and never
    // a transition's, so the two agree everywhere; this list is empty and
    // the test says so, so that a change of either shows here.
    const std::set<std::string> FirstTypeDiffers = {};

    void check_changes(const zone& z, const oracle::ZoneCase& c, int64_t from) {
        State prev = expected_state(c.from);
        EXPECT_EQ(state(z, from), prev) << c.name << " at " << from;
        int64_t last = from;
        for (uint32_t i = 0; i < c.changes; ++i) {
            const auto& change = oracle::ZoneChanges[c.first_change + i];
            State to = expected_state(change.to);
            ASSERT_EQ(state(z, change.at - 1), prev) << c.name << " before " << change.at;
            ASSERT_EQ(state(z, change.at), to) << c.name << " at " << change.at;
            ASSERT_EQ(access::next_change(access::data(z), last), optional<int64_t>(change.at)) << c.name << " after " << last;
            ASSERT_EQ(access::next_change(access::data(z), change.at - 1), optional<int64_t>(change.at)) << c.name;
            ASSERT_EQ(access::previous_change(access::data(z), change.at + 1), optional<int64_t>(change.at)) << c.name;
            if (i != 0) {
                ASSERT_EQ(access::previous_change(access::data(z), change.at), optional<int64_t>(last)) << c.name;
            }
            prev = to;
            last = change.at;
        }
        for (uint32_t i = 0; i < c.samples; ++i) {
            const auto& s = oracle::ZoneSamples[c.first_sample + i];
            EXPECT_EQ(state(z, s.at), expected_state(s.to)) << c.name << " at " << s.at;
        }
    }
}

TEST(Zone_Tests, EveryZoneOfTheDatabaseAsGoReadsIt) {
    if (system_version() != oracle::ZoneVersion) {
        GTEST_SKIP() << "the system's tz database is " << system_version() << ", the cases were made from " << oracle::ZoneVersion;
    }
    std::vector<std::string> first_differs;
    size_t changes = 0;
    for (const auto& c : oracle::Zones) {
        auto z = zone::load(c.name);
        ASSERT_TRUE(z) << c.name << ": " << text(z.error().message());
        EXPECT_EQ(text(z->name()), c.name);
        check_changes(*z, c, -5364662400);   // 1800-01-01
        changes += c.changes;
        if (!(state(*z, -11676096000) == expected_state(c.before_all))) {   // 1600-01-01
            first_differs.push_back(c.name);
        }
    }
    EXPECT_EQ(std::set<std::string>(first_differs.begin(), first_differs.end()), FirstTypeDiffers);
    EXPECT_GT(changes, 50000u);   // the walk did walk
}

TEST(Zone_Tests, PosixRulesAsGoReadsThem) {
    for (const auto& c : oracle::PosixRules) {
        auto z = zone::from_posix(c.name);
        ASSERT_TRUE(z) << c.name << ": " << text(z.error().message());
        EXPECT_EQ(text(z->name()), c.name);
        check_changes(*z, c, 0);   // 1970-01-01
    }
}

TEST(Zone_Tests, ARuleInAFileWithNoTransitionIsTheRuleForAllTime) {
    for (const auto& c : oracle::PosixRules) {
        auto rule = time::detail::read_posix(c.name);
        ASSERT_TRUE(rule);
        Tzif f;
        f.types = {{rule->std_offset, 0, 0}};
        f.chars = rule->std_name + '\0';
        f.footer = c.name;
        auto z = zone::from_tzif(bytes_of(f.bytes()), c.name);
        ASSERT_TRUE(z) << c.name << ": " << text(z.error().message());
        check_changes(*z, c, 0);
        EXPECT_EQ(*z, *zone::from_tzif(bytes_of(f.bytes()), c.name));   // the same bytes, the same zone
        EXPECT_NE(*z, *zone::from_posix(c.name));                          // made another way
    }
}

// The two places tools/time_oracle.go leaves out, because Go 1.27 is
// wrong there, checked by hand
TEST(Zone_Tests, DaylightSavingTimeAllYear) {
    for (const char* rule : {"EST5EDT,0/0,J365/25", "WART4WARST,J1/0,J365/25"}) {
        auto z = zone::from_posix(rule);
        ASSERT_TRUE(z);
        bool est = rule[0] == 'E';
        State dst = est ? State{-14400, true, "EDT"} : State{-10800, true, "WARST"};
        for (int year = 1700; year <= 2400; year += 7) {
            int64_t new_year = time::detail::days_from_civil(year, 1) * 86400;
            for (int64_t t : {new_year - 1, new_year, new_year + 2 * 3600, new_year + 5 * 3600 - 1, new_year + 5 * 3600, new_year + 180 * 86400}) {
                ASSERT_EQ(state(*z, t), dst) << rule << " at " << t;
            }
        }
        EXPECT_EQ(access::next_change(access::data(*z), 0), nullopt) << rule;
        EXPECT_EQ(access::previous_change(access::data(*z), 0), nullopt) << rule;
    }
}

TEST(Zone_Tests, ARuleBefore1970) {
    // The last Sunday of March at 01:00 UTC and the last of October at
    // 01:00 UTC, every year from 1700, found with time::date
    auto z = zone::from_posix("CET-1CEST,M3.5.0,M10.5.0/3");
    ASSERT_TRUE(z);
    auto last_sunday = [](int year, int month) {
        time::date d(year, month + 1, 0);
        while (d.weekday() != time::weekday::sunday) {
            d = d.add_days(-1);
        }
        return int64_t(d.days_until(time::date(1970, 1, 1))) * -86400 + 3600;
    };
    for (int year = 1700; year <= 2300; ++year) {
        int64_t start = last_sunday(year, 3);
        int64_t end = last_sunday(year, 10);
        ASSERT_EQ(state(*z, start - 1), (State{3600, false, "CET"})) << year;
        ASSERT_EQ(state(*z, start), (State{7200, true, "CEST"})) << year;
        ASSERT_EQ(state(*z, end - 1), (State{7200, true, "CEST"})) << year;
        ASSERT_EQ(state(*z, end), (State{3600, false, "CET"})) << year;
        ASSERT_EQ(access::next_change(access::data(*z), start - 1), optional<int64_t>(start)) << year;
        ASSERT_EQ(access::next_change(access::data(*z), start), optional<int64_t>(end)) << year;
        ASSERT_EQ(access::previous_change(access::data(*z), end), optional<int64_t>(start)) << year;
    }
}

TEST(Zone_Tests, LoadedOnceAndNamed) {
    auto a = zone::load("Europe/Warsaw");
    auto b = zone::load("Europe/Warsaw");
    ASSERT_TRUE(a && b);
    EXPECT_EQ(*a, *b);
    EXPECT_EQ(text(a->name()), "Europe/Warsaw");
    auto poland = zone::load("Poland");   // a link: the same rules, another zone
    ASSERT_TRUE(poland);
    EXPECT_NE(*a, *poland);
    EXPECT_EQ(state(*a, 1790000000), state(*poland, 1790000000));
    EXPECT_EQ(*zone::load("UTC"), zone::utc());
    EXPECT_EQ(text(zone::load("Etc/GMT+5")->name()), "Etc/GMT+5");
    EXPECT_EQ(state(*zone::load("Etc/GMT+5"), 0), (State{-18000, false, "-05"}));
}

TEST(Zone_Tests, ANameThatIsNotOneIsRefused) {
    for (const char* name : {"", "../../etc/passwd", "/etc/passwd", "Europe/../../etc/passwd", ".hidden", "Europe/Warsaw\n", "Europe\\Warsaw", "a b"}) {
        auto z = zone::load(name);
        ASSERT_FALSE(z) << name;
        EXPECT_NE(text(z.error().message()).find("not a name of a time zone"), std::string::npos) << name;
    }
    auto z = zone::load("Europe/Warsw");
    ASSERT_FALSE(z);
    EXPECT_EQ(text(z.error().message()), "unknown time zone \"Europe/Warsw\"");
    auto table = zone::load("zone.tab");   // there, and not a TZif file
    ASSERT_FALSE(table);
    EXPECT_NE(text(table.error().message()).find("not a TZif file"), std::string::npos);
}

TEST(Zone_Tests, FixedOffsets) {
    EXPECT_EQ(text(zone::fixed(5h + 30min).name()), "+05:30");
    EXPECT_EQ(text(zone::fixed(-(3h + 30min)).name()), "-03:30");
    EXPECT_EQ(text(zone::fixed(5h + 30min + 15s).name()), "+05:30:15");
    EXPECT_EQ(text(zone::fixed(23h + 59min + 59s).name()), "+23:59:59");
    EXPECT_EQ(zone::fixed(0s), zone::utc());
    EXPECT_EQ(zone::fixed(1500ms), zone::fixed(1s));   // whole seconds
    EXPECT_EQ(zone::fixed(5h), zone::fixed(300min));
    EXPECT_NE(zone::fixed(5h), zone::fixed(-5h));
    EXPECT_THROW((void)zone::fixed(24h), invalid_argument);
    EXPECT_THROW((void)zone::fixed(-24h), invalid_argument);
    EXPECT_EQ(state(zone::fixed(-(3h + 30min)), 123), (State{-12600, false, "-03:30"}));
    EXPECT_EQ(state(zone::utc(), 123), (State{0, false, "UTC"}));
    EXPECT_EQ(text(zone::utc().name()), "UTC");
    EXPECT_EQ(zone(), zone::utc());
    EXPECT_FALSE(access::next_change(access::data(zone::fixed(1h)), 0));
    EXPECT_FALSE(access::previous_change(access::data(zone::utc()), 0));
    static_assert(sizeof(zone) == 8);
    // a tracked_ptr to the zone's data; not trivially copyable (DESIGN 219)
}

// A zone made from a TZ string (and from the bytes of a file alike) lives
// while a zone or a datetime has it, and is the same zone while it does;
// the registry forgets it once nothing has it, so strings from outside do
// not grow it. The zones made here are dropped where they were made
// (off_frame), and a datetime keeps one through the collections
TEST(Zone_Tests, ForeignZonesLiveWhileHeld) {
    auto& r = time::detail::registry();
    auto settle = [&] {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
        time::detail::sweep_dead(r);
    };
    time::datetime kept = time::datetime::from_unix(0, *zone::from_posix("<ABC>-3"));
    size_t before = 0;
    off_frame([&] {
        for (int i = 0; i < 20000; ++i) {
            auto z = zone::from_posix(sgcl::string("<ZZZ" + std::to_string(i) + ">-2"));
            ASSERT_TRUE(z);
            ASSERT_EQ(z->offset_at(kept), 2h);
        }
    });
    settle();
    before = r.by_content.size();
    EXPECT_LE(before, size_t(16)) << "the entries of the zones nothing has";
    EXPECT_EQ(text(kept.abbreviation()), "ABC");
    EXPECT_EQ(kept.offset(), 3h);
    EXPECT_EQ(kept.hour(), 3);
    EXPECT_EQ(*zone::from_posix("<ABC>-3"), kept.zone()) << "the same zone while it lives";
    // the amortized sweep of the inserting threads keeps the table near
    // what lives, round after round
    for (int round = 0; round < 4; ++round) {
        off_frame([&] {
            for (int i = 0; i < 10000; ++i) {
                (void)zone::from_posix(sgcl::string("<R" + std::to_string(round) + "x" + std::to_string(i) + ">+1"));
            }
        });
        collector::clear_stack();
        collector::force_collect(true);
    }
    EXPECT_LT(r.by_content.size(), size_t(25000)) << "40000 made, swept on the way";
    EXPECT_EQ(text(kept.abbreviation()), "ABC");
}

// The registry of zones made from strings under every race it has: threads
// making zones of a few strings over and over (dropping most of them at
// once, so that entries die all the time), a thread sweeping the dead
// entries, a thread collecting. Two zones of one string held at the same
// time must be one zone; every zone must read as its string says
TEST(Zone_Tests, ForeignZonesUnderRaces) {
    auto& r = time::detail::registry();
    constexpr int Threads = 6;
    std::atomic<bool> stop{false};
    std::atomic<int> wrong{0};
    std::thread sweeper([&] {
        while (!stop.load()) {
            time::detail::sweep_dead(r);
        }
    });
    std::thread collecting([&] {
        while (!stop.load()) {
            collector::force_collect(true);
        }
    });
    std::vector<std::thread> ts;
    for (int t = 0; t < Threads; ++t) {
        ts.emplace_back([&, t] {
            std::mt19937 rng{unsigned(t)};
            for (int i = 0; i < 20000; ++i) {
                int k = int(rng() % 8);
                sgcl::string rule("<RACE" + std::to_string(k) + ">-" + std::to_string(k + 1));
                auto a = zone::from_posix(rule);
                auto b = zone::from_posix(rule);   // while a is held: the same zone
                if (!a || !b || !(*a == *b) || access::offset_at(access::data(*a), 0) != (k + 1) * 3600) {
                    wrong.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : ts) {
        th.join();
    }
    stop = true;
    sweeper.join();
    collecting.join();
    EXPECT_EQ(wrong.load(), 0);
}

TEST(Zone_Tests, AvailableIsTheDatabase) {
    auto names = zone::available();
    std::vector<std::string> list;
    for (auto& n : names) {
        list.push_back(text(n));
    }
    EXPECT_TRUE(std::is_sorted(list.begin(), list.end()));
    auto has = [&](const std::string& n) { return std::binary_search(list.begin(), list.end(), n); };
    EXPECT_TRUE(has("Europe/Warsaw"));
    EXPECT_TRUE(has("America/Argentina/Buenos_Aires"));
    EXPECT_TRUE(has("UTC"));
    EXPECT_FALSE(has("posixrules"));
    EXPECT_FALSE(has("zone.tab"));
    EXPECT_FALSE(has("+VERSION"));
    if (system_version() == oracle::ZoneVersion) {
        EXPECT_EQ(list.size(), std::size(oracle::Zones));
    }
}

TEST(Zone_Tests, TheLocalZone) {
    using time::detail::zone_from_tz;
    EXPECT_EQ(zone_from_tz(""), optional<zone>(zone::utc()));
    EXPECT_EQ(zone_from_tz(":Europe/Warsaw"), optional<zone>(*zone::load("Europe/Warsaw")));
    EXPECT_EQ(zone_from_tz("Europe/Warsaw"), optional<zone>(*zone::load("Europe/Warsaw")));
    auto tokyo = zone_from_tz(":/usr/share/zoneinfo/Asia/Tokyo");
    ASSERT_TRUE(tokyo);
    EXPECT_EQ(text(tokyo->name()), "Asia/Tokyo");
    EXPECT_EQ(state(*tokyo, 1790000000), (State{32400, false, "JST"}));
    auto rule = zone_from_tz("CET-1CEST,M3.5.0,M10.5.0/3");
    ASSERT_TRUE(rule);
    EXPECT_EQ(text(rule->name()), "CET-1CEST,M3.5.0,M10.5.0/3");
    EXPECT_EQ(state(*rule, 1790000000), (State{7200, true, "CEST"}));
    EXPECT_FALSE(zone_from_tz("Nowhere/Nothing"));
    EXPECT_FALSE(zone_from_tz("/nowhere/nothing"));
    // A path to what is not a zone, read to a bound and refused: an
    // endless device, a directory, a file of text
    EXPECT_FALSE(zone_from_tz("/dev/zero"));
    EXPECT_FALSE(zone_from_tz("/usr/share/zoneinfo"));
    EXPECT_FALSE(zone_from_tz(":/etc/hosts"));
    // /etc/localtime: a link into the database gives the zone by name
    auto here = time::detail::zone_from_localtime("/etc/localtime");
    ASSERT_TRUE(here);
    EXPECT_FALSE(here->name().empty());
    EXPECT_EQ(zone::local(), zone::local());
    EXPECT_FALSE(zone::local().name().empty());
}

TEST(Zone_Tests, OneRegistryForEveryThread) {
    std::vector<std::string> names = {"Europe/Warsaw", "America/New_York", "Asia/Kathmandu", "Australia/Lord_Howe", "Pacific/Chatham", "Africa/Casablanca", "Europe/Dublin", "America/Nuuk"};
    constexpr int Threads = 8;
    sgcl::vector<sgcl::vector<zone>> seen(Threads);
    std::atomic<int> ready{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < Threads; ++t) {
        ts.emplace_back([&, t] {
            ready.fetch_add(1);
            while (ready.load() < Threads) {
            }
            for (int round = 0; round < 50; ++round) {
                for (size_t i = 0; i < names.size(); ++i) {
                    const auto& n = names[(i + t) % names.size()];
                    auto z = zone::load(sgcl::string(n));
                    if (round == 0) {
                        seen[t].push_back(*z);
                    }
                    (void)access::offset_at(access::data(*z), 1790000000 + round);
                }
                auto p = zone::from_posix("CET-1CEST,M3.5.0,M10.5.0/3");
                if (round == 0) {
                    seen[t].push_back(*p);
                }
            }
        });
    }
    for (auto& t : ts) {
        t.join();
    }
    for (int t = 1; t < Threads; ++t) {
        for (size_t i = 0; i < names.size(); ++i) {
            EXPECT_EQ(seen[t][(i + names.size() - t % names.size()) % names.size()], seen[0][i]);
        }
        EXPECT_EQ(seen[t].back(), seen[0].back());
    }
}

//------------------------------------------------------------------------------
// TZif byte by byte
//------------------------------------------------------------------------------
TEST(Tzif_Tests, VersionsOneToFour) {
    Tzif f;
    f.at = {-1000, 0, 1000};
    f.type_of = {1, 0, 1};
    f.types = {{3600, 0, 0}, {7200, 1, 4}};
    f.chars = std::string("CET\0CEST\0", 9);
    f.footer = "";
    for (char v : {'1', '2', '3', '4'}) {
        f.version = v;
        auto r = read(f.bytes());
        ASSERT_TRUE(r) << v << ": " << text(r.error().message());
        EXPECT_EQ(r->version, v == '1' ? 1 : v - '0');
        EXPECT_EQ(r->at, (std::vector<int64_t>{-1000, 0, 1000}));
        EXPECT_EQ(r->type_of, (std::vector<uint8_t>{1, 0, 1}));
        ASSERT_EQ(r->types.size(), 2u);
        EXPECT_EQ(r->types[1].abbreviation, "CEST");
        EXPECT_TRUE(r->types[1].dst);
        EXPECT_EQ(r->footer, "");
    }
    // 64-bit times only in the second block: one before 1901 and one after 2038
    f.version = '2';
    f.at = {-5000000000, 5000000000};
    f.type_of = {1, 0};
    auto r = read(f.bytes());
    ASSERT_TRUE(r);
    EXPECT_EQ(r->at, (std::vector<int64_t>{-5000000000, 5000000000}));
}

TEST(Tzif_Tests, TheBeginningAndTheEnd) {
    // Type 0 before the first transition (RFC 9636: the LMT of Warsaw),
    // the table's last type without a footer after the last, the rule
    // with one
    Tzif f;
    f.at = {0, 1000};
    f.type_of = {1, 2};
    f.types = {{5040, 0, 0}, {3600, 0, 4}, {7200, 1, 8}};
    f.chars = std::string("LMT\0CET\0CEST\0", 13);
    f.footer = "";
    auto z = zone::from_tzif(bytes_of(f.bytes()), "T");
    ASSERT_TRUE(z);
    EXPECT_EQ(state(*z, -1), (State{5040, false, "LMT"}));
    EXPECT_EQ(state(*z, 0), (State{3600, false, "CET"}));
    EXPECT_EQ(state(*z, 1000), (State{7200, true, "CEST"}));
    EXPECT_EQ(state(*z, 4000000000), (State{7200, true, "CEST"}));
    EXPECT_EQ(access::next_change(access::data(*z), 1000), nullopt);
    EXPECT_EQ(access::previous_change(access::data(*z), 0), nullopt);
    EXPECT_EQ(access::next_change(access::data(*z), -5), optional<int64_t>(0));
    f.footer = "CET-1CEST,M3.5.0,M10.5.0/3";
    auto r = zone::from_tzif(bytes_of(f.bytes()), "T");
    ASSERT_TRUE(r);
    EXPECT_EQ(state(*r, 1790000000), (State{7200, true, "CEST"}));
    EXPECT_EQ(state(*r, 1795000000), (State{3600, false, "CET"}));
    EXPECT_EQ(state(*r, 4102444800 + 200 * 86400), (State{7200, true, "CEST"}));   // 2100-07-20: the table
    EXPECT_EQ(state(*r, 7258118400 + 200 * 86400), (State{7200, true, "CEST"}));   // 2200-07-20: the rule
    EXPECT_EQ(state(*r, 7258118400 + 350 * 86400), (State{3600, false, "CET"}));
    // A transition that changes nothing is not one
    f.at = {0, 1000, 2000};
    f.type_of = {1, 1, 2};
    f.footer = "";
    auto same = zone::from_tzif(bytes_of(f.bytes()), "T");
    ASSERT_TRUE(same);
    EXPECT_EQ(access::next_change(access::data(*same), 0), optional<int64_t>(2000));
}

TEST(Tzif_Tests, WhatAFileGetsWrongIsRefusedWithWhere) {
    Tzif good;
    good.at = {0, 1000};
    good.type_of = {1, 0};
    good.types = {{3600, 0, 0}, {7200, 1, 4}};
    good.chars = std::string("CET\0CEST\0", 9);
    good.footer = "CET-1CEST,M3.5.0,M10.5.0/3";
    ASSERT_TRUE(read(good.bytes()));
    struct Case {
        const char* why;
        std::string bytes;
        const char* message;
        size_t at;
    };
    std::vector<Case> cases;
    // The cases of the data block are made in version 1, where the block
    // read is the first and the offsets are counted from its header; a
    // reader of version 2 and later reads past that block to the second
    auto with = [&](const char* why, auto change, const char* message, size_t at) {
        Tzif f = good;
        f.version = '1';
        change(f);
        cases.push_back({why, f.bytes(), message, at});
    };
    auto with_v2 = [&](const char* why, auto change, const char* message, size_t at) {
        Tzif f = good;
        change(f);
        cases.push_back({why, f.bytes(), message, at});
    };
    auto bytes_with = [&](const char* why, auto change, const char* message, size_t at) {
        std::string b = good.bytes();
        change(b);
        cases.push_back({why, b, message, at});
    };
    size_t v1 = 44 + 2 * 4 + 2 + 2 * 6 + 9;   // the size of the first header and block
    bytes_with("the magic", [](std::string& b) { b[0] = 'X'; }, "not a TZif file", 0);
    bytes_with("version 5", [](std::string& b) { b[4] = '5'; }, "a version this reader does not know", 4);
    bytes_with("version 1 then 2", [&](std::string& b) { b[v1 + 4] = '3'; }, "the two headers give different versions", v1 + 4);
    with("no type", [](Tzif& f) { f.types.clear(); f.type_of = {}; f.at = {}; }, "no local time type", 36);
    with("no characters", [](Tzif& f) { f.chars.clear(); f.types = {{0, 0, 0}}; f.at = {}; f.type_of = {}; }, "no designation characters", 40);
    with("isutcnt", [](Tzif& f) { f.isut = {0}; }, "isutcnt is neither zero nor typecnt", 20);
    with("isstdcnt", [](Tzif& f) { f.isstd = {0}; }, "isstdcnt is neither zero nor typecnt", 24);
    with("times out of order", [](Tzif& f) { f.at = {1000, 0}; }, "not in ascending order", 44 + 4);
    with("times equal", [](Tzif& f) { f.at = {0, 0}; }, "not in ascending order", 44 + 4);
    with("an index past the types", [](Tzif& f) { f.type_of = {1, 2}; }, "a transition to a local time type that is not there", 44 + 8 + 1);
    with("isdst 2", [](Tzif& f) { f.types[1].dst = 2; }, "isdst neither 0 nor 1", 44 + 8 + 2 + 6 + 4);
    with("a designation index past the characters", [](Tzif& f) { f.types[1].index = 9; }, "a designation index past the characters", 44 + 8 + 2 + 6 + 5);
    with("a designation with no NUL", [](Tzif& f) { f.chars = std::string("CET\0CEST", 8); }, "a designation with no NUL after it", 44 + 8 + 2 + 12 + 4);
    with("an offset of -2^31", [](Tzif& f) { f.types[0].offset = INT32_MIN; }, "an offset of 26 hours or more", 44 + 8 + 2);
    with("an offset of 26 hours", [](Tzif& f) { f.types[1].offset = 93600; }, "an offset of 26 hours or more", 44 + 8 + 2 + 6);
    with("isstd 2", [](Tzif& f) { f.isstd = {0, 2}; }, "a standard/wall indicator neither 0 nor 1", 44 + 8 + 2 + 12 + 9 + 1);
    with("isut without isstd", [](Tzif& f) { f.isstd = {0, 0}; f.isut = {0, 1}; }, "a UT indicator set on a type whose standard indicator is not", 44 + 8 + 2 + 12 + 9 + 2 + 1);
    with("leaps out of order", [](Tzif& f) { f.leaps = {{100, 1}, {50, 2}}; }, "the leap-second records are not in ascending order", 44 + 8 + 2 + 12 + 9 + 8);
    with("a negative first leap", [](Tzif& f) { f.leaps = {{-1, 1}}; }, "the leap-second records are not in ascending order", 44 + 8 + 2 + 12 + 9);
    bytes_with("no footer", [](std::string& b) { b.resize(b.size() - 28); }, "a newline before the footer expected", 0);
    bytes_with("a footer with no end", [](std::string& b) { b.pop_back(); }, "the footer does not end with a newline", 0);
    bytes_with("bytes after the footer", [](std::string& b) { b += 'x'; }, "bytes after the footer", 0);
    bytes_with("a NUL in the footer", [](std::string& b) { b[b.size() - 3] = '\0'; }, "a NUL in the footer", 0);
    with_v2("a footer that is no rule", [](Tzif& f) { f.footer = "CET-1CEST,M13.5.0,M10.5.0/3"; }, "the footer is not a POSIX TZ string", 0);
    // Version 2: the first block is not read, the second is
    with_v2("times out of order, version 2", [](Tzif& f) { f.at = {1000, 0}; }, "not in ascending order", v1 + 44 + 8);
    bytes_with("the first block broken, version 2", [](std::string& b) { b[44 + 8] = char(9); }, "", 0);
    for (auto& c : cases) {
        auto r = zone::from_tzif(bytes_of(c.bytes), "T");
        if (!*c.message) {
            EXPECT_TRUE(r) << c.why;   // what a reader of version 2 does not read
            continue;
        }
        ASSERT_FALSE(r) << c.why;
        EXPECT_NE(text(r.error().message()).find(c.message), std::string::npos) << c.why << ": " << text(r.error().message());
        if (c.at) {
            EXPECT_EQ(r.error().offset(), c.at) << c.why;
        }
        EXPECT_LE(r.error().offset(), c.bytes.size()) << c.why;
    }
    // Where the footer's own error is: the position in the file
    Tzif f = good;
    f.footer = "CET-1CEST,M13.5.0,M10.5.0/3";
    std::string b = f.bytes();
    auto r = zone::from_tzif(bytes_of(b), "T");
    ASSERT_FALSE(r);
    EXPECT_EQ(b[r.error().offset() - 1], ',');
}

TEST(Tzif_Tests, EveryPrefixAndEveryByteOfARealFile) {
    std::string warsaw = file_of("Europe/Warsaw");
    ASSERT_GT(warsaw.size(), 1000u);
    ASSERT_TRUE(read(warsaw));
    for (size_t n = 0; n < warsaw.size(); ++n) {
        auto r = read(warsaw.substr(0, n));
        ASSERT_FALSE(r) << n;
        EXPECT_LE(r.error().offset(), n);
    }
    // Every byte changed to a few values: refused or read, never read
    // outside the file (the address sanitizer's part); what is read has
    // its transitions in order and its indices in range
    std::mt19937 rng(20260925);
    size_t refused = 0, read_ok = 0;
    for (size_t i = 0; i < warsaw.size(); ++i) {
        for (int k = 0; k < 3; ++k) {
            std::string b = warsaw;
            b[i] = char(k == 0 ? 0 : k == 1 ? 0xFF : rng());
            auto r = read(b);
            if (!r) {
                ++refused;
                EXPECT_LE(r.error().offset(), b.size());
                continue;
            }
            ++read_ok;
            EXPECT_TRUE(std::is_sorted(r->at.begin(), r->at.end()));
            for (auto t : r->type_of) {
                EXPECT_LT(t, r->types.size());
            }
            (void)zone::from_tzif(bytes_of(b), "T");
        }
    }
    EXPECT_GT(refused, 1000u);
    EXPECT_GT(read_ok, 1000u);   // the times and offsets change and still read
}

//------------------------------------------------------------------------------
// POSIX TZ strings
//------------------------------------------------------------------------------
TEST(Posix_Tests, TheFormsOfTheString) {
    auto r = time::detail::read_posix("<+0545>-5:45");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->std_name, "+0545");
    EXPECT_EQ(r->std_offset, 20700);
    EXPECT_FALSE(r->has_dst);
    r = time::detail::read_posix("EST5EDT");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->dst_offset, -14400);
    EXPECT_EQ(r->start.month, 3);
    EXPECT_EQ(r->end.month, 11);
    r = time::detail::read_posix("AAA3BBB2:30,J60/-1:15:30,300/167");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->std_offset, -10800);
    EXPECT_EQ(r->dst_offset, -9000);
    EXPECT_EQ(r->start.kind, 'J');
    EXPECT_EQ(r->start.day, 60);
    EXPECT_EQ(r->start_time, -(3600 + 15 * 60 + 30));
    EXPECT_EQ(r->end.kind, 'n');
    EXPECT_EQ(r->end.day, 300);
    EXPECT_EQ(r->end_time, 167 * 3600);
    r = time::detail::read_posix("AAA+3BBB,M1.1.0/+2,M12.5.6");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->start_time, 7200);
}

TEST(Posix_Tests, WhatIsNotAStringIsRefusedWithWhere) {
    struct Case {
        const char* text;
        size_t at;
    };
    for (auto c : std::initializer_list<Case>{
             {"", 0}, {"AB1", 0}, {"<AB>1", 0}, {"<ABC", 0}, {"A1B", 0}, {"CET", 3}, {"CET-", 3}, {"CET25", 3},
             {"CET1:60", 3}, {"CET1:30:60", 3}, {"CET-1CE", 5}, {"CET-1CEST;", 9}, {"CET-1CEST,", 10},
             {"CET-1CEST,M3.5.0", 16}, {"CET-1CEST,M13.5.0,M10.5.0", 10}, {"CET-1CEST,M3.6.0,M10.5.0", 10},
             {"CET-1CEST,M3.5.7,M10.5.0", 10}, {"CET-1CEST,M0.5.0,M10.5.0", 10}, {"CET-1CEST,J0,J100", 10},
             {"CET-1CEST,J366,J100", 10}, {"CET-1CEST,366,100", 10}, {"CET-1CEST,M3.5.0/168,M10.5.0", 10},
             {"CET-1CEST,M3.5.0,M10.5.0/", 17}, {"CET-1CEST,M3.5.0,M10.5.0x", 24}, {"CET-1CEST-2x", 11},
             {"CET-1CEST,M3.5,M10.5.0", 10}, {"CET-1 CEST", 5}}) {
        auto r = time::detail::read_posix(c.text);
        ASSERT_FALSE(r) << c.text;
        EXPECT_FALSE(r.error().message().empty());
        EXPECT_EQ(r.error().offset(), c.at) << c.text;
        auto z = zone::from_posix(c.text);
        ASSERT_FALSE(z) << c.text;
    }
}

TEST(Posix_Tests, RandomTextIsReadOrRefusedAndNothingElse) {
    std::mt19937 rng(7);
    const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ<>+-:,./0123456789JM";
    size_t accepted = 0;
    for (int i = 0; i < 100000; ++i) {
        std::string s;
        int n = int(rng() % 30);
        for (int k = 0; k < n; ++k) {
            s += alphabet[rng() % (sizeof alphabet - 1)];
        }
        if (rng() % 2) {
            s = "CET-1CEST," + s;
        }
        auto r = time::detail::read_posix(s);
        if (r) {
            ++accepted;
            // A rule that is read gives answers for every year
            auto z = zone::from_posix(sgcl::string(s));
            ASSERT_TRUE(z);
            for (int64_t t : {int64_t(-9000000000), int64_t(0), int64_t(1790000000), int64_t(9000000000)}) {
                (void)access::state_at(access::data(*z), t);
                (void)access::next_change(access::data(*z), t);
            }
        } else {
            EXPECT_LE(r.error().offset(), s.size());
        }
    }
    EXPECT_GT(accepted, 100u);
}
