[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [config](README.md)

# sgcl::net::oauth2::config::exchange, async_exchange

```cpp
expected<token, error> exchange(const string& code, const pkce& p) const;                  // (1)
async::task<expected<token, error>> async_exchange(string code, pkce p) const noexcept;    // (2)
```

The code the redirect brought exchanged for a token (RFC 6749 §4.1.3): `grant_type=authorization_code`, the code, the
redirect, and the PKCE verifier the server checks against the challenge of the
[authorization_url](authorization_url.md). A code is good once: the server refuses it again with `invalid_grant`.

- (1) Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Returns a task that does the same: a handler writes `co_await cfg.async_exchange(...)`.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the `code` of the redirect |
| `p` | the PKCE pair the authorization URL was made with |

## Return value

The [token](../token/README.md), or an [error](../error/README.md): the server's (its `error` code, `error_description`, `error_uri`, the status), or one of the exchange in its `transport()` (the connection, a response that is not one of OAuth).

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
    cfg.client_id = "photos";
    cfg.client_secret = "s3cret";
    cfg.endpoints.token = base + "/token";
    cfg.redirect_url = "http://127.0.0.1:8080/callback";
    net::oauth2::pkce p = net::oauth2::pkce::generate();  // the one of the authorization URL
    auto t = cfg.exchange("code-of-the-redirect", p);
    println("{} {}", t->access_token, t->refresh_token);
    as.close();
}
```

Output:

```text
at-1 rt-1
```

## See also

- [authorization_url](authorization_url.md)
- [source](source.md)
- [config](README.md)
