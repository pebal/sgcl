[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md)

# sgcl::net::smtp::check_sender, async_check_sender

```cpp
sender_verdict check_sender(const string& message, const envelope& e, const sender_checks& o = {});
async::task<sender_verdict> async_check_sender(string message, envelope e, sender_checks o = {}) noexcept;
```

The checks of a message's origin as a receiver makes them, on the envelope its server got and the message's bytes:
[SPF](../spf/check.md) of the client's address for MAIL FROM (HELO's name for the null sender),
[DKIM](../dkim/verify.md) of every signature, [DMARC](../dmarc/check.md) of From's domain with what the other two
found. A check the options turn off is nullopt in the [sender_verdict](sender_verdict/README.md); DKIM is verified
for DMARC even when its own result is not wanted. A [server](server/README.md) whose `sender_checks` is set makes
these checks for every message before its handler runs.

From's domain is the one domain of its addresses; a message with no From, with two From fields or with addresses of
two domains is DMARC's `permerror`.

`check_sender` waits on the calling thread; a task awaits `async_check_sender`.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the message's text as it came |
| `e` | the envelope: `client` (the address SPF checks), `from` (MAIL FROM), `helo` |
| `o` | which checks, the resolver, the receiver's name for SPF's `%{r}` |

## Return value

The [sender_verdict](sender_verdict/README.md): each check made, its result.

## Complexity

SPF's lookups, a lookup and a verification for each signature, DMARC's one or two lookups.

## Exceptions

- `check_sender`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_check_sender`: none.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::envelope e;
    e.from = "alice@example.com";
    e.helo = "mail.example.com";
    e.client = net::endpoint(net::ip_address("192.0.2.25"), 41234);
    string m = "From: Alice <alice@example.com>\r\nSubject: Hi\r\n\r\nHello.\r\n";
    net::smtp::sender_verdict a = net::smtp::check_sender(m, e);
    println("{}", a.results("mx.example.org").to_string());
}
```

Sample output:

```text
mx.example.org; spf=fail reason=-all smtp.mailfrom=example.com; dkim=none; dmarc=fail reason="neither SPF nor DKIM passed for an aligned domain" policy.dmarc=reject header.from=example.com
```

## See also

- [sender_verdict](sender_verdict/README.md), [sender_checks](sender_checks.md)
- [server](server/README.md): `sender_checks`, the checks of every message
- [spf](../spf/README.md), [dkim](../dkim/README.md), [dmarc](../dmarc/README.md)
- [smtp](README.md)
