[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](../fold_matches.md)

# sgcl::txt::fold_matches::points

```cpp
const mapped_text& points() const noexcept;
```

Returns the text the range walks as it was mapped, a [mapped_text](../mapped_text.md): the code points it folded to
for `fold_matches`, or decomposed and put in canonical order for `normalized_matches`, and the byte of the text each
of them came from, with the size of the text after the last. The occurrences are found in it once and reported in
the bytes of the text.

## Parameters

None.

## Return value

The mapped text, held by the range's tracked object; for a range made by the default constructor, no code points and
one position, `0`.

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
    txt::fold_matches matches("Straße", "SS");
    println("{} code points, the match at {}", matches.points().points.size(), matches.begin().pos());
}
```

Output:

```text
7 code points, the match at 4
```

## See also

- [text](text.md): the text as it was given
- [mapped_text](../mapped_text.md)
- [sgcl::txt::fold_matches, normalized_matches](../fold_matches.md)
