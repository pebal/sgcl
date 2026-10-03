[sgcl](../../README.md) › [txt](../README.md) › [word_breaks](../word_breaks.md) › [iterator](../word_breaks-iterator.md)

# sgcl::txt::word_breaks::iterator::size

```cpp
size_t size() const noexcept;
```

Returns the bytes the iterator's element takes in the text: `(*it).size()`, without making the slice. The element runs
from [pos()](pos.md) to `pos() + size()`.

## Parameters

None.

## Return value

The size of the element in bytes; 0 at [end](../word_breaks/end.md).

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
    string s = "ćma, żuk";
    txt::word_breaks parts(s);
    for (auto it = parts.begin(); it != parts.end(); ++it) {
        print("{} ", it.size());
    }
    println();
}
```

Output:

```text
4 1 1 4 
```

## See also

- [pos](pos.md): where the element begins
- [sgcl::txt::word_breaks::iterator](../word_breaks-iterator.md)
