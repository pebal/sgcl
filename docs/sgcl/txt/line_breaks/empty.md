[sgcl](../../README.md) › [txt](../README.md) › [line_breaks](../line_breaks.md)

# sgcl::txt::line_breaks::empty

```cpp
bool empty() const noexcept;
```

Checks whether the text is empty, and so has no pieces: a text of one byte has one.

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
    println("{} {}", txt::line_breaks("").empty(), txt::line_breaks(" ").empty());
}
```

Output:

```text
true false
```

## See also

- [count](count.md): the number of pieces
- [sgcl::txt::line_breaks](../line_breaks.md)
