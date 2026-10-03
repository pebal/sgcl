[sgcl](../../README.md) › [txt](../README.md) › [line_breaks](../line_breaks.md)

# sgcl::txt::line_breaks::begin

```cpp
iterator begin() const noexcept;
```

Returns an [iterator](../line_breaks-iterator.md) to the first of the pieces, found: at byte position 0. For a text
with no bytes it equals [end](end.md).

## Parameters

None.

## Return value

An iterator to the first element.

## Complexity

Linear in the bytes of the first element.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto it = txt::line_breaks("(a) b").begin();
    println("[{}]", *it);
}
```

Output:

```text
[(a) ]
```

## See also

- [end](end.md): the iterator past the last element
- [sgcl::txt::line_breaks](../line_breaks.md)
