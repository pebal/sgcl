[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::headers

```cpp
http::headers headers() const noexcept;
```

Returns the fields of the response: as the head went at the first flush, so that a field set after it is not among
them (as a client would not see it), or the writer's fields as they stand when nothing was flushed.

## Parameters

None.

## Return value

A copy of the fields.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::response_recorder rec;
    auto handler = [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.set_header("X-Before", "1");
        co_await w.async_flush();
        w.set_header("X-After", "2");  // the head has gone
    };
    handler(net::http::test_request("GET", "/"), rec.writer()).wait();
    for (auto [name, value] : rec.headers()) {
        println("{}: {}", name, value);
    }
}
```

Output:

```text
X-Before: 1
```

## See also

- [header](header.md): one field
- [sgcl::net::http::response_recorder](README.md)
