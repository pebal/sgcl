// Same shape as benchmarks/encoding/asn1.cpp: Go's encoding/asn1 over the same
// bytes, a certificate's structure (RFC 5280's shape, an ECDSA key and
// signature, three names, four extensions).
//   asn1 [op=parse] [count]
// ops: parse (Unmarshal into the tagged structs, every field read),
// marshal (Marshal of the same structs), walk (Unmarshal of every element
// into a RawValue, recursively: the tree without a schema).
// Prints nanoseconds per certificate and megabytes of it per second.
package main

import (
	"crypto/sha256"
	"encoding/asn1"
	"fmt"
	"math/big"
	"os"
	"strconv"
	"time"
)

type algorithm struct {
	Algorithm asn1.ObjectIdentifier
}

type attribute struct {
	Type  asn1.ObjectIdentifier
	Value string `asn1:"utf8"`
}

// A relative distinguished name: Go's Marshal writes a slice whose type's
// name ends in SET as a SET OF
type rdnSET []attribute

type validity struct {
	NotBefore time.Time `asn1:"utc"`
	NotAfter  time.Time `asn1:"utc"`
}

type keyAlgorithm struct {
	Algorithm asn1.ObjectIdentifier
	Curve     asn1.ObjectIdentifier
}

type publicKey struct {
	Algorithm keyAlgorithm
	Key       asn1.BitString
}

type extension struct {
	ID       asn1.ObjectIdentifier
	Critical bool `asn1:"optional"`
	Value    []byte
}

type tbs struct {
	Version    int `asn1:"explicit,tag:0"`
	Serial     *big.Int
	Signature  algorithm
	Issuer     []rdnSET
	Validity   validity
	Subject    []rdnSET
	Key        publicKey
	Extensions []extension `asn1:"explicit,tag:3"`
}

type certificate struct {
	TBS       tbs
	Algorithm algorithm
	Signature asn1.BitString
}

func name(org string) []rdnSET {
	return []rdnSET{
		{{asn1.ObjectIdentifier{2, 5, 4, 6}, "PL"}},
		{{asn1.ObjectIdentifier{2, 5, 4, 10}, org}},
		{{asn1.ObjectIdentifier{2, 5, 4, 3}, "www.example.com"}},
	}
}

func filled(n int, b byte) []byte {
	out := make([]byte, n)
	for i := range out {
		out[i] = b + byte(i)
	}
	return out
}

func cert() certificate {
	serial, _ := new(big.Int).SetString("123456789012345678901234567890123456789", 10)
	return certificate{
		TBS: tbs{
			Version:   2,
			Serial:    serial,
			Signature: algorithm{asn1.ObjectIdentifier{1, 2, 840, 10045, 4, 3, 2}},
			Issuer:    name("Example Issuing CA"),
			Validity:  validity{time.Unix(1700000000, 0).UTC(), time.Unix(1731536000, 0).UTC()},
			Subject:   name("Example Subject Ltd"),
			Key: publicKey{keyAlgorithm{asn1.ObjectIdentifier{1, 2, 840, 10045, 2, 1}, asn1.ObjectIdentifier{1, 2, 840, 10045, 3, 1, 7}},
				asn1.BitString{Bytes: filled(65, 4), BitLength: 520}},
			Extensions: []extension{
				{asn1.ObjectIdentifier{2, 5, 29, 15}, true, filled(4, 3)},
				{asn1.ObjectIdentifier{2, 5, 29, 19}, true, filled(2, 0x30)},
				{asn1.ObjectIdentifier{2, 5, 29, 17}, false, filled(40, 0x82)},
				{asn1.ObjectIdentifier{2, 5, 29, 14}, false, filled(22, 4)},
			},
		},
		Algorithm: algorithm{asn1.ObjectIdentifier{1, 2, 840, 10045, 4, 3, 2}},
		Signature: asn1.BitString{Bytes: filled(72, 0x30), BitLength: 576},
	}
}

var sink int

func walk(der []byte) int {
	n := 0
	for len(der) > 0 {
		var rv asn1.RawValue
		rest, err := asn1.Unmarshal(der, &rv)
		if err != nil {
			panic(err)
		}
		n++
		if rv.IsCompound {
			n += walk(rv.Bytes)
		}
		der = rest
	}
	return n
}

func main() {
	op := "parse"
	if len(os.Args) > 1 {
		op = os.Args[1]
	}
	c := cert()
	der, err := asn1.Marshal(c)
	if err != nil {
		panic(err)
	}
	count := 2_000_000_000 / len(der) / 20
	if len(os.Args) > 2 {
		count, _ = strconv.Atoi(os.Args[2])
	}
	var f func()
	switch op {
	case "parse":
		f = func() {
			var out certificate
			if _, err := asn1.Unmarshal(der, &out); err != nil {
				panic(err)
			}
			sink += len(out.TBS.Extensions)
		}
	case "marshal":
		f = func() {
			b, err := asn1.Marshal(c)
			if err != nil {
				panic(err)
			}
			sink += len(b)
		}
	case "walk":
		f = func() {
			sink += walk(der)
		}
	default:
		fmt.Fprintln(os.Stderr, "asn1: no op called", op)
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
	sum := sha256.Sum256(der)
	fmt.Printf("go op=%s bytes=%d sha256=%x count=%d ns/op=%.0f MB/s=%.0f\n", op, len(der), sum[:8], count, ns, float64(len(der))/ns*1e3)
}
