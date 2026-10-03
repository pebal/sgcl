[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](../fold_matches.md) › [iterator](../fold_matches-iterator.md)

# sgcl::txt::fold_matches::iterator::pos

```cpp
size_t pos() const noexcept;
```

Returns the byte position in the text where the occurrence the iterator stands on begins: in the text as it was
given, not in its folded or decomposed copy.

## Parameters

None.

## Return value

The position of the first byte of the occurrence. The iterator must stand on an occurrence, not at the end.

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
    string text = "Żółw, ŻÓŁW i żółw";
    auto all = txt::fold_matches(text, "żółw");
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{} ", it.pos());
    }
    println();
}
```

Output:

```text
0 9 19 
```

## See also

- [size](size.md): the bytes it covers
- [sgcl::txt::fold_matches::iterator](../fold_matches-iterator.md)
