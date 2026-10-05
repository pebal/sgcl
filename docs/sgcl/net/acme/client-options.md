[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::client::options

```cpp
#include "sgcl/net/acme/client.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    class client {
    public:
        struct options {
            net::http::client http;
            string user_agent;
            string account_url;
            int bad_nonce_retries = 5;
            duration max_retry_after = std::chrono::seconds(60);
            duration poll_interval = std::chrono::seconds(1);
            duration poll_timeout = std::chrono::minutes(3);
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

How a [client](client/README.md) talks to its CA, given to its [constructor](client/client.md): Go's `acme.Client`
fields `HTTPClient`, `UserAgent`, `KID` and `RetryBackoff` as plain values.

## Member objects

| Object | Description |
|---|---|
| `http` | the [http::client](../http/client/README.md) every request goes through: its TLS roots (`http.tls.roots` for a CA of a private PKI), proxy and timeouts; a default client by default |
| `user_agent` | put before `sgcl-acme/1.0` in User-Agent, which RFC 8555 §6.1 asks of every request; empty by default |
| `account_url` | the account's URL when the program knows it, saving the lookup by the key; empty by default |
| `bad_nonce_retries` | how many times a request answered with `badNonce` is sent again; 5 by default |
| `max_retry_after` | the longest Retry-After of a rate limit or a 503 the client waits for before it sends the request again; a longer one is the error. A minute by default; zero waits for none |
| `poll_interval` | the pause between two polls when the CA gives no Retry-After; a second by default |
| `poll_timeout` | how long a wait polls before it ends with `ETIMEDOUT`; three minutes by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.tls = true});   // the API over https, its own CA's certificate
    net::acme::client::options o;
    o.http.tls.roots = ca.roots();
    o.user_agent = "myapp/2.1";
    o.poll_timeout = std::chrono::seconds(30);
    net::acme::client acme(ca.directory_url(), net::acme::account_key(), o);
    println("{}", net::acme::to_string(acme.register_account({.terms_agreed = true})->status));
}
```

Output:

```text
valid
```

## See also

- [client](client/README.md)
- [net::acme](README.md)
