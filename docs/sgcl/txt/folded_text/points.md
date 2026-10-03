[sgcl](../../README.md) › [txt](../README.md) › [folded_text](../folded_text.md)

# sgcl::txt::folded_text::points

```cpp
const mapped_text& points() const noexcept;
```

Returns the text as the object mapped it, a [mapped_text](../mapped_text.md): the code points it folded to for a
`folded_text`, or decomposed and put in canonical order for a `normalized_text`, and the byte of the text each of
them came from, with the size of the text after the last. It is what every search of the object looks through.

## Parameters

None.

## Return value

The mapped text, held by the object; for one made by the default constructor, no code points and one position, `0`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::normalized_text nt("Café");
    for (char32_t c : nt.points().points) {
        print("U+{:04X} ", uint32_t(c));
    }
    println("| {} positions", nt.points().at.size());
}
```

Output:

```text
U+0043 U+0061 U+0066 U+0065 U+0301 | 6 positions
```

## See also

- [size](size.md): how many code points
- [mapped_text](../mapped_text.md)
- [sgcl::txt::folded_text, normalized_text](../folded_text.md)
