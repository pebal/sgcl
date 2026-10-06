[sgcl](../README.md) › [math](README.md)

# sgcl::math::mode

```cpp
double mode(const slice<const double>& values);
```

The most common value; of several equally common, the first of them in the sequence — Python's `statistics.mode`. Values are compared as numbers: −0 and +0 are one value. The header is `sgcl/math/statistics.h`.

## Parameters

| Parameter | Description |
|---|---|
| `values` | the numbers |

## Return value

The mode.

## Complexity

Linear on average (a hash table of the values).

## Exceptions

- `domain_error` when there are no values or a NaN is among them.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::mode(vector<double>{1, 3, 3, 2, 2}),
            math::mode(vector<double>{2, 1, 1, 2}));
}
```

Output:

```text
3 2
```

## See also

- [median](median.md), [mean](mean.md): the other middles
- [README: math](README.md)
