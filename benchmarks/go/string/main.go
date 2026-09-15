// Same shape as benchmarks/string.cpp: the cost of a string kept in
// objects (Go's string: a pointer and a length, the bytes immutable).
//   string [op=make|copy|hash1|hashn] [len=10]
//   make: 2 M strings made from tokens of a byte buffer, each stored in a node
//   copy: 2 M strings copied from one node to another, in order
//   hash1: 2 M strings each hashed once (hash/maphash, what a map does)
//   hashn: 2 M strings each hashed eight times in a row
// Prints nanoseconds per operation.
package main

import (
	"fmt"
	"hash/maphash"
	"math/rand"
	"os"
	"runtime"
	"strconv"
	"time"
)

const count = 2000000

type node struct {
	s string
}

var sink uint64

func main() {
	op := "make"
	if len(os.Args) > 1 {
		op = os.Args[1]
	}
	length := 10
	if len(os.Args) > 2 {
		length, _ = strconv.Atoi(os.Args[2])
	}
	rng := rand.New(rand.NewSource(1))
	buffer := make([]byte, count*length)
	for i := range buffer {
		buffer[i] = byte('a' + rng.Intn(26))
	}
	nodes := make([]*node, count)
	for i := range nodes {
		nodes[i] = &node{}
	}
	var ns float64
	switch op {
	case "make":
		runtime.GC()
		t0 := time.Now()
		for i := 0; i < count; i++ {
			nodes[i].s = string(buffer[i*length : (i+1)*length])
		}
		ns = float64(time.Since(t0).Nanoseconds()) / count
	default:
		for i := 0; i < count; i++ {
			nodes[i].s = string(buffer[i*length : (i+1)*length])
		}
		switch op {
		case "copy":
			targets := make([]*node, count)
			for i := range targets {
				targets[i] = &node{}
			}
			runtime.GC()
			t0 := time.Now()
			for i := 0; i < count; i++ {
				targets[i].s = nodes[i].s
			}
			ns = float64(time.Since(t0).Nanoseconds()) / count
			sink += uint64(len(targets[7].s))
		case "hash1", "hashn":
			times := 1
			if op == "hashn" {
				times = 8
			}
			seed := maphash.MakeSeed()
			var sum uint64
			t0 := time.Now()
			for i := 0; i < count; i++ {
				for k := 0; k < times; k++ {
					sum += maphash.String(seed, nodes[i].s)
				}
			}
			ns = float64(time.Since(t0).Nanoseconds()) / count / float64(times)
			sink += sum
		}
	}
	fmt.Printf("go op=%s len=%d ns/op=%.2f\n", op, length, ns)
}
