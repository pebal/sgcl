[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [handler](README.md)

# sgcl::net::http::handler::handler

```cpp
handler() noexcept;    // (1)
template<class H>
handler(H h);          // (2)
```

1. A handler that answers every request 404, as a server answers a request of no route.
2. A handler of the function `h`, `(request, response_writer) -> void` for one that never waits, or
   `(request, response_writer) -> async::task<>` for one that does; any other function does not take part. Not
   explicit: a function converts where a handler is asked for, as in a middleware's `wrap`.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the function |

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) What the move of `h` throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::handler none;
    net::http::handler later = [](net::http::request req,
                                  net::http::response_writer w) -> async::task<> {
        auto body = co_await req.async_text();
        w.write("got " + *body);
    };
    net::http::response_recorder a, b;
    none.task(net::http::test_request("GET", "/"), a.writer()).wait();
    later.task(net::http::test_request("POST", "/", "x"), b.writer()).wait();
    println("{} {}", a.status(), b.body());
}
```

Output:

```text
404 got x
```

## See also

- [handler](README.md)
