[sgcl](../../README.md) › [math](../README.md)

# sgcl::math::random

```cpp
#include "sgcl/math/random.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class random;
}
```

`sgcl::math::random` is a generator of random numbers and the questions a program asks of one: a number below a
bound, a die, a coin, a double, a normal or an exponential variate, a shuffle, one element of a list. It is what
Go's `math/rand/v2` is, as one type, with nothing global and no second road: where Go has `rand.IntN` on a global
source and `rand.New(src).IntN` on a source of one's own, here a program makes a `random` and asks it,
`math::random r; r.next_int(n)`.

The generator is ChaCha8Rand, the one behind Go's `ChaCha8` (Go also offers `PCG`; here there is ChaCha8Rand
only). A seed gives Go's stream: `math::random(42)` draws what Go's `rand.NewChaCha8` draws from the key of 42 in
eight bytes little-endian and 24 zeros, and `next_int`, `next_double`, `shuffle` and `permutation` answer what
Go's `Int64N`, `Float64`, `Shuffle` and `Perm` answer from it. `next_int(first, last)` is half-open like
[range](../../core/range/README.md), where Go has `IntN(n)` alone. The normal and the exponential variates are this
library's own ziggurats, where Go has `NormFloat64` and `ExpFloat64`; every distribution of `<random>` takes a
`random` as well, through `operator()`.

The name is always written `math::random`: with `using namespace sgcl::math` a bare `random` would meet the C
library's `random()`.

## Rules

- **The generator is ChaCha8Rand**, as the C2SP specifies it ([c2sp.org/chacha8rand](https://c2sp.org/chacha8rand)):
  the one behind Go's `ChaCha8` and its runtime. A key of 32 bytes gives the same stream of 64-bit words here as in
  Go, on every platform. ChaCha with eight rounds, sixteen blocks to a key, and the last 32 bytes of every 1024 the
  next key: what was drawn cannot be worked back from the state.
- **`random(seed)` repeats; `random()` does not.** A seed is the key's first eight bytes, little-endian, and 24
  zeros: Go's `rand.NewChaCha8` of that key draws the same `Uint64`s, and the same `Int64N`, `Float64`, `Shuffle`
  and `Perm`, which are computed from them as here. `random()` takes its key from a generator of the thread, which
  takes its own from the system (`std::random_device`) the first time the thread asks, and again in the child of a
  `fork()`; two defaults never repeat each other, in two threads or across a fork.
- **A value of 304 bytes, no pointer in it**, trivially copyable. It goes anywhere, a `std::vector` and a global
  included. A copy copies the stream: both draw the same numbers after. It is not for sharing between threads:
  one to a thread or a task, as any value.
- **An error of the program throws.** A bound of zero or below, an empty range, a negative standard deviation, a
  rate of zero or below are `domain_error`; `pick` of an empty range is `out_of_range`.
- **Not for secrets.** The stream is strong, but a seed is a number and a copy repeats; keys, nonces and tokens
  come from `crypto` ([crypto::random](../../crypto/random/README.md)), and neither Go's `math/rand` nor this is for them.

## Member types

| Type | Definition |
|---|---|
| `result_type` | `uint64_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](random.md) | a generator keyed from the thread's generator, or from a seed |

#### Generation

| Function | Description |
|---|---|
| [next_int](next_int.md) | a whole number below a bound or in a half-open range, every value as likely |
| [next_uint64](next_uint64.md) | all 64 bits of a draw |
| [next_double](next_double.md) | a double in `[0, 1)`, in steps of 2^-53 |
| [next_bool](next_bool.md) | a coin |
| [next_normal](next_normal.md) | a normal variate of a mean and a standard deviation |
| [next_exponential](next_exponential.md) | an exponential variate of a rate |
| [next_bytes](next_bytes.md) | fills bytes from the stream |
| [operator()](operator_call.md) | the next draw, for the distributions of `<random>` |

#### Ranges

| Function | Description |
|---|---|
| [shuffle](shuffle.md) | the elements of a range in a random order |
| [pick](pick.md) | one element of a range, each as likely |
| [permutation](permutation.md) | `0 … n - 1` in a random order |

#### Characteristics

| Function | Description |
|---|---|
| [min](min.md) | the smallest value of a draw, 0 (static) |
| [max](max.md) | the largest value of a draw, 2^64 - 1 (static) |

## Complexity

A draw is a word of a buffer of 32, made four ChaCha8 blocks at a time; the blocks are written with the
compiler's generic vectors, which it lowers to one NEON or SSE register for the four blocks, so no intrinsic of
one instruction set is in the source. A default `random` costs a few draws of the thread's generator and a block
of ChaCha8, a call to the system only the first time in a thread. The times against Go's `math/rand/v2` and
`mt19937_64` are on [benchmarks](../benchmarks.md#random).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);  // the same numbers in every run
    println("{} {} {}", r.next_int(1, 7), r.next_int(100), r.next_bool());

    vector cards = {1, 2, 3, 4, 5, 6, 7, 8};
    r.shuffle(cards);
    println("{} {}", cards, r.pick(cards));

    double sum = 0;
    for (int i : range(1000)) {
        sum += r.next_normal(100, 15);
    }
    println("{:.1f}", sum / 1000);
}
```

Output:

```text
6 20 false
[3, 5, 7, 4, 2, 1, 8, 6] 6
99.9
```

## See also

- [big_integer](../big_integer/README.md): [next_int](next_int.md) draws one below a bound of any size
- [crypto::random](../../crypto/random/README.md): the generator for keys and secrets
- [range](../../core/range/README.md): the half-open ranges `next_int(first, last)` follows
- [The module](../README.md)
