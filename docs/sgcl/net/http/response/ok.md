[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](README.md)

# sgcl::net::http::response::ok

```cpp
bool ok() const noexcept;
```

Checks whether the status is a success, 200 to 299. A 4xx or a 5xx is a response, not an error of the send, so a
program that wants only a success asks this, as [download](../client/download.md) does.

## Parameters

None.

## Return value

`true` for a status from 200 to 299.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /here", [](net::http::request, net::http::response_writer w) {
        w.write("here\n");
    });
    srv.route("GET /broken", [](net::http::request, net::http::response_writer w) {
        w.error(net::http::status::service_unavailable);
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    for (string path : {"/here", "/broken", "/missing"}) {
        net::http::response res = web.get(base + path);
        println("{} {}", res.status(), res.ok());
        res.close();
    }
    srv.close();
}
```

Output:

```text
200 true
503 false
404 false
```

## See also

- [status](status.md): the code itself
- [sgcl::net::http::response](README.md)
