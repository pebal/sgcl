// Same shape as benchmarks/allocation.cpp: each goroutine allocates n
// objects of `size` bytes keeping only the newest (the store into a package
// variable keeps the object on the heap and goes through Go's barrier).
//   allocation [threads=1] [size=32] [n=50000000]
package main

import (
	"fmt"
	"os"
	"strconv"
	"sync"
	"syscall"
	"time"
)

type obj8 struct{ b [8]byte }
type obj32 struct{ b [32]byte }
type obj256 struct{ b [256]byte }

// one slot per goroutine, 128 B apart: no false sharing between them
type slot struct {
	p8   *obj8
	p32  *obj32
	p256 *obj256
	pad  [13]uintptr
}

var sinks [256]slot

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func main() {
	threads, size, n := 1, 32, int64(50000000)
	if len(os.Args) > 1 {
		threads, _ = strconv.Atoi(os.Args[1])
	}
	if len(os.Args) > 2 {
		size, _ = strconv.Atoi(os.Args[2])
	}
	if len(os.Args) > 3 {
		n, _ = strconv.ParseInt(os.Args[3], 10, 64)
	}
	var wg sync.WaitGroup
	t0 := time.Now()
	for t := 0; t < threads; t++ {
		wg.Add(1)
		go func(t int) {
			defer wg.Done()
			s := &sinks[t]
			switch size {
			case 8:
				for i := int64(0); i < n; i++ {
					s.p8 = &obj8{}
				}
			case 256:
				for i := int64(0); i < n; i++ {
					s.p256 = &obj256{}
				}
			default:
				for i := int64(0); i < n; i++ {
					s.p32 = &obj32{}
				}
			}
		}(t)
	}
	wg.Wait()
	ns := time.Since(t0).Seconds() * 1e9 / float64(n)
	fmt.Printf("go threads=%d size=%d ns/alloc=%.2f cpu=%.2fs\n", threads, size, ns, cpuSeconds())
}
