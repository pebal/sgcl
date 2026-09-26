// The async module's counterparts in Go: goroutines on the runtime's
// scheduler, channels, time.After, sync.Cond, sync.Mutex and a
// range-over-func iterator, one case per run (benchmarks/async/async.cpp
// has the SGCL side; the cases with no counterpart here, an executor's
// yield and a strand, are left out). Prints one line, ns per operation.
package main

import (
	"fmt"
	"os"
	"runtime"
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
	fmt.Printf("async %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall*1e9/float64(ops), float64(ops)/wall, wall, cpuSeconds())
}

func counting(n int64) func(func(int64) bool) {
	return func(yield func(int64) bool) {
		for i := int64(0); i < n; i++ {
			if !yield(i) {
				return
			}
		}
	}
}

func main() {
	what := "yield"
	if len(os.Args) > 1 {
		what = os.Args[1]
	}
	var n int64
	if len(os.Args) > 2 {
		n, _ = strconv.ParseInt(os.Args[2], 10, 64)
	}
	var sum int64
	switch what {
	case "yield": // runtime.Gosched in a goroutine
		if n == 0 {
			n = 2000000
		}
		done := make(chan struct{})
		t0 := time.Now()
		go func() {
			for i := int64(0); i < n; i++ {
				runtime.Gosched()
			}
			close(done)
		}()
		<-done
		report("yield", time.Since(t0).Seconds(), n)
	case "await", "spawn": // a goroutine started and waited for, one at a time
		if n == 0 {
			n = 500000
		}
		t0 := time.Now()
		r := make(chan int64)
		for i := int64(0); i < n; i++ {
			go func(v int64) { r <- v }(i)
			sum += <-r
		}
		report(what, time.Since(t0).Seconds(), n)
	case "whenall": // two goroutines and a WaitGroup, per goroutine
		if n == 0 {
			n = 500000
		}
		t0 := time.Now()
		var a, b int64
		for i := int64(0); i < n; i++ {
			var wg sync.WaitGroup
			wg.Add(2)
			go func() { a = i; wg.Done() }()
			go func() { b = i; wg.Done() }()
			wg.Wait()
			sum += a + b
		}
		report("whenall", time.Since(t0).Seconds(), 2*n)
	case "timeout": // a goroutine that answers at once, raced against time.After(1h)
		if n == 0 {
			n = 200000
		}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			r := make(chan int64, 1)
			go func(v int64) { r <- v }(i)
			select {
			case v := <-r:
				sum += v
			case <-time.After(time.Hour):
			}
		}
		report("timeout", time.Since(t0).Seconds(), n)
	case "select": // a channel with an element beside time.After(1h)
		if n == 0 {
			n = 500000
		}
		ch := make(chan int64, 1)
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			ch <- i
			select {
			case v := <-ch:
				sum += v
			case <-time.After(time.Hour):
			}
		}
		report("select", time.Since(t0).Seconds(), n)
	case "cv": // a turn handed between two goroutines through sync.Cond
		if n == 0 {
			n = 200000
		}
		var m sync.Mutex
		cv := sync.NewCond(&m)
		turn := 0
		side := func(mine int, done chan<- struct{}) {
			for i := int64(0); i < n; i++ {
				m.Lock()
				for turn%2 != mine {
					cv.Wait()
				}
				turn++
				cv.Signal()
				m.Unlock()
			}
			done <- struct{}{}
		}
		done := make(chan struct{}, 2)
		t0 := time.Now()
		go side(0, done)
		go side(1, done)
		<-done
		<-done
		report("cv", time.Since(t0).Seconds(), 2*n)
	case "pingpong": // two goroutines over two unbuffered channels, per hop
		if n == 0 {
			n = 500000
		}
		a, b := make(chan int64), make(chan int64)
		done := make(chan struct{}, 2)
		t0 := time.Now()
		go func() {
			for i := int64(0); i < n; i++ {
				a <- i
				<-b
			}
			done <- struct{}{}
		}()
		go func() {
			for i := int64(0); i < n; i++ {
				<-a
				b <- i
			}
			done <- struct{}{}
		}()
		<-done
		<-done
		report("pingpong", time.Since(t0).Seconds(), 2*n)
	case "generator": // a range-over-func iterator: the consumer and the producer hand control directly, as the async::generator does
		if n == 0 {
			n = 2000000
		}
		t0 := time.Now()
		for v := range counting(n) {
			sum += v
		}
		report("generator", time.Since(t0).Seconds(), n)
	case "mutex": // sync.Mutex, uncontended
		if n == 0 {
			n = 2000000
		}
		var m sync.Mutex
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			m.Lock()
			sum += i
			m.Unlock()
		}
		report("mutex", time.Since(t0).Seconds(), n)
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
	if sum == -1 {
		fmt.Print("?")
	}
}
