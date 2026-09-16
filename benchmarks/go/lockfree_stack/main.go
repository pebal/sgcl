// Same shape as benchmarks/lockfree_stack.cpp: a Treiber stack shared by
// every goroutine, the head an atomic.Pointer, the collector taking care
// of ABA. mixed: every goroutine pushes a node and pops one, n times over;
// pairs: half push n each, the other half pop n each.
//   lockfree_stack [threads=4] [mode=mixed] [n=1000000]
package main

import (
	"fmt"
	"os"
	"runtime"
	"strconv"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
)

type node struct {
	next  *node
	value int64
}

var head atomic.Pointer[node]

// The backoff of sgcl/detail/backoff.h: a spin that doubles after every
// lost exchange, up to backoffMax pauses: an isb on arm64 (isb_arm64.s,
// the pause of the C++ variants), the loop alone elsewhere
const backoffMax = 4096

func backoff(pauses *int) {
	for i := 0; i < *pauses; i++ {
		isb()
	}
	if *pauses < backoffMax {
		*pauses *= 2
	}
}

func push(v int64) {
	n := &node{value: v}
	pauses := 1
	for {
		h := head.Load()
		n.next = h
		if head.CompareAndSwap(h, n) {
			return
		}
		backoff(&pauses)
	}
}

func pop() int64 {
	pauses := 1
	for {
		h := head.Load()
		if h == nil {
			return -1
		}
		if head.CompareAndSwap(h, h.next) {
			return h.value
		}
		backoff(&pauses)
	}
}

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func main() {
	threads, mode, n := 4, "mixed", int64(1000000)
	if len(os.Args) > 1 {
		threads, _ = strconv.Atoi(os.Args[1])
	}
	if len(os.Args) > 2 {
		mode = os.Args[2]
	}
	if len(os.Args) > 3 {
		n, _ = strconv.ParseInt(os.Args[3], 10, 64)
	}
	pairs := mode == "pairs"
	runtime.GOMAXPROCS(runtime.NumCPU())
	var wg sync.WaitGroup
	t0 := time.Now()
	for t := 0; t < threads; t++ {
		wg.Add(1)
		go func(t int) {
			defer wg.Done()
			var sum int64
			if !pairs {
				for i := int64(0); i < n; i++ {
					push(i)
					sum += pop()
				}
			} else if t%2 == 0 {
				for i := int64(0); i < n; i++ {
					push(i)
				}
			} else {
				for i := int64(0); i < n; {
					v := pop()
					if v >= 0 {
						sum += v
						i++
					} else {
						runtime.Gosched()
					}
				}
			}
			if sum == -1 {
				fmt.Print("?")
			}
		}(t)
	}
	wg.Wait()
	wall := time.Since(t0).Seconds()
	ops := float64(n) * float64(threads)
	if !pairs {
		ops *= 2
	}
	fmt.Printf("threads=%d mode=%s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", threads, mode, wall*1e9/ops, ops/wall, wall, cpuSeconds())
}
