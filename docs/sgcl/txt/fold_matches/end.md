[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](README.md)

# sgcl::txt::fold_matches::end

```cpp
iterator end() const noexcept;
```

Returns the iterator past the last occurrence: an iterator walked off the last occurrence compares equal to it.

## Parameters

None.

## Return value

The end iterator.

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
    txt::normalized_matches all("Zoë, Zoe\u0308", "Zoë");
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{}+{} ", it.pos(), it.size());
    }
    println();
}
```

Output:

```text
0+4 6+5 
```

## See also

- [begin](begin.md): an iterator to the first occurrence
- [sgcl::txt::fold_matches, normalized_matches](README.md)
