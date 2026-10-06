// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of benchmarks/crypto/hpke.cpp: crypto/hpke, the same work a
// case. One line with ns/op.
//
//	hpke <case>
package main

import (
	"bytes"
	"crypto/ecdh"
	"crypto/hpke"
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
	fmt.Printf("hpke %s ns/op=%.1f wall=%.2fs\n", what, wall*1e9/float64(calls), wall)
}

func main() {
	what := os.Args[1]
	data := bytes.Repeat([]byte("x"), 1024)
	info := []byte("info")
	kem, kdf, aead := hpke.DHKEM(ecdh.X25519()), hpke.HKDFSHA256(), hpke.AES128GCM()
	switch {
	case strings.Contains(what, "p256"):
		kem = hpke.DHKEM(ecdh.P256())
	case strings.Contains(what, "p384"):
		kem, kdf, aead = hpke.DHKEM(ecdh.P384()), hpke.HKDFSHA384(), hpke.AES256GCM()
	case strings.Contains(what, "p521"):
		kem, kdf, aead = hpke.DHKEM(ecdh.P521()), hpke.HKDFSHA512(), hpke.AES256GCM()
	}
	key, _ := kem.GenerateKey()
	pub := key.PublicKey()
	switch {
	case strings.HasPrefix(what, "seal_"):
		measure(what, func() int {
			out, err := hpke.Seal(pub, kdf, aead, info, data)
			if err != nil {
				panic(err)
			}
			return len(out)
		})
	case strings.HasPrefix(what, "open_"):
		sealed, _ := hpke.Seal(pub, kdf, aead, info, data)
		measure(what, func() int {
			out, err := hpke.Open(key, kdf, aead, info, sealed)
			if err != nil {
				panic(err)
			}
			return len(out)
		})
	case what == "context_seal":
		_, s, _ := hpke.NewSender(pub, kdf, aead, info)
		measure(what, func() int {
			out, _ := s.Seal(nil, data)
			return len(out)
		})
	case what == "export":
		_, s, _ := hpke.NewSender(pub, kdf, aead, info)
		measure(what, func() int {
			out, _ := s.Export("context", 32)
			return len(out)
		})
	default:
		fmt.Fprintln(os.Stderr, "hpke: unknown case", what)
		os.Exit(2)
	}
}
