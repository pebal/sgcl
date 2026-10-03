[sgcl](../../README.md) › [txt](../README.md) › [bidi_runs](README.md)

# sgcl::txt::bidi_runs::empty

```cpp
bool empty() const noexcept;
```

Checks whether there is no piece: the text is empty, or every code point of it is one rule X9 removes.

## Parameters

None.

## Return value

`true` when there is no piece to draw.

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
    println("{} {} {}", txt::bidi_runs("").empty(), txt::bidi_runs("\u202E").empty(),
            txt::bidi_runs("a").empty());
}
```

Output:

```text
true true false
```

## See also

- [count](count.md): the number of pieces
- [sgcl::txt::bidi_runs](README.md)
