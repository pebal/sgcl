[sgcl](../README.md) › math

# sgcl::math

```cpp
#include "sgcl/math.h"   // namespace sgcl::math
```

Numbers that the language does not have, and randomness that the standard library gives in a shape nobody
wants: whole numbers of any size and fractions of them, and a generator with the questions a program asks of one.
What Go has in `math/big` and `math/rand/v2`, as values with the manners of an `int`: `a * 2 + 1`, `a < 10`,
`third * 3 == 1`, `r.next_int(1, 7)`.

The idea under the numbers is the one under [string](../core/string/README.md): an immutable object on the managed heap,
shared by copying a word, and nothing on the heap at all for a value that fits in `int64_t`. A copy is a copy, an
operator gives a new value, and the arithmetic never overflows and never rounds.

The module depends on [core](../core/README.md) and on [txt](../txt/README.md) (for
[txt::format](../txt/format.md)) and on nothing else, never on `concurrent`, `async` or [io](../io/README.md), so
that `encoding` (ASN.1) and `crypto` (the public keys of X.509) are built on it. The index of the whole interface
is [the modules](../README.md).

## The rules

**A number is a value.** `big_integer` behaves as `int` does: operators, conversions from any whole number,
comparisons, a copy that is a copy. A copy never changes with the value it was taken from: `a += b` means
`a = a + b`, and the old value is still whole for whoever else holds it; only a value nobody else has, the result
of an operation not copied since, is written in place by `+=`, `-=` and `*=` by a small value, which is where Go
reuses its receiver. A `big_integer` and a `rational` hold a `tracked_ptr`, so they live where one may: on a
stack, in a managed object, in a container of the library ([The rules](../core/README.md#the-rules) of core); a
`random` holds no pointer and lives anywhere.

**An error of the program throws, an error of data does not.** A division by zero, a negative shift, a NaN made
into a number, a base outside 2 to 36, a bound of zero for `next_int` are mistakes of the code that asked, and
throw (`domain_error`, `invalid_argument`, `length_error`, `out_of_range`) where Go panics and `int` would be
undefined. Text that is not a number is a fact about the text and comes back as an
[expected](../core/expected/README.md) whose [parse_error](parse_error/README.md) says where the reading stopped (`offset()`)
and why (`message()`). A text the program itself writes is constructed, `math::big_integer p("1157…")`, and throws
when it is wrong; a text from outside is parsed.

**One road to each thing.** `/` and `%` are C++'s; the one other division modular arithmetic wants is `mod`.
There is one generator and no function of a hidden global one: make a `random` and ask it.

**Not for secrets.** Nothing in `big_integer` takes the same time whatever the value (the length of a number, its
fast paths and the corrections of the division all show), and a `random` is keyed from a number or copied at
will. A private key, a signature and the choosing of a prime for a key belong to `crypto`, which has types of its
own for them; `big_integer` may carry what is public (a modulus, a serial number, an INTEGER of DER). Neither Go's
`math/big` nor its `math/rand` is for secrets either.

**Its own implementation, from the specification.** The algorithms are written from Knuth (TAOCP vol. 2: 4.3.1
for the division, 4.3.3 for Karatsuba and Toom, 4.5.2 for Lehmer's gcd), Möller and Granlund (division by an
invariant integer), Bodrato and Zanoni (the interpolation of Toom-3), Burnikel and Ziegler and Brent and
Zimmermann's Modern Computer Arithmetic (the recursive division and the conversions), Montgomery (multiplication
without division), Baillie and Wagstaff (the Lucas test), Marsaglia and Tsang (the ziggurat method) and the C2SP
specification of ChaCha8Rand. Python's `int`, Go's `math/big` and Go's `math/rand/v2` are oracles in the tests
and nowhere else.

The tests are `tests_math`, one program (`ctest -R math`, or the target alone). The oracle for `big_integer` is
Python's `int`: `tools/math_vectors.py` writes `tests/math/vectors.h`, every operation over every pair of named
operands (the edges of `int64_t` and one either side, `2^64k` and one either side, limbs of all ones and of zeros,
all four combinations of signs) and over random operands of 1 to 40 limbs; divisions that take the rare add-back
step of Knuth's algorithm, each confirmed to take it by a model of the algorithm in the generator; the ties of
`to_double`; text in every base. The fast algorithms have oracles of two kinds: `tools/math_vectors.py fast`
writes `tests/math/fast_vectors.h`, the answers of Python for products, squares and divisions of up to a hundred
thousand limbs and for conversions of up to a million digits, whose operands both sides make from the same seeds;
and the schoolbook algorithms themselves, which every fast road must agree with when the thresholds are lowered
to a few limbs, so that each is taken on small numbers, and at the thresholds as shipped. The number theory is
checked against Python's `math` (`tools/math_vectors.py number`: `pow`, `isqrt`, `gcd`, `lcm`, `pow(a, e, m)`,
`pow(a, -1, m)`, `factorial`, `comb`), and `is_probable_prime` against Go's `ProbablyPrime`
(`tools/math_primes_oracle.go`), against a sieve below 2^16, and against the pseudoprimes of the literature:
Carmichael numbers, strong pseudoprimes to base 2 and to every prime base up to 41, the squares of the Wieferich
primes, strong Lucas pseudoprimes, each half of Baillie–PSW shown to pass the other's liars and to fail its own.
`rational` is checked against Python's `fractions.Fraction` and `float` (`tools/math_vectors.py rational`): the
four operations and the order over pairs of named and random fractions, `floor` and `ceil`, the double nearest a
fraction at the ties of the normal and the subnormal range and at the edges, the fraction a double is, parsing;
`to_decimal` against the definition of rounding half away from zero. The laws of arithmetic are checked over
thousands of random operands besides, and `big_integer` is exercised under a collector running in a loop, with a
count of the pages left when the numbers are gone. For `random` the first test is the C2SP specification's own
sample output; `tools/math_oracle.go` writes `tests/math/random_vectors.h` from Go's `math/rand/v2`; the
distributions are held to their shapes by chi-square and Kolmogorov–Smirnov tests on fixed seeds, and to the
values `random(42)` gave when they were written down.

## Classes

| Class | Header | Description |
|---|---|---|
| [big_integer](big_integer/README.md) | `big_integer.h` | a whole number of any size with the manners of an `int`: the operators, `/` cut towards zero and `mod` never negative, the bits in two's complement, text in the bases 2 to 36, bytes, the literal `_big`, `txt::format`; `pow`, `sqrt`, `gcd`, `lcm`, `mod_pow`, `mod_inverse`, `is_probable_prime`, `factorial`, `binomial`; sixteen bytes, a value within `int64_t` allocating nothing |
| [parse_error](parse_error/README.md) | `big_integer.h` | why a text did not read as a number: the byte where the reading stopped and a sentence |
| [random](random/README.md) | `random.h` | a generator, ChaCha8Rand with the stream of Go's `ChaCha8` from the same key, and what is asked of it: `next_int`, `next_double`, `next_normal`, `next_exponential`, `next_bytes`, `shuffle`, `pick`, `permutation`; a uniform random bit generator of the standard's for everything else |
| [rational](rational/README.md) | `rational.h` | a fraction of two `big_integer`s, always in lowest terms: exact arithmetic with the operators, `pow` with a negative exponent, `floor`, `ceil`; `parse` of `"3/4"` and of decimals, `rational(double)` exactly, `to_double` rounded once, `to_decimal` half away from zero |

## See also

- [Benchmarks](benchmarks.md): `big_integer` against Go's `math/big`, `random` against Go's `math/rand/v2` and
  `mt19937_64`
- [string](../core/string/README.md): the immutable object shared by a word that `big_integer` follows
- [txt::format](../txt/format.md): the formatting of the numbers
- [The modules](../README.md)
