[sgcl](../../README.md) › [net](../README.md) › [dkim](README.md)

# sgcl::net::dkim::status

```cpp
#include "sgcl/net/dkim.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dkim {
    enum class status : uint8_t { none, pass, fail, policy, neutral, temperror, permerror };
}
```

A signature's verdict, as RFC 8601 §2.7.1 names DKIM's results in an Authentication-Results field;
[to_string](to_string.md) writes it so. [verify](verify.md) gives `pass`, `fail`, `temperror` and `permerror`; `none`
is what a message without a signature reports, `policy` and `neutral` are a receiver's own.

| Value | Description |
|---|---|
| `none` | the message carries no signature |
| `pass` | the signature verified: the domain vouches for the message as it is |
| `fail` | it did not: the head or the body changed after the signing |
| `policy` | it verified, and a local policy would not accept it |
| `neutral` | it could not be processed for a reason of no other value |
| `temperror` | a failure that may pass later: the key's lookup failed |
| `permerror` | one that will not: a malformed signature, no key, a revoked key, an expired signature |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    string m = "DKIM-Signature: v=2; d=example.com; s=s1\r\nFrom: alice@example.com\r\n\r\nHi\r\n";
    auto r = net::dkim::verify(m);
    println("{}", r[0].status == net::dkim::status::permerror);
}
```

Output:

```text
true
```

## See also

- [result](result.md)
- [to_string](to_string.md)
- [dkim](README.md)
