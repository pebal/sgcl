[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](../response_writer.md)

# sgcl::net::http::response_writer::status

```cpp
int status() const noexcept;
```

The status of the response: the one [set_status](set_status.md), [error](error.md) or [redirect](redirect.md) set, 200
when none did. A middleware of the program's, a function that wraps a handler, reads it after the handler to log or
count what was answered.

## Parameters

None.

## Return value

The status.

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
    async::channel<int> answered(2);
    net::http::server srv;
    srv.route("GET /{name}", [&](net::http::request req, net::http::response_writer w) {
        if (req.path_value("name") == "missing") {
            w.error(net::http::status::not_found);
        } else {
            w.write("found\n");
        }
        answered.try_send(w.status());
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));
    auto base = "http://127.0.0.1:" + to_string(incoming.local_endpoint().port());

    net::http::client web;
    for (auto path : {"/here", "/missing"}) {
        net::http::response res = web.get(base + path);
        res.text();
        println("{}", answered.receive().wait().value());
    }
    srv.shutdown();
}
```

Output:

```text
200
404
```

## See also

- [set_status](set_status.md): sets it
- [sgcl::net::http::response_writer](../response_writer.md)
