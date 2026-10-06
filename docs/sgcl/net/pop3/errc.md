[sgcl](../../README.md) › [net](../README.md) › [pop3](README.md)

# sgcl::net::pop3::errc

```cpp
#include "sgcl/net/pop3/error.h"   // or "sgcl/net/pop3.h"

namespace sgcl::net::pop3 {
    enum class errc {
        err = 1,
        malformed_response,
        authentication_failed,
        starttls_unavailable,
        not_supported,
        in_use,
        login_delay,
        sys_temp,
        sys_perm,
        no_such_message
    };
}
```

The failures of POP3 that neither `errno` nor [io](../../io/errc.md) names, in the category `"pop3"` ([category](category.md)); an error's path is the server's text after `-ERR`. A `-ERR` with a response code (RFC 2449 §8, RFC 3206) is the code's value, one without the value of the command.

| Value | Description |
|---|---|
| `err` | "the server refused the command": a -ERR without a code of its own |
| `malformed_response` | "malformed POP3 response": a reply that is neither +OK nor -ERR, a listing that does not read |
| `authentication_failed` | "authentication failed": USER, PASS, APOP or AUTH refused (`[AUTH]`) |
| `starttls_unavailable` | "the server does not offer STLS": TLS required and not offered |
| `not_supported` | "the server does not support the command": TOP, APOP or SASL PLAIN the server lacks |
| `in_use` | "the maildrop is in use": `[IN-USE]`, another session holds it |
| `login_delay` | "logged in too soon": `[LOGIN-DELAY]` |
| `sys_temp` | "temporary server failure": `[SYS/TEMP]` |
| `sys_perm` | "permanent server failure": `[SYS/PERM]` |
| `no_such_message` | "no such message": a number not in the maildrop, or marked deleted |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    error_code e = net::pop3::errc::in_use;
    println("{}: {}", e.category().name(), e.message());
}
```

Output:

```text
pop3: the maildrop is in use
```

## See also

- [category](category.md), [make_error_code](make_error_code.md)
- [pop3](README.md)
