// Same shape as benchmarks/math/math.cpp: Go's math/big and
// math/rand/v2 (ChaCha8) under the names bench_math gives its cases.
//   math <op> [n]
//
// big.Int (n is the length of the operands in 64-bit words, or in decimal
// digits for the conversions):
//   add     z.Add(a, b)
//   mul     z.Mul(a, b)
//   sqr     z.Mul(a, a)
//   div     z.Quo(a, b), a of 2n words and b of n
//   tostr   a.Text(10) of a number of n digits
//   parse   z.SetString of n digits
//   sum     s.Add(s, x) a thousand times, x of n words
//   fact    f.Mul(f, i) for i up to n, one step reported
//   small   a*b + c over values within int64 (big.NewInt for each)
//   pow     z.Exp(3, e, nil) with e such that the result has n words
//   modpow  z.Exp(a, e, m), all three of n bits, m odd
//   gcd     z.GCD(nil, nil, a, b), both n words
//   modinv  z.ModInverse(a, m), both n words, m odd
//   sqrt    z.Sqrt(a) of n words
//   prime   p.ProbablyPrime(20) of a prime of n bits
//   pi      n digits of π by the same Chudnovsky splitting as the C++ case
//   factorial z.MulRange(1, n)
//   binomial z.Binomial(n, n/2)
//   fib     the n-th Fibonacci number by the same doubling as the C++ case
//   harmonic the big.Rat sum 1/1 + 1/2 + … + 1/n, reported per term
//
// rand/v2 over ChaCha8 (n ignored):
//   uint64  r.Uint64()
//   intn    r.IntN(1000)
//   double  r.Float64()
//   normal  r.NormFloat64()
//   exp     r.ExpFloat64()
//   shuffle r.Shuffle of a million ints, reported per element
//   make    rand.New(rand.NewChaCha8(seed from crypto/rand)) and one draw
//
// Each op is repeated, doubling the count, until a run takes half a
// second; prints nanoseconds per operation.
package main

import (
	crand "crypto/rand"
	"fmt"
	"math/big"
	"math/bits"
	"math/rand/v2"
	"os"
	"strconv"
	"time"
)

var sink uint64

func measure(f func(count int)) float64 {
	count := 1
	for {
		t0 := time.Now()
		f(count)
		s := time.Since(t0).Seconds()
		if s > 0.5 || count > 1<<40 {
			return s * 1e9 / float64(count)
		}
		count *= 2
	}
}

func randomBig(r *rand.Rand, words int) *big.Int {
	v := big.NewInt(1)
	w := new(big.Int)
	for i := 0; i < words; i++ {
		v.Lsh(v, 64)
		w.SetUint64(r.Uint64())
		v.Or(v, w)
	}
	return v.Rsh(v, 1)
}

type split struct{ p, q, t *big.Int }

// Chudnovsky's series by binary splitting over [a, b), as the C++ case
func chudnovsky(a, b int64, c324 *big.Int) split {
	if b-a == 1 {
		p, q := big.NewInt(1), big.NewInt(1)
		if a != 0 {
			p = big.NewInt(6*a - 5)
			p.Mul(p, big.NewInt(2*a-1))
			p.Mul(p, big.NewInt(6*a-1))
			q = big.NewInt(a)
			q.Mul(q, big.NewInt(a))
			q.Mul(q, big.NewInt(a))
			q.Mul(q, c324)
		}
		t := new(big.Int).Mul(p, big.NewInt(13591409+545140134*a))
		if a%2 == 1 {
			t.Neg(t)
		}
		return split{p, q, t}
	}
	m := (a + b) / 2
	l, r := chudnovsky(a, m, c324), chudnovsky(m, b, c324)
	t := new(big.Int).Mul(l.t, r.q)
	t.Add(t, new(big.Int).Mul(l.p, r.t))
	return split{new(big.Int).Mul(l.p, r.p), new(big.Int).Mul(l.q, r.q), t}
}

func piDigits(digits int) string {
	c324 := new(big.Int).Exp(big.NewInt(640320), big.NewInt(3), nil)
	c324.Quo(c324, big.NewInt(24))
	s := chudnovsky(0, int64(digits/14+2), c324)
	unity := new(big.Int).Exp(big.NewInt(10), big.NewInt(int64(digits)), nil)
	root := new(big.Int).Mul(unity, unity)
	root.Mul(root, big.NewInt(10005))
	root.Sqrt(root)
	pi := new(big.Int).Mul(root, big.NewInt(426880))
	pi.Mul(pi, s.q)
	pi.Quo(pi, s.t)
	return pi.Text(10)
}

// F(n) by doubling, as the C++ case
func fibonacci(n int) *big.Int {
	a, b := big.NewInt(0), big.NewInt(1)
	for i := 63 - bits.LeadingZeros64(uint64(n)); i >= 0; i-- {
		c := new(big.Int).Lsh(b, 1)
		c.Sub(c, a)
		c.Mul(c, a)
		d := new(big.Int).Mul(a, a)
		d.Add(d, new(big.Int).Mul(b, b))
		if (n>>i)&1 == 1 {
			a, b = d, c.Add(c, d)
		} else {
			a, b = c, d
		}
	}
	return a
}

func bigOp(op string, n int) float64 {
	r := rand.New(rand.NewPCG(1, 2))
	switch op {
	case "add", "mul":
		a, b := randomBig(r, n), randomBig(r, n)
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				c := new(big.Int)
				if op == "add" {
					c.Add(a, b)
				} else {
					c.Mul(a, b)
				}
				sink += uint64(c.BitLen())
			}
		})
	case "sqr":
		a := randomBig(r, n)
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				c := new(big.Int).Mul(a, a)
				sink += uint64(c.BitLen())
			}
		})
	case "div":
		a, b := randomBig(r, 2*n), randomBig(r, n)
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				c := new(big.Int).Quo(a, b)
				sink += uint64(c.BitLen())
			}
		})
	case "tostr", "parse":
		digits := make([]byte, n)
		for i := range digits {
			digits[i] = byte('0' + r.IntN(10))
		}
		digits[0] = '7'
		text := string(digits)
		a, _ := new(big.Int).SetString(text, 10)
		if op == "tostr" {
			return measure(func(count int) {
				for i := 0; i < count; i++ {
					sink += uint64(len(a.Text(10)))
				}
			})
		}
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				z, _ := new(big.Int).SetString(text, 10)
				sink += uint64(z.BitLen())
			}
		})
	case "sum":
		x := randomBig(r, n)
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				s := new(big.Int)
				for k := 0; k < 1000; k++ {
					s.Add(s, x)
				}
				sink += uint64(s.BitLen())
			}
		}) / 1000
	case "fact":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				f := big.NewInt(1)
				k := new(big.Int)
				for j := 2; j <= n; j++ {
					f.Mul(f, k.SetInt64(int64(j)))
				}
				sink += uint64(f.BitLen())
			}
		}) / float64(n)
	case "pow":
		e := big.NewInt(int64(float64(n) * 64 / 1.584962500721156))
		three := big.NewInt(3)
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(new(big.Int).Exp(three, e, nil).BitLen())
			}
		})
	case "modpow":
		words := (n + 63) / 64
		m := randomBig(r, words)
		m.Rsh(m, uint(words*64-n)).SetBit(m, 0, 1)
		a := randomBig(r, words)
		a.Mod(a, m)
		e := randomBig(r, words)
		e.Rsh(e, uint(words*64-n))
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(new(big.Int).Exp(a, e, m).BitLen())
			}
		})
	case "gcd", "modinv":
		a := randomBig(r, n)
		b := randomBig(r, n)
		b.SetBit(b, 0, 1)
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				z := new(big.Int)
				if op == "gcd" {
					z.GCD(nil, nil, a, b)
				} else if z.ModInverse(a, b) == nil {
					z.SetInt64(0)
				}
				sink += uint64(z.BitLen())
			}
		})
	case "sqrt":
		a := randomBig(r, n)
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(new(big.Int).Sqrt(a).BitLen())
			}
		})
	case "prime":
		words := (n + 63) / 64
		p := randomBig(r, words)
		p.Rsh(p, uint(words*64-n)).SetBit(p, 0, 1)
		for !p.ProbablyPrime(20) {
			p.Add(p, big.NewInt(2))
		}
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				if p.ProbablyPrime(20) {
					sink++
				}
			}
		})
	case "pi":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(len(piDigits(n)))
			}
		})
	case "factorial":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(new(big.Int).MulRange(1, int64(n)).BitLen())
			}
		})
	case "binomial":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(new(big.Int).Binomial(int64(n), int64(n/2)).BitLen())
			}
		})
	case "fib":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(fibonacci(n).BitLen())
			}
		})
	case "harmonic":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sum := new(big.Rat)
				for k := 1; k <= n; k++ {
					sum.Add(sum, big.NewRat(1, int64(k)))
				}
				sink += uint64(sum.Denom().BitLen())
			}
		}) / float64(n)
	case "small":
		values := make([]int64, 1024)
		for i := range values {
			values[i] = int64(r.IntN(2000000)) - 1000000
		}
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				a := big.NewInt(values[i&1023])
				b := big.NewInt(values[(i+1)&1023])
				c := new(big.Int).Mul(a, b)
				c.Add(c, big.NewInt(values[(i+2)&1023]))
				sink += uint64(c.Int64())
			}
		})
	}
	return -1
}

func randomOp(op string) float64 {
	var seed [32]byte
	seed[0] = 1
	r := rand.New(rand.NewChaCha8(seed))
	switch op {
	case "uint64":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += r.Uint64()
			}
		})
	case "intn":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(r.IntN(1000))
			}
		})
	case "double":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(r.Float64() * 1e6)
			}
		})
	case "normal":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(int64(r.NormFloat64() * 1e6))
			}
		})
	case "exp":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				sink += uint64(r.ExpFloat64() * 1e6)
			}
		})
	case "shuffle":
		v := make([]int, 1000000)
		for i := range v {
			v[i] = i
		}
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				r.Shuffle(len(v), func(i, j int) { v[i], v[j] = v[j], v[i] })
			}
		}) / float64(len(v))
	case "make":
		return measure(func(count int) {
			for i := 0; i < count; i++ {
				var s [32]byte
				crand.Read(s[:])
				g := rand.New(rand.NewChaCha8(s))
				sink += g.Uint64()
			}
		})
	}
	return -1
}

func main() {
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: math <op> [n]")
		os.Exit(1)
	}
	op := os.Args[1]
	n := 0
	if len(os.Args) > 2 {
		n, _ = strconv.Atoi(os.Args[2])
	}
	ns := randomOp(op)
	if ns < 0 {
		if n == 0 {
			switch op {
			case "tostr", "parse", "fact", "pi", "factorial", "binomial", "harmonic":
				n = 1000
			case "modpow", "prime":
				n = 1024
			case "fib":
				n = 100000
			default:
				n = 10
			}
		}
		ns = bigOp(op, n)
	}
	if ns < 0 {
		fmt.Fprintf(os.Stderr, "math: no op called %s\n", op)
		os.Exit(1)
	}
	fmt.Printf("go op=%s n=%d ns/op=%.2f\n", op, n, ns)
}
