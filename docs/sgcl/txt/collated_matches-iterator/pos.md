[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](../collated_matches/README.md) › [iterator](README.md)

# sgcl::txt::collated_matches::iterator::pos

```cpp
size_t pos() const noexcept;
```

Returns the byte position in the text where the occurrence the iterator stands on begins: the start of the
combining sequence the match begins with, in the text as it was given.

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
    txt::collator primary{txt::strength::primary};
    auto all = txt::collated_matches(primary, "Żółw, ŻÓŁW i zolw", "zolw");
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
- [sgcl::txt::collated_matches::iterator](README.md)
