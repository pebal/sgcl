[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [test_server](README.md)

# sgcl::net::http::test_server::requests

```cpp
vector<request> requests() const;
```

Returns the requests the server received, in the order they reached their routes, when the server was made with
[keep_requests](../test_server-options.md); none otherwise. Each is the request its handler got: its method, URL,
fields, and its body as far as the handler read it. They stay as long as the server does (memory a load test does
not want: the option is off by default).

## Parameters

None.

## Return value

A copy of the list, a handle per request.

## Complexity

Linear in the number of requests.

## Exceptions

`invalid_argument` for a moved-from server.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto nothing = [](net::http::request, net::http::response_writer) {};
    net::http::test_server ts(nothing, {.keep_requests = true});
    auto c = ts.client();
    net::http::request put("PUT", ts.url() + "/items/1");
    put.set_header("X-Trace", "abc");
    put.set_body("data");
    c.send(put);
    c.get(ts.url() + "/items");
    for (auto& r : ts.requests()) {
        println("{} {} {} {}", r.method(), r.url().path(), r.header("X-Trace"),
                r.content_length().value_or(0));
    }
}
```

Output:

```text
PUT /items/1 abc 4
GET /items  0
```

## See also

- [options](../test_server-options.md)
- [sgcl::net::http::test_server](README.md)
