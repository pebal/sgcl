[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [config](README.md)

# sgcl::net::oauth2::config::introspect, async_introspect

```cpp
expected<introspection, error> introspect(const string& tok) const;                         // (1)
async::task<expected<introspection, error>> async_introspect(string tok) const noexcept;    // (2)
```

What the server says of a token (RFC 7662): posted to the `introspection` endpoint, which a resource server asks about
the tokens it receives. A token the server does not know is `active` false, not an error.

- (1) Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Returns a task that does the same: a handler writes `co_await cfg.async_introspect(...)`.

## Parameters

| Parameter | Description |
|---|---|
| `tok` | the token |

## Return value

The [introspection](../introspection.md), or an [error](../error/README.md): the server's (its `error` code, `error_description`, `error_uri`, the status), or one of the exchange in its `transport()` (the connection, a response that is not one of OAuth).

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
    as.route("POST /introspect", [](net::http::request req, net::http::response_writer w) {
        w.write(R"({"active":true,"client_id":"reports","sub":"ann","exp":2000000000})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.endpoints.introspection = base + "/introspect";
    auto info = cfg.introspect("at-1");
    println("{} {} {}", info->active, info->client_id, info->expiry->unix());
    as.close();
}
```

Output:

```text
true reports 2000000000
```

## See also

- [introspection](../introspection.md)
- [revoke](revoke.md)
- [config](README.md)
