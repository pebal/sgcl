[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::jaro_winkler

```cpp
#include "sgcl/txt/distance.h"   // or "sgcl/txt.h"

double jaro_winkler(const string& a, const string& b, const jaro_options& o = {});
```

Returns the Jaro-Winkler similarity: Jaro's (see [jaro](jaro.md)), raised for a common prefix, which names that are
typed the same at the start usually have — `j + l · p · (1 - j)`, l the common prefix up to `o.max_prefix` units and
p `o.prefix_scale`, when Jaro's similarity is above `o.threshold`. With the defaults (0.1, four units, 0.7:
Winkler's) it stays in [0, 1]; a scale above 1 / max_prefix can take it past 1.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts |
| `o` | the [jaro_options](jaro_options.md) |

## Return value

The similarity, in [0, 1] with the default options.

## Complexity

As [jaro](jaro.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{:.4f}", txt::jaro_winkler("MARTHA", "MARHTA"));
    println("{:.4f}", txt::jaro_winkler("DWAYNE", "DUANE"));
    println("{:.4f}", txt::jaro_winkler("Kowalski", "Kowalsky"));
    println("{:.4f}", txt::jaro_winkler("Kowalski", "Kowalsky", {.prefix_scale = 0.2}));
}
```

Output:

```text
0.9611
0.8400
0.9500
0.9833
```

## See also

- [jaro](jaro.md)
- [jaro_options](jaro_options.md)
- [sgcl::txt](README.md)
