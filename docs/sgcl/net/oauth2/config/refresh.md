[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [config](README.md)

# sgcl::net::oauth2::config::refresh, async_refresh

```cpp
expected<token, error> refresh(const token& t) const;                         // (1)
async::task<expected<token, error>> async_refresh(token t) const noexcept;    // (2)
```

A new access token of the token's refresh token (RFC 6749 §6): `grant_type=refresh_token`. A server that rotates refresh
tokens sends a new one, and the old one is refused from then on; one that does not sends none, and the token returned
keeps the old one. A token without a refresh token is `invalid_grant` at once, nothing sent. A
[token_source](../token_source/README.md) calls it by itself.

- (1) Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Returns a task that does the same: a handler writes `co_await cfg.async_refresh(...)`.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the token whose refresh token is used |

## Return value

The new [token](../token/README.md), or an [error](../error/README.md): the server's (its `error` code, `error_description`, `error_uri`, the status), or one of the exchange in its `transport()` (the connection, a response that is not one of OAuth).

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
        w.write(R"({"access_token":"at-2","expires_in":3600})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.endpoints.token = base + "/token";
    net::oauth2::token old;
    old.access_token = "at-1";
    old.refresh_token = "rt-1";
    auto t = cfg.refresh(old);
    println("{} {}", t->access_token, t->refresh_token);
    println("{}", cfg.refresh(net::oauth2::token()).error().code());
    as.close();
}
```

Output:

```text
at-2 rt-1
invalid_grant
```

## See also

- [token_source](../token_source/README.md)
- [config](README.md)
