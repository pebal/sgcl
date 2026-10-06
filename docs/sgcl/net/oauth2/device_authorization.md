[sgcl](../../README.md) › [net](../README.md) › [oauth2](README.md)

# sgcl::net::oauth2::device_authorization

```cpp
#include "sgcl/net/oauth2/oauth2.h"   // or "sgcl/net/oauth2.h"

namespace sgcl::net::oauth2 {
    struct device_authorization {
        string device_code;
        string user_code;
        string verification_uri;
        string verification_uri_complete;
        time::datetime expiry;
        duration interval = std::chrono::seconds(5);
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What the device authorization endpoint answered (RFC 8628 §3.2), [device_authorize](config/device_authorize.md)'s
result: the device shows its user the code and the URI, and polls with [device_token](config/device_token.md) until the
user approves on another device.

## Member objects

| Member | Description |
|---|---|
| `device_code` | what the device polls with |
| `user_code` | what the user types at the URI, `WDJB-MJHT` |
| `verification_uri` | where the user goes |
| `verification_uri_complete` | the URI with the code in it, for a QR code; `""` when the server sends none |
| `expiry` | when the codes expire (`expires_in` from when they came; 10 minutes when the server says nothing) |
| `interval` | how long the device waits between two polls; 5 seconds when the server says nothing |

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
                R"("verification_uri":"https://as.example/device","interval":2})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.client_id = "tv";
    cfg.endpoints.device_authorization = base + "/device";
    net::oauth2::device_authorization d = cfg.device_authorize().value();
    println("go to {} and type {}", d.verification_uri, d.user_code);
    println("polled every {}", d.interval);
    as.close();
}
```

Output:

```text
go to https://as.example/device and type WDJB-MJHT
polled every 2s
```

## See also

- [config::device_authorize](config/device_authorize.md)
- [config::device_token](config/device_token.md)
