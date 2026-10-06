[sgcl](../../README.md) › [concurrent](../README.md) › [bloom_filter](README.md)

# sgcl::concurrent::bloom_filter::bloom_filter

```cpp
explicit bloom_filter(size_t expected_items, double false_positive_rate = 0.01);    // (1)
bloom_filter(const bloom_filter&) noexcept = default;                               // (2)
bloom_filter(bloom_filter&&) noexcept = default;                                    // (3)
```

1. A filter of the optimal shape for `expected_items` keys and a rate of false positives: *m* = −*n* ln *p* / ln²2
   bits, rounded up to whole words of 64, and *k* = *m*/*n* ln 2 hashes, from 1 to 64. An expected count of zero is
   one. More keys than expected may be added; the rate grows with them ([false_positive_rate](false_positive_rate.md)).
2. A handle of the same filter: the copy shares the bits.
3. The same, taken from the other handle, which stands for the same filter still.

## Parameters

| Parameter | Description |
|---|---|
| `expected_items` | the keys the filter is made for, *n* |
| `false_positive_rate` | the rate of false positives at *n* keys, *p*, in (0, 1); 1% by default |

## Complexity

- (1) Linear in *m*: the bits allocated, zero.
- (2–3) Constant.

## Exceptions

- (1) `invalid_argument` for a rate outside (0, 1), or NaN.
- (2–3) None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::bloom_filter f(1'000'000, 0.001);
    println("{} bits, {} hashes", f.bit_count(), f.hash_count());
    concurrent::bloom_filter same = f;
    println("{}", same == f);
}
```

Output:

```text
14377600 bits, 10 hashes
true
```

## See also

- [with_size](with_size.md): a shape given outright
- [sgcl::concurrent::bloom_filter](README.md)
