[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](README.md)

# sgcl::net::http::response_writer::send_informational, async_send_informational

```cpp
expected<void, io::error> send_informational(int code, const http::headers& fields = {}) const;             // (1)
async::task<expected<void, io::error>> async_send_informational(int code,                                   // (2)
                                                                const http::headers& fields = {}) const;
```

Sends an informational response now, before the head of the final one: 103 Early Hints (RFC 8297) with its `Link`
fields, so that a browser starts to load what the page will need while the handler is still making it, or any other
1xx. Go's `WriteHeader(103)`. Over HTTP/1.1 it is a head of its own on the connection, over HTTP/2 a HEADERS without
END_STREAM; the fields of a connection (Connection, Keep-Alive, Transfer-Encoding, Upgrade) are left out of it. A
client of HTTP/1.0 gets none (RFC 9110 §15.2: it does not know 1xx): the call succeeds and sends nothing. Several may
be sent, each before the final head.

1. Blocks the calling thread until the bytes are sent: for a response written from one of the program's threads,
   never from a worker.
2. The same for a task: a handler writes `co_await w.async_send_informational(103, hints)`.

## Parameters

| Parameter | Description |
|---|---|
| `code` | the status, 102 to 199: 101 is [hijack](hijack.md)'s, 100 the server's own (sent when a handler reads a body that asked for it) |
| `fields` | the fields of the informational response; none by default |

## Return value

Nothing, or the error:

- `io::errc::closed` after the head of the final response has gone (a [flush](flush.md)), after the response has
  ended, or after a [hijack](hijack.md);
- `std::errc::invalid_argument` for a field whose name is not a token or whose value holds CR, LF or another
  control, nothing sent;
- the connection's error when the client went away.

## Complexity

Linear in the fields.

## Exceptions

`invalid_argument` for a code outside 102 to 199.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto handler = [](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::headers hints;
        hints.add("Link", "</style.css>; rel=preload; as=style");
        co_await w.async_send_informational(103, hints);
        w.write("<html>...</html>");
    };
    net::http::test_server ts(handler);
    // the bytes a client reads: the 103's head, then the response's
    net::connection c = net::tcp::connect(ts.endpoint());
    c.write("GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n");
    string answer = c.read_all_text();
    for (auto line : answer.split("\r\n")) {
        if (line.starts_with("HTTP/") || line.starts_with("Link")) {
            println("{}", line);
        }
    }
}
```

Output:

```text
HTTP/1.1 103 Early Hints
Link: </style.css>; rel=preload; as=style
HTTP/1.1 200 OK
```

## See also

- [set_status](set_status.md): the status of the final response
- [hijack](hijack.md): a switch of protocols (101)
- [reverse_proxy](../reverse_proxy/README.md): passes a backend's 103 on
- [sgcl::net::http::response_writer](README.md)
