// The io module's exec counterparts in Go: os/exec, one case per run
// (benchmarks/io/io.cpp has the SGCL side). Prints one line, ns per
// operation.
package main

import (
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"sync"
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

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: exec <run|output|asyncrun|parallel> [n]")
		os.Exit(2)
	}
	what := os.Args[1]
	var n int64
	if len(os.Args) > 2 {
		n, _ = strconv.ParseInt(os.Args[2], 10, 64)
	}
	ok := int64(0)
	switch what {
	case "run", "asyncrun":
		if n == 0 {
			n = 1000
		}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if exec.Command("true").Run() == nil {
				ok++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	case "output":
		if n == 0 {
			n = 1000
		}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			out, err := exec.Command("echo", "hello").Output()
			if err == nil && len(out) == 6 {
				ok++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	case "parallel":
		if n == 0 {
			n = 2000
		}
		t0 := time.Now()
		var mu sync.Mutex
		for done := int64(0); done < n; done += 32 {
			var wg sync.WaitGroup
			for i := int64(0); i < 32 && done+i < n; i++ {
				wg.Add(1)
				go func() {
					defer wg.Done()
					if exec.Command("true").Run() == nil {
						mu.Lock()
						ok++
						mu.Unlock()
					}
				}()
			}
			wg.Wait()
		}
		report(what, time.Since(t0).Seconds(), n)
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
	if ok != n {
		os.Exit(1)
	}
}
