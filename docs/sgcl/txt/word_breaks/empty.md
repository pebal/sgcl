[sgcl](../../README.md) › [txt](../README.md) › [word_breaks](../word_breaks.md)

# sgcl::txt::word_breaks::empty

```cpp
bool empty() const noexcept;
```

Checks whether the text is empty, and so has no segments: a text of one byte has one.

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
    println("{} {}", txt::word_breaks("").empty(), txt::word_breaks(", ").empty());
}
```

Output:

```text
true false
```

## See also

- [count](count.md): the number of segments
- [sgcl::txt::word_breaks](../word_breaks.md)
