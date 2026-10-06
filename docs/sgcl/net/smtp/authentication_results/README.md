[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md)

# sgcl::net::smtp::authentication_results

```cpp
#include "sgcl/net/smtp/sender_checks.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    struct authentication_results {
        struct method_result;
        string authserv_id;
        vector<method_result> results;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::authentication_results` is an Authentication-Results field's value (RFC 8601): the receiver that
checked a message — its authserv-id — and a [result](../authentication_results-method_result.md) per method it ran,
each with its reason and properties. A receiver writes one at the head of a message
([sender_verdict::results](../sender_verdict/results.md), a [server](../server/README.md) with `sender_checks`
set), a reader of the message reads what its own receiver found with [parse](parse.md) — trusting only the field of
its receiver's id, which that receiver strips from what comes in.

## Member objects

| Member | Description |
|---|---|
| `authserv_id` | the receiver's name |
| `results` | a [method_result](../authentication_results-method_result.md) per method, in the field's order; empty: the field says `none` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](authentication_results.md) | empty, or of a literal's text |
| [parse](parse.md) | the value of a field |
| [to_string](to_string.md) | the value on one line |
| [operator==](operator_cmp.md) | whether two values say the same |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto m = encoding::email::parse("Authentication-Results: mx.example.org; dkim=pass header.d=example.com;\r\n"
                                    "  dmarc=pass header.from=example.com\r\nFrom: a@example.com\r\n\r\nHi\r\n");
    auto ar = net::smtp::authentication_results::parse(m->header("Authentication-Results"));
    for (auto& r : ar->results) {
        println("{}={} ({} properties)", r.method, r.result, r.properties.size());
    }
}
```

Output:

```text
dkim=pass (1 properties)
dmarc=pass (1 properties)
```

## See also

- [sender_verdict](../sender_verdict/README.md)
- [smtp](../README.md)
