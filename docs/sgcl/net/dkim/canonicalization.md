[sgcl](../../README.md) › [net](../README.md) › [dkim](README.md)

# sgcl::net::dkim::canonicalization

```cpp
#include "sgcl/net/dkim.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dkim {
    enum class canonicalization : uint8_t { simple, relaxed };
}
```

How the head and the body are made into the bytes a signature covers (RFC 6376 §3.4), `c=` of a signature, chosen
for each apart by [sign_options](sign_options.md). `relaxed` survives what servers on the way do to whitespace and
the case of names; `simple` survives nothing.

| Value | Description |
|---|---|
| `simple` | the head's fields as they are; the body as it is, its empty lines at the end taken off |
| `relaxed` | names in lower case, values unfolded, runs of whitespace one space, whitespace at line ends taken off; the body's empty lines at the end taken off |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto s = net::dkim::signer::generate("example.com", "s1", net::dkim::algorithm::ed25519_sha256);
    net::dkim::sign_options o;
    o.header = net::dkim::canonicalization::simple;
    string m = s.sign("From: alice@example.com\r\n\r\nHi\r\n", o).value();
    println("{}", m.contains("c=simple/relaxed;"));
}
```

Output:

```text
true
```

## See also

- [sign_options](sign_options.md)
- [dkim](README.md)
