[sgcl](../../README.md) › [txt](../README.md) › [word_breaks](README.md)

# sgcl::txt::word_breaks::begin

```cpp
iterator begin() const noexcept;
```

Returns an [iterator](../word_breaks-iterator/README.md) to the first of the segments, found: at byte position 0. For a text
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
    auto it = txt::word_breaks("192.168.0.1 is a host").begin();
    println("[{}]", *it);
}
```

Output:

```text
[192.168.0.1]
```

## See also

- [end](end.md): the iterator past the last element
- [sgcl::txt::word_breaks](README.md)
