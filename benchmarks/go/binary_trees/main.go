// Same shape as benchmarks/binary_trees.cpp: a stretch tree, one long-lived
// tree of max_depth, and for the depths 4, 6, ..., max_depth many short-lived
// trees, the depths split over `threads` goroutines. Prints wall and process
// CPU time.
//   binary_trees [max_depth=21] [threads=1]
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
	maxDepth, threads := 21, 1
	if len(os.Args) > 1 {
		maxDepth, _ = strconv.Atoi(os.Args[1])
	}
	if len(os.Args) > 2 {
		threads, _ = strconv.Atoi(os.Args[2])
	}
	const minDepth = 4
	if maxDepth < minDepth+2 {
		maxDepth = minDepth + 2
	}
	t0 := time.Now()
	stretch := make_(maxDepth + 1)
	fmt.Printf("stretch tree of depth %d\t check: %d\n", maxDepth+1, check(stretch))
	stretch = nil
	longLived := make_(maxDepth)
	lines := make([]string, (maxDepth-minDepth)/2+1)
	var next int32 = 0
	var wg sync.WaitGroup
	for t := 0; t < threads; t++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for {
				i := int(atomic.AddInt32(&next, 1)) - 1
				if i >= len(lines) {
					break
				}
				depth := minDepth + 2*i
				iterations := int64(1) << uint(maxDepth-depth+minDepth)
				var sum int64
				for j := int64(0); j < iterations; j++ {
					sum += check(make_(depth))
				}
				lines[i] = fmt.Sprintf("%d\t trees of depth %d\t check: %d", iterations, depth, sum)
			}
		}()
	}
	wg.Wait()
	for _, l := range lines {
		fmt.Println(l)
	}
	fmt.Printf("long lived tree of depth %d\t check: %d\n", maxDepth, check(longLived))
	fmt.Printf("wall=%.2fs cpu=%.2fs\n", time.Since(t0).Seconds(), cpuSeconds())
}
