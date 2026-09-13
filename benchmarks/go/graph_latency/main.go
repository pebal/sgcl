// Same shape as benchmarks/graph_latency.cpp: each goroutine keeps a ring of
// roots; an operation is an insert (a new node linked to four random roots,
// stored into a random root slot) or a walk (32 random steps copying the
// pointer). Every operation is timed. Prints percentiles in ns, the drop-all
// time, the rate, wall and process CPU time.
//   graph_latency [threads=cores] [seconds=5] [roots=4096]
package main

import (
	"fmt"
	"math/rand"
	"os"
	"runtime"
	"sort"
	"strconv"
	"sync"
	"syscall"
	"time"
)

const links = 4
const walkSteps = 32

type Node struct {
	link  [links]*Node
	value int64
	pad   [3]int64
}

type result struct {
	insert, walk []uint32
	ops          int64
	checksum     int64
	dropNs       float64
}

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func worker(id int, seconds float64, rootCount int) result {
	var r result
	r.insert = make([]uint32, 0, 1<<22)
	r.walk = make([]uint32, 0, 1<<22)
	rng := rand.New(rand.NewSource(int64(1234 + id)))
	roots := make([]*Node, rootCount)
	for i := range roots {
		roots[i] = &Node{value: int64(i)}
	}
	t0 := time.Now()
	var op int64
	for {
		if op&1023 == 0 && time.Since(t0).Seconds() >= seconds {
			break
		}
		op++
		start := time.Now()
		if op%4 == 0 {
			n := &Node{value: op}
			for l := 0; l < links; l++ {
				n.link[l] = roots[rng.Intn(rootCount)]
			}
			roots[rng.Intn(rootCount)] = n
			r.insert = append(r.insert, uint32(time.Since(start).Nanoseconds()))
		} else {
			cur := roots[rng.Intn(rootCount)]
			var sum int64
			for s := 0; s < walkSteps; s++ {
				sum += cur.value
				next := cur.link[rng.Intn(links)]
				if next == nil {
					break
				}
				cur = next
			}
			r.checksum += sum
			r.walk = append(r.walk, uint32(time.Since(start).Nanoseconds()))
		}
	}
	r.ops = op
	drop := time.Now()
	for i := range roots {
		roots[i] = nil
	}
	roots = nil
	r.dropNs = float64(time.Since(drop).Nanoseconds())
	return r
}

func pct(v []uint32) (p50, p90, p99, p999, p9999, max float64) {
	sort.Slice(v, func(i, j int) bool { return v[i] < v[j] })
	at := func(q float64) float64 { return float64(v[int(q*float64(len(v)-1))]) }
	return at(0.5), at(0.9), at(0.99), at(0.999), at(0.9999), float64(v[len(v)-1])
}

func main() {
	threads, seconds, rootCount := runtime.NumCPU(), 5.0, 4096
	if len(os.Args) > 1 {
		threads, _ = strconv.Atoi(os.Args[1])
	}
	if len(os.Args) > 2 {
		seconds, _ = strconv.ParseFloat(os.Args[2], 64)
	}
	if len(os.Args) > 3 {
		rootCount, _ = strconv.Atoi(os.Args[3])
	}
	results := make([]result, threads)
	var wg sync.WaitGroup
	t0 := time.Now()
	for t := 0; t < threads; t++ {
		wg.Add(1)
		go func(t int) {
			defer wg.Done()
			results[t] = worker(t, seconds, rootCount)
		}(t)
	}
	wg.Wait()
	wall := time.Since(t0).Seconds()
	var insert, walk []uint32
	var ops, checksum int64
	var dropMax float64
	for _, r := range results {
		insert = append(insert, r.insert...)
		walk = append(walk, r.walk...)
		ops += r.ops
		checksum += r.checksum
		if r.dropNs > dropMax {
			dropMax = r.dropNs
		}
	}
	a, b, c, d, e, f := pct(insert)
	fmt.Printf("insert p50=%.0f p90=%.0f p99=%.0f p99.9=%.0f p99.99=%.0f max=%.0f ns\n", a, b, c, d, e, f)
	a, b, c, d, e, f = pct(walk)
	fmt.Printf("walk   p50=%.0f p90=%.0f p99=%.0f p99.9=%.0f p99.99=%.0f max=%.0f ns\n", a, b, c, d, e, f)
	fmt.Printf("drop-all max=%.3f ms  ops/s=%.0f  wall=%.2fs cpu=%.2fs  (checksum %d)\n", dropMax*1e-6, float64(ops)/wall, wall, cpuSeconds(), checksum)
}
