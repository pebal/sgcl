[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](README.md)

# sgcl::net::http::response_writer::header_sent

```cpp
bool header_sent() const noexcept;
```

Checks whether the head of the response has gone: after a [flush](flush.md), from which point the status and the
fields no longer change anything, and [hijack](hijack.md) is refused.

## Parameters

None.

## Return value

`true` when the head was sent, `false` before.

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
    srv.route("GET /", [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.write("before: " + to_string(w.header_sent()) + "\n");
        co_await w.async_flush();
        w.set_status(net::http::status::accepted);  // too late: ignored
        w.write("after: " + to_string(w.header_sent()) + "\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;
    net::http::response res =
        web.get("http://127.0.0.1:" + to_string(incoming.local_endpoint().port()) + "/");
    string text = res.text();
    print("{}\n{}", res.status(), text);
    srv.shutdown();
}
```

Output:

```text
200
before: false
after: true
```

## See also

- [flush, async_flush](flush.md): what sends the head
- [sgcl::net::http::response_writer](README.md)
