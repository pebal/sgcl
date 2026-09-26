// The time module's counterparts in Go: package time (and http.ParseTime),
// one case a run (benchmarks/time/time.cpp has the SGCL side, the same
// cases over the same instants). Prints one line: ns per call.
//
//	time <case>
//
// The cases are the C++ side's; where Go writes, it appends to a buffer
// kept between calls (AppendFormat), as the C++ side writes to one; Go's
// LoadLocation reads the file every time, so load_cached is what a load
// costs there.
package main

import (
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

var sink uint64

func splitmix(s *uint64) uint64 {
	*s += 0x9e3779b97f4a7c15
	z := *s
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb
	return z ^ (z >> 31)
}

func instants(from, to int64) []int64 {
	out := make([]int64, 4096)
	s := uint64(1)
	for i := range out {
		sec := from + int64(splitmix(&s)%uint64(to-from))
		out[i] = sec*1000000000 + int64(splitmix(&s)%1000000000)
	}
	return out
}

func runFor(f func(int) uint64, seconds float64) (uint64, float64) {
	var calls, acc uint64
	t0 := time.Now()
	wall := 0.0
	i := 0
	for wall < seconds {
		for k := 0; k < 1024; k++ {
			acc += f(i & 4095)
			i++
		}
		calls += 1024
		wall = time.Since(t0).Seconds()
	}
	sink = acc
	return calls, wall
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: time <case>")
		os.Exit(2)
	}
	what := os.Args[1]
	warsaw, _ := time.LoadLocation("Europe/Warsaw")
	recent := instants(1767225600, 1798761600)
	wide := instants(-2208988800, 4102444800)
	utc := make([]time.Time, len(wide))
	local := make([]time.Time, len(wide))
	var rfc, httpText, pattern, durText []string
	var durs []time.Duration
	for i, ns := range wide {
		utc[i] = time.Unix(0, ns).UTC()
		local[i] = time.Unix(0, ns).In(warsaw)
		rfc = append(rfc, local[i].Format(time.RFC3339))
		httpText = append(httpText, local[i].UTC().Format(http.TimeFormat))
		pattern = append(pattern, local[i].Format("02.01.2006 15:04"))
		durs = append(durs, time.Duration(ns%100000000000000))
		durText = append(durText, durs[i].String())
	}
	buf := make([]byte, 0, 128)
	measure := func(f func(int) uint64) {
		runFor(f, 0.25)
		calls, wall := runFor(f, 2.0)
		fmt.Printf("time %s variant=go ns/op=%.2f wall=%.2fs\n", what, wall*1e9/float64(calls), wall)
	}
	switch what {
	case "now":
		measure(func(int) uint64 { return uint64(time.Now().UnixNano()) })
	case "fields_utc", "fields_local":
		ts := utc
		if what == "fields_local" {
			ts = local
		}
		measure(func(i int) uint64 {
			t := ts[i]
			y, m, d := t.Date()
			h, mi, s := t.Clock()
			return uint64(y + int(m) + d + h + mi + s)
		})
	case "offset_now", "offset_random":
		src := wide
		if what == "offset_now" {
			src = recent
		}
		ts := make([]time.Time, len(src))
		for i, ns := range src {
			ts[i] = time.Unix(0, ns).In(warsaw)
		}
		measure(func(i int) uint64 { _, off := ts[i].Zone(); return uint64(off) })
	case "local_to_instant":
		measure(func(i int) uint64 {
			y, m, d := local[i].Date()
			return uint64(time.Date(y, m, d, i%24, i%60, i%60, 0, warsaw).UnixNano())
		})
	case "format_rfc3339":
		measure(func(i int) uint64 { buf = local[i].AppendFormat(buf[:0], time.RFC3339); return uint64(len(buf)) })
	case "format_http":
		measure(func(i int) uint64 { buf = local[i].UTC().AppendFormat(buf[:0], http.TimeFormat); return uint64(len(buf)) })
	case "format_pattern":
		measure(func(i int) uint64 { buf = local[i].AppendFormat(buf[:0], "02.01.2006 15:04"); return uint64(len(buf)) })
	case "format_string":
		measure(func(i int) uint64 { return uint64(len(local[i].Format(time.RFC3339))) })
	case "parse_rfc3339":
		measure(func(i int) uint64 { t, _ := time.Parse(time.RFC3339, rfc[i]); return uint64(t.UnixNano()) })
	case "parse_http":
		measure(func(i int) uint64 { t, _ := http.ParseTime(httpText[i]); return uint64(t.UnixNano()) })
	case "parse_pattern":
		measure(func(i int) uint64 { t, _ := time.Parse("02.01.2006 15:04", pattern[i]); return uint64(t.UnixNano()) })
	case "duration_string":
		measure(func(i int) uint64 { return uint64(len(durs[i].String())) })
	case "duration_parse":
		measure(func(i int) uint64 { d, _ := time.ParseDuration(durText[i]); return uint64(d) })
	case "load_cold":
		var names []string
		root := "/usr/share/zoneinfo/"
		filepath.WalkDir(root, func(p string, e os.DirEntry, err error) error {
			if err != nil || e.IsDir() {
				if e != nil && e.IsDir() && (e.Name() == "posix" || e.Name() == "right") {
					return filepath.SkipDir
				}
				return nil
			}
			n := strings.TrimPrefix(p, root)
			if b, err := os.ReadFile(p); err == nil && len(b) >= 4 && string(b[:4]) == "TZif" && n != "posixrules" && n != "localtime" {
				names = append(names, n)
			}
			return nil
		})
		sort.Strings(names)
		t0 := time.Now()
		var acc uint64
		for _, n := range names {
			l, _ := time.LoadLocation(n)
			acc += uint64(len(l.String()))
		}
		wall := time.Since(t0).Seconds()
		sink = acc
		fmt.Printf("time %s variant=go zones=%d ns/op=%.2f wall=%.2fs\n", what, len(names), wall*1e9/float64(len(names)), wall)
	case "load_cached":
		measure(func(int) uint64 { l, _ := time.LoadLocation("America/New_York"); return uint64(len(l.String())) })
	default:
		fmt.Fprintf(os.Stderr, "unknown case %s\n", what)
		os.Exit(2)
	}
}
