[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::criteria

```cpp
criteria() noexcept;                          // (1)
criteria(const criteria& other) = default;    // (2)
```

1. Every message (ALL).
2. The same keys as `other`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | other criteria |

## Return value

None.

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
    net::imap::criteria every;
    println("{}", every.to_string());
}
```

Output:

```text
ALL
```

## See also

- [all](all.md)
- [sgcl::net::imap::criteria](README.md)
