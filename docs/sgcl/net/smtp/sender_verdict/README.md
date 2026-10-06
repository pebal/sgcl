[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md)

# sgcl::net::smtp::sender_verdict

```cpp
#include "sgcl/net/smtp/sender_checks.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    struct sender_verdict {
        optional<spf::result> spf;
        optional<vector<dkim::result>> dkim;
        optional<dmarc::result> dmarc;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::sender_verdict` is what the checks of one message found: [SPF](../../spf/result.md) of its
envelope, [DKIM](../../dkim/result.md) of each signature, [DMARC](../../dmarc/result.md) of its From; nullopt for a
check not made. [check_sender](../check_sender.md) makes it; a [server](../server/README.md) with `sender_checks`
set gives it to its handler as the [message](../message/README.md)'s [sender_verdict](../message/sender_verdict.md);
[results](results.md) writes it as an Authentication-Results field.

## Member objects

| Member | Description |
|---|---|
| `spf` | SPF's [result](../../spf/result.md); nullopt when not checked |
| `dkim` | a [result](../../dkim/result.md) per signature, empty for a message without one; nullopt when not checked |
| `dmarc` | DMARC's [result](../../dmarc/result.md); nullopt when not checked |

## Member functions

| Function | Description |
|---|---|
| [results](results.md) | the results as an Authentication-Results field |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::smtp::sender_verdict a;
    a.dkim = vector<net::dkim::result>();
    println("{} {}", a.spf.has_value(), a.dkim->size());
    println("{}", a.results("mx.example.org").to_string());
}
```

Output:

```text
false 0
mx.example.org; dkim=none
```

## See also

- [check_sender](../check_sender.md)
- [authentication_results](../authentication_results/README.md)
- [smtp](../README.md)
