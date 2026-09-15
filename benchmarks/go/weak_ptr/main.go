// Same shape as benchmarks/weak_ptr.cpp: the cost of a weak pointer
// (weak.Pointer, Go 1.24 and later).
//   weak_ptr [threads=1] [op=lock|copy|make|expired]
//   lock: a weak pointer to a live object dereferenced (Value)
//   expired: a weak pointer to a collected object dereferenced: the nil answer
//   copy: a weak pointer copied into a local, then tested
//   make: a weak pointer made from a strong one (weak.Make), then tested
// Prints nanoseconds per operation.
package main

import (
	"fmt"
	"os"
	"runtime"
	"strconv"
	"sync"
	"syscall"
	"time"
	"weak"
)

type node struct {
	v   int64
	pad [120]byte
}

const iters = 20000000

var sink [256]int64

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func expired(w weak.Pointer[node]) int64 {
	if w.Value() == nil {
		return 1
	}
	return 0
}

func main() {
	threads, op := 1, "lock"
	if len(os.Args) > 1 {
		threads, _ = strconv.Atoi(os.Args[1])
	}
	if len(os.Args) > 2 {
		op = os.Args[2]
	}
	var wg sync.WaitGroup
	t0 := time.Now()
	for t := 0; t < threads; t++ {
		wg.Add(1)
		go func(t int) {
			defer wg.Done()
			strong := &node{v: 1}
			wk := weak.Make(strong)
			if op == "expired" {
				strong = &node{v: 1} // the first node dropped: collected below, the pointer cleared
				for i := 0; i < 10 && wk.Value() != nil; i++ {
					runtime.GC()
				}
				if wk.Value() != nil {
					fmt.Fprintln(os.Stderr, "no object expired: the numbers below are of a live one")
				}
			}
			var sum int64
			switch op {
			case "lock", "expired":
				for i := 0; i < iters; i++ {
					if p := wk.Value(); p != nil {
						sum += p.v
					}
				}
			case "copy":
				for i := 0; i < iters; i++ {
					cp := wk
					sum += expired(cp)
				}
			default:
				for i := 0; i < iters; i++ {
					made := weak.Make(strong)
					sum += expired(made)
				}
			}
			sink[t] = sum + strong.v
		}(t)
	}
	wg.Wait()
	ns := time.Since(t0).Seconds() * 1e9 / iters
	fmt.Printf("go threads=%d op=%s ns/op=%.2f cpu=%.2fs\n", threads, op, ns, cpuSeconds())
}
