[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md)

# sgcl::net::smtp::reply

```cpp
#include "sgcl/net/smtp/envelope.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    struct reply {
        int code = 0;
        string enhanced;
        string text;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::reply` is a reply of an SMTP server (RFC 5321 §4.2): the three digits, the enhanced status code
of RFC 3463 (`"5.1.1"`) when the server gives one, the text of its lines without the codes, joined by `"\n"`. What
a [receipt](../receipt.md) and a [rejection](../rejection.md) hold and [reply_of](../reply_of.md) reads out of an
error; and what a [server](../server/README.md)'s handler and callbacks answer with, where a code of 0 stands for
the server's own acceptance.

## Member objects

| Member | Description |
|---|---|
| `code` | the code, 200 to 599; 0 for none |
| `enhanced` | the enhanced code, `"class.subject.detail"`; `""` when the server gave none |
| `text` | the text, a line's after `"\n"` |

## Member functions

| Function | Description |
|---|---|
| [to_string](to_string.md) | `550 5.1.1 No such user` |
| [positive](positive.md) | whether the code is 2xx or 3xx |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two replies are the same |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::reply r{451, "4.3.0", "Try again later\nMailbox busy"};
    println("{} {}", r.to_string(), r.positive());
}
```

Output:

```text
451 4.3.0 Try again later; Mailbox busy false
```

## See also

- [reply_of](../reply_of.md)
- [smtp](../README.md)
