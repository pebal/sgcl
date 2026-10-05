[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::flushes

```cpp
int flushes() const noexcept;
```

Returns how many times the handler flushed, Go's `Flushed` as a count: a test of a stream checks that its parts
went out as they were made.

## Parameters

None.

## Return value

The count of flushes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto events = [](net::http::request, net::http::response_writer w) -> async::task<> {
        for (int i : range(3)) {
            w.write("data: " + to_string(i) + "\n\n");
            co_await w.async_flush();
        }
    };
    net::http::response_recorder rec;
    events(net::http::test_request("GET", "/events"), rec.writer()).wait();
    println("{} flushes, {} bytes", rec.flushes(), rec.body().size());
}
```

Output:

```text
3 flushes, 27 bytes
```

## See also

- [response_writer::flush](../response_writer/flush.md)
- [sgcl::net::http::response_recorder](README.md)
