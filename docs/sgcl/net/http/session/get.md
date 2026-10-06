[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::get

```cpp
string get(const string& key) const noexcept;
```

The value of `key`, or `""` when there is none ([contains](contains.md) tells the two apart).

## Parameters

| Parameter | Description |
|---|---|
| `key` | the name |

## Return value

The value, or `""`.

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
        s.set("lang", "pl");
        w.write("[" + s.get("lang") + "][" + s.get("none") + "]");
    });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/"));
    println("{}", rec.body());
}
```

Output:

```text
[pl][]
```

## See also

- [set](set.md)
- [session](README.md)
