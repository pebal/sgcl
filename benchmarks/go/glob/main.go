// The io module's glob counterparts in Go: path/filepath's Match and Glob,
// one case per run (benchmarks/io/io.cpp has the SGCL side, the same paths
// and the same tree). Prints one line, ns per operation. Go has no `**`:
// globstar has no Go side.
package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"syscall"
	"time"
)

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func report(what string, wall float64, ops int64) {
	fmt.Printf("io %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall*1e9/float64(ops), float64(ops)/wall, wall, cpuSeconds())
}

func paths() []string {
	out := make([]string, 0, 1000)
	for i := 0; i < 1000; i++ {
		suffix := ".go"
		if i%3 == 0 {
			suffix = "_test.go"
		}
		out = append(out, fmt.Sprintf("pkg%03d/%cname%d%s", i%37, 'a'+rune(i%26), i, suffix))
	}
	return out
}

func tree() string {
	dir := os.Getenv("TMPDIR")
	if dir == "" {
		dir = "/tmp"
	}
	root := filepath.Join(dir, fmt.Sprintf("sgcl_bench_io_glob_go_%d", os.Getpid()))
	for d := 0; d < 20; d++ {
		for s := 0; s < 20; s++ {
			path := filepath.Join(root, fmt.Sprintf("d%02d", d), fmt.Sprintf("s%02d", s))
			os.MkdirAll(path, 0o777)
			for f := 0; f < 15; f++ {
				name := fmt.Sprintf("f%02d.txt", f)
				if f >= 10 {
					name = fmt.Sprintf("g%02d.dat", f)
				}
				os.WriteFile(filepath.Join(path, name), nil, 0o666)
			}
		}
	}
	return root
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: glob <globmatch|globwalk> [n]")
		os.Exit(2)
	}
	what := os.Args[1]
	var n int64
	if len(os.Args) > 2 {
		n, _ = strconv.ParseInt(os.Args[2], 10, 64)
	}
	switch what {
	case "globmatch":
		if n == 0 {
			n = 20000000
		}
		ps := paths()
		matched := 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if ok, _ := filepath.Match("*/[a-m]*_test.go", ps[i%1000]); ok {
				matched++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
		if matched == 0 {
			os.Exit(1)
		}
	case "globwalk":
		if n == 0 {
			n = 200
		}
		root := tree()
		ok := int64(0)
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			m, err := filepath.Glob(filepath.Join(root, "*/*/*.txt"))
			if err == nil && len(m) == 4000 {
				ok++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
		os.RemoveAll(root)
		if ok != n {
			os.Exit(1)
		}
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
}
