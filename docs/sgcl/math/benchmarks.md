[sgcl](../README.md) › [math](README.md)

# Benchmarks: math

The setup, the machine, the environments and how the timers are read are described with
[the benchmarks of the engine](../../garbage_collector/benchmarks.md); the numbers here were taken the same way, on a
quiet machine. `bench_math` (`benchmarks/math/math.cpp`) and its Go counterpart `benchmarks/go/math` hold the same
cases under the same names: each operation is repeated, the count doubling, until a run takes half a second, and
the time is in nanoseconds per operation. `benchmarks/compare.sh` runs them side by side (`CASES=math`).

## big_integer

The cases, against Go's `math/big`, with `n` the length of the operands in limbs of 64 bits, or in decimal digits
for the conversions: `add`, `mul` and `sqr` of `n` limbs, `div` of `2n` limbs by `n`, `tostr` and `parse` of `n`
decimal digits, `sum` (`sum += x` a thousand times, `x` of `n` limbs, written in place), `fact` (`f *= i` up to
`n`, one step reported), `small` (`a * b + c` over values that stay within `int64_t`), `pow`, `modpow` (all three
of `n` bits, an odd modulus), `gcd`, `modinv`, `sqrt`, `prime` (a prime of `n` bits, 20 rounds), `pi` (`n` digits
of π by Chudnovsky's series), `factorial`, `binomial` and `fib`.

The loops over limbs are C++ without assembler: the sums eight and sixteen limbs a step with the carry kept in
the processor's flags through the step (the compiler's add-with-carry builtins), the products of a number by one
limb first and their sums after, in two chains of carries. That puts the multiplication, the division, the
conversions to text and `mod_pow` ahead of Go's `math/big` and its assembler at every length measured, and `gcd`
behind it by a tenth or two. Go's `math/big` multiplies by Karatsuba's method alone; SGCL goes on to Toom-3 above
a hundred limbs and more, and divides long numbers by Burnikel and Ziegler's recursion
([operator_arith](big_integer/operator_arith.md)).

A result nobody else has is written in place by `+=`, `-=` and `*=` by a small value. Measured before that was
done and after: `sum += x` over numbers of a hundred limbs took 109 ns a step when every sum allocated, and takes
35.2 now (4 October 2026, `-O3`, `env -i`), where Go's `z.Add(z, x)` takes 35; `f *= k` costs less than Go's.

The thresholds between the algorithms of the multiplication, the square, the division and the conversions are
set by measurement: `bench_math sgcl cross <n> <algorithm>` times the operation at `n` limbs with the threshold
just above `n`, so that the top level takes the road below, and at `n`, so that it takes the road above, and
prints the ratio.

## rational

The case `harmonic`, the sum `1/1 + 1/2 + … + 1/n` reported per term, against Go's `big.Rat`.

## random

The cases, against Go's `math/rand/v2` with its `ChaCha8` and against the standard library's `mt19937_64` with
its distributions: `uint64` (`next_uint64`, `mt19937_64()`), `intn` (`next_int(1000)`,
`uniform_int_distribution`), `double` (`next_double`, `generate_canonical`), `normal` (`next_normal`,
`normal_distribution`), `exp` (`next_exponential`, `exponential_distribution`), `shuffle` (a million `int`s,
reported per element, `std::shuffle`) and `make` (a default `math::random` made and one draw taken, against a
`random_device` and an `mt19937_64` seeded from it).
