[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::headers

```cpp
http::headers& headers() const noexcept;
```

Returns the fields of the request by reference, Go's `r.Header`: the [headers](../headers/README.md) the request holds, to
read every field or to change them. On the server they are the fields of the head as it came, in their order, the
names as written. The reference is valid while the request is.

## Parameters

None.

## Return value

A reference to the fields.

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
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        for (auto [name, value] : req.headers()) {
            if (name.starts_with("X-")) {
                w.write(name + ": " + value + "\n");
            }
        }
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::request req("GET", base + "/");
    req.headers().add("X-Tag", "a").add("X-Tag", "b").set("X-Trace", "1");
    net::http::client web;
    print("{}", web.send(req)->text().value());
    srv.close();
}
```

Output:

```text
X-Tag: a
X-Tag: b
X-Trace: 1
```

## See also

- [header](header.md): the first value of a name
- [sgcl::net::http::headers](../headers/README.md)
- [sgcl::net::http::request](README.md)
