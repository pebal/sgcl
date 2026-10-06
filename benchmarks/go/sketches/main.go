// The concurrent module's sketches in Go, single-threaded (Go's standard
// library has none): a Bloom filter (double hashing of XXH3-128,
// multiply-shift positions, words of 64 bits), a HyperLogLog of byte
// registers with Ertl's estimator, a count-min sketch of 64-bit counters,
// written out over github.com/zeebo/xxh3, the hash the C++ side uses
// (benchmarks/concurrent/sketches.cpp has the cases). Prints one line, ns
// per operation.
package main

import (
	"encoding/binary"
	"fmt"
	"math"
	"math/bits"
	"os"
	"strconv"
	"syscall"
	"time"

	"github.com/zeebo/xxh3"
)

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func report(what string, wall float64, ops int64) {
	fmt.Printf("sketches %s ns/op=%.2f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall*1e9/float64(ops), float64(ops)/wall, wall, cpuSeconds())
}

func hash128(key uint64) (uint64, uint64) {
	var b [8]byte
	binary.LittleEndian.PutUint64(b[:], key)
	h := xxh3.Hash128(b[:])
	return h.Lo, h.Hi
}

func hash64(key uint64) uint64 {
	var b [8]byte
	binary.LittleEndian.PutUint64(b[:], key)
	return xxh3.Hash(b[:])
}

type bloom struct {
	bits  uint64
	k     uint
	words []uint64
}

func newBloom(n float64, p float64) *bloom {
	m := math.Ceil(-n * math.Log(p) / (math.Ln2 * math.Ln2))
	k := uint(math.Round(m / n * math.Ln2))
	w := (uint64(m) + 63) / 64
	return &bloom{bits: w * 64, k: k, words: make([]uint64, w)}
}

func (f *bloom) add(key uint64) bool {
	lo, hi := hash128(key)
	fresh := false
	x := lo
	for i := uint(0); i < f.k; i++ {
		b, _ := bits.Mul64(x, f.bits)
		m := uint64(1) << (b & 63)
		if f.words[b>>6]&m == 0 {
			f.words[b>>6] |= m
			fresh = true
		}
		x += hi
	}
	return fresh
}

func (f *bloom) contains(key uint64) bool {
	lo, hi := hash128(key)
	x := lo
	for i := uint(0); i < f.k; i++ {
		b, _ := bits.Mul64(x, f.bits)
		if f.words[b>>6]>>(b&63)&1 == 0 {
			return false
		}
		x += hi
	}
	return true
}

func (f *bloom) count() float64 {
	x := 0
	for _, w := range f.words {
		x += bits.OnesCount64(w)
	}
	m := float64(f.bits)
	return -m / float64(f.k) * math.Log1p(-float64(x)/m)
}

type hll struct {
	p   uint
	reg []uint8
}

func (h *hll) add(key uint64) {
	x := hash64(key)
	i := x >> (64 - h.p)
	rank := uint8(bits.LeadingZeros64(x<<h.p|1<<(h.p-1)) + 1)
	if rank > h.reg[i] {
		h.reg[i] = rank
	}
}

func sigma(x float64) float64 {
	if x == 1 {
		return math.Inf(1)
	}
	y, z := 1.0, x
	for {
		x *= x
		prev := z
		z += x * y
		y += y
		if z == prev {
			return z
		}
	}
}

func tau(x float64) float64 {
	if x == 0 || x == 1 {
		return 0
	}
	y, z := 1.0, 1-x
	for {
		x = math.Sqrt(x)
		prev := z
		y *= 0.5
		z -= (1 - x) * (1 - x) * y
		if z == prev {
			return z / 3
		}
	}
}

func (h *hll) estimate() float64 {
	var c [66]uint32
	for _, r := range h.reg {
		c[r]++
	}
	q := 64 - h.p
	m := float64(len(h.reg))
	z := m * tau(1-float64(c[q+1])/m)
	for k := q; k >= 1; k-- {
		z = 0.5 * (z + float64(c[k]))
	}
	z += m * sigma(float64(c[0])/m)
	return 0.7213475204444817 * m * m / z
}

func (h *hll) merge(o *hll) {
	for i, r := range o.reg {
		if r > h.reg[i] {
			h.reg[i] = r
		}
	}
}

type cms struct {
	w, d  uint64
	c     []uint64
	total uint64
}

func newCms(eps, delta float64) *cms {
	w := uint64(math.Ceil(math.E / eps))
	d := uint64(math.Ceil(math.Log(1 / delta)))
	return &cms{w: w, d: d, c: make([]uint64, w*d)}
}

func (s *cms) add(key, n uint64) {
	lo, hi := hash128(key)
	x := lo
	for r := uint64(0); r < s.d; r++ {
		b, _ := bits.Mul64(x, s.w)
		s.c[r*s.w+b] += n
		x += hi
	}
	s.total += n
}

func (s *cms) estimate(key uint64) uint64 {
	lo, hi := hash128(key)
	x := lo
	least := uint64(math.MaxUint64)
	for r := uint64(0); r < s.d; r++ {
		b, _ := bits.Mul64(x, s.w)
		if v := s.c[r*s.w+b]; v < least {
			least = v
		}
		x += hi
	}
	return least
}

func main() {
	what := "bloom_add"
	if len(os.Args) > 1 {
		what = os.Args[1]
	}
	var n int64
	if len(os.Args) > 2 {
		n, _ = strconv.ParseInt(os.Args[2], 10, 64)
	}
	var sink float64
	switch what {
	case "bloom_add":
		if n == 0 {
			n = 20000000
		}
		f := newBloom(1e6, 0.01)
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if f.add(uint64(i)) {
				sink++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	case "bloom_contains":
		if n == 0 {
			n = 20000000
		}
		f := newBloom(1e6, 0.01)
		for i := uint64(0); i < 1000000; i++ {
			f.add(i * 2)
		}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if f.contains(uint64(i % 2000000)) {
				sink++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	case "bloom_count":
		f := newBloom(1e6, 0.01)
		for i := uint64(0); i < 1000000; i++ {
			f.add(i)
		}
		if n == 0 {
			n = 2000
		}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			sink += f.count()
		}
		report(what, time.Since(t0).Seconds(), n)
	case "hll_add":
		if n == 0 {
			n = 50000000
		}
		h := &hll{p: 14, reg: make([]uint8, 1<<14)}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			h.add(uint64(i))
		}
		sink = h.estimate()
		report(what, time.Since(t0).Seconds(), n)
	case "hll_estimate", "hll_merge":
		a := &hll{p: 14, reg: make([]uint8, 1<<14)}
		b := &hll{p: 14, reg: make([]uint8, 1<<14)}
		for i := uint64(0); i < 1000000; i++ {
			a.add(i)
			b.add(i + 500000)
		}
		if n == 0 {
			n = 20000
		}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if what == "hll_estimate" {
				sink += a.estimate()
			} else {
				a.merge(b)
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	case "cms_add", "cms_estimate":
		if n == 0 {
			n = 20000000
		}
		c := newCms(0.001, 0.01)
		if what == "cms_estimate" {
			for i := uint64(0); i < 1000000; i++ {
				c.add(i%100000, 1)
			}
		}
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if what == "cms_add" {
				c.add(uint64(i%100000), 1)
			} else {
				sink += float64(c.estimate(uint64(i % 100000)))
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
	if sink < 0 {
		fmt.Println(sink)
	}
}
