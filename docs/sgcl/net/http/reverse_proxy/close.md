[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [reverse_proxy](README.md)

# sgcl::net::http::reverse_proxy::close

```cpp
void close() const;
```

Stops the active health checks and closes the idle connections of the client to the backends. Requests still go
through the proxy (a new connection made as needed); a backend's health stays as the last check left it. A second
close does nothing more.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the idle connections.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

using namespace std::chrono_literals;

int main() {
    net::http::test_server backend([](net::http::request r, net::http::response_writer w) {
        w.write(r.url().path() == "/health" ? "fine" : "page\n");
    });
    net::http::reverse_proxy::options o;
    o.health_path = "/health";
    o.health_interval = 50ms;
    net::http::reverse_proxy proxy(backend.url(), o);
    proxy.close();
    net::http::response_recorder rec;
    proxy(net::http::test_request("GET", "/"), rec.writer()).wait();
    print("{}", rec.body());
}
```

Output:

```text
page
```

## See also

- [options](../reverse_proxy-options.md): the health checks
- [sgcl::net::http::reverse_proxy](README.md)
