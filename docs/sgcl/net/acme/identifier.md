[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::identifier

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct identifier {
        string type;
        string value;

        friend bool operator==(const identifier& a, const identifier& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What a certificate is asked for: a DNS name (RFC 8555 §9.7.7), a wildcard among them, or an IP address (RFC 8738).
The client makes one of each name it is given ([new_order](client/new_order.md)); equal when both fields are.

## Member objects

| Object | Description |
|---|---|
| `type` | `"dns"` or `"ip"` |
| `value` | the name in A-labels and lower case, `"*.example.com"`, or the address as RFC 5952 writes it |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca;
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    net::acme::order o =
        acme.new_order({"Example.COM.", "*.example.com", "2001:DB8::1", "żółw.example"});
    for (auto& id : o.identifiers) {
        println("{} {}", id.type, id.value);
    }
}
```

Output:

```text
dns example.com
dns *.example.com
ip 2001:db8::1
dns xn--w-uga1v8h.example
```

## See also

- [order](order.md)
- [net::acme](README.md)
