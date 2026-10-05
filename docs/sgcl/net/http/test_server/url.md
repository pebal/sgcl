[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [test_server](README.md)

# sgcl::net::http::test_server::url

```cpp
string url() const noexcept;
```

Returns the URL to ask the server at: `http://127.0.0.1:port`, or `https://127.0.0.1:port` with TLS, the port the
system chose. A path and a query are appended to it. An empty string for a moved-from server.

## Parameters

None.

## Return value

The URL, without a path.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server ts([](net::http::request r, net::http::response_writer w) {
        w.write(r.url().path() + "?" + r.query("q") + "\n");
    });
    print("{}", ts.client().get(ts.url() + "/search?q=sgcl")->text().value());
    println("{}", ts.url().starts_with("http://127.0.0.1:"));
}
```

Output:

```text
/search?sgcl
true
```

## See also

- [endpoint](endpoint.md): the address and the port as a value
- [sgcl::net::http::test_server](README.md)
