[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [config](README.md)

# sgcl::net::oauth2::config::client_credentials, async_client_credentials

```cpp
expected<token, error> client_credentials() const;                                // (1)
async::task<expected<token, error>> async_client_credentials() const noexcept;    // (2)
```

The client's own token (RFC 6749 §4.4): `grant_type=client_credentials` and the scopes, for a service that acts for
itself rather than for a user. Such a token comes without a refresh token: a [source](source.md) of the client's
credentials asks for a new one when it expires.

- (1) Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Returns a task that does the same: a handler writes `co_await cfg.async_client_credentials(...)`.

## Parameters

None.

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
        w.write(R"({"access_token":"for-)" + req.basic_auth()->user + R"("})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.client_id = "reports";
    cfg.client_secret = "s3cret";
    cfg.endpoints.token = base + "/token";
    println("{}", cfg.client_credentials()->access_token);
    as.close();
}
```

Output:

```text
for-reports
```

## See also

- [source](source.md)
- [client_auth](../client_auth.md)
- [config](README.md)
