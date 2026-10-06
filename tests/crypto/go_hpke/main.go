// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of crypto::hpke's tests: Go's crypto/hpke (base mode, single
// shot), one command a run, everything in hex:
//
//	seal <kem> <kdf> <aead> <pkR> <info> <plaintext>    <enc || ciphertext>
//	open <kem> <kdf> <aead> <skR> <info> <sealed>       ok <plaintext> | fail <why>
//	keygen <kem>                                       <skR> <pkR>
package main

import (
	"crypto/hpke"
	"encoding/hex"
	"fmt"
	"os"
	"strconv"
)

func id(s string) uint16 {
	v, err := strconv.ParseUint(s, 0, 16)
	if err != nil {
		panic(err)
	}
	return uint16(v)
}

func unhex(s string) []byte {
	b, err := hex.DecodeString(s)
	if err != nil {
		panic(err)
	}
	return b
}

func main() {
	a := os.Args[1:]
	kem, err := hpke.NewKEM(id(a[1]))
	if err != nil {
		panic(err)
	}
	switch a[0] {
	case "keygen":
		k, _ := kem.GenerateKey()
		b, _ := k.Bytes()
		fmt.Println(hex.EncodeToString(b), hex.EncodeToString(k.PublicKey().Bytes()))
		return
	}
	kdf, err := hpke.NewKDF(id(a[2]))
	if err != nil {
		panic(err)
	}
	aead, err := hpke.NewAEAD(id(a[3]))
	if err != nil {
		panic(err)
	}
	switch a[0] {
	case "seal":
		pk, err := kem.NewPublicKey(unhex(a[4]))
		if err != nil {
			panic(err)
		}
		out, err := hpke.Seal(pk, kdf, aead, unhex(a[5]), unhex(a[6]))
		if err != nil {
			panic(err)
		}
		fmt.Println(hex.EncodeToString(out))
	case "open":
		sk, err := kem.NewPrivateKey(unhex(a[4]))
		if err != nil {
			panic(err)
		}
		pt, err := hpke.Open(sk, kdf, aead, unhex(a[5]), unhex(a[6]))
		if err != nil {
			fmt.Println("fail " + err.Error())
			return
		}
		fmt.Println("ok " + hex.EncodeToString(pt))
	}
}
