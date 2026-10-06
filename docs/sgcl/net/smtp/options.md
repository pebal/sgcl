[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md) › options

# sgcl::net::smtp::options

```cpp
#include "sgcl/net/smtp/envelope.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    struct options {
        string username;
        string password;
        string oauth_token;
        string auth;
        string hostname;
        tls::config tls;
        bool starttls = true;
        bool require_tls = false;
        bool allow_insecure_auth = false;
        duration connect_timeout = 30 * second;
        duration timeout = duration::zero();
        uint16_t port = 0;
        net::dns::options dns;
        optional<dkim::signer> dkim;
        dkim::sign_options dkim_options;
        async::stop_token stop;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::options` is how a client talks to its server: the credentials, TLS, the waits, the stop. A plain
struct, its fields set by name; the forms without it take its defaults: no credentials beyond the URL's,
STARTTLS when offered, the waits of RFC 5321.

## Member objects

| Member | Description |
|---|---|
| `username`, `password` | the credentials of AUTH; empty: the URL's, or no AUTH |
| `oauth_token` | a bearer token for XOAUTH2 (RFC 7628's form), with `username` |
| `auth` | the mechanism to use, `"PLAIN"`, `"LOGIN"` or `"XOAUTH2"`; empty: XOAUTH2 with a token, else PLAIN, else LOGIN, as the server offers |
| `hostname` | the name EHLO gives; empty: this host's name when it is a domain, else the connection's address as a literal (`[127.0.0.1]`) |
| `tls` | the [tls::config](../tls/config.md) of STARTTLS and `smtps://`: the roots, a client certificate, a session cache; the server's name from the URL when it has none |
| `starttls` | upgrade a plain connection when the server offers STARTTLS; `true` by default |
| `require_tls` | fail with `errc::smtp_tls_required` rather than go on in clear text; `false` by default |
| `allow_insecure_auth` | AUTH over a connection without TLS; `false` by default |
| `connect_timeout` | the dial; 30 s by default |
| `timeout` | every wait for the server; zero by default: RFC 5321 §4.5.3.2's (5 minutes for the greeting, MAIL and RCPT, 2 for DATA, 3 for each block of data, 10 for its end) |
| `port` | [deliver](deliver.md)'s port of the exchangers; zero: 25 |
| `dns` | [deliver](deliver.md)'s [resolver](../dns-options.md) of MX records |
| `dkim` | a [dkim::signer](../dkim/signer/README.md): every message the client sends signed with it before DATA ([dkim::sign](../dkim/signer/sign.md)); none by default |
| `dkim_options` | how: the [sign_options](../dkim/sign_options.md); relaxed both ways, the recommended fields, by default |
| `stop` | ends any wait with `ECANCELED`, the connection closed |

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
    srv.hostname = "mx.example";
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());

    net::smtp::options o;
    o.require_tls = true;
    auto r = net::smtp::client::connect(url, o);
    println("{}", r.error().message());
    srv.close();
    serving.wait();
}
```

Output:

```text
smtp STARTTLS the server offers no STARTTLS: TLS required but not available
```

## See also

- [send](send.md), [client::connect](client/connect.md), [deliver](deliver.md): what take them
- [smtp](README.md)
