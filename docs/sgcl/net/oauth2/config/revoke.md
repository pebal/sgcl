[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [config](README.md)

# sgcl::net::oauth2::config::revoke, async_revoke

```cpp
expected<void, error> revoke(const string& tok, const string& hint = {}) const;                  // (1)
async::task<expected<void, error>> async_revoke(string tok, string hint = {}) const noexcept;    // (2)
```

A token revoked (RFC 7009 §2.1): posted to the `revocation` endpoint with the hint of its kind. A server answers 200
for a token it does not know too: after the call the token is of no use, whatever it was. Revoking a refresh token
revokes the access tokens it gave, at most servers.

- (1) Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Returns a task that does the same: a handler writes `co_await cfg.async_revoke(...)`.

## Parameters

| Parameter | Description |
|---|---|
| `tok` | the access token or the refresh token |
| `hint` | `"access_token"` or `"refresh_token"`; none by default |

## Return value

Nothing, or an [error](../error/README.md): the server's (its `error` code, `error_description`, `error_uri`, the status), or one of the exchange in its `transport()` (the connection, a response that is not one of OAuth).

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
    as.route("POST /revoke", [](net::http::request req, net::http::response_writer w) {
        w.write(R"()");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.endpoints.revocation = base + "/revoke";
    println("{}", cfg.revoke("rt-1", "refresh_token").has_value());
    as.close();
}
```

Output:

```text
true
```

## See also

- [introspect](introspect.md)
- [config](README.md)
