[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md)

# sgcl::net::smtp::deliver, async_deliver

```cpp
expected<receipt, io::error> deliver(const encoding::email& m, const options& o = {});
async::task<expected<receipt, io::error>> async_deliver(encoding::email m, options o = {}) noexcept;
```

The message to its recipients' own servers, without a server of one's own between: the recipients grouped by
domain, each domain's MX records looked up ([dns::lookup_mx](../dns/lookup_mx.md), the domain as a full name),
its exchangers tried in order of preference on port 25 (`o.port` when set), the next on a failure to connect or a
temporary refusal; the domain itself when it has no MX record (RFC 5321 §5.1), none for a null MX (RFC 7505).
STARTTLS when offered, as [send](send.md) takes it.

What a domain refused, or could not take at all, is a [rejection](rejection.md) of each of its recipients: the
reply, or a reply of code 0 with the error's text when there was none.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the message |
| `o` | the waits, TLS, the resolver (`o.dns`), the port, the stop |

## Return value

The [receipt](receipt.md): the reply of the last domain that took the message, and every recipient refused; or the error of the last domain when none took it.

## Complexity

A lookup a domain, a session an exchanger tried.

## Exceptions

- `deliver`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_deliver`: none.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;


int main() {
    encoding::email m;  // no recipient: nothing looked up
    m.set_from("alice@example.com");
    println("{}", net::smtp::deliver(m).error().message());
}
```

Output:

```text
smtp no recipients: Invalid argument
```

## See also

- [send](send.md)
- [dns::lookup_mx](../dns/lookup_mx.md)
- [smtp](README.md)
