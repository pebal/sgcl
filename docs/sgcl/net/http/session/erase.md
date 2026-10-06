[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::erase

```cpp
void erase(const string& key);
```

The value of `key` gone; nothing happens when there is none.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the name |

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
        s.set("flash", "saved!");
        s.erase("flash");
        s.erase("never there");
        w.write(to_string(s.contains("flash")));
    });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/"));
    println("{}", rec.body());
}
```

Output:

```text
false
```

## See also

- [clear](clear.md)
- [session](README.md)
