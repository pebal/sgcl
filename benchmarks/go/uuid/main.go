// Same shape as benchmarks/encoding/uuid.cpp. Go's standard library has no
// UUID: the work github.com/google/uuid does, with the standard library —
// v4 from crypto/rand (NewRandom), v7 from the clock and crypto/rand
// (NewV7, a counter in rand_a), the text by hex with hyphens (String),
// parse of the canonical form by hex decoding (Parse).
//   uuid [op=v4] [count]
// ops: v4, v7 (a UUID made), v4_string (made and written), parse (the text
// of a UUID read).
// Prints nanoseconds per operation.
package main

import (
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"strconv"
	"sync"
	"time"
)

type UUID [16]byte

func v4() UUID {
	var u UUID
	if _, err := rand.Read(u[:]); err != nil {
		panic(err)
	}
	u[6] = (u[6] & 0x0f) | 0x40
	u[8] = (u[8] & 0x3f) | 0x80
	return u
}

var (
	lastV7 int64
	mu     sync.Mutex
)

func v7() UUID {
	var u UUID
	if _, err := rand.Read(u[:]); err != nil {
		panic(err)
	}
	// google/uuid's getV7Time: milliseconds and a 12-bit sequence, under a lock
	nano := time.Now().UnixNano()
	milli := nano / 1e6
	seq := (nano - milli*1e6) >> 8
	now := milli<<12 + seq
	mu.Lock()
	if now <= lastV7 {
		now = lastV7 + 1
		milli = now >> 12
		seq = now & 0xfff
	}
	lastV7 = now
	mu.Unlock()
	u[0], u[1], u[2], u[3], u[4], u[5] = byte(milli>>40), byte(milli>>32), byte(milli>>24), byte(milli>>16), byte(milli>>8), byte(milli)
	u[6] = 0x70 | (0x0f & byte(seq>>8))
	u[7] = byte(seq)
	u[8] = (u[8] & 0x3f) | 0x80
	return u
}

func (u UUID) String() string {
	var buf [36]byte
	hex.Encode(buf[:], u[:4])
	buf[8] = '-'
	hex.Encode(buf[9:13], u[4:6])
	buf[13] = '-'
	hex.Encode(buf[14:18], u[6:8])
	buf[18] = '-'
	hex.Encode(buf[19:23], u[8:10])
	buf[23] = '-'
	hex.Encode(buf[24:], u[10:])
	return string(buf[:])
}

func parse(s string) (UUID, error) {
	var u UUID
	if len(s) != 36 || s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-' {
		return u, errors.New("invalid UUID")
	}
	j := 0
	for i := 0; i < 36; {
		if i == 8 || i == 13 || i == 18 || i == 23 {
			i++
			continue
		}
		if _, err := hex.Decode(u[j:j+1], []byte(s[i:i+2])); err != nil {
			return u, err
		}
		i += 2
		j++
	}
	return u, nil
}

var sink int

func main() {
	op := "v4"
	if len(os.Args) > 1 {
		op = os.Args[1]
	}
	count := 10_000_000
	if len(os.Args) > 2 {
		count, _ = strconv.Atoi(os.Args[2])
	}
	text := "f81d4fae-7dec-11d0-a765-00a0c91e6bf6"
	var f func()
	switch op {
	case "v4":
		f = func() { u := v4(); sink += int(u[0]) }
	case "v7":
		f = func() { u := v7(); sink += int(u[15]) }
	case "v4_string":
		f = func() { sink += len(v4().String()) }
	case "parse":
		f = func() {
			u, err := parse(text)
			if err != nil {
				panic(err)
			}
			sink += int(u[3])
		}
	default:
		fmt.Fprintln(os.Stderr, "uuid: no op called", op)
		os.Exit(2)
	}
	for i := 0; i < 1000; i++ {
		f()
	}
	t0 := time.Now()
	for i := 0; i < count; i++ {
		f()
	}
	ns := float64(time.Since(t0).Nanoseconds()) / float64(count)
	fmt.Printf("go op=%s count=%d ns/op=%.1f\n", op, count, ns)
}
