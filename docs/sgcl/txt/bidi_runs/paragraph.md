[sgcl](../../README.md) › [txt](../README.md) › [bidi_runs](README.md)

# sgcl::txt::bidi_runs::paragraph

```cpp
direction paragraph() const noexcept;
```

Returns the direction the first paragraph of the text runs in: the one asked for at the construction, or the one its
first strong character gave ([direction](../direction.md)). A text of several paragraphs has a direction for each,
which the levels of its pieces show. An empty range runs left to right.

## Parameters

None.

## Return value

`direction::left_to_right` or `direction::right_to_left`; never `direction::automatic`.

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
    for (auto s : {"Nazwa: שלום", "שלום: Nazwa"}) {
        txt::bidi_runs pieces(s);
        println("{}", pieces.paragraph() == txt::direction::right_to_left);
    }
}
```

Output:

```text
false
true
```

## See also

- [paragraph_direction](../paragraph_direction.md): the direction without the pieces
- [sgcl::txt::bidi_runs](README.md)
