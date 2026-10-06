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

## decimal

The cases, with `n` the digits of the operands, two of them after the point: `dadd`, `daddmix` (the scales 2 and
4, one brought to the other), `dmul`, `dmoney` (a price times a rate rescaled to cents), `ddiv` (the quotient to
`n` digits), `dsqrt` (the root of 2 to `n` digits), `dparse`, `dformat` and `dsum` (a thousand values summed,
per term). The reference is Python's `decimal`, whose arithmetic is libmpdec's, by `benchmarks/math/decimal_python.py`
under the same names: its numbers carry the interpreter's own cost of a statement, tens of nanoseconds, which
weighs on the short cases. Go has no decimal type: its side holds the same values in `big.Rat`, for orientation
only (an exact `Quo` for `ddiv`, `FloatString(2)` for `dformat` and the rounding of `dmoney`, no `dsqrt`).

## algebra

The cases, against the same arithmetic written plainly (`bench_math plain`), which the compiler vectorizes as it
likes: `mat4mul` (a chain of products of 4×4 matrices, each waiting on the one before), `mat4apply` (`apply` of a
matrix to `n` vectors, per vector) and `affine` (two transforms composed and a point through them). The product and
`apply` are written on NEON (SSE2 on x86-64) and kept because they measured faster than the plain form.

## big_float

The cases, against Go's `math/big.Float`, with `n` the precision in bits and operands of `n` bits: `fadd`, `fmul`,
`fdiv`, `fsqrt` (Go's receiver at the precision, made for each operation as a result is here), `ftext` (the shortest
decimal that reads back, Go's `Text('g', -1)`) and `fparse` (that text read back at the precision, Go's
`ParseFloat`).

## curves

The cases, with no reference (Go has no Bézier curves nor easing functions): `ease` (`easing::ease`, CSS's
cubic-bezier solved for a progress that changes each time), `bounce` (`ease_in_out_bounce`), `flatten` (a cubic of
about 200 units flattened within 0.25, per curve) and `curvelen` (its length to 1e-3).

## fft

The cases, with Apple's vDSP as the reference (Go has no FFT; vDSP is measured by a program outside the tree, linked
with `-framework Accelerate`, in its own split form of complex numbers): `fft` and `fftf` (a forward and an inverse
complex transform of `n` doubles or floats, reported per transform, the inverse's scaling included on both sides),
`rfftf` (`forward_real` of `n` floats against `vDSP_fft_zrip` with its packing) and `dftf` (a forward complex
transform of a length that is not a power of two, against `vDSP_DFT_Execute`, which takes 2^k times 3, 5 or 15).

## statistics

The cases, with `n` values (a million by default) of a lognormal distribution: `ssummary` (`summary::add`, per
value), `sdigest` (`t_digest::add`, per value), `squantile` (a `t_digest` quantile of a digest of `n` values),
`shist` (`histogram::add` into 30 exponential buckets, per value), `smean`, `smedian` and `spct` (`mean`, `median`
and the 0.99 `quantile` of the `n` values, per value). The functions of a sequence are measured against Python's
`statistics` (`benchmarks/math/statistics_python.py`: `fmean`, `median`, `quantiles(n=100, method='inclusive')`); the
accumulators have no reference, Go's standard library having none.

## random

The cases, against Go's `math/rand/v2` with its `ChaCha8` and against the standard library's `mt19937_64` with
its distributions: `uint64` (`next_uint64`, `mt19937_64()`), `intn` (`next_int(1000)`,
`uniform_int_distribution`), `double` (`next_double`, `generate_canonical`), `normal` (`next_normal`,
`normal_distribution`), `exp` (`next_exponential`, `exponential_distribution`), `shuffle` (a million `int`s,
reported per element, `std::shuffle`) and `make` (a default `math::random` made and one draw taken, against a
`random_device` and an `mt19937_64` seeded from it).
