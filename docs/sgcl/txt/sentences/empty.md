[sgcl](../../README.md) › [txt](../README.md) › [sentences](../sentences.md)

# sgcl::txt::sentences::empty

```cpp
bool empty() const noexcept;
```

Checks whether the text is empty, and so has no sentences: a text of one byte has one.

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
    println("{} {}", txt::sentences("").empty(), txt::sentences(".").empty());
}
```

Output:

```text
true false
```

## See also

- [count](count.md): the number of sentences
- [sgcl::txt::sentences](../sentences.md)
