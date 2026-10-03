[sgcl](../../README.md) › [math](../README.md) › [random](../random.md)

# sgcl::math::random::permutation

```cpp
vector<size_t> permutation(size_t n);
```

The numbers `0 … n - 1` in a random order, every order as likely: the numbers in order, then
[shuffle](shuffle.md)d. It is Go's `Perm`, and from Go's stream the same order.

## Parameters

| Parameter | Description |
|---|---|
| `n` | how many numbers |

## Return value

A [vector](../../core/vector.md) of the `n` numbers; empty for `n` of zero.

## Complexity

Linear in `n`.

## Exceptions

`length_error` when `n` is above the vector's `max_size()`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);
    println("{}", r.permutation(10));
    println("{}", r.permutation(0));
}
```

Output:

```text
[9, 7, 2, 5, 0, 4, 6, 3, 1, 8]
[]
```

## See also

- [shuffle](shuffle.md): the elements of a range of one's own in a random order
- [sgcl::math::random](../random.md)
