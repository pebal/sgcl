[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::empty

```cpp
bool empty() const noexcept;
```

Returns whether the set holds nothing (`$` is not empty).

## Parameters

None.

## Return value

`true` when empty.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    println("{} {}", net::imap::sequence_set().empty(), net::imap::sequence_set(1).empty());
}
```

Output:

```text
true false
```

## See also

- [sequence_set](README.md)
