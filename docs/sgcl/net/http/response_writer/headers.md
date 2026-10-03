[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](../response_writer.md)

# sgcl::net::http::response_writer::headers

```cpp
http::headers& headers() const noexcept;
```

The fields of the response, Go's `w.Header()`: the list [set_header](set_header.md), [add_header](add_header.md) and
[add_cookie](add_cookie.md) change, given by reference for what they do not do — a field erased, a field read back,
a [date](../headers/set_date.md) set. Changed after the head has gone, the fields go nowhere.

## Parameters

None.

## Return value

A reference to the fields, valid while the writer is.

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
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.set_header("X-Debug", "on").set_header("Content-Type", "text/plain");
        w.headers().erase("X-Debug");
        w.write("fields: " + to_string(w.headers().size()) + "\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;
    net::http::response res =
        web.get("http://127.0.0.1:" + to_string(incoming.local_endpoint().port()) + "/");
    string text = res.text();
    println("{}{}", text, res.headers().contains("X-Debug"));
    srv.shutdown();
}
```

Output:

```text
fields: 1
false
```

## See also

- [headers](../headers.md): the class of the list
- [sgcl::net::http::response_writer](../response_writer.md)
