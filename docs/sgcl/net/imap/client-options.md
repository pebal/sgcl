[sgcl](../../README.md) › [net](../README.md) › [imap](README.md) › [client](client/README.md) › options

# sgcl::net::imap::client::options

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    class client {
    public:
        struct options {
            string user;
            string password;
            string token;
            imap::mechanism mechanism = imap::mechanism::automatic;
            imap::security security = imap::security::automatic;
            net::tls::config tls;
            duration timeout = std::chrono::seconds(30);
            async::stop_token stop;
            function<async::task<expected<net::connection, io::error>>(const string&, async::stop_token)> dial;
            bool compress = false;
            vector<pair<string, string>> id;
            function<void(const imap::update&)> on_update;
            size_t max_literal_bytes = size_t(256) << 20;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

How a [client](client/README.md) connects and logs in, given to [connect](client/connect.md). A URL's user name and
password fill `user` and `password` when they are empty. With neither a user nor a token the client is connected and
not logged in (a server's PREAUTH greeting logs it in by itself).

## Member objects

| Member | Description |
|---|---|
| `user` | the login; empty by default |
| `password` | LOGIN's or AUTHENTICATE PLAIN's password |
| `token` | an OAuth 2.0 access token, for AUTHENTICATE OAUTHBEARER (RFC 7628) or XOAUTH2 |
| `mechanism` | the SASL mechanism ([mechanism](mechanism.md)); `automatic` by default |
| `security` | TLS from the first byte, STARTTLS or none ([security](security.md)); `automatic` by default: TLS for `imaps://` and port 993, STARTTLS required for the rest |
| `tls` | the [TLS settings](../tls/config.md): roots, a client certificate, versions; the server's name is the address's host when none is set |
| `timeout` | bounds the dial, TLS and the login together, then each command's wait for its answer; 30 s by default, zero none |
| `stop` | stops the dial |
| `dial` | how the connection is made, given `"host:port"` and a token stopped at the timeout; [tcp::connect](../tcp/connect.md) by default |
| `compress` | COMPRESS=DEFLATE (RFC 4978) when the server offers it; `false` by default |
| `id` | the client's ID (RFC 2971), sent when not empty: `{{"name", "my-app"}}`; [server_id](client/server_id.md) has the answer |
| `on_update` | called with every [update](update.md) the client reads (EXISTS, EXPUNGE, FETCH, VANISHED, ALERT, BYE), on the task that read it |
| `max_literal_bytes` | the largest literal taken from the server; 256 MiB by default |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "From: Bob <bob@example.com>\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
    net::imap::server srv;
    srv.backend = mail;
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::imap::security::none;   // the loopback, no TLS
    o.id = {{"name", "docs"}};
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    println("{}", session.server_id()[0].second);
    srv.close();
}
```

Output:

```text
sgcl
```

## See also

- [connect](client/connect.md)
- [security](security.md), [mechanism](mechanism.md)
- [sgcl::net::imap::client](client/README.md)
