[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md) › sender_checks

# sgcl::net::smtp::sender_checks

```cpp
#include "sgcl/net/smtp/sender_checks.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    struct sender_checks {
        bool spf = true;
        bool dkim = true;
        bool dmarc = true;
        net::dns::options dns;
        string authserv_id;
        bool add_header = true;
        bool reject = false;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::sender_checks` is which checks a receiver makes and what it does with what they find: the
argument of [check_sender](check_sender.md), and the [server](server/README.md)'s `sender_checks`, which turns the
checks on for every message.

## Member objects

| Member | Description |
|---|---|
| `spf`, `dkim`, `dmarc` | the checks made; all `true` by default |
| `dns` | the resolver of every record ([dns::options](../dns-options.md)): its servers empty, `/etc/resolv.conf`'s |
| `authserv_id` | the receiver's name in the Authentication-Results field and SPF's `%{r}`; empty by default: the server's hostname |
| `add_header` | a server puts the field at the head of the message its handler reads, after taking out the fields of the same authserv-id that came with the message (RFC 8601 §5: they are forged); `true` by default |
| `reject` | a server refuses a message whose DMARC fails with the disposition `reject` (550 5.7.1), its handler never called; `false` by default: the handler decides (a quarantine is always the handler's) |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    net::smtp::sender_checks checks;
    checks.authserv_id = "mx.example.org";
    checks.reject = true;
    srv.sender_checks = checks;
    srv.handle([](net::smtp::message m) {
        auto& a = *m.sender_verdict();
        println("dmarc: {}", net::dmarc::to_string(a.dmarc->status));
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::smtp::send(string::concat("smtp://", l.local_endpoint().to_string()),
                    encoding::email("alice@example.net", "bob@example.org", "Hi", "Hello."));
    srv.close();
    serving.wait();
}
```

Sample output:

```text
dmarc: none
```

## See also

- [check_sender](check_sender.md), [server](server/README.md)
- [smtp](README.md)
