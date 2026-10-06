[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [config](README.md)

# sgcl::net::oauth2::config::device_authorize, async_device_authorize

```cpp
expected<device_authorization, error> device_authorize() const;                                // (1)
async::task<expected<device_authorization, error>> async_device_authorize() const noexcept;    // (2)
```

The first step of the device authorization grant (RFC 8628 §3.1), for a device without a browser or a keyboard — a
television, a command-line program: the client and its scopes posted to the `device_authorization` endpoint, and the
codes back. The device shows the user code and the URI, and polls with [device_token](device_token.md).

- (1) Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Returns a task that does the same: a handler writes `co_await cfg.async_device_authorize(...)`.

## Parameters

None.

## Return value

The [device_authorization](../device_authorization.md) (its device code, user code and URI all present), or an [error](../error/README.md): the server's (its `error` code, `error_description`, `error_uri`, the status), or one of the exchange in its `transport()` (the connection, a response that is not one of OAuth).

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
    as.route("POST /device", [](net::http::request req, net::http::response_writer w) {
        w.write(R"({"device_code":"d-1","user_code":"WDJB-MJHT",)"
                R"("verification_uri":"https://as.example/device","expires_in":600})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.client_id = "tv";
    cfg.endpoints.device_authorization = base + "/device";
    auto d = cfg.device_authorize();
    println("go to {} and type {}", d->verification_uri, d->user_code);
    as.close();
}
```

Output:

```text
go to https://as.example/device and type WDJB-MJHT
```

## See also

- [device_token](device_token.md)
- [device_authorization](../device_authorization.md)
- [config](README.md)
