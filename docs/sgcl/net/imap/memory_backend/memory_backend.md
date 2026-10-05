[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::memory_backend

```cpp
memory_backend();                                         // (1)
memory_backend(const memory_backend& other) = default;    // (2)
```

1. Constructs an empty store: no users.
2. The same store as `other`. A move is the copy.

## Parameters

| Parameter | Description |
|---|---|
| `other` | another store |

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
    net::imap::memory_backend mail;
    net::imap::memory_backend same = mail;
    same.add_user("alice", "secret");
    println("{}", mail.authenticate("alice", "secret"));
}
```

Output:

```text
true
```

## See also

- [add_user](add_user.md)
- [memory_backend](README.md)
