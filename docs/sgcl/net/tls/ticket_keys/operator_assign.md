[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ticket_keys](README.md)

# sgcl::net::tls::ticket_keys::operator=

```cpp
ticket_keys& operator=(const ticket_keys& other) noexcept;    // (1), implicitly declared
ticket_keys& operator=(ticket_keys&& other) noexcept;         // (2), implicitly declared
```

Makes this handle one of the keys `other` holds: a ticket either seals, either opens. The move is the copy: `other`
keeps the keys.

The keys this handle held before are left to the collector when nothing else holds them, and zeroed then; the
tickets sealed under them no longer resume where this handle is used.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose keys this one takes |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config old_cfg, new_cfg;
    new_cfg.ticket_keys = old_cfg.ticket_keys;   // a server's settings changed, its tickets kept
    new_cfg.ticket_lifetime = 2 * hour;
    println("{}", new_cfg.ticket_lifetime);
}
```

Output:

```text
2h0m0s
```

## See also

- [(constructor)](ticket_keys.md): keys made, or a copy
- [sgcl::net::tls::ticket_keys](README.md)
