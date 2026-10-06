[sgcl](../../README.md) › [net](../README.md) › [spf](README.md) › result

# sgcl::net::spf::result

```cpp
#include "sgcl/net/spf.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::spf {
    struct result {
        spf::status status = spf::status::none;
        string domain;
        string mechanism;
        string explanation;
        string reason;
        size_t lookups = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::spf::result` is a check's outcome, what [check](check.md) gives: the [status](status.md), the domain
checked, the term that decided it, and for a fail the domain's own explanation; what [dmarc::check](../dmarc/check.md)
takes for its alignment, and what Received-SPF and Authentication-Results report.

## Member objects

| Member | Description |
|---|---|
| `status` | the result: `pass`, `fail`, `softfail`, `neutral`, `none`, `temperror`, `permerror` |
| `domain` | the domain checked: the sender's, or the HELO name's for the null sender; in lower case |
| `mechanism` | the term that matched, as the record writes it (`"ip4:192.0.2.0/24"`, `"-all"`, `"include:_spf.example.net"`); `"default"` when none did; empty for `none` and the errors |
| `explanation` | `exp=`'s text for a fail, its macros expanded (`"192.0.2.9 is not one of example.com's designated mail servers."`); empty when the domain gives none |
| `reason` | why a `none`, `temperror` or `permerror` came: `"no SPF record"`, `"more than one SPF record"`, `"too many DNS lookups"`, `"SPF record syntax error"`, ... |
| `lookups` | the terms that asked DNS, of the ten allowed |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::spf::result r = net::spf::check(net::ip_address("192.0.2.25"), "user@single-label", "h");
    println("{}: {}", net::spf::to_string(r.status), r.reason);
}
```

Output:

```text
none: no valid domain
```

## See also

- [check](check.md)
- [status](status.md)
- [spf](README.md)
