[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [config](README.md)

# sgcl::net::oauth2::config::device_token, async_device_token

```cpp
expected<token, error> device_token(const device_authorization& d,                                     // (1)
                                    const async::stop_token& stop = {}) const;
async::task<expected<token, error>> async_device_token(device_authorization d,                         // (2)
                                                       async::stop_token stop = {}) const noexcept;
```

The token of the device flow, polled for (RFC 8628 §3.4, §3.5): the device code posted to the token endpoint every
`interval` until the user approves on another device. `authorization_pending` polls again, `slow_down` adds five
seconds to the interval; the user's refusal ends it with `access_denied`, the codes' expiry with `expired_token`, the
stop with `operation_canceled` in the error's `transport()` (within a tenth of a second of it).

- (1) Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Returns a task that does the same: a handler writes `co_await cfg.async_device_token(...)`.

## Parameters

| Parameter | Description |
|---|---|
| `d` | what [device_authorize](device_authorize.md) gave |
| `stop` | ends the polling (the user gave up) |

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
    as.route("POST /device", [](net::http::request req, net::http::response_writer w) {
        w.write(R"({"device_code":"d-1","user_code":"WDJB-MJHT",)"
                R"("verification_uri":"https://as.example/device","interval":1})");
    });
    as.route("POST /token", [](net::http::request req, net::http::response_writer w) {
        w.write(R"({"access_token":"at-1","token_type":"Bearer",)"
                R"("expires_in":3600,"refresh_token":"rt-1"})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.client_id = "tv";
    cfg.endpoints.device_authorization = base + "/device";
    cfg.endpoints.token = base + "/token";
    auto d = cfg.device_authorize();
    auto t = cfg.device_token(*d);  // a second later, the server approves at once
    println("{}", t->access_token);
    as.close();
}
```

Output:

```text
at-1
```

## See also

- [device_authorize](device_authorize.md)
- [config](README.md)
