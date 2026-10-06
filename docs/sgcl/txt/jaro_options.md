[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::jaro_options

```cpp
#include "sgcl/txt/distance.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct jaro_options {
        double prefix_scale = 0.1;
        size_t max_prefix = 4;
        double threshold = 0.7;
        bool bytes = false;
    };
}
```

`sgcl::txt::jaro_options` is how [jaro](jaro.md) and [jaro_winkler](jaro_winkler.md) measure.

## Member objects

| Field | Description |
|---|---|
| `prefix_scale` | Winkler's p, the weight of a unit of common prefix; 0.1 by default |
| `max_prefix` | the common prefix counted up to this many units; 4 by default |
| `threshold` | the prefix counts only when Jaro's similarity is above it (Winkler's boost threshold); 0.7 by default, 0 for always |
| `bytes` | bytes as the units, not code points; `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{:.4f}", txt::jaro_winkler("abcdxyz", "abcdzyx"));
    println("{:.4f}", txt::jaro_winkler("abcdxyz", "abcdzyx", {.max_prefix = 2}));
    println("{:.4f}", txt::jaro_winkler("ab", "ac", {.threshold = 0}));
}
```

Output:

```text
0.9714
0.9619
0.7000
```

## See also

- [jaro_winkler](jaro_winkler.md)
- [distance_options](distance_options.md)
- [sgcl::txt](README.md)
