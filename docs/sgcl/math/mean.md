[sgcl](../README.md) › [math](README.md)

# sgcl::math::mean

```cpp
double mean(const slice<const double>& values);
```

The arithmetic mean of the values, from a sum with Neumaier's compensation divided by the count: within a unit or two of the last place of the exact mean, as Python's `statistics.fmean`, where a plain sum of a million values may lose several digits. A NaN among the values makes NaN. The header is `sgcl/math/statistics.h`; a [summary](summary.md) gives the mean with the other moments, one value at a time.

## Parameters

| Parameter | Description |
|---|---|
| `values` | the numbers |

## Return value

The mean.

## Complexity

Linear.

## Exceptions

- `domain_error` when there are no values.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    vector<double> v = {1, 2, 3, 4};
    println("{} {}", math::mean(v), math::mean(vector<double>{1e16, 1, -1e16, 1}));
}
```

Output:

```text
2.5 0.5
```

## See also

- [summary](summary.md): the mean and the other moments, streaming
- [median](median.md): the middle value
- [README: math](README.md)
