// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for sgcl/hash: Go's own hash/crc32, hash/crc64, hash/adler32
// and hash/fnv asked the same questions, and the system zlib asked what
// Go has no answer for (crc32_combine, adler32_combine). The answers are
// written out as a C++ header the tests include. Run it from the root of
// the tree (cgo, for zlib):
//
//     go run tools/hash_oracle.go > tests/hash/hash_vectors.h
//
// What is asked, each named, since an oracle checks only the cases it is
// given:
//
//   - every algorithm over four patterns of bytes: random (splitmix64 from
//     seed 1, each word's bytes little-endian, which the test makes again
//     the same way), all zeros, all 0xFF (the largest sums, which is what
//     Adler-32's bound on a run is about) and rising (byte i is i mod 256);
//   - the random pattern at every length 0…1100, which crosses every
//     path of the CRCs (the words of eight, the 128-byte threshold of the
//     folding, a fold of one to sixteen blocks and every remainder after
//     it) and Adler-32's blocks of 32; the other three at 0…300;
//   - all four at 2^k − 1, 2^k and 2^k + 1 for k = 11…20, up to a
//     megabyte, where Adler-32 goes through many runs of 5536 bytes;
//   - Adler-32 going on from a checksum of 0xFFF0FFF0 (both sums one below
//     the modulus, set through Go's UnmarshalBinary) over runs of 0xFF of
//     lengths about its run and zlib's bound;
//   - CRC-32 and CRC-64 going on from a given CRC (crc32.Update,
//     crc64.Update) over the random pattern;
//   - zlib's crc32_combine and adler32_combine over random values and
//     random lengths, with 0, 1, lengths past 2^32 and up to 2^63 − 1
//     (the length only: no data is made);
//   - combine for all four CRCs by a method of the oracle's own (gf2Shift
//     below: the register's response to zero bits as a matrix over GF(2),
//     where the library multiplies by x^(8n) mod P), over random values and
//     lengths up to 2^64 − 1, which zlib's signed length cannot reach.
package main

/*
#cgo LDFLAGS: -lz
#include <zlib.h>
static unsigned long oracle_crc32_combine(unsigned long a, unsigned long b, long long n) { return crc32_combine(a, b, (z_off_t)n); }
static unsigned long oracle_adler32_combine(unsigned long a, unsigned long b, long long n) { return adler32_combine(a, b, (z_off_t)n); }
*/
import "C"

import (
	"encoding/binary"
	"fmt"
	"hash/adler32"
	"hash/crc32"
	"hash/crc64"
	"hash/fnv"
	"strings"
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

// A reflected CRC register moved past `bytes` zero bytes, by matrices over
// GF(2): column i of a matrix is what the step does to bit i of the
// register. One zero bit shifts the register down and, when bit 0 falls
// out, XORs in the polynomial; three squarings make that one byte, and a
// squaring a bit of the length doubles it. combine(a, b, n) is this of a,
// XORed with b.
type gf2 []uint64

func (m gf2) apply(v uint64) uint64 {
	var r uint64
	for i := 0; v != 0; i, v = i+1, v>>1 {
		if v&1 != 0 {
			r ^= m[i]
		}
	}
	return r
}

func (m gf2) square() gf2 {
	out := make(gf2, len(m))
	for i := range m {
		out[i] = m.apply(m[i])
	}
	return out
}

func gf2Shift(poly uint64, width int, value, bytes uint64) uint64 {
	m := make(gf2, width)
	m[0] = poly
	for i := 1; i < width; i++ {
		m[i] = 1 << (i - 1)
	}
	m = m.square().square().square()
	for ; bytes != 0; bytes >>= 1 {
		if bytes&1 != 0 {
			value = m.apply(value)
		}
		m = m.square()
	}
	return value
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

var (
	ieee = crc32.MakeTable(crc32.IEEE)
	cast = crc32.MakeTable(crc32.Castagnoli)
	ecma = crc64.MakeTable(crc64.ECMA)
	iso  = crc64.MakeTable(crc64.ISO)
)

func row(kind, n int) string {
	b := pattern(kind, n)
	f32 := fnv.New32()
	f32.Write(b)
	f32a := fnv.New32a()
	f32a.Write(b)
	f64 := fnv.New64()
	f64.Write(b)
	f64a := fnv.New64a()
	f64a.Write(b)
	f128 := fnv.New128()
	f128.Write(b)
	s128 := f128.Sum(nil)
	f128a := fnv.New128a()
	f128a.Write(b)
	s128a := f128a.Sum(nil)
	return fmt.Sprintf("    {%d, %d, 0x%08x, 0x%08x, 0x%016xull, 0x%016xull, 0x%08x, 0x%08x, 0x%08x, 0x%016xull, 0x%016xull, 0x%016xull, 0x%016xull, 0x%016xull, 0x%016xull},\n",
		kind, n, crc32.Checksum(b, ieee), crc32.Checksum(b, cast), crc64.Checksum(b, ecma), crc64.Checksum(b, iso),
		adler32.Checksum(b), f32.Sum32(), f32a.Sum32(), f64.Sum64(), f64a.Sum64(),
		binary.BigEndian.Uint64(s128[:8]), binary.BigEndian.Uint64(s128[8:]),
		binary.BigEndian.Uint64(s128a[:8]), binary.BigEndian.Uint64(s128a[8:]))
}

func main() {
	var out strings.Builder
	out.WriteString(`// Generated by tools/hash_oracle.go from Go's hash packages and the system
// zlib; do not edit. go run tools/hash_oracle.go > tests/hash/hash_vectors.h
#pragma once

#include <cstdint>

namespace hash_vectors {
    // pattern: 0 random (splitmix64 from 1, little-endian), 1 zeros,
    // 2 0xFF, 3 rising (i mod 256)
    struct Row {
        int pattern;
        uint32_t length;
        uint32_t crc32, crc32c;
        uint64_t crc64, crc64_iso;
        uint32_t adler32, fnv32, fnv32a;
        uint64_t fnv64, fnv64a;
        uint64_t fnv128_high, fnv128_low, fnv128a_high, fnv128a_low;
    };

    inline constexpr Row rows[] = {
`)
	var big []int
	for k := 11; k <= 20; k++ {
		big = append(big, (1<<k)-1, 1<<k, (1<<k)+1)
	}
	for kind := 0; kind < 4; kind++ {
		limit := 300
		if kind == 0 {
			limit = 1100
		}
		for n := 0; n <= limit; n++ {
			out.WriteString(row(kind, n))
		}
		for _, n := range big {
			out.WriteString(row(kind, n))
		}
	}
	out.WriteString("    };\n\n")

	// Adler-32 from both sums at their largest, over runs of 0xFF
	out.WriteString(`    // Adler-32 going on from 0xFFF0FFF0 over n bytes of 0xFF
    struct AdlerFrom {
        uint32_t length;
        uint32_t adler32;
    };

    inline constexpr AdlerFrom adler_from_max[] = {
`)
	for _, n := range []int{1, 31, 32, 33, 5535, 5536, 5537, 5551, 5552, 5553, 11072, 11104, 100000, 1 << 20} {
		h := adler32.New()
		state := append([]byte("adl\x01"), 0xff, 0xf0, 0xff, 0xf0)
		if err := h.(interface{ UnmarshalBinary([]byte) error }).UnmarshalBinary(state); err != nil {
			panic(err)
		}
		h.Write(pattern(2, n))
		fmt.Fprintf(&out, "        {%d, 0x%08x},\n", n, h.Sum32())
	}
	out.WriteString("    };\n\n")

	// CRCs going on from a given value
	out.WriteString(`    // The CRCs going on from a given value over the random pattern
    struct CrcFrom {
        uint32_t length;
        uint32_t from32;
        uint32_t crc32, crc32c;
        uint64_t from64;
        uint64_t crc64, crc64_iso;
    };

    inline constexpr CrcFrom crc_from[] = {
`)
	r := splitmix{7}
	for _, n := range []int{0, 1, 7, 8, 100, 127, 128, 129, 1000, 4096, 65537} {
		b := pattern(0, n)
		f32 := uint32(r.next())
		f64 := r.next()
		fmt.Fprintf(&out, "        {%d, 0x%08x, 0x%08x, 0x%08x, 0x%016xull, 0x%016xull, 0x%016xull},\n",
			n, f32, crc32.Update(f32, ieee, b), crc32.Update(f32, cast, b), f64, crc64.Update(f64, ecma, b), crc64.Update(f64, iso, b))
	}
	out.WriteString("    };\n\n")

	// zlib's combine
	out.WriteString(`    // zlib's crc32_combine and adler32_combine
    struct Combine {
        uint32_t first, second;               // for crc32
        uint32_t adler_first, adler_second;   // each half below 65521
        uint64_t second_length;
        uint32_t crc32, adler32;
    };

    inline constexpr Combine combine[] = {
`)
	lengths := []uint64{0, 1, 2, 3, 7, 8, 15, 16, 65520, 65521, 65522, 1 << 20, 1<<32 - 1, 1 << 32, 1<<32 + 1, 1 << 40, 1<<63 - 1}
	for i := 0; i < 64; i++ {
		lengths = append(lengths, r.next()>>uint(1+r.next()%63))
	}
	for _, n := range lengths {
		a, b := uint32(r.next()), uint32(r.next())
		// Adler-32 values with both halves below the modulus, as zlib expects
		aa := (a>>16)%65521<<16 | (a&0xffff)%65521
		ab := (b>>16)%65521<<16 | (b&0xffff)%65521
		c := uint32(C.oracle_crc32_combine(C.ulong(a), C.ulong(b), C.longlong(n)))
		d := uint32(C.oracle_adler32_combine(C.ulong(aa), C.ulong(ab), C.longlong(n)))
		fmt.Fprintf(&out, "        {0x%08x, 0x%08x, 0x%08x, 0x%08x, %dull, 0x%08x, 0x%08x},\n", a, b, aa, ab, n, c, d)
	}
	out.WriteString("    };\n\n")

	// combine of the four CRCs by matrices over GF(2)
	out.WriteString(`    // combine of the four CRCs by the oracle's own method (matrices over
    // GF(2)); the 32-bit ones take the low halves of first and second
    struct CombineAll {
        uint64_t first, second, second_length;
        uint32_t crc32, crc32c;
        uint64_t crc64, crc64_iso;
    };

    inline constexpr CombineAll combine_all[] = {
`)
	all := []uint64{0, 1, 2, 7, 8, 1 << 32, 1<<63 - 1, 1 << 63, 1<<64 - 1, 1<<64 - 2}
	for i := 0; i < 200; i++ {
		switch i % 3 {
		case 0:
			all = append(all, r.next())
		case 1:
			all = append(all, r.next()>>uint(r.next()%64))
		default:
			all = append(all, r.next()%100000)
		}
	}
	for _, n := range all {
		a, b := r.next(), r.next()
		a32, b32 := a&0xffffffff, b&0xffffffff
		fmt.Fprintf(&out, "        {0x%016xull, 0x%016xull, %dull, 0x%08x, 0x%08x, 0x%016xull, 0x%016xull},\n", a, b, n,
			gf2Shift(0xEDB88320, 32, a32, n)^b32, gf2Shift(0x82F63B78, 32, a32, n)^b32,
			gf2Shift(0xC96C5795D7870F42, 64, a, n)^b, gf2Shift(0xD800000000000000, 64, a, n)^b)
	}
	out.WriteString("    };\n}\n")
	fmt.Print(out.String())
}
