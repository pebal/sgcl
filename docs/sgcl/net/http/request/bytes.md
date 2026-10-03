[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::bytes, async_bytes

```cpp
expected<vector<byte>, io::error> bytes() const;                                // (1)
async::task<expected<vector<byte>, io::error>> async_bytes() const noexcept;    // (2)
```

Reads the whole body of a received request as bytes, Go's `io.ReadAll(r.Body)`: into a vector of its declared length
when it has one, or gathered and copied once. The read is bounded by the server's `max_body_bytes`, as for
[text](text.md). The body is read once: a second read gives what is left. A request without a body gives an empty
vector.

1. Blocks the calling thread: the reading runs on the scheduler and the thread waits for it. For code on a thread of
   its own, never a handler, which runs on a worker.
2. Returns a task that does the same: a handler writes `co_await req.async_bytes()`.

## Parameters

None.

## Return value

The body, or the [error](../../../io/error.md) of its reading: `net::errc::body_too_large`,
`io::errc::unexpected_eof` for a body cut short, or the error of the connection.

## Complexity

Linear in the size of the body.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("PUT /blob", [](net::http::request req, net::http::response_writer w)
                               -> async::task<> {
        vector<byte> data = co_await req.async_bytes();
        int sum = 0;
        for (byte b : data) {
            sum += int(b);
        }
        w.write(to_string(data.size()) + " bytes, sum " + to_string(sum) + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::request put("PUT", base + "/blob");
    put.set_body(vector<byte>{byte(1), byte(2), byte(250)});
    net::http::client web;
    print("{}", web.send(put)->text().value());
    srv.close();
}
```

Output:

```text
3 bytes, sum 253
```

## See also

- [text](text.md): the body as text; [body](body.md): the body as a stream
- [sgcl::net::http::request](../request.md)
