[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::writer

```cpp
response_writer writer() const noexcept;
```

Returns the writer of the recorder, to hand to a handler: a [response_writer](../response_writer/README.md) whose
[flush](../response_writer/flush.md) succeeds and sends nothing (the head kept as it is then, what was buffered
moved into the body), whose [send_informational](../response_writer/send_informational.md) keeps the response, and
whose end is the recorder's to read. Every call gives a handle of the same writer.

## Parameters

None.

## Return value

The writer.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto handler = [](net::http::request, net::http::response_writer w) -> async::task<> {
        w.write("part one, ");
        co_await w.async_flush();
        w.write("part two");
    };
    net::http::response_recorder rec;
    handler(net::http::test_request("GET", "/"), rec.writer()).wait();
    println("{} | {} | {}", rec.body(), rec.flushes(), rec.writer().header_sent());
}
```

Output:

```text
part one, part two | 1 | true
```

## See also

- [serve, async_serve](serve.md): a request through routes
- [sgcl::net::http::response_recorder](README.md)
