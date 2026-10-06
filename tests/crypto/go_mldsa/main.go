// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle of crypto::mldsa (tests/crypto/mldsa.cpp): Go's crypto/mldsa.
//
//	<44|65|87> <seed hex> <message hex or -> <context hex or -> <signature hex>
//	    prints the public key of the seed, its deterministic signature of the
//	    message under the context, and whether the signature given verifies
package main

import (
	"crypto/mldsa"
	"encoding/hex"
	"fmt"
	"os"
)

func unhex(s string) []byte {
	if s == "-" {
		return nil
	}
	b, err := hex.DecodeString(s)
	if err != nil {
		panic(err)
	}
	return b
}

func main() {
	a := os.Args[1:]
	params := map[string]mldsa.Parameters{"44": mldsa.MLDSA44(), "65": mldsa.MLDSA65(), "87": mldsa.MLDSA87()}[a[0]]
	key, err := mldsa.NewPrivateKey(params, unhex(a[1]))
	if err != nil {
		panic(err)
	}
	msg, ctx := unhex(a[2]), string(unhex(a[3]))
	sig, err := key.SignDeterministic(msg, &mldsa.Options{Context: ctx})
	if err != nil {
		panic(err)
	}
	ok := mldsa.Verify(key.PublicKey(), msg, unhex(a[4]), &mldsa.Options{Context: ctx}) == nil
	fmt.Println(hex.EncodeToString(key.PublicKey().Bytes()), hex.EncodeToString(sig), ok)
}
