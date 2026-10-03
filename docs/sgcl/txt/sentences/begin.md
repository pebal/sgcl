[sgcl](../../README.md) › [txt](../README.md) › [sentences](README.md)

# sgcl::txt::sentences::begin

```cpp
iterator begin() const noexcept;
```

Returns an [iterator](../sentences-iterator/README.md) to the first of the sentences, found: at byte position 0. For a text
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
    auto it = txt::sentences("U.S.A. is far. Poland is near.").begin();
    println("[{}]", *it);
}
```

Output:

```text
[U.S.A. is far. ]
```

## See also

- [end](end.md): the iterator past the last element
- [sgcl::txt::sentences](README.md)
