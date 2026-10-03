[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](README.md)

# sgcl::txt::fold_matches::text

```cpp
slice<const char> text() const noexcept;
```

Returns the text the range walks, as a slice of the string the range holds: the elements are slices of it and the
positions of the iterator are bytes of it. A piece of a text or a C text is the copy the range holds.

## Parameters

None.

## Return value

The text; an empty slice for a range made by the default constructor.

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
    string full = "start: OK, stop: ok";
    txt::fold_matches rest(full.as_slice(7), txt::fold_searcher("ok"));
    println("[{}] {}", rest.text(), rest.begin().pos());
}
```

Output:

```text
[OK, stop: ok] 0
```

## See also

- [pattern](pattern.md): the pattern
- [sgcl::txt::fold_matches, normalized_matches](README.md)
