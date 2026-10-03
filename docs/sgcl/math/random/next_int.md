[sgcl](../../README.md) › [math](../README.md) › [random](README.md)

# sgcl::math::random::next_int

```cpp
int64_t next_int(int64_t bound);                   // (1)
int64_t next_int(int64_t first, int64_t last);     // (2)
big_integer next_int(const big_integer& bound);    // (3)
```

A whole number drawn so that every value is as likely.

1. In `[0, bound)`: Lemire's multiplication with the rejection that makes it exact, and a mask for a power of two.
   It is what Go's `Int64N` does, so from the same stream the same numbers.
2. In `[first, last)`, half-open like [range(first, last)](../../core/range/README.md): a die is `next_int(1, 7)`. Any two
   `int64_t` are taken, the span between them up to 2^64 - 1: `next_int(INT64_MIN, INT64_MAX)` is fine.
3. In `[0, bound)` for a bound of any size: as many bits as the bound has, drawn again while the value is not
   below it, which takes fewer than two draws on the average. A bound within `int64_t` gives what (1) gives from the
   same stream.

(3) is declared with the class and defined in `sgcl/math/big_integer.h`, which a program that has a
[big_integer](../big_integer/README.md) includes anyway; `sgcl/math/random.h` alone does not bring it.

## Parameters

| Parameter | Description |
|---|---|
| `bound` | the end of the range, above zero |
| `first` | the first value of the range |
| `last` | the end of the range, above `first` |

## Return value

The number drawn.

## Complexity

- (1–2) A draw, rarely two: a rejection happens with a probability below `bound` / 2^64.
- (3) As many draws as the bound has words, fewer than two times over on the average.

## Exceptions

- (1), (3) `domain_error` when `bound` is zero or below.
- (2) `domain_error` when `first` is not below `last`.

Nothing is drawn when the arguments are refused.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);
    vector<int64_t> dice;
    for (int i : range(10)) {
        dice.push_back(r.next_int(1, 7));
    }
    println("{}", dice);
    println("{} {}", r.next_int(100), r.next_int(-5, 5));

    math::big_integer bound = math::big_integer(10).pow(30);
    println("{}", r.next_int(bound));

    try {
        r.next_int(0);
    } catch (const domain_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
[6, 2, 2, 6, 5, 1, 3, 5, 1, 5]
59 2
587184250426410343082188278754
sgcl::math::random::next_int: a bound of zero or below
```

## See also

- [next_double](next_double.md): a fraction of one
- [pick](pick.md): an element of a range, at a position drawn so
- [sgcl::math::random](README.md)
