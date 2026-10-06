[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::contains

```cpp
bool contains(const string& key) const noexcept;
```

Whether the session has a value of `key`, an empty one included.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the name |

## Return value

`true` when it has.

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
        s.set("empty", "");
        w.write(to_string(s.contains("empty")) + " " + to_string(s.contains("none")));
    });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/"));
    println("{}", rec.body());
}
```

Output:

```text
true false
```

## See also

- [get](get.md)
- [session](README.md)
