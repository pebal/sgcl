[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::body

```cpp
io::reader body() const noexcept;
```

Returns the body as a [stream](../../../io/reader.md), Go's `resp.Body`, for a body read in parts as it comes: lines
of a long answer through a [buffered_reader](../../../io/buffered_reader.md), a copy into a writer of the program's.
The end of the stream is the end of the body, and reaching it gives the connection back to the client's pool. The
stream has no close of its own ([has_close](../../../io/reader/has_close.md) is `false`, and its `close` does
nothing): a body given up before its end is given up by the response's [close](close.md), not by Go's
`resp.Body.Close()`. The stream and [text](text.md) read the same body: what one took, the other does not see.

## Parameters

None.

## Return value

The body as a stream.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /log", [](net::http::request, net::http::response_writer w) {
        w.write("09:00 start\n09:05 warning: disk\n09:10 stop\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/log");
    io::buffered_reader lines(res.body());
    for (;;) {
        auto line = lines.read_line();
        if (!line || !*line) {
            break;  // an error, or the end of the body
        }
        string text(**line);
        if (text.contains("warning")) {
            println("{}", text);
        }
    }
    println("{}", res.body().has_close());
    srv.close();
}
```

Output:

```text
09:05 warning: disk
false
```

## See also

- [text](text.md), [bytes](bytes.md): the whole body at once
- [close](close.md): the body given up
- [sgcl::net::http::response](../response.md)
