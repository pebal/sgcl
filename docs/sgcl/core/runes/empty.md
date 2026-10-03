[sgcl](../../README.md) › [core](../README.md) › [runes](../runes.md)

# sgcl::runes::empty

```cpp
bool empty() const noexcept;
```

Checks whether the text has no bytes, and so no code point.

## Parameters

None.

## Return value

`true` when the text is empty.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string empty, word = "ćma";
    println("{} {}", empty.runes().empty(), word.runes().empty());
}
```

Output:

```text
true false
```

## See also

- [count](count.md): the number of code points
- [sgcl::runes](../runes.md)
