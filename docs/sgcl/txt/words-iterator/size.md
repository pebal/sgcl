[sgcl](../../README.md) › [txt](../README.md) › [words](../words/README.md) › [iterator](README.md)

# sgcl::txt::words::iterator::size

```cpp
size_t size() const noexcept;
```

Returns the bytes the iterator's element takes in the text: `(*it).size()`, without making the slice. The element runs
from [pos()](pos.md) to `pos() + size()`.

## Parameters

None.

## Return value

The size of the element in bytes; 0 at [end](../words/end.md).

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
    string s = "żółw i ćma";
    txt::words all(s);
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{} ", it.size());
    }
    println();
}
```

Output:

```text
7 1 4 
```

## See also

- [pos](pos.md): where the element begins
- [sgcl::txt::words::iterator](README.md)
