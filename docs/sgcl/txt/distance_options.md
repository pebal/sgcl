[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::distance_options

```cpp
#include "sgcl/txt/distance.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct distance_options {
        size_t max = SIZE_MAX;
        bool bytes = false;
    };
}
```

`sgcl::txt::distance_options` is how [levenshtein](levenshtein.md), [osa_distance](osa_distance.md),
[damerau_levenshtein](damerau_levenshtein.md) and [lcs_length](lcs_length.md) measure.

## Member objects

| Field | Description |
|---|---|
| `max` | a cutoff: an answer larger than it is given as `max + 1`, so `levenshtein(a, b, {.max = 2}) <= 2` reads as meant, and texts whose lengths alone differ by more are not measured; no cutoff by default |
| `bytes` | bytes as the units, not code points; `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::levenshtein("short", "a much longer text", {.max = 3}));
    println("{} {}", txt::levenshtein("ą", "a"), txt::levenshtein("ą", "a", {.bytes = true}));
}
```

Output:

```text
4
1 2
```

## See also

- [levenshtein](levenshtein.md)
- [jaro_options](jaro_options.md)
- [sgcl::txt](README.md)
