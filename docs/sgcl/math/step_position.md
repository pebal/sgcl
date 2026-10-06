[sgcl](../README.md) › [math](README.md)

# sgcl::math::step_position

```cpp
#include "sgcl/math/interpolation.h"   // or "sgcl/math.h"

namespace sgcl::math {
    enum class step_position : uint8_t {
        jump_start,
        jump_end,
        jump_none,
        jump_both
    };
}
```

Where the jumps of [easing](easing.md)`::steps(count, position)` fall, CSS's step positions: a progress in
[0, 1] split into `count` equal intervals, the output jumping at their starts or their ends.

| Value | Description |
|---|---|
| `jump_start` | a jump at the start of each interval: the output leaves 0 at once (CSS's `start`) |
| `jump_end` | a jump at the end of each interval: the output reaches 1 only at the end (CSS's `end`, the default) |
| `jump_none` | no jump at either end of the whole: `count` levels from 0 to 1, each held for an interval |
| `jump_both` | a jump at both ends: `count + 1` jumps, 0 and 1 each held for a moment only |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    using math::step_position;
    for (auto position : {step_position::jump_start, step_position::jump_end,
                          step_position::jump_none, step_position::jump_both}) {
        math::easing e = math::easing::steps(4, position);
        println("{} {} {} {}", e(0), e(0.3f), e(0.9f), e(1));
    }
}
```

Output:

```text
0.25 0.5 1 1
0 0.25 0.75 1
0 0.33333334 1 1
0.2 0.4 0.8 1
```

## See also

- [easing](easing.md): steps() and the other timing functions
- [README: math](README.md)
