[sgcl](../README.md) › math

# sgcl::math

```cpp
#include "sgcl/math.h"   // namespace sgcl::math
```

Numbers that the language does not have, and randomness that the standard library gives in a shape nobody
wants: whole numbers of any size, fractions of them and decimals of any precision, and a generator with the
questions a program asks of one. What Go has in `math/big` and `math/rand/v2`, and Java in `BigDecimal`, as values
with the manners of an `int`: `a * 2 + 1`, `a < 10`, `third * 3 == 1`, `price * 3`, `r.next_int(1, 7)`. And the
plane's shapes and transforms and the small algebra of a 3D scene, in floats as a user interface, SVG and a GPU take
them: points, sizes, rectangles, affine transforms, vectors, matrices and quaternions; Bézier curves, interpolation
and the timing functions of animations; the Fourier transform; binary floating point of any precision; statistics,
streaming and of a whole sequence.

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
reuses its receiver. A `big_integer`, a `rational`, a `decimal` and a `big_float` hold a `tracked_ptr`, so they live where one may: on a
stack, in a managed object, in a container of the library ([The rules](../core/README.md#the-rules) of core); a
`random` and the shapes, vectors and matrices hold no pointer and live anywhere; an `fft` plan is a handle and lives
where a `tracked_ptr` may.

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
`to_decimal` against the definition of rounding half away from zero. `decimal` is checked against Python's
`decimal` (`tools/math_vectors.py decimal`), which is libmpdec, in contexts of the largest precision for the exact
operations and of the precision asked for the others: the representation — the unscaled part and the scale — of
every sum, difference, product, remainder and quotient over pairs of named and random values, the division to
digits and the rounding to digits and places in each of the seven modes, square roots, the text both ways, the
doubles both ways; the division to a scale and the rounding of fractions against Python's `fractions` and the
definitions of the modes; and a fuzzing harness holds it against libmpdec itself. The laws of arithmetic are checked over
thousands of random operands besides, and `big_integer` is exercised under a collector running in a loop, with a
count of the pages left when the numbers are gone. For `random` the first test is the C2SP specification's own
sample output; `tools/math_oracle.go` writes `tests/math/random_vectors.h` from Go's `math/rand/v2`; the
distributions are held to their shapes by chi-square and Kolmogorov–Smirnov tests on fixed seeds, and to the
values `random(42)` gave when they were written down.

## Functions

| Function | Header | Description |
|---|---|---|
| [convolve](convolve.md) | `fft.h` | the linear convolution of two sequences, directly or by real transforms |
| [inverse_lerp](inverse_lerp.md) | `interpolation.h` | where a value lies from a to b, `(v − a)/(b − a)` |
| [lerp](lerp.md) | `interpolation.h` | the value a fraction of the way from a to b: numbers, points, vectors |
| [mean](mean.md) | `statistics.h` | the arithmetic mean of a sequence, from a compensated sum |
| [median](median.md) | `statistics.h` | the middle of a sequence, or the mean of the two in the middle |
| [mode](mode.md) | `statistics.h` | the most common value, the first of a tie |
| [quantile](quantile.md) | `statistics.h` | a quantile of a sequence, linear between the order statistics (R's type 7) |
| [smoothstep](smoothstep.md) | `interpolation.h` | GLSL's smoothstep, a step with no slope at its ends |

## Classes

| Class | Header | Description |
|---|---|---|
| [affine](affine.md) | `geometry.h` | an affine transform of the plane, SVG's `matrix(a, b, c, d, e, f)`: translation, scaling, rotation, skew, composed as matrices, `inverse`, `apply` to a point or a rectangle |
| [big_float](big_float/README.md) | `big_float.h` | a binary floating-point number of any precision, as Go's `big.Float`: the precision and the rounding on the value, each result rounded once, ±0 and ±infinity, no NaN; parse decimal and hexadecimal, the shortest text that reads back and Go's formats, exact conversions to fractions and decimals |
| [big_integer](big_integer/README.md) | `big_integer.h` | a whole number of any size with the manners of an `int`: the operators, `/` cut towards zero and `mod` never negative, the bits in two's complement, text in the bases 2 to 36, bytes, the literal `_big`, `txt::format`; `pow`, `sqrt`, `gcd`, `lcm`, `mod_pow`, `mod_inverse`, `is_probable_prime`, `factorial`, `binomial`; sixteen bytes, a value within `int64_t` allocating nothing |
| [cubic_bezier](cubic_bezier.md) | `bezier.h` | a cubic Bézier curve of the plane: `at`, `derivative`, `split`, tight `bounds`, `flatten` within a tolerance (Wang's formula), `length` |
| [decimal](decimal/README.md) | `decimal.h` | a decimal number of any precision with a scale, as Java's `BigDecimal` and PostgreSQL's `NUMERIC`: `+`, `-`, `*` exact; `div`, `div_precision`, `sqrt`, `rescale`, `round_precision` with a [rounding](../core/rounding.md); NaN and ±Infinity as PostgreSQL has them; text plain and scientific, doubles exactly and shortest, fractions; a value of up to eighteen digits allocating nothing |
| [easing](easing.md) | `interpolation.h` | a timing function of an animation as a value: CSS's `cubic-bezier()` (`bezier`) and `steps()`, its keywords and the named set of easings.net |
| [fft](fft/README.md) | `fft.h` | a plan for the discrete Fourier transform of one length, any length: complex and real, double and float, in place; Stockham radix 4/2/3/5 on NEON, Bluestein for other primes |
| [histogram](histogram.md) | `statistics.h` | counts over fixed buckets, Prometheus's: linear and exponential buckets, merge, histogram_quantile |
| [int_point](int_point.md) | `geometry.h` | a point of whole numbers: a pixel, a cell |
| [int_rect](int_rect.md) | `geometry.h` | a rectangle of whole pixels, the members of `rect` with edges in 64 bits |
| [int_size](int_size.md) | `geometry.h` | a width and a height in whole numbers |
| [mat3](mat3.md) | `algebra.h` | a 3×3 matrix of floats, column-major: products, `transposed`, `determinant`, `inverse`, from an `affine` |
| [mat4](mat4.md) | `algebra.h` | a 4×4 matrix of floats, column-major: `translation`, `scaling`, `rotation`, `look_at`, `perspective`, `orthographic` (right-handed, depth 0 to 1), products and a batch `apply` on NEON, `inverse` |
| [paired_summary](paired_summary.md) | `statistics.h` | covariance, correlation and the least-squares line of pairs, streaming and mergeable |
| [parse_error](parse_error/README.md) | `big_integer.h` | why a text did not read as a number: the byte where the reading stopped and a sentence |
| [point](point.md) | `geometry.h` | a point of the plane in floats, with the arithmetic of a vector |
| [quadratic_bezier](quadratic_bezier.md) | `bezier.h` | a quadratic Bézier curve of the plane, the members of `cubic_bezier` and `to_cubic` |
| [quaternion](quaternion.md) | `algebra.h` | a rotation of 3D space: `from_axis_angle`, Hamilton's product, `rotate`, `slerp`, `to_mat3`, `to_mat4` |
| [random](random/README.md) | `random.h` | a generator, ChaCha8Rand with the stream of Go's `ChaCha8` from the same key, and what is asked of it: `next_int`, `next_double`, `next_normal`, `next_exponential`, `next_bytes`, `shuffle`, `pick`, `permutation`; a uniform random bit generator of the standard's for everything else |
| [rational](rational/README.md) | `rational.h` | a fraction of two `big_integer`s, always in lowest terms: exact arithmetic with the operators, `pow` with a negative exponent, `floor`, `ceil`; `parse` of `"3/4"` and of decimals, `rational(double)` exactly, `to_double` rounded once, `to_decimal` half away from zero |
| [rect](rect.md) | `geometry.h` | a rectangle of the plane in floats, x, y, width and height, half-open: `contains`, `intersects`, `intersection`, `united`, `inflated`, `rounded_out` |
| [size](size.md) | `geometry.h` | a width and a height in floats |
| [summary](summary.md) | `statistics.h` | count, mean, variance, skewness, kurtosis, min, max of numbers added one at a time, mergeable |
| [t_digest](t_digest.md) | `statistics.h` | streaming quantiles and the cdf in bounded memory, Dunning's merging t-digest, mergeable |
| [vec2](vec2.md) | `algebra.h` | a vector of two floats: the operators, `dot`, `cross`, `length`, `normalized` |
| [vec3](vec3.md) | `algebra.h` | a vector of three floats: the operators, `dot`, `cross`, `length`, `normalized` |
| [vec4](vec4.md) | `algebra.h` | a vector of four floats, homogeneous coordinates or a colour |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [step_position](step_position.md) | `interpolation.h` | where the jumps of `easing::steps` fall, CSS's step positions |

## See also

- [Benchmarks](benchmarks.md): `big_integer` against Go's `math/big`, `random` against Go's `math/rand/v2` and
  `mt19937_64`
- [string](../core/string/README.md): the immutable object shared by a word that `big_integer` follows
- [txt::format](../txt/format.md): the formatting of the numbers
- [The modules](../README.md)
