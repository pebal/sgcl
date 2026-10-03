[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](../collated_matches/README.md) › [iterator](README.md)

# sgcl::txt::collated_matches::iterator::size

```cpp
size_t size() const noexcept;
```

Returns the number of bytes of the text the occurrence covers, to the end of the combining sequence it ends with:
the text's own count, which need not be the pattern's — an accent takes bytes the pattern never had.

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
    txt::collator primary{txt::strength::primary};
    auto all = txt::collated_matches(primary, "resume résumé resume\u0301", "resume");
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{} ", it.size());
    }
    println();
}
```

Output:

```text
6 8 8 
```

## See also

- [pos](pos.md): where it begins
- [sgcl::txt::collated_matches::iterator](README.md)
