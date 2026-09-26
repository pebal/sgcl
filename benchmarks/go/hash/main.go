// The hash module's counterparts in Go: hash/crc32, hash/crc64,
// hash/adler32, hash/fnv and hash/maphash, and for XXH3 and SipHash, which
// the standard library has not, github.com/zeebo/xxh3 (NEON assembly on
// arm64) and github.com/dchest/siphash; one case and one length a run
// (benchmarks/hash/hash.cpp has the SGCL side, the same bytes and the same
// timing). Prints one line: ns per call and GB/s.
//
//	hash <crc32|crc32c|crc64|crc64_iso|adler32|fnv32|fnv32a|fnv64|fnv64a|fnv128|fnv128a|xxh3_64|xxh3_128|xxh3_64-seeded|maphash|siphash> [length=1024]
//
// A CRC and Adler-32 are the one-shot functions (crc32.Checksum with the
// table made once, as crc32.ChecksumIEEE does); an FNV is a hasher made once
// and Reset before each call, Write and Sum, which is how Go offers it.
// maphash is maphash.Bytes with a seed made once, XXH3 and SipHash the
// one-shot functions of their packages.
package main

import (
	"fmt"
	"hash/adler32"
	"hash/crc32"
	"hash/crc64"
	"hash/fnv"
	"hash/maphash"
	"os"
	"strconv"
	"time"

	"github.com/dchest/siphash"
	"github.com/zeebo/xxh3"
)

var sink uint64

func randomBytes(n int) []byte {
	b := make([]byte, n+8)
	s := uint64(1)
	for i := 0; i < n; i += 8 {
		s += 0x9e3779b97f4a7c15
		z := s
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9
		z = (z ^ (z >> 27)) * 0x94d049bb133111eb
		z ^= z >> 31
		for k := 0; k < 8; k++ {
			b[i+k] = byte(z >> (8 * k))
		}
	}
	return b[:n]
}

// Runs f for about `seconds`: calls and the time they took
func runFor(f func([]byte) uint64, data []byte, seconds float64) (uint64, float64) {
	batch := 16384
	if len(data) >= 65536 {
		batch = 16
	} else if len(data) >= 1024 {
		batch = 1024
	}
	var calls, acc uint64
	t0 := time.Now()
	wall := 0.0
	for wall < seconds {
		for i := 0; i < batch; i++ {
			acc += f(data)
		}
		calls += uint64(batch)
		wall = time.Since(t0).Seconds()
	}
	sink = acc
	return calls, wall
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: hash <case> [length]")
		os.Exit(2)
	}
	what := os.Args[1]
	n := 1024
	if len(os.Args) > 2 {
		n, _ = strconv.Atoi(os.Args[2])
	}
	data := randomBytes(n)
	ieee := crc32.MakeTable(crc32.IEEE)
	cast := crc32.MakeTable(crc32.Castagnoli)
	ecma := crc64.MakeTable(crc64.ECMA)
	iso := crc64.MakeTable(crc64.ISO)
	f32, f32a, f64, f64a := fnv.New32(), fnv.New32a(), fnv.New64(), fnv.New64a()
	f128, f128a := fnv.New128(), fnv.New128a()
	var sum [16]byte
	var f func([]byte) uint64
	switch what {
	case "crc32":
		f = func(b []byte) uint64 { return uint64(crc32.Checksum(b, ieee)) }
	case "crc32c":
		f = func(b []byte) uint64 { return uint64(crc32.Checksum(b, cast)) }
	case "crc64":
		f = func(b []byte) uint64 { return crc64.Checksum(b, ecma) }
	case "crc64_iso":
		f = func(b []byte) uint64 { return crc64.Checksum(b, iso) }
	case "adler32":
		f = func(b []byte) uint64 { return uint64(adler32.Checksum(b)) }
	case "fnv32":
		f = func(b []byte) uint64 { f32.Reset(); f32.Write(b); return uint64(f32.Sum32()) }
	case "fnv32a":
		f = func(b []byte) uint64 { f32a.Reset(); f32a.Write(b); return uint64(f32a.Sum32()) }
	case "fnv64":
		f = func(b []byte) uint64 { f64.Reset(); f64.Write(b); return f64.Sum64() }
	case "fnv64a":
		f = func(b []byte) uint64 { f64a.Reset(); f64a.Write(b); return f64a.Sum64() }
	case "fnv128":
		f = func(b []byte) uint64 { f128.Reset(); f128.Write(b); s := f128.Sum(sum[:0]); return uint64(s[0]) ^ uint64(s[15])<<8 }
	case "fnv128a":
		f = func(b []byte) uint64 { f128a.Reset(); f128a.Write(b); s := f128a.Sum(sum[:0]); return uint64(s[0]) ^ uint64(s[15])<<8 }
	case "xxh3_64":
		f = func(b []byte) uint64 { return xxh3.Hash(b) }
	case "xxh3_128":
		f = func(b []byte) uint64 { h := xxh3.Hash128(b); return h.Hi ^ h.Lo<<8 }
	case "xxh3_64-seeded":
		f = func(b []byte) uint64 { return xxh3.HashSeed(b, 0x9e3779b97f4a7c15) }
	case "maphash":
		seed := maphash.MakeSeed()
		f = func(b []byte) uint64 { return maphash.Bytes(seed, b) }
	case "siphash":
		f = func(b []byte) uint64 { return siphash.Hash(0x0706050403020100, 0x0f0e0d0c0b0a0908, b) }
	default:
		fmt.Fprintf(os.Stderr, "unknown case %s\n", what)
		os.Exit(2)
	}
	runFor(f, data, 0.25) // thrown away
	calls, wall := runFor(f, data, 2.0)
	fmt.Printf("hash %s length=%d ns/op=%.2f GB/s=%.2f wall=%.2fs\n", what, n, wall*1e9/float64(calls), float64(n)*float64(calls)/wall/1e9, wall)
}
