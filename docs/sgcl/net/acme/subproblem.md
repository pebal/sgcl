[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::subproblem

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct subproblem {
        string type;
        string detail;
        acme::identifier identifier;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

One error of a compound problem (RFC 8555 §6.7.1): what failed for one identifier of an order of several. The client
puts every subproblem's detail, with its identifier, into the text of the error it makes of a problem.

## Member objects

| Object | Description |
|---|---|
| `type` | the type, `urn:ietf:params:acme:error:…` |
| `detail` | the text for a human |
| `identifier` | the identifier it is of |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::problem p;
    p.type = "urn:ietf:params:acme:error:compound";
    p.subproblems.push_back({"urn:ietf:params:acme:error:rejectedIdentifier", "a forbidden name",
                             {"dns", "example.net"}});
    println("{} {}", p.subproblems[0].identifier.value, p.subproblems[0].detail);
}
```

Output:

```text
example.net a forbidden name
```

## See also

- [problem](problem/README.md)
- [net::acme](README.md)
