[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md)

# sgcl::net::acme::problem

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct problem {
        string type;
        string detail;
        int status = 0;
        string instance;
        optional<acme::identifier> identifier;
        vector<subproblem> subproblems;

        errc code() const noexcept;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

A problem document (RFC 7807, RFC 8555 §6.7) as an order, an authorization or a challenge holds it: why it is invalid.
The CA's answer to a request that failed is the same document, made into an [io::error](../../../io/error/README.md)
of the category `"acme"` by the client; `code()` gives its type as that error's code. Go's `acme.Error`.

## Member objects

| Object | Description |
|---|---|
| `type` | the type, `urn:ietf:params:acme:error:…`; `"about:blank"` when the CA gave none |
| `detail` | the text for a human |
| `status` | the HTTP status, 0 when absent |
| `instance` | a URL to visit (`userActionRequired`) |
| `identifier` | the identifier it is of, when it is of one |
| `subproblems` | the errors of a compound problem, each of one identifier ([subproblem](../subproblem.md)) |

## Member functions

| Function | Description |
|---|---|
| [code](code.md) | the type as a code of the acme category |

## See also

- [errc](../errc.md)
- [subproblem](../subproblem.md)
- [net::acme](../README.md)
