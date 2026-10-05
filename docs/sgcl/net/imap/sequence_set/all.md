[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::all

```cpp
static sequence_set all() noexcept;
```

Returns `1:*`: every message, every UID.

## Parameters

None.

## Return value

The set.

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
    net::imap::sequence_set every = net::imap::sequence_set::all();
    println("{} {}", every.to_string(), every.expand(3).size());
}
```

Output:

```text
1:* 3
```

## See also

- [sequence_set](README.md)
