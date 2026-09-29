// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of the ML-KEM oracle (FIPS 203): Go's crypto/mlkem and
// crypto/mlkem/mlkemtest (the standard library alone, nothing fetched)
// asked the questions tools/mlkem_oracle.cpp asks sgcl and OpenSSL, and
// the answers folded into one hash that the C++ tool prints too. Go has
// ML-KEM-768 and ML-KEM-1024 (no 512). From the root of the tree:
//
//     go run tools/mlkem_oracle.go 768 100000
//
// The cases, the same on both sides: SHAKE128 of "sgcl mlkem oracle
// ML-KEM-768" (or -1024) read as a stream, 99 bytes a case — the seed d‖z
// (64), the message m (32), a position (two bytes, little-endian, taken
// modulo the ciphertext's length) and a bit (one byte, bit b mod 8). For
// each: the decapsulation key of the seed, its encapsulation key ek, the
// encapsulation of m to it (c, K; derandomized, mlkemtest), and the
// decapsulation of c with the one bit flipped (K', the implicit rejection).
// The hash: SHAKE128 over ek‖c‖K‖K' of every case in order, 32 bytes out
// (the accumulated form of C2SP's CCTV vectors, made here, not fetched).
package main

import (
	"crypto/mlkem"
	"crypto/mlkem/mlkemtest"
	"crypto/sha3"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"os"
	"strconv"
	"time"
)

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintln(os.Stderr, "usage: mlkem_oracle <768|1024> <cases>")
		os.Exit(2)
	}
	set := os.Args[1]
	n, err := strconv.Atoi(os.Args[2])
	if err != nil {
		panic(err)
	}
	stream := sha3.NewSHAKE128()
	stream.Write([]byte("sgcl mlkem oracle ML-KEM-" + set))
	acc := sha3.NewSHAKE128()
	start := time.Now()
	for i := 0; i < n; i++ {
		seed := make([]byte, 64)
		m := make([]byte, 32)
		place := make([]byte, 3)
		stream.Read(seed)
		stream.Read(m)
		stream.Read(place)
		var ek, c, k, rejected []byte
		switch set {
		case "768":
			dk, err := mlkem.NewDecapsulationKey768(seed)
			if err != nil {
				panic(err)
			}
			pub := dk.EncapsulationKey()
			ek = pub.Bytes()
			k, c, err = mlkemtest.Encapsulate768(pub, m)
			if err != nil {
				panic(err)
			}
			bad := append([]byte(nil), c...)
			bad[int(binary.LittleEndian.Uint16(place))%len(bad)] ^= 1 << (place[2] % 8)
			rejected, err = dk.Decapsulate(bad)
			if err != nil {
				panic(err)
			}
		case "1024":
			dk, err := mlkem.NewDecapsulationKey1024(seed)
			if err != nil {
				panic(err)
			}
			pub := dk.EncapsulationKey()
			ek = pub.Bytes()
			k, c, err = mlkemtest.Encapsulate1024(pub, m)
			if err != nil {
				panic(err)
			}
			bad := append([]byte(nil), c...)
			bad[int(binary.LittleEndian.Uint16(place))%len(bad)] ^= 1 << (place[2] % 8)
			rejected, err = dk.Decapsulate(bad)
			if err != nil {
				panic(err)
			}
		default:
			fmt.Fprintln(os.Stderr, "Go has ML-KEM-768 and ML-KEM-1024")
			os.Exit(2)
		}
		acc.Write(ek)
		acc.Write(c)
		acc.Write(k)
		acc.Write(rejected)
	}
	sum := make([]byte, 32)
	acc.Read(sum)
	fmt.Printf("go ML-KEM-%s cases %d accumulated %s (%.2f s)\n", set, n, hex.EncodeToString(sum), time.Since(start).Seconds())
}
