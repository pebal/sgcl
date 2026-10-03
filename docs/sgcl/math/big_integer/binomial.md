[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::binomial

```cpp
static big_integer binomial(int64_t n, int64_t k);
```

The number of ways to choose `k` of `n`, `n! / (k! (n - k)!)`: Go's `Binomial`, and as Python's `math.comb`, 0
when `k > n`. `k` is taken as the smaller of `k` and `n - k`; the product of the top `k` numbers and the product of 2 to
`k` are each a balanced tree of products, as [factorial](factorial.md)'s, and one is divided by the other. A result past
2^52 bits, the longest number, is refused before anything is computed, as [pow](pow.md) refuses one.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number to choose from, zero or more |
| `k` | the number chosen, zero or more |

## Return value

The binomial coefficient; 0 when `k > n`, 1 when `k` is 0 or `n`.

## Complexity

Two trees of products, each level of which costs at most a multiplication at the length of the result, and one
division.

## Exceptions

- `domain_error` when `n` or `k` is negative.
- `length_error` when the result would be longer than 2^52 bits (the longest number, as for [pow](pow.md)), before
  anything is computed.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::big_integer::binomial(52, 5), math::big_integer::binomial(5, 7),
            math::big_integer::binomial(10, 0));
    println("{}", math::big_integer::binomial(100, 50));
    try {
        math::big_integer::binomial(-1, 0);
    } catch (const domain_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
2598960 0 1
100891344545564193334812497256
sgcl::math::big_integer::binomial: a negative argument
```

## See also

- [factorial](factorial.md): `n!`
- [sgcl::math::big_integer](README.md)
