[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md) › [authentication_results](authentication_results/README.md) › method_result

# sgcl::net::smtp::authentication_results::method_result

```cpp
#include "sgcl/net/smtp/authentication.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    struct authentication_results {
        struct method_result {
            string method;
            string result;
            string reason;
            vector<pair<string, string>> properties;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::authentication_results::method_result` is one method's result in an
[Authentication-Results](authentication_results/README.md) field: `dkim=pass reason="..." header.d=example.com`.

## Member objects

| Member | Description |
|---|---|
| `method` | the method: `"spf"`, `"dkim"`, `"dmarc"`, `"auth"`, `"arc"`, ... |
| `result` | its result: `"pass"`, `"fail"`, `"none"`, `"softfail"`, `"temperror"`, ... |
| `reason` | `reason=`; empty for none |
| `properties` | `ptype.property` and its value, in their order: `("smtp.mailfrom", "example.com")`, `("header.d", "example.com")` |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::smtp::authentication_results ar("mx; auth=pass smtp.auth=alice@example.com");
    net::smtp::authentication_results::method_result r = ar.results[0];
    println("{} {} {}={}", r.method, r.result, r.properties[0].first, r.properties[0].second);
}
```

Output:

```text
auth pass smtp.auth=alice@example.com
```

## See also

- [authentication_results](authentication_results/README.md)
- [smtp](README.md)
