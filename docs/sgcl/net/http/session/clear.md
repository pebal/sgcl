[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [session](README.md)

# sgcl::net::http::session::clear

```cpp
void clear() noexcept;
```

Every value gone. The session goes on, with its identity: [destroy](destroy.md) ends it.

## Parameters

None.

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
        s.set("a", "1");
        s.set("b", "2");
        s.clear();
        w.write(to_string(s.contains("a") || s.contains("b")));
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

- [destroy](destroy.md)
- [session](README.md)
