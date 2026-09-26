//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::format of a value that reads the rest of its field as a pattern of
// its own (a formatter with takes_layout, as the times of sgcl/time have):
// the field of std::format's <chrono> types, [[fill]align][width] and then
// the pattern from its first '%' to the brace, colons and all. A type of
// the test's own, so that txt is tested without the module above it.
#include "sgcl/txt/format.h"
#include "tests/types.h"

#include <string>

namespace {
    struct Clock {
        int hour;
        int minute;
    };
}

namespace sgcl::txt {
    template<>
    struct formatter<Clock> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        // %H and %M, and anything that is not a '%'
        static constexpr bool takes_layout(std::string_view p) noexcept {
            if (!p.data()) {
                return true;
            }
            for (size_t i = 0; i < p.size(); ++i) {
                if (p[i] == '%' && (++i == p.size() || (p[i] != 'H' && p[i] != 'M'))) {
                    return false;
                }
            }
            return true;
        }

        static void write(format_sink& out, const Clock& c, const format_spec& spec, std::string_view p) {
            detail::put_body_in_field(out, spec, [&](format_sink& to) {
                if (!p.data()) {
                    p = "%H:%M";
                }
                for (size_t i = 0; i < p.size(); ++i) {
                    if (p[i] == '%') {
                        int v = p[++i] == 'H' ? c.hour : c.minute;
                        to.put(char('0' + v / 10));
                        to.put(char('0' + v % 10));
                    } else {
                        to.put(p[i]);
                    }
                }
            });
        }
    };
}

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }
}

TEST(FormatLayout_Tests, TheFieldOfChrono) {
    Clock c{9, 5};
    EXPECT_EQ(text(txt::format("{}", c)), "09:05");
    EXPECT_EQ(text(txt::format("{:%H:%M}", c)), "09:05");
    EXPECT_EQ(text(txt::format("{:%Hh%Mm}", c)), "09h05m");
    EXPECT_EQ(text(txt::format("[{:>8%H:%M}]", c)), "[   09:05]");
    EXPECT_EQ(text(txt::format("[{:%<8%H:%M}]", c)), "[09:05%%%]");   // '%' may be the fill
    EXPECT_EQ(text(txt::format("[{:8}]", c)), "[09:05   ]");
    EXPECT_EQ(text(txt::format("{0:%H} {0:%M} {1}", c, 7)), "09 05 7");
    EXPECT_EQ(text(txt::format("{:%H:%M}|{}", optional<Clock>(c), optional<Clock>())), "09:05|nullopt");
    sgcl::vector<Clock> v;
    v.push_back(c);
    v.push_back(Clock{23, 59});
    EXPECT_EQ(text(txt::format("{}", v)), "[09:05, 23:59]");
    EXPECT_EQ(text(txt::format("{::%H}", v)), "[09, 23]");
    EXPECT_EQ(text(txt::format("{:n:%M}", v)), "05, 59");
    // Checked where the program is compiled
    static_assert(txt::detail::fits<Clock>("{:%H:%M}"));
    static_assert(txt::detail::fits<Clock>("{:>10%H}"));
    static_assert(!txt::detail::fits<Clock>("{:%S}"));
    static_assert(!txt::detail::fits<Clock>("{:H}"));
    static_assert(!txt::detail::fits<Clock>("{:>10H}"));
    static_assert(!txt::detail::fits<Clock>("{:+%H}"));
    static_assert(!txt::detail::fits<Clock>("{:#%H}"));
    static_assert(!txt::detail::fits<Clock>("{:.2%H}"));
    static_assert(!txt::detail::fits<Clock>("{:%H"));
    // The colon of others is what it was: a fill for a number, a nest for a list
    EXPECT_EQ(text(txt::format("{::>6}", 42)), "::::42");
    sgcl::vector<int> n;
    n.push_back(1);
    n.push_back(22);
    EXPECT_EQ(text(txt::format("{::>3}", n)), "[  1,  22]");
    EXPECT_FALSE(txt::format(txt::runtime("{:%S}"), c));
    EXPECT_EQ(text(*txt::format(txt::runtime("{:%M}"), c)), "05");
}
