// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for sgcl/slog: Go's log/slog writes the same records through
// its TextHandler and its JSONHandler, and the lines, byte for byte, are
// written out as a C++ header the test includes. Run it from the root of
// the tree:
//
//	go run tools/slog_oracle.go > tests/slog/oracle_cases.h
//
// The time of every record is one instant (TimeNs, in UTC: the C++ test
// holds a manual clock there and logs with .utc()). What is asked:
//
//   - texts: every string below, and strings drawn from a fixed seed over
//     an alphabet of the bytes and code points that decide the quoting
//     (controls, space, '=', '"', '\\', DEL, invalid UTF-8, spaces and
//     formats of Unicode, the line separators, unassigned and private
//     code points), each as a key and a value of one attribute and as the
//     message;
//   - floats (by their bits: the named ones, then 400 drawn from the seed
//     over every exponent), integers, durations, times at a few offsets,
//     and every level from -10 to 14;
//   - scenarios: with(), groups, empty groups, keys twice, a group
//     without a name, and the values of the program a type described by
//     its fields becomes in sgcl (a group of its fields, a container
//     among them as Go's KindAny) — written here as Go's own groups,
//     slices and maps. Each is written again in the test by its name.
//
// Where sgcl parts from Go by design the case is not asked: a pointer in
// a KindAny value (Go's %+v writes its address), an Attr as a value, a
// value of a kind sgcl refuses to compile. And one of Go's own slips: a
// group whose only attribute is empty (slog.Group("g", "", nil), "b", 1)
// leaves the group's prefix on the next key in text (g.b=1) and drops the
// comma before it in JSON ("msg":"m""b":1); sgcl leaves the group out.
package main

import (
	"bytes"
	"fmt"
	"log/slog"
	"math"
	"math/rand"
	"strings"
	"time"
)

const TimeNs = int64(1790604301123456789) // 2026-09-28T14:05:01.123456789Z

var at = time.Unix(0, TimeNs).UTC()

func handlers(buf *bytes.Buffer) (*slog.Logger, *slog.Logger) {
	o := &slog.HandlerOptions{
		Level: slog.Level(-100),
		ReplaceAttr: func(groups []string, a slog.Attr) slog.Attr {
			if a.Key == slog.TimeKey && len(groups) == 0 && a.Value.Kind() == slog.KindTime {
				return slog.Time(slog.TimeKey, at)
			}
			return a
		},
	}
	return slog.New(slog.NewTextHandler(buf, o)), slog.New(slog.NewJSONHandler(buf, o))
}

// Both lines of one record: text, then JSON
func both(f func(l *slog.Logger)) (string, string) {
	var buf bytes.Buffer
	t, j := handlers(&buf)
	f(t)
	text := buf.String()
	buf.Reset()
	f(j)
	return text, buf.String()
}

// A C++ string literal of any bytes: printable ASCII as it is, the rest
// as three octal digits (which never run into the next character)
func lit(s string) string {
	var b strings.Builder
	b.WriteByte('"')
	for i := 0; i < len(s); i++ {
		c := s[i]
		switch {
		case c == '"' || c == '\\':
			b.WriteByte('\\')
			b.WriteByte(c)
		case c == '?':
			b.WriteString("\\?")
		case c >= 0x20 && c < 0x7f:
			b.WriteByte(c)
		default:
			fmt.Fprintf(&b, "\\%03o", c)
		}
	}
	b.WriteByte('"')
	return b.String()
}

var texts = []string{
	"", "a", "x y", "a=b", "=", "\"", "\\", "a\\b", "a\"b", "'", "#", "a:b", "{}[]", "<a&b>", "key", "msg", "time",
	"\t", "\n", "\r", "\x00", "\x07", "\x08", "\x0b", "\x0c", "\x1b", "\x1f", "\x7f", " ", "  ", "a ", " a",
	"\x80", "\xff", "\xc0\x80", "\xc2", "\xe2\x82", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf8\x88\x80\x80\x80", "\xef\xbf\xbd",
	"\xc3\xa9", "\xe6\x97\xa5\xe6\x9c\xac", "za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84", "\xc2\xa0", "\xc2\xad", "\xcc\x80", "a\xcc\x81", "\xcd\xb8", "\xd8\x9c", "\u0085", "\xe1\x9a\x80",
	"\xe2\x80\x80", "\xe2\x80\x8a", "\xe2\x80\x8b", "\xe2\x80\x8d", "\xe2\x80\xa8", "\xe2\x80\xa9", "\xe2\x80\xaf", "\xe2\x81\x9f", "\xe3\x80\x80", "\xef\xbb\xbf", "\xef\xbf\xbe",
	"\U0001F600", "\U000E0001", "\U000F0000", "\U0010FFFF", "\xee\x80\x80", "\xe2\x81\xa6",
	"quote \"me\"", "tab\there", "line\nbreak", "path/to/file.go:12", "1.5", "-", "true", "null",
}

// The alphabet the drawn strings are made of
var alphabet = []string{
	"a", "Z", "0", " ", "=", "\"", "\\", ".", ":", "\t", "\n", "\x00", "\x1b", "\x7f", "\x80", "\xbf", "\xc3", "\xe2", "\xf0", "\xff",
	"\xc3\xa9", "\xc5\x82", "\xc2\xa0", "\xc2\xad", "\xcc\x80", "\xe2\x80\xa8", "\xe3\x80\x80", "\xe2\x80\x8b", "\U0001F600", "\U000E0001", "\xee\x80\x80", "\xcd\xb8", "<", "&",
}

func drawn(r *rand.Rand, n int) []string {
	out := make([]string, 0, n)
	for i := 0; i < n; i++ {
		k := r.Intn(9)
		var b strings.Builder
		for j := 0; j < k; j++ {
			b.WriteString(alphabet[r.Intn(len(alphabet))])
		}
		out = append(out, b.String())
	}
	return out
}

var namedFloats = []float64{
	0, math.Copysign(0, -1), 1, -1, 0.1, 0.5, 1.5, 2.5, 3.14159, 100, 999999, 123456, 1234567, 1e6, 1e7, 1e20, 1e21, 1e22,
	1e-4, 1.5e-4, 1e-5, 1e-6, 1.5e-6, 1e-7, 5e-324, math.SmallestNonzeroFloat64 * 3, math.MaxFloat64, 1 << 53, (1 << 53) + 2,
	float64(float32(0.1)), float64(float32(1e-10)), 1e100, 1.5e-300, -2.5e-8, 123456.789, 0.000123, 9.999999999999999e20,
	math.NaN(), math.Inf(1), math.Inf(-1),
}

var ints = []int64{0, 1, -1, 42, -42, 1000000, math.MaxInt64, math.MinInt64, math.MaxInt32, math.MinInt32}
var uints = []uint64{0, 1, 42, math.MaxUint32, math.MaxUint64, 1 << 63}

var durations = []int64{0, 1, 999, 1000, 1500, 999999, 1000000, 1500000, 999999999, 1000000000, 1500000000, 60000000000,
	90000000000, 3600000000000, 5400000000000, 86400000000000, -1, -1500, -1500000000, -5400000000000,
	math.MaxInt64, math.MinInt64, 1234567890123}

type timeCase struct {
	ns     int64
	offset int
}

var times = []timeCase{
	{TimeNs, 0}, {TimeNs, 7200}, {TimeNs, -18000}, {TimeNs, 19800}, {TimeNs, -34200}, {0, 0}, {1, 0}, {999999, 3600},
	{1000000, 0}, {1790604301000000000, 0}, {1790604301120000000, 0}, {1790604301100000000, 0}, {-1, 0}, {-1500000000, 0},
	{946684799999999999, 0}, {4102444800000000000, 0}, {-2208988800000000000, 0}, {1790604301123456789, 45 * 60},
}

type scenario struct {
	name string
	run  func(l *slog.Logger)
}

var scenarios = []scenario{
	{"plain", func(l *slog.Logger) { l.Info("server started", "port", 8080, "tls", true) }},
	{"no_attrs", func(l *slog.Logger) { l.Info("m") }},
	{"empty_message", func(l *slog.Logger) { l.Info("", "k", "v") }},
	{"with_groups", func(l *slog.Logger) { l.With("a", 1).WithGroup("g").With("b", 2).WithGroup("h").Info("m", "c", 3) }},
	{"group_no_attrs", func(l *slog.Logger) { l.WithGroup("g").Info("m") }},
	{"group_empty_group", func(l *slog.Logger) { l.WithGroup("g").Info("m", slog.Group("e")) }},
	{"group_group_with", func(l *slog.Logger) { l.WithGroup("g").WithGroup("h").With("x", 1).Info("m") }},
	{"group_group_with_call", func(l *slog.Logger) { l.WithGroup("g").WithGroup("h").With("x", 1).Info("m", "y", 2) }},
	{"inline_groups", func(l *slog.Logger) {
		l.Info("m", slog.Group("", "a", 1), slog.Group("gg", slog.Group("hh")), "", nil, "a=b", "k")
	}},
	{"keys_twice", func(l *slog.Logger) { l.Info("m", "a", 1, "a", 2) }},
	{"group_empty_key", func(l *slog.Logger) { l.WithGroup("g").Info("m", "", 1) }},
	{"group_quoted", func(l *slog.Logger) { l.WithGroup("a b").Info("m", "k", 1, "x=y", 2) }},
	{"with_nothing", func(l *slog.Logger) { l.With().Info("m", "a", 1) }},
	{"with_empty_group", func(l *slog.Logger) { l.With(slog.Group("e")).WithGroup("g").Info("m", "a", 1) }},
	{"nested_groups", func(l *slog.Logger) {
		l.Info("m", slog.Group("r", "id", 5, slog.Group("in", "x", 1.5, "s", "a b")), "after", true)
	}},
	{"with_then_group", func(l *slog.Logger) { l.With("a", 1).WithGroup("g").Info("m") }},
	{"group_with_empty_group", func(l *slog.Logger) { l.WithGroup("g").With(slog.Group("e")).Info("m", "a", 1) }},
	{"group_with_empty_group_no_call", func(l *slog.Logger) { l.WithGroup("g").With(slog.Group("e")).Info("m") }},
	{"with_twice", func(l *slog.Logger) { l.With("a", 1).With("b", "x y").Info("m", "c", 3) }},
	{"deep", func(l *slog.Logger) {
		l.WithGroup("a").With("x", 1).WithGroup("b").WithGroup("c").With("y", 2).Info("m", "z", 3)
	}},
	{"deep_with_group_values", func(l *slog.Logger) {
		l.WithGroup("a").With(slog.Group("v", "w", 1)).Info("m", slog.Group("q", "r", "s"))
	}},
	{"level_debug_offset", func(l *slog.Logger) { l.Log(nil, slog.Level(-3), "m") }},
	// the values a type described by its fields becomes
	{"described", func(l *slog.Logger) { l.Info("request", slog.Group("req", "id", 5, "path", "/a")) }},
	{"described_nested", func(l *slog.Logger) {
		l.Info("m", slog.Group("user", "name", "Ala", slog.Group("home", "city", "Kraków"), "age", 30))
	}},
	{"described_vector", func(l *slog.Logger) { l.Info("m", slog.Group("r", "tags", []string{"a b", "<c>", ""})) }},
	{"described_numbers", func(l *slog.Logger) {
		l.Info("m", slog.Group("r", "v", []float64{1e6, 0.5, 1e21, 1e-7, -0.0}, "i", []int{1, -2, 3}))
	}},
	{"described_map", func(l *slog.Logger) {
		l.Info("m", slog.Group("r", "m", map[string]int{"b": 1, "a": 2, "c d": 3}))
	}},
	{"described_nan", func(l *slog.Logger) { l.Info("m", slog.Group("r", "v", []float64{1, math.NaN()})) }},
	{"described_inf", func(l *slog.Logger) { l.Info("m", slog.Group("r", "v", []float64{math.Inf(-1)})) }},
	{"described_escapes", func(l *slog.Logger) {
		l.Info("m", slog.Group("r", "v", []string{"\b\f\x1b\x7f\xff\xe2\x80\xa8\xc2\xa0\"\\\t\n"}))
	}},
	{"described_bools", func(l *slog.Logger) { l.Info("m", slog.Group("r", "v", []bool{true, false})) }},
	{"described_records", func(l *slog.Logger) {
		type P struct {
			ID   int    `json:"ID"`
			Path string `json:"Path"`
		}
		l.Info("m", slog.Group("r", "v", []P{{5, "/a"}, {6, "x y"}}))
	}},
	{"described_nested_vectors", func(l *slog.Logger) {
		l.Info("m", slog.Group("r", "v", [][]int{{1, 2}, {}, {3}}))
	}},
	{"described_map_of_vectors", func(l *slog.Logger) {
		l.Info("m", slog.Group("r", "v", map[string][]string{"k": {"a", "b"}}))
	}},
	{"described_null", func(l *slog.Logger) { l.Info("m", slog.Group("r", "o", nil, "n", 1)) }},
	{"null_value", func(l *slog.Logger) { l.Info("m", "o", nil) }},
	{"described_empty", func(l *slog.Logger) { l.Info("m", slog.Group("r"), "x", 1) }},
	{"with_described", func(l *slog.Logger) { l.With(slog.Group("req", "id", 5, "path", "/a")).Info("m", "k", 1) }},
	{"duration_and_time", func(l *slog.Logger) {
		l.Info("m", "took", 1500*time.Millisecond, "at", time.Unix(0, TimeNs).In(time.FixedZone("", 7200)))
	}},
	{"sampled_out", func(l *slog.Logger) { l.Warn("records sampled out", "count", uint64(7)) }},
}

func main() {
	r := rand.New(rand.NewSource(20260928))
	w := &strings.Builder{}
	p := func(format string, a ...any) { fmt.Fprintf(w, format, a...) }

	p("//------------------------------------------------------------------------------\n")
	p("// SGCL: a C++20 application platform\n")
	p("// Copyright (c) 2022-2026 Sebastian Nibisz\n")
	p("// SPDX-License-Identifier: Apache-2.0\n")
	p("//------------------------------------------------------------------------------\n")
	p("#pragma once\n\n")
	p("// Generated by tools/slog_oracle.go: do not edit.\n//\n")
	p("// What Go's log/slog writes, through the TextHandler and the JSONHandler, of\n")
	p("// the records the program lists; every record at TimeNs, in UTC.\n\n")
	p("#include <cstddef>\n#include <cstdint>\n\n")
	p("namespace slog_oracle {\n")
	p("    inline constexpr int64_t TimeNs = %d;\n\n", TimeNs)

	p("    struct Line {\n        const char* text;\n        size_t text_n;\n        const char* json;\n        size_t json_n;\n    };\n\n")
	line := func(t, j string) string {
		return fmt.Sprintf("{%s, %d, %s, %d}", lit(t), len(t), lit(j), len(j))
	}

	// texts: as a key and a value, and as the message
	all := append(append([]string{}, texts...), drawn(r, 400)...)
	p("    struct TextCase {\n        const char* s;\n        size_t n;\n        Line attr;\n        Line message;\n    };\n\n")
	p("    inline constexpr TextCase Texts[] = {\n")
	for _, s := range all {
		at, aj := both(func(l *slog.Logger) { l.Info("m", s, s) })
		mt, mj := both(func(l *slog.Logger) { l.Info(s) })
		p("        {%s, %d, %s, %s},\n", lit(s), len(s), line(at, aj), line(mt, mj))
	}
	p("    };\n\n")

	// floats by their bits
	floats := append([]float64{}, namedFloats...)
	for i := 0; i < 400; i++ {
		var f float64
		switch i % 4 {
		case 0:
			f = math.Float64frombits(r.Uint64())
		case 1:
			f = r.Float64() * math.Pow(10, float64(r.Intn(44)-22))
		case 2:
			f = float64(r.Intn(10000000)) / math.Pow(10, float64(r.Intn(12)))
		default:
			f = float64(float32(r.NormFloat64() * 1e3))
		}
		if r.Intn(2) == 0 {
			f = -f
		}
		floats = append(floats, f)
	}
	p("    struct FloatCase {\n        uint64_t bits;\n        Line line;\n    };\n\n")
	p("    inline constexpr FloatCase Floats[] = {\n")
	for _, f := range floats {
		t, j := both(func(l *slog.Logger) { l.Info("m", "f", f) })
		p("        {0x%016xull, %s},\n", math.Float64bits(f), line(t, j))
	}
	p("    };\n\n")

	p("    struct IntCase {\n        int64_t value;\n        Line line;\n    };\n\n")
	p("    inline constexpr IntCase Ints[] = {\n")
	for _, v := range ints {
		t, j := both(func(l *slog.Logger) { l.Info("m", "i", v) })
		if v == math.MinInt64 {
			p("        {INT64_MIN, %s},\n", line(t, j))
		} else {
			p("        {%dll, %s},\n", v, line(t, j))
		}
	}
	p("    };\n\n")

	p("    struct UintCase {\n        uint64_t value;\n        Line line;\n    };\n\n")
	p("    inline constexpr UintCase Uints[] = {\n")
	for _, v := range uints {
		t, j := both(func(l *slog.Logger) { l.Info("m", "u", v) })
		p("        {%dull, %s},\n", v, line(t, j))
	}
	p("    };\n\n")

	p("    inline constexpr IntCase Durations[] = {\n")
	for _, v := range durations {
		t, j := both(func(l *slog.Logger) { l.Info("m", "d", time.Duration(v)) })
		if v == math.MinInt64 {
			p("        {INT64_MIN, %s},\n", line(t, j))
		} else {
			p("        {%dll, %s},\n", v, line(t, j))
		}
	}
	p("    };\n\n")

	p("    struct TimeCase {\n        int64_t ns;\n        int offset;\n        Line line;\n    };\n\n")
	p("    inline constexpr TimeCase Times[] = {\n")
	for _, c := range times {
		v := time.Unix(0, c.ns).In(time.FixedZone("", c.offset))
		if c.offset == 0 {
			v = time.Unix(0, c.ns).UTC()
		}
		t, j := both(func(l *slog.Logger) { l.Info("m", "t", v) })
		p("        {%dll, %d, %s},\n", c.ns, c.offset, line(t, j))
	}
	p("    };\n\n")

	p("    struct LevelCase {\n        int level;\n        Line line;\n    };\n\n")
	p("    inline constexpr LevelCase Levels[] = {\n")
	for lv := -10; lv <= 14; lv++ {
		t, j := both(func(l *slog.Logger) { l.Log(nil, slog.Level(lv), "m") })
		p("        {%d, %s},\n", lv, line(t, j))
	}
	p("    };\n\n")

	p("    struct Scenario {\n        const char* name;\n        Line line;\n    };\n\n")
	p("    inline constexpr Scenario Scenarios[] = {\n")
	for _, s := range scenarios {
		t, j := both(s.run)
		p("        {%s, %s},\n", lit(s.name), line(t, j))
	}
	p("    };\n")
	p("}\n")
	fmt.Print(w.String())
}
