[sgcl](../README.md) › [math](README.md)

# sgcl::math::int_size

```cpp
#include "sgcl/math/geometry.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct int_size;
}
```

`sgcl::math::int_size` is a width and a height in whole numbers, two `int32_t`: the size of an image or of a grid in
pixels or cells. [size](size.md) is the form in floats, made from one explicitly.

## Member objects

| Member | Description |
|---|---|
| `int32_t width` | the width; 0 by default |
| `int32_t height` | the height; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | nothing (0 by 0); `int_size(int32_t width, int32_t height)` |
| `is_empty` | `bool is_empty() const`: a width or a height of zero or below |

## Non-member functions

| Function | Description |
|---|---|
| `operator==` | both sides equal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::int_size image(1920, 1080);
    math::size scaled = math::size(image) * 0.5f;
    println("{} {} {}", scaled.width, scaled.height, math::int_size(0, 1).is_empty());
}
```

Output:

```text
960 540 true
```

## See also

- [size](size.md): the size in floats
- [int_rect](int_rect.md): a rectangle of pixels
- [README: math](README.md)
