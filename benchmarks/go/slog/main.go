// The slog module's counterparts in Go: log/slog, one case a run
// (benchmarks/slog/slog.cpp has the SGCL side, the same records). Prints
// one line of named fields: case=<name> impl=go ns=<per record>
// allocs=<per record> (runtime.MemStats.Mallocs over the run).
//
//	slog <case> [path]
//
// The cases are the C++ side's; json_described logs slog.Group of the three
// fields the C++ type describes; text_buffered and text_file_buffered have
// no counterpart in slog and are not run here. The loop runs for about two
// seconds after a quarter of a second thrown away.
package main

import (
	"fmt"
	"io"
	"log/slog"
	"os"
	"runtime"
	"sync"
	"sync/atomic"
	"time"
)

func run(name string, record func(), seconds float64) {
	t0 := time.Now()
	for time.Since(t0).Seconds() < 0.25 {
		for k := 0; k < 256; k++ {
			record()
		}
	}
	var m0, m1 runtime.MemStats
	runtime.ReadMemStats(&m0)
	n := 0
	t0 = time.Now()
	wall := 0.0
	for wall < seconds {
		for k := 0; k < 1024; k++ {
			record()
		}
		n += 1024
		wall = time.Since(t0).Seconds()
	}
	runtime.ReadMemStats(&m1)
	fmt.Printf("case=%s impl=go ns=%.2f allocs=%.3f\n", name, wall*1e9/float64(n), float64(m1.Mallocs-m0.Mallocs)/float64(n))
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: slog <case> [path]")
		os.Exit(2)
	}
	what := os.Args[1]
	took := 1500 * time.Microsecond
	method := "GET"
	switch what {
	case "text_info3":
		log := slog.New(slog.NewTextHandler(io.Discard, nil))
		run(what, func() { log.Info("request", "method", method, "status", 200, "took", took) }, 2)
	case "text_disabled":
		log := slog.New(slog.NewTextHandler(io.Discard, nil))
		run(what, func() { log.Debug("request", "method", method, "status", 200, "took", took) }, 2)
	case "text_with5":
		log := slog.New(slog.NewTextHandler(io.Discard, nil)).With("service", "api", "node", "n1", "version", 3, "region", "eu-central", "tls", true)
		run(what, func() { log.Info("request", "status", 200) }, 2)
	case "json_info3":
		log := slog.New(slog.NewJSONHandler(io.Discard, nil))
		run(what, func() { log.Info("request", "method", method, "status", 200, "took", took) }, 2)
	case "json_described":
		log := slog.New(slog.NewJSONHandler(io.Discard, nil))
		id, path, score := 42, "/users/42", 0.75
		run(what, func() { log.Info("request", slog.Group("req", "id", id, "path", path, "score", score)) }, 2)
	case "text_file":
		f, err := os.OpenFile(os.Args[2], os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0666)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		log := slog.New(slog.NewTextHandler(f, nil))
		run(what, func() { log.Info("request", "method", method, "status", 200, "took", took) }, 1)
		f.Close()
	case "parallel_text":
		log := slog.New(slog.NewTextHandler(io.Discard, nil))
		var stop atomic.Bool
		var total atomic.Uint64
		var wg sync.WaitGroup
		t0 := time.Now()
		for t := 0; t < 8; t++ {
			wg.Add(1)
			go func() {
				defer wg.Done()
				n := uint64(0)
				for !stop.Load() {
					for k := 0; k < 256; k++ {
						log.Info("request", "method", "GET", "status", 200, "took", took)
					}
					n += 256
				}
				total.Add(n)
			}()
		}
		time.Sleep(2 * time.Second)
		stop.Store(true)
		wg.Wait()
		wall := time.Since(t0).Seconds()
		fmt.Printf("case=parallel_text impl=go ns=%.2f allocs=0\n", wall*1e9/float64(total.Load()))
	default:
		fmt.Fprintln(os.Stderr, "no such case:", what)
		os.Exit(2)
	}
}
