[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [sessions](README.md)

# sgcl::net::http::sessions::size

```cpp
size_t size() const;
```

How many sessions the server's store of [in_memory](in_memory.md) holds now, those past their time and not yet swept
among them; 0 for [in_cookie](in_cookie.md), which keeps none.

## Parameters

None.

## Return value

The count.

## Complexity

Constant, under the store's mutex.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto store = net::http::sessions::in_memory();
    net::http::server srv;
    srv.use(store);
    srv.route("GET /visit", [](net::http::request req, net::http::response_writer w) {
        net::http::session(req).set("seen", "yes");
    });
    for (int i : range(3)) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request("GET", "/visit"));
    }
    println("{}", store.size());
}
```

Output:

```text
3
```

## See also

- [sessions](README.md)
