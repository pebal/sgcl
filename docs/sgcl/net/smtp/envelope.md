[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md) › envelope

# sgcl::net::smtp::envelope

```cpp
#include "sgcl/net/smtp/envelope.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    struct envelope {
        string from;
        vector<string> to;
        string ret;
        string envid;
        vector<string> notify;
        vector<string> orcpt;
        uint64_t size = 0;
        string body;
        bool smtputf8 = false;
        endpoint client;
        string helo;
        bool tls = false;
        string user;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::envelope` is the envelope of a message (RFC 5321 §2.3.1): the reverse path, the recipients and
the parameters of MAIL and RCPT. A client sends it ([client::send](client/send.md); [send](send.md) makes it of the
message's From, To, Cc and Bcc); a server fills all of it for its handler ([message::envelope](message/envelope.md))
and for its callbacks, with what it learned of the session. One type on both sides, so that what a server took a
client can pass on as it came. A plain struct, its fields set by name.

## Member objects

| Member | Description |
|---|---|
| `from` | MAIL FROM's address; `""` for the null path `<>` of a bounce |
| `to` | RCPT TO's addresses, in their order |
| `ret` | DSN (RFC 3461): `"FULL"` or `"HDRS"`, what a notification returns; `""` for none |
| `envid` | DSN: the envelope's id, sent as xtext; `""` for none |
| `notify` | DSN: per recipient of `to`, `"SUCCESS,FAILURE,DELAY"` or `"NEVER"`; one value for every recipient; a server leaves it empty when no RCPT had one |
| `orcpt` | DSN: per recipient of `to`, the original recipient, `"rfc822;bob@example.org"` (`rfc822;` added to an address alone) |
| `size` | a server's: MAIL's SIZE (RFC 1870); 0 when none was declared |
| `body` | a server's: MAIL's BODY, `"7BIT"`, `"8BITMIME"`, `"BINARYMIME"`, or `""` |
| `smtputf8` | a server's: whether MAIL said SMTPUTF8 (RFC 6531) |
| `client` | a server's: the client's address |
| `helo` | a server's: the name of the client's EHLO or HELO |
| `tls` | a server's: whether the session runs over TLS |
| `user` | a server's: the user AUTH authenticated; `""` for none |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    srv.handle([](net::smtp::message m) {
        const net::smtp::envelope& e = m.envelope();
        println("{} -> {} recipients, RET={} NOTIFY={}", e.from, e.to.size(), e.ret, e.notify[0]);
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    net::smtp::client c = net::smtp::client::connect(url);

    net::smtp::envelope e;
    e.from = "bounces@example.com";
    e.to = {"bob@example.org", "carol@example.net"};
    e.ret = "HDRS";
    e.notify = {"FAILURE"};
    c.send(e, "Subject: hi\r\n\r\nHello.\r\n");
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
bounces@example.com -> 2 recipients, RET=HDRS NOTIFY=FAILURE
```

## See also

- [client::send](client/send.md)
- [message::envelope](message/envelope.md)
- [smtp](README.md)
