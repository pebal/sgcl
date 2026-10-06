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
	"log/syslog"
	"net"
	"os"
	"path/filepath"
	"runtime"
	"strings"
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
	case "syslog_unix":
		path := os.Args[2]
		os.Remove(path)
		conn, err := net.ListenUnixgram("unixgram", &net.UnixAddr{Name: path, Net: "unixgram"})
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		var stop atomic.Bool
		go func() {
			buf := make([]byte, 4096)
			for !stop.Load() {
				conn.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
				conn.Read(buf)
			}
		}()
		w, err := syslog.Dial("unixgram", path, syslog.LOG_INFO|syslog.LOG_USER, "bench")
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		log := slog.New(slog.NewTextHandler(w, &slog.HandlerOptions{ReplaceAttr: func(groups []string, a slog.Attr) slog.Attr {
			if len(groups) == 0 && (a.Key == slog.TimeKey || a.Key == slog.LevelKey) {
				return slog.Attr{} // syslog's header carries them, as the C++ side's
			}
			return a
		}}))
		run(what, func() { log.Info("request", "method", method, "status", 200, "took", took) }, 1)
		stop.Store(true)
		w.Close()
		conn.Close()
		os.Remove(path)
	case "text_rotating":
		w, err := newRotating(os.Args[2], 16<<20, 3)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		log := slog.New(slog.NewTextHandler(w, nil))
		run(what, func() { log.Info("request", "method", method, "status", 200, "took", took) }, 1)
		w.f.Close()
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

// A rotating log file of lumberjack's shape (gopkg.in/natefinch/lumberjack is not in the standard library): a
// mutex around the file, its size, and at a write that would pass the limit a rename to name-time.ext, a new file
// opened, and the oldest beyond keep removed
type rotating struct {
	mu    sync.Mutex
	path  string
	f     *os.File
	size  int64
	max   int64
	keep  int
	names []string
}

func newRotating(path string, max int64, keep int) (*rotating, error) {
	f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
	if err != nil {
		return nil, err
	}
	info, _ := f.Stat()
	return &rotating{path: path, f: f, size: info.Size(), max: max, keep: keep}, nil
}

func (r *rotating) Write(p []byte) (int, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.size > 0 && r.size+int64(len(p)) > r.max {
		ext := filepath.Ext(r.path)
		name := strings.TrimSuffix(r.path, ext) + "-" + time.Now().Format("2006-01-02T15-04-05.000") + ext
		r.f.Close()
		os.Rename(r.path, name)
		r.names = append(r.names, name)
		for len(r.names) > r.keep {
			os.Remove(r.names[0])
			r.names = r.names[1:]
		}
		f, err := os.OpenFile(r.path, os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
		if err != nil {
			return 0, err
		}
		r.f = f
		r.size = 0
	}
	n, err := r.f.Write(p)
	r.size += int64(n)
	return n, err
}
