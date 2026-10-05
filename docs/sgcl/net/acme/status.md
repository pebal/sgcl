[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::status

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    enum class status : uint8_t {
        pending,
        ready,
        processing,
        valid,
        invalid,
        revoked,
        deactivated,
        expired,
    };
}
```

The status of an account, an order, an authorization or a challenge (RFC 8555 §7.1.6): one enumeration for all, each
object using the values its state machine has. [to_string](to_string.md) gives the name the RFC writes.

| Value | Description |
|---|---|
| `pending` | an order whose authorizations are not all valid; an authorization or a challenge not yet proved |
| `ready` | an order to be finalized |
| `processing` | an order the CA issues; a challenge the CA validates |
| `valid` | done: an account in use, an order with its certificate, a proved authorization or challenge |
| `invalid` | failed: an order or an authorization of a challenge that failed |
| `revoked` | an account or an authorization the CA revoked |
| `deactivated` | an account or an authorization its holder deactivated |
| `expired` | an authorization past its time |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    using net::acme::status;
    for (auto s : {status::pending, status::valid, status::deactivated}) {
        println("{}", net::acme::to_string(s));
    }
}
```

Output:

```text
pending
valid
deactivated
```

## See also

- [to_string](to_string.md)
- [net::acme](README.md)
