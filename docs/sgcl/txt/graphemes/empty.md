[sgcl](../../README.md) › [txt](../README.md) › [graphemes](README.md)

# sgcl::txt::graphemes::empty

```cpp
bool empty() const noexcept;
```

Checks whether the text is empty, and so has no grapheme clusters: a text of one byte has one.

## Parameters

None.

## Return value

`true` when the text has no bytes.

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
    println("{} {}", txt::graphemes("").empty(), txt::graphemes(" ").empty());
}
```

Output:

```text
true false
```

## See also

- [count](count.md): the number of grapheme clusters
- [sgcl::txt::graphemes](README.md)
