// Same shape as benchmarks/write_barrier.cpp: the cost of copying a pointer.
// mode stack: a local takes objs[i] (a store the collector never sees:
// Go's stacks are scanned precisely, so no barrier); mode heap: a field of
// a heap object takes objs[i], through Go's write barrier while a cycle
// marks. targets: distinct pointees cycled through; shared: every goroutine
// copies pointers to the same objects.
//   write_barrier [threads=1] [mode=stack|heap] [targets=1] [shared]
package main

import (
	"fmt"
	"os"
	"strconv"
	"sync"
	"syscall"
	"time"
)

type node struct {
	v    int64
	next *node
}

const iters = 50000000

var sink [256]*node

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func main() {
	threads, mode, targets, shared := 1, "stack", 1, false
	if len(os.Args) > 1 {
		threads, _ = strconv.Atoi(os.Args[1])
	}
	if len(os.Args) > 2 {
		mode = os.Args[2]
	}
	if len(os.Args) > 3 {
		targets, _ = strconv.Atoi(os.Args[3])
	}
	if len(os.Args) > 4 && os.Args[4] == "shared" {
		shared = true
	}
	var sharedObjs []*node
	if shared {
		for i := 0; i < targets; i++ {
			sharedObjs = append(sharedObjs, &node{})
		}
	}
	var wg sync.WaitGroup
	t0 := time.Now()
	for t := 0; t < threads; t++ {
		wg.Add(1)
		go func(t int) {
			defer wg.Done()
			objs := sharedObjs
			if !shared {
				for i := 0; i < targets; i++ {
					objs = append(objs, &node{})
				}
			}
			holder := &node{}
			if mode == "stack" {
				var dst *node
				for i := 0; i < iters; i++ {
					dst = objs[i%targets]
				}
				sink[t] = dst
			} else {
				for i := 0; i < iters; i++ {
					holder.next = objs[i%targets]
				}
				sink[t] = holder
			}
		}(t)
	}
	wg.Wait()
	ns := time.Since(t0).Seconds() * 1e9 / iters
	extra := ""
	if shared {
		extra = " shared"
	}
	fmt.Printf("go threads=%d mode=%s targets=%d%s ns/copy=%.2f cpu=%.2fs\n", threads, mode, targets, extra, ns, cpuSeconds())
}
