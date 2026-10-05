[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md) › rejection

# sgcl::net::smtp::rejection

```cpp
#include "sgcl/net/smtp/envelope.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    struct rejection {
        string recipient;
        smtp::reply reply;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::rejection` is a recipient a server refused, with its reply; what a [receipt](receipt.md) lists.
[deliver](deliver.md) lists a recipient of a domain it could not reach at all with a reply of code 0 and the
error's text.

## Member objects

| Member | Description |
|---|---|
| `recipient` | the address as RCPT TO gave it |
| `reply` | the [reply](reply/README.md) |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::rejection r{"bob@example.org", net::smtp::reply{550, "5.1.1", "No such user"}};
    println("{}: {}", r.recipient, r.reply.to_string());
}
```

Output:

```text
bob@example.org: 550 5.1.1 No such user
```

## See also

- [receipt](receipt.md)
- [smtp](README.md)
