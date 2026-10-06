[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md)

# sgcl::net::oauth2::error

```cpp
#include "sgcl/net/oauth2/oauth2.h"   // or "sgcl/net/oauth2.h"

namespace sgcl::net::oauth2 {
    class error;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

What a call of the module failed with. Either the server's answer (RFC 6749 §5.2): a JSON object whose `error` is the
code (`invalid_grant`, `invalid_client`, `authorization_pending`, `access_denied`, …), with its description, its URI and
the HTTP status it came with; or a failure of the exchange itself — the connection, a stop, a response that is not one of
OAuth — whose [io::error](../../../io/error/README.md) is in [transport](transport.md). Go's `oauth2.RetrieveError`.

## Rules

- A response without an OAuth error object is the code `http_status` and the status as its description; a 2xx whose
  body is not a JSON object is a transport error, `net::errc::malformed_response`.
- A token response without `access_token` is the code `invalid_response`.

## Member functions

| Function | Description |
|---|---|
| [error](error.md) | constructs the error |
| [code](code.md) | the server's code |
| [description](description.md) | its `error_description` |
| [uri](uri.md) | its `error_uri` |
| [status](status.md) | the HTTP status of the response |
| [transport](transport.md) | the failure of the exchange, when it was one |
| [message](message.md) | the error as one line |

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
        w.set_status(400);
        w.write(R"({"error":"invalid_grant","error_description":"the code expired"})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.endpoints.token = base + "/token";
    auto t = cfg.exchange("c-1", net::oauth2::pkce::generate());
    println("{} | {} | {}", t.error().code(), t.error().status(), t.error().message());
    as.close();
}
```

Output:

```text
invalid_grant | 400 | invalid_grant: the code expired
```

## See also

- [config](../config/README.md)
- [io::error](../../../io/error/README.md)
