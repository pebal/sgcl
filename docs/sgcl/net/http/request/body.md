[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::body

```cpp
io::reader body() const noexcept;
```

Returns the body of a received request as a [stream](../../../io/reader/README.md), Go's `r.Body`, for a body read in parts as
it comes: an upload copied to a file, lines read one at a time through a
[buffered_reader](../../../io/buffered_reader/README.md). Every read is bounded by the server's `max_body_bytes`, as for
[text](text.md), and the end of the stream is the end of the body. The stream has no close of its own
([has_close](../../../io/reader/has_close.md) is `false`): what a handler leaves unread is read for it after it
returns, up to 256 KB, and past that the connection is closed. A request without a body gives an empty stream, one for
the program, at its end at once. The stream and [text](text.md) read the same body: what one took, the other does not
see.

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
    srv.route("POST /lines", [](net::http::request req, net::http::response_writer w)
                                 -> async::task<> {
        io::buffered_reader lines(req.body());
        int count = 0;
        while (true) {
            auto line = co_await lines.async_read_line();
            if (!line || !*line) {
                break;
            }
            ++count;
        }
        w.write(to_string(count) + " lines\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    print("{}", web.post(base + "/lines", "text/plain", "buy milk\ncall Ann\nwater plants\n")
                    ->text().value());
    srv.close();
}
```

Output:

```text
3 lines
```

## See also

- [text](text.md), [bytes](bytes.md): the whole body at once
- [sgcl::net::http::request](README.md)
