[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](../fold_matches.md) › [iterator](../fold_matches-iterator.md)

# sgcl::txt::fold_matches::iterator::size

```cpp
size_t size() const noexcept;
```

Returns the number of bytes of the text the occurrence covers: the text's own count, which need not be the
pattern's. A match takes whole characters, so the next character begins at `pos() + size()`.

## Parameters

None.

## Return value

The bytes of the occurrence. The iterator must stand on an occurrence, not at the end.

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
    auto all = txt::fold_matches("Straße, STRASSE", "strasse");
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{} ", it.size());
    }
    println();
}
```

Output:

```text
7 7 
```

## See also

- [pos](pos.md): where it begins
- [sgcl::txt::fold_matches::iterator](../fold_matches-iterator.md)
