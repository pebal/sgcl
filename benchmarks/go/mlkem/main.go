// ML-KEM in Go: crypto/mlkem, one case a run (benchmarks/crypto/mlkem.cpp
// has the SGCL and OpenSSL sides, the same cases and the same seed). Go
// has ML-KEM-768 and ML-KEM-1024, not 512. Prints one line: ns per
// operation and operations per second, after a quarter of a second thrown
// away and about two seconds timed.
//
//	mlkem <keygen|import|encaps|decaps><768|1024>
package main

import (
	"crypto/mlkem"
	"fmt"
	"os"
	"time"
)

var sink uint64

func runFor(f func() uint64, seconds float64) (uint64, float64) {
	var calls, acc uint64
	t0 := time.Now()
	wall := 0.0
	for wall < seconds {
		for i := 0; i < 16; i++ {
			acc += f()
		}
		calls += 16
		wall = time.Since(t0).Seconds()
	}
	sink = acc
	return calls, wall
}

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "usage: mlkem <keygen|import|encaps|decaps><768|1024>")
		os.Exit(2)
	}
	what := os.Args[1]
	seed := make([]byte, 64)
	for i := range seed {
		seed[i] = byte(i*7 + 1)
	}
	var f func() uint64
	switch what {
	case "keygen768":
		f = func() uint64 {
			dk, _ := mlkem.NewDecapsulationKey768(seed)
			return uint64(dk.EncapsulationKey().Bytes()[0])
		}
	case "import768":
		dk, _ := mlkem.NewDecapsulationKey768(seed)
		b := dk.EncapsulationKey().Bytes()
		f = func() uint64 {
			ek, err := mlkem.NewEncapsulationKey768(b)
			if err != nil {
				return 0
			}
			return uint64(len(ek.Bytes()))
		}
	case "import1024":
		dk, _ := mlkem.NewDecapsulationKey1024(seed)
		b := dk.EncapsulationKey().Bytes()
		f = func() uint64 {
			ek, err := mlkem.NewEncapsulationKey1024(b)
			if err != nil {
				return 0
			}
			return uint64(len(ek.Bytes()))
		}
	case "encaps768", "decaps768":
		dk, _ := mlkem.NewDecapsulationKey768(seed)
		ek := dk.EncapsulationKey()
		_, c := ek.Encapsulate()
		if what == "encaps768" {
			f = func() uint64 {
				_, c := ek.Encapsulate()
				return uint64(c[0])
			}
		} else {
			f = func() uint64 {
				k, _ := dk.Decapsulate(c)
				return uint64(k[0])
			}
		}
	case "keygen1024":
		f = func() uint64 {
			dk, _ := mlkem.NewDecapsulationKey1024(seed)
			return uint64(dk.EncapsulationKey().Bytes()[0])
		}
	case "encaps1024", "decaps1024":
		dk, _ := mlkem.NewDecapsulationKey1024(seed)
		ek := dk.EncapsulationKey()
		_, c := ek.Encapsulate()
		if what == "encaps1024" {
			f = func() uint64 {
				_, c := ek.Encapsulate()
				return uint64(c[0])
			}
		} else {
			f = func() uint64 {
				k, _ := dk.Decapsulate(c)
				return uint64(k[0])
			}
		}
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
	runFor(f, 0.25)
	calls, wall := runFor(f, 2.0)
	ns := wall * 1e9 / float64(calls)
	fmt.Printf("mlkem %s go ns/op=%.0f op/s=%.0f wall=%.2fs\n", what, ns, 1e9/ns, wall)
}
