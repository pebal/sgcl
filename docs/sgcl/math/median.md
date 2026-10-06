[sgcl](../README.md) › [math](README.md)

# sgcl::math::median

```cpp
double median(const slice<const double>& values);
```

The middle value of the values in order, or the mean of the two in the middle of an even count — Python's `statistics.median`, but never past the two where their sum would overflow (the median of the largest double twice is it, not +∞). The values are copied and the middle found by selection, not by sorting the whole. An infinity is a value (the median of −∞ and 5 is −∞, of −∞ and +∞ NaN, as Python's). The header is `sgcl/math/statistics.h`.

## Parameters

| Parameter | Description |
|---|---|
| `values` | the numbers, in any order |

## Return value

The median.

## Complexity

Linear on average (a selection of the copy).

## Exceptions

- `domain_error` when there are no values or a NaN is among them (it has no place in an order).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::median(vector<double>{3, 1, 2}),
            math::median(vector<double>{4, 1, 3, 2}));
}
```

Output:

```text
2 2.5
```

## See also

- [quantile](quantile.md): any fraction of the way
- [t_digest](t_digest.md): the median streaming
- [README: math](README.md)
