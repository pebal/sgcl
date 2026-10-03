[sgcl](../../README.md) › [txt](../README.md) › [graphemes](../graphemes/README.md) › [iterator](README.md)

# sgcl::txt::graphemes::iterator::size

```cpp
size_t size() const noexcept;
```

Returns the bytes the iterator's element takes in the text: `(*it).size()`, without making the slice. The element runs
from [pos()](pos.md) to `pos() + size()`.

## Parameters

None.

## Return value

The size of the element in bytes; 0 at [end](../graphemes/end.md).

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
    string s = "a\U0001F1F5\U0001F1F1e\u0301";
    txt::graphemes all(s);
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{} ", it.size());
    }
    println();
}
```

Output:

```text
1 8 3 
```

## See also

- [pos](pos.md): where the element begins
- [sgcl::txt::graphemes::iterator](README.md)
