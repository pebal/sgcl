# sgcl::math::random

```cpp
#include "sgcl/math/random.h"   // or "sgcl/math/math.h"

namespace sgcl::math {
    class random;
}
```

`math::random` is a generator of random numbers and the questions a program asks of one: a number below a bound, a die, a coin, a double, a normal or an exponential variate, a shuffle, one element of a list. What Go's `math/rand/v2` is, as one type, with nothing global and no second road: make one and ask it.

```cpp
using namespace sgcl;

math::random r;
int64_t die = r.next_int(1, 7);            // 1 … 6
double x = r.next_double();                // [0, 1)
bool heads = r.next_bool();

vector cards = {1, 2, 3, 4, 5, 6, 7, 8};
r.shuffle(cards);
int winner = r.pick(cards);

math::random replay(42);                   // the same numbers in every run
vector<double> samples;
for (auto i : range(10)) {
    samples.push_back(replay.next_normal(100, 15));
}
```

The name is always written `math::random`: with `using namespace sgcl::math` a bare `random` would meet the C library's `random()`.

## Rules

- **The generator is ChaCha8Rand**, as the C2SP specifies it ([c2sp.org/chacha8rand](https://c2sp.org/chacha8rand)) — the one behind Go's `ChaCha8` and its runtime. A key of 32 bytes gives the same stream of 64-bit words here as in Go, on every platform. ChaCha with eight rounds, sixteen blocks to a key, and the last 32 bytes of every 1024 the next key: what was drawn cannot be worked back from the state.
- **`random(seed)` repeats; `random()` does not.** A seed is the key's first eight bytes, little-endian, and 24 zeros: Go's `rand.NewChaCha8` of that key draws the same `Uint64`s, and the same `Int64N`, `Float64`, `Shuffle` and `Perm`, which are computed from them as here. `random()` takes its key from a generator of the thread, which takes its own from the system (`std::random_device`) the first time the thread asks, and again in the child of a `fork()`; two defaults never repeat each other, in two threads or across a fork.
- **A value, about 300 bytes, no pointer in it.** It goes anywhere, a `std::vector` and a global included. A copy copies the stream: both draw the same numbers after. It is not for sharing between threads — one to a thread or a task, as any value.
- **An error of the program throws.** A bound of zero or below, an empty range, a negative standard deviation, a rate of zero or below are `domain_error`; `pick` of an empty range is `out_of_range`.
- **Not for secrets.** The stream is strong, but a seed is a number and a copy repeats; keys, nonces and tokens come from `crypto`.

## Members

### Construction

```cpp
random();                                  // unpredictable: a key from the thread's generator
explicit random(uint64_t seed) noexcept;   // repeatable: Go's ChaCha8 stream of that key
```

A default `random` costs a few draws of the thread's generator and a block of ChaCha8 — a call to the system only the first time in a thread; making one inside a hot loop is still the wrong shape — make it once and ask it many times.

### Numbers

```cpp
int64_t next_int(int64_t bound);                   // [0, bound); bound <= 0 → domain_error
int64_t next_int(int64_t first, int64_t last);     // [first, last); empty → domain_error
big_integer next_int(const big_integer& bound);    // [0, bound) of any size; with big_integer.h
uint64_t next_uint64() noexcept;                   // all 64 bits
double next_double() noexcept;                     // [0, 1), in steps of 2^-53
bool next_bool() noexcept;
double next_normal(double mean = 0, double stddev = 1);
double next_exponential(double rate = 1);          // the mean is 1 / rate
void next_bytes(const slice<byte>& out) noexcept;
```

`next_int` is exact — every value as likely — by Lemire's multiplication with its rejection, and a mask for a power of two. Its form with a [`big_integer`](big_integer.md) bound is declared here and defined in `big_integer.h`, which a program that has a `big_integer` includes anyway: as many bits as the bound has, drawn again while the value is not below it (fewer than two draws on the average); a bound within `int64_t` gives what `next_int(int64_t)` gives from the same stream. `next_int(first, last)` is half-open like [`range(first, last)`](../core/range.md), so a die is `next_int(1, 7)`, and takes any two `int64_t`: `next_int(INT64_MIN, INT64_MAX)` is fine. `next_double` is the low 53 bits of a draw over 2^53, as Go's `Float64`.

`next_normal` and `next_exponential` are ziggurats (Marsaglia and Tsang, 2000), of 128 and 256 layers, the layer chosen by the low bits of a draw and the value taken from the 56 bits above them, so the two are never the same bits. Their tables are literals in the source (`tools/math_tables.py`), so a seed gives the same numbers on every platform — which `std::normal_distribution` does not promise — but for the rare draw that falls outside the rectangles and goes through `std::exp` or `std::log`, whose last bit a platform's library may round otherwise. They are this library's own and not Go's: `NormFloat64` from the same stream gives other numbers.

`next_bytes` fills eight bytes from each draw, little-endian; the bytes of a last draw not wanted are dropped, so every call starts on a new draw.

### Ranges

```cpp
template<std::ranges::random_access_range R> void shuffle(R&& range);
template<std::ranges::random_access_range R> decltype(auto) pick(R& range);   // empty → out_of_range
vector<size_t> permutation(size_t n);                                         // 0 … n - 1 in a random order
```

`shuffle` is Fisher and Yates from the back, every order as likely, over any range of random access — a [`vector`](../core/vector.md), a `std::vector`, an array. `pick` hands back the element itself, not a copy, so it takes only a range that outlives the call: `r.pick(cards) = 0` writes into `cards`, and a temporary does not compile. `permutation(n)` is Go's `Perm`.

### The standard's generator

```cpp
using result_type = uint64_t;
static constexpr result_type min() noexcept;       // 0
static constexpr result_type max() noexcept;       // 2^64 - 1
result_type operator()() noexcept;                 // next_uint64()
```

A `random` is a `std::uniform_random_bit_generator`, so every distribution of `<random>` and `std::ranges::sample` take it as they take `mt19937_64`.

## What it costs

A draw is a word of a buffer of 32 made four ChaCha8 blocks at a time; the blocks are written with the compiler's generic vectors, which it lowers to one NEON or SSE register for the four blocks, so no intrinsic of one instruction set is in the source. `bench_math` measures every method against Go's `math/rand/v2` (`benchmarks/go/math`) and against `mt19937_64` with the distributions of `<random>`.
