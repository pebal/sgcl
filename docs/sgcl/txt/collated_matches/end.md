[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](README.md)

# sgcl::txt::collated_matches::end

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
    txt::collator czech(txt::locale("cs"), txt::strength::primary);
    txt::collated_matches all(czech, "chata, CHATA, hrad", "chata");
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{}+{} ", it.pos(), it.size());
    }
    println();
}
```

Output:

```text
0+5 7+5 
```

## See also

- [begin](begin.md): an iterator to the first occurrence
- [sgcl::txt::collated_matches](README.md)
