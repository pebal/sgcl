[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::set

```cpp
void set(const string& key, const string& value);
```

The value of `key` set, in the place of one before it, or after the others. The session is saved with the head of the
response; set after the head has gone, it is not.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the name |
| `value` | the value |

## Return value

None.

## Complexity

Linear in the number of values.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::sessions::in_memory());
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        net::http::session s(req);
        s.set("step", "1");
        s.set("step", "2");
        w.write(s.get("step"));
    });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/"));
    println("{} {}", rec.body(), rec.header("Set-Cookie").empty() ? "no cookie" : "a cookie");
}
```

Output:

```text
2 a cookie
```

## See also

- [get](get.md)
- [erase](erase.md)
- [session](README.md)
