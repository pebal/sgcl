// Same shape as benchmarks/encoding/encoding.cpp: the byte codecs of Go's
// encoding packages over the same bytes.
//   encoding [op=b64enc] [size=1024] [count]
// ops: b64enc b64dec b64to b64decto b32enc b32dec hexenc hexdec
// b64to and b64decto write into a buffer kept across calls (Encode,
// Decode); every other op makes its result each time (EncodeToString,
// DecodeString).
// Prints nanoseconds per call and megabytes of input per second.
package main

import (
	"encoding/base32"
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"os"
	"strconv"
	"time"
)

// splitmix64 seeded by the size, a byte of each output: the input of the
// C++ side
func input(n int) []byte {
	state := uint64(n)*0x9E3779B97F4A7C15 + 1
	out := make([]byte, n)
	for i := range out {
		state += 0x9E3779B97F4A7C15
		z := state
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
		z = (z ^ (z >> 27)) * 0x94D049BB133111EB
		z ^= z >> 31
		out[i] = byte(z >> 17)
	}
	return out
}

var sink int

// The first case in a process reads high: the road is walked first
func timed(count int, f func()) float64 {
	warm := count
	if warm > 1000 {
		warm = 1000
	}
	for i := 0; i < warm; i++ {
		f()
	}
	t0 := time.Now()
	for i := 0; i < count; i++ {
		f()
	}
	return float64(time.Since(t0).Nanoseconds()) / float64(count)
}

func main() {
	op := "b64enc"
	if len(os.Args) > 1 {
		op = os.Args[1]
	}
	size := 1024
	if len(os.Args) > 2 {
		size, _ = strconv.Atoi(os.Args[2])
	}
	count := 2000000000 / max(size, 1)
	if len(os.Args) > 3 {
		count, _ = strconv.Atoi(os.Args[3])
	}
	data := input(size)
	var ns float64
	switch op {
	case "b64enc":
		ns = timed(count, func() { sink += len(base64.StdEncoding.EncodeToString(data)) })
	case "b64dec":
		text := base64.StdEncoding.EncodeToString(data)
		ns = timed(count, func() { b, _ := base64.StdEncoding.DecodeString(text); sink += len(b) })
	case "b64to":
		out := make([]byte, base64.StdEncoding.EncodedLen(size))
		ns = timed(count, func() { base64.StdEncoding.Encode(out, data); sink += len(out) })
	case "b64decto":
		text := []byte(base64.StdEncoding.EncodeToString(data))
		out := make([]byte, base64.StdEncoding.DecodedLen(len(text)))
		ns = timed(count, func() { n, _ := base64.StdEncoding.Decode(out, text); sink += n })
	case "b32enc":
		ns = timed(count, func() { sink += len(base32.StdEncoding.EncodeToString(data)) })
	case "b32dec":
		text := base32.StdEncoding.EncodeToString(data)
		ns = timed(count, func() { b, _ := base32.StdEncoding.DecodeString(text); sink += len(b) })
	case "hexenc":
		ns = timed(count, func() { sink += len(hex.EncodeToString(data)) })
	case "hexdec":
		text := hex.EncodeToString(data)
		ns = timed(count, func() { b, _ := hex.DecodeString(text); sink += len(b) })
	default:
		fmt.Fprintf(os.Stderr, "encoding: no op called %s\n", op)
		os.Exit(2)
	}
	fmt.Printf("go op=%s size=%d count=%d ns/op=%.1f MB/s=%.0f\n", op, size, count, ns, float64(size)/ns*1e3)
}
