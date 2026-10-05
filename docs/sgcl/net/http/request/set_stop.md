[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::set_stop

```cpp
request& set_stop(const async::stop_token& token) noexcept;
```

Ties a request the program sends to a [stop token](../../../async/stop_token/README.md), Go's
`NewRequestWithContext`: a stop before the send makes it fail at once, and a stop while it waits for the head or reads
the body ends it, the connection closed (HTTP/1.1) or the stream reset (HTTP/2). The send then returns `ECANCELED`,
and a read of the body after the stop fails. The token is watched from the moment the connection is had until the
body has ended; a request the server received keeps its own token, [stop](stop.md).

## Parameters

| Parameter | Description |
|---|---|
| `token` | the token whose stop cancels the exchange |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The dial itself is bounded by the client's `connect_timeout`, not by the token: a stop while the connection is being
made is seen once it is made.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    auto handler = [](net::http::request, net::http::response_writer w) -> async::task<> {
        co_await async::sleep(2s);
        w.write("late\n");
    };
    net::http::test_server ts(handler);
    async::stop_source cancel;
    cancel.stop_after(100ms);
    net::http::request req("GET", ts.url());
    req.set_stop(cancel.token());
    auto r = ts.client().send(req);
    println("{}", r ? "answered" : r.error().code().message());
}
```

Output:

```text
Operation canceled
```

## See also

- [stop](stop.md): the token of a request the server received
- [client::send](../client/send.md): the send it cancels
- [sgcl::net::http::request](README.md)
