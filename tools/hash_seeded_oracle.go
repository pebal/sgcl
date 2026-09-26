// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the seeded hashes of sgcl/hash (xxh3.h, siphash.h): two
// oracles for each, asked the same questions and required to agree before
// an answer is written out as a C++ header the tests include —
//
//   - XXH3: Go's github.com/zeebo/xxh3 and the reference implementation in
//     C (xxhash.h of github.com/Cyan4973/xxHash, through cgo);
//   - SipHash-2-4: Go's github.com/dchest/siphash and the reference
//     implementation in C (siphash.c of github.com/veorq/SipHash).
//
// None of it is in the tree: the Go modules and both C sources live under
// ~/Programming/oracles, and the program runs from a module there:
//
//     mkdir -p ~/Programming/oracles/hash-go && cd ~/Programming/oracles/hash-go
//     git clone --depth 1 https://github.com/Cyan4973/xxHash.git ../xxhash
//     git clone --depth 1 https://github.com/veorq/SipHash.git ../siphash
//     printf 'module hashoracle\n\ngo 1.27.1\n' > go.mod
//     export GOMODCACHE=$HOME/Programming/oracles/go-mod
//     go get github.com/zeebo/xxh3@v1.1.0 github.com/dchest/siphash@v1.2.3
//     CGO_CFLAGS="-I$HOME/Programming/oracles/xxhash -I$HOME/Programming/oracles/siphash" \
//         go run <tree>/tools/hash_seeded_oracle.go > <tree>/tests/hash/seeded_vectors.h
//
// What is asked, each named, since an oracle checks only the cases it is
// given (the patterns are those of tools/hash_oracle.go: 0 random from
// splitmix64 seeded 1, 1 zeros, 2 0xFF, 3 rising i mod 256):
//
//   - XXH3 64 and 128 bits, seed 0, the random pattern at every length
//     0…2048: every short path (0, 1–3, 4–8, 9–16, 17–128, 129–240), the
//     long path's first stripe, every count of stripes in the first two
//     blocks and the block boundary at 1024;
//   - the same with the seeds 1, 2^64 − 1 and 0x9E3779B97F4A7C15 at every
//     length 0…300 and every seventh from 301 to 2048 with 1023…1025 and
//     2047, 2048 (a seed other than 0 makes a secret of its own past 240);
//   - zeros, 0xFF and the rising pattern with seed 0 at every length 0…256
//     (zeros make every product of the long path's lanes zero);
//   - every pattern and every seed at 2^k − 1, 2^k and 2^k + 1 for
//     k = 11…20, up to a megabyte, and at 16·1024 ± 1 and 100003;
//   - SipHash-2-4 with the key 00 01 … 0f over the rising pattern at every
//     length 0…63 (the 64 vectors of the paper and of the reference's
//     vectors.h), and with that key and three random keys over the random
//     pattern at every length 0…300 and at 2^k ± 1, 2^k for k = 11…20.
package main

/*
#define XXH_INLINE_ALL
#include "xxhash.h"
#include "siphash.c"

static unsigned long long ref_xxh3_64(const void* p, size_t n, unsigned long long seed) {
	return XXH3_64bits_withSeed(p, n, seed);
}

static void ref_xxh3_128(const void* p, size_t n, unsigned long long seed, unsigned long long* high, unsigned long long* low) {
	XXH128_hash_t h = XXH3_128bits_withSeed(p, n, seed);
	*high = h.high64;
	*low = h.low64;
}

static unsigned long long ref_siphash(const void* p, size_t n, const void* key) {
	uint8_t out[8];
	siphash(p, n, key, out, 8);
	unsigned long long v = 0;
	for (int i = 7; i >= 0; --i) v = v << 8 | out[i];
	return v;
}
*/
import "C"

import (
	"encoding/binary"
	"fmt"
	"strings"
	"unsafe"

	"github.com/dchest/siphash"
	"github.com/zeebo/xxh3"
)

// splitmix64: the generator of the random pattern, the same on both sides
type splitmix struct{ s uint64 }

func (r *splitmix) next() uint64 {
	r.s += 0x9e3779b97f4a7c15
	z := r.s
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb
	return z ^ (z >> 31)
}

func pattern(kind, n int) []byte {
	b := make([]byte, n+8)
	switch kind {
	case 0:
		r := splitmix{1}
		for i := 0; i < n; i += 8 {
			binary.LittleEndian.PutUint64(b[i:], r.next())
		}
	case 2:
		for i := range b {
			b[i] = 0xff
		}
	case 3:
		for i := range b {
			b[i] = byte(i)
		}
	}
	return b[:n]
}

func pointer(b []byte) unsafe.Pointer {
	if len(b) == 0 {
		return nil
	}
	return unsafe.Pointer(&b[0])
}

var seeds = []uint64{0, 1, 1<<64 - 1, 0x9E3779B97F4A7C15}

func xxh3Row(kind, n, seed int) string {
	b := pattern(kind, n)
	s := seeds[seed]
	h64 := xxh3.HashSeed(b, s)
	h128 := xxh3.Hash128Seed(b, s)
	r64 := uint64(C.ref_xxh3_64(pointer(b), C.size_t(n), C.ulonglong(s)))
	var high, low C.ulonglong
	C.ref_xxh3_128(pointer(b), C.size_t(n), C.ulonglong(s), &high, &low)
	if h64 != r64 || h128.Hi != uint64(high) || h128.Lo != uint64(low) {
		panic(fmt.Sprintf("the two XXH3 oracles disagree: pattern %d length %d seed %x", kind, n, s))
	}
	return fmt.Sprintf("{%d,%d,%d,0x%016x,0x%016x,0x%016x},\n", kind, n, seed, h64, h128.Hi, h128.Lo)
}

func sipRow(kind, n, key int, keys [][]byte) string {
	b := pattern(kind, n)
	k := keys[key]
	v := siphash.Hash(binary.LittleEndian.Uint64(k), binary.LittleEndian.Uint64(k[8:]), b)
	h := siphash.New(k)
	h.Write(b)
	r := uint64(C.ref_siphash(pointer(b), C.size_t(n), unsafe.Pointer(&k[0])))
	if v != r || h.Sum64() != r {
		panic(fmt.Sprintf("the two SipHash oracles disagree: pattern %d length %d key %d", kind, n, key))
	}
	return fmt.Sprintf("{%d,%d,%d,0x%016x},\n", kind, n, key, v)
}

func main() {
	var out strings.Builder
	out.WriteString(`// Generated by tools/hash_seeded_oracle.go from github.com/zeebo/xxh3 and
// github.com/dchest/siphash, each checked against the reference
// implementation in C; do not edit. The header of the generator says how to
// run it and which cases were asked.
#pragma once

#include <cstdint>

namespace seeded_vectors {
    // pattern: 0 random (splitmix64 from 1, little-endian), 1 zeros,
    // 2 0xFF, 3 rising (i mod 256); seed: an index into seeds
    inline constexpr uint64_t seeds[] = {0, 1, 0xffffffffffffffff, 0x9e3779b97f4a7c15};

    struct Xxh3 {
        int pattern;
        uint32_t length;
        int seed;
        uint64_t xxh3_64, xxh3_128_high, xxh3_128_low;
    };

    inline constexpr Xxh3 xxh3[] = {
`)
	var big []int
	for k := 11; k <= 20; k++ {
		big = append(big, (1<<k)-1, 1<<k, (1<<k)+1)
	}
	big = append(big, 16*1024-1, 16*1024+1, 100003)
	for n := 0; n <= 2048; n++ {
		out.WriteString(xxh3Row(0, n, 0))
	}
	for seed := 1; seed < len(seeds); seed++ {
		for n := 0; n <= 2048; n++ {
			if n <= 300 || (n-301)%7 == 0 || (n >= 1023 && n <= 1025) || n >= 2047 {
				out.WriteString(xxh3Row(0, n, seed))
			}
		}
	}
	for kind := 1; kind < 4; kind++ {
		for n := 0; n <= 256; n++ {
			out.WriteString(xxh3Row(kind, n, 0))
		}
	}
	for kind := 0; kind < 4; kind++ {
		for seed := range seeds {
			for _, n := range big {
				out.WriteString(xxh3Row(kind, n, seed))
			}
		}
	}
	out.WriteString("    };\n\n")

	r := splitmix{11}
	keys := [][]byte{make([]byte, 16)}
	for i := range keys[0] {
		keys[0][i] = byte(i)
	}
	for i := 0; i < 3; i++ {
		k := make([]byte, 16)
		binary.LittleEndian.PutUint64(k, r.next())
		binary.LittleEndian.PutUint64(k[8:], r.next())
		keys = append(keys, k)
	}
	out.WriteString(`    // SipHash-2-4; key: an index into keys
    inline constexpr unsigned char keys[][16] = {
`)
	for _, k := range keys {
		out.WriteString("        {")
		for i, c := range k {
			if i > 0 {
				out.WriteString(", ")
			}
			fmt.Fprintf(&out, "0x%02x", c)
		}
		out.WriteString("},\n")
	}
	out.WriteString(`    };

    struct Sip {
        int pattern;
        uint32_t length;
        int key;
        uint64_t siphash;
    };

    inline constexpr Sip siphash[] = {
`)
	for n := 0; n < 64; n++ {
		out.WriteString(sipRow(3, n, 0, keys))
	}
	for key := range keys {
		for n := 0; n <= 300; n++ {
			out.WriteString(sipRow(0, n, key, keys))
		}
		for _, n := range big[:30] {
			out.WriteString(sipRow(0, n, key, keys))
		}
	}
	out.WriteString("    };\n}\n")
	fmt.Print(out.String())
}
