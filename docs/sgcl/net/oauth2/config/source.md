[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [config](README.md)

# sgcl::net::oauth2::config::source

```cpp
token_source source(const token& t) const;    // (1)
token_source source() const;                  // (2)
```

A [token_source](../token_source/README.md) of the config: the tokens it gives are valid, refreshed when they expire.

1. From a token the program has (of [exchange](exchange.md), of [device_token](device_token.md), one it kept): refreshed
   by its refresh token.
2. Of the client's credentials: a new [client_credentials](client_credentials.md) token at the first call and whenever
   it expires.

The source keeps a copy of the config: a change to the config after it does not reach the source.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the first token |

## Return value

The source.

## Complexity

One exchange with the server.

## Exceptions

None: every failure is in the returned error.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/oauth2.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server as;  // the authorization server, here in the program
    as.route("POST /token", [](net::http::request req, net::http::response_writer w) {
        w.write(R"({"access_token":"at-1","token_type":"Bearer",)"
                R"("expires_in":3600,"refresh_token":"rt-1"})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.endpoints.token = base + "/token";
    net::oauth2::token_source tokens = cfg.source();
    println("{}", tokens.token()->access_token);
    as.close();
}
```

Output:

```text
at-1
```

## See also

- [token_source](../token_source/README.md)
- [config](README.md)
