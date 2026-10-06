[sgcl](../README.md) › [math](README.md)

# sgcl::math::size

```cpp
#include "sgcl/math/geometry.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct size;
}
```

`sgcl::math::size` is a width and a height, two floats: the extent of a [rect](rect.md), of a window or of a picture
drawn. [int_size](int_size.md) is the form for pixels. A plain value with no pointer in it.

## Member objects

| Member | Description |
|---|---|
| `float width` | the width; 0 by default |
| `float height` | the height; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | nothing (0 by 0); `size(float width, float height)`; `explicit size(const int_size&)` |
| `is_empty` | `bool is_empty() const`: whether it covers nothing, a width or a height of zero or below (or NaN) |

## Non-member functions

| Function | Description |
|---|---|
| `operator*`, `operator/` | both sides times or over a float, the float on either side of `*` |
| `operator==` | both sides equal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::size window(1280, 720);
    math::size half = window / 2;
    println("{} {} {}", half.width, half.height, half.is_empty());
    println("{} {}", math::size(0, 10).is_empty(), math::size(-1, 10).is_empty());
}
```

Output:

```text
640 360 false
true true
```

## See also

- [rect](rect.md): a size at a place
- [int_size](int_size.md): the size in pixels
- [README: math](README.md)
