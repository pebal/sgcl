[sgcl](../../README.md) › [txt](../README.md) › [graphemes](README.md)

# sgcl::txt::graphemes::begin

```cpp
iterator begin() const noexcept;
```

Returns an [iterator](../graphemes-iterator/README.md) to the first of the grapheme clusters, found: at byte position 0. For
a text with no bytes it equals [end](end.md).

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
    string s = "\U0001F1F5\U0001F1F1 flag";
    auto it = txt::graphemes(s).begin();
    println("[{}] {} bytes", *it, it.size());
}
```

Output:

```text
[🇵🇱] 8 bytes
```

## See also

- [end](end.md): the iterator past the last element
- [sgcl::txt::graphemes](README.md)
