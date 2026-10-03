[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::factorial

```cpp
static big_integer factorial(int64_t n);
```

`n!`, the product of 1 to `n`, as Go's `MulRange(1, n)` and Python's `math.factorial`; 0! and 1! are 1. The odd
parts of 3 … `n` are multiplied in a balanced tree of products, whose leaves gather numbers into a word while it
holds them, so that the products at the top are of numbers of like length; the twos are one shift at the end,
`n - popcount(n)` of them. A result past 2^52 bits, the longest number, is refused before anything is computed, as
[pow](pow.md) refuses one.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number, zero or more |

## Return value

`n!`.

## Complexity

Each level of the tree costs at most a multiplication at the length of the result, and the tree has about
log2(`n`) levels.

## Exceptions

- `domain_error` when `n` is negative.
- `length_error` when `n!` would be longer than 2^52 bits (the longest number, as for [pow](pow.md)), before
  anything is computed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer f = 1;
    for (int i : range(1, 101)) {
        f *= i;
    }
    println("100! has {} digits, {} twos", f.to_string().size(), f.trailing_zeros());
    println("{}", f == math::big_integer::factorial(100));
    println("{} {}", math::big_integer::factorial(0), math::big_integer::factorial(20));
}
```

Output:

```text
100! has 158 digits, 97 twos
true
1 2432902008176640000
```

## See also

- [binomial](binomial.md): the number of ways to choose k of n
- [operator*=](operator_arith.md): a product grown in place
- [sgcl::math::big_integer](README.md)
