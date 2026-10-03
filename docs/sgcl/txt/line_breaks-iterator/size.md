[sgcl](../../README.md) › [txt](../README.md) › [line_breaks](../line_breaks/README.md) › [iterator](README.md)

# sgcl::txt::line_breaks::iterator::size

```cpp
size_t size() const noexcept;
```

Returns the bytes the iterator's element takes in the text: `(*it).size()`, without making the slice. The element runs
from [pos()](pos.md) to `pos() + size()`.

## Parameters

None.

## Return value

The size of the element in bytes; 0 at [end](../line_breaks/end.md).

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
    string s = "ćma  i żuk";
    txt::line_breaks pieces(s);
    for (auto it = pieces.begin(); it != pieces.end(); ++it) {
        print("{} ", it.size());
    }
    println();
}
```

Output:

```text
6 2 4 
```

## See also

- [pos](pos.md): where the element begins
- [sgcl::txt::line_breaks::iterator](README.md)
