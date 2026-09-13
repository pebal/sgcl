// Same shape as benchmarks/large_tree.cpp: a large tree held for the whole
// run, threads making and dropping small trees. Prints wall and process
// CPU time and small trees per second.
//   large_tree [big_depth=22] [small_depth=8] [iterations=100000] [threads=1]
package main

import (
	"fmt"
	"os"
	"strconv"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
)

type Tree struct{ left, right *Tree }

func make_(depth int) *Tree {
	n := &Tree{}
	if depth > 0 {
		n.left = make_(depth - 1)
		n.right = make_(depth - 1)
	}
	return n
}

func check(n *Tree) int64 {
	if n.left != nil {
		return 1 + check(n.left) + check(n.right)
	}
	return 1
}

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func main() {
	big, small, iterations, threads := 22, 8, int64(100000), 1
	if len(os.Args) > 1 {
		big, _ = strconv.Atoi(os.Args[1])
	}
	if len(os.Args) > 2 {
		small, _ = strconv.Atoi(os.Args[2])
	}
	if len(os.Args) > 3 {
		iterations, _ = strconv.ParseInt(os.Args[3], 10, 64)
	}
	if len(os.Args) > 4 {
		threads, _ = strconv.Atoi(os.Args[4])
	}
	t0 := time.Now()
	large := make_(big)
	built := time.Since(t0).Seconds()
	var sum int64
	t1 := time.Now()
	var wg sync.WaitGroup
	for t := 0; t < threads; t++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			var s int64
			for i := int64(0); i < iterations; i++ {
				tree := make_(small)
				s += check(tree)
			}
			atomic.AddInt64(&sum, s)
		}()
	}
	wg.Wait()
	loop := time.Since(t1).Seconds()
	fmt.Printf("large tree of depth %d (%d nodes) built in %.2fs, check %d\n", big, (1<<(big+1))-1, built, check(large))
	fmt.Printf("threads=%d small=%d iterations=%d sum=%d wall=%.2fs cpu=%.2fs trees/s=%.0f\n",
		threads, small, iterations, sum, loop, cpuSeconds(), float64(iterations*int64(threads))/loop)
}
