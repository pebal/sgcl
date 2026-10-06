// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of benchmarks/crypto/mldsa.cpp: crypto/mldsa, the same work a
// case. One line with ns/op.
//
//	mldsa <case>
package main

import (
	"crypto/mldsa"
	"fmt"
	"os"
	"strings"
	"time"
)

var sink int

func measure(what string, f func() int) {
	run := func(seconds float64) (int, float64) {
		calls := 0
		t0 := time.Now()
		wall := 0.0
		for wall < seconds {
			for i := 0; i < 16; i++ {
				sink += f()
			}
			calls += 16
			wall = time.Since(t0).Seconds()
		}
		return calls, wall
	}
	run(0.25)
	calls, wall := run(2.0)
	fmt.Printf("mldsa %s ns/op=%.1f wall=%.2fs\n", what, wall*1e9/float64(calls), wall)
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: mldsa <case>")
		os.Exit(2)
	}
	what := os.Args[1]
	kind, set, _ := strings.Cut(what, "_")
	params, ok := map[string]mldsa.Parameters{"44": mldsa.MLDSA44(), "65": mldsa.MLDSA65(), "87": mldsa.MLDSA87()}[set]
	if !ok {
		fmt.Fprintln(os.Stderr, "mldsa: unknown case", what)
		os.Exit(2)
	}
	seed := make([]byte, 32)
	for i := range seed {
		seed[i] = byte(i*7 + 1)
	}
	msg := []byte(strings.Repeat("m", 64))
	key, err := mldsa.NewPrivateKey(params, seed)
	if err != nil {
		panic(err)
	}
	switch kind {
	case "keygen":
		measure(what, func() int {
			k, _ := mldsa.NewPrivateKey(params, seed)
			return len(k.PublicKey().Bytes())
		})
	case "sign":
		measure(what, func() int {
			s, _ := key.Sign(nil, msg, nil)
			return len(s)
		})
	case "verify":
		pub := key.PublicKey()
		sig, _ := key.Sign(nil, msg, nil)
		measure(what, func() int {
			if mldsa.Verify(pub, msg, sig, nil) == nil {
				return 1
			}
			return 0
		})
	default:
		fmt.Fprintln(os.Stderr, "mldsa: unknown case", what)
		os.Exit(2)
	}
}
