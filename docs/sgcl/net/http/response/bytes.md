[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::bytes, async_bytes

```cpp
/*(1)*/ expected<vector<byte>, io::error> bytes() const;
/*(2)*/ async::task<expected<vector<byte>, io::error>> async_bytes() const noexcept;
```

Reads the whole body as bytes, Go's `io.ReadAll(resp.Body)`: into a vector of its declared length when it has one, or
gathered and copied once. The end of the body gives the connection back to the client's pool. The body is read once:
a second read gives what is left.

1. Blocks the calling thread: the reading runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

None.

## Return value

The body, or the error of its reading, as for [text](text.md).

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
    srv.route("GET /pixel", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "application/octet-stream");
        w.write(vector<byte>{byte(0xFF), byte(0x80), byte(0x00)});
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    vector<byte> pixel = web.get(base + "/pixel")->bytes();
    println("{} bytes: {} {} {}", pixel.size(), int(pixel[0]), int(pixel[1]), int(pixel[2]));
    srv.close();
}
```

Output:

```text
3 bytes: 255 128 0
```

## See also

- [text](text.md): the body as text
- [sgcl::net::http::response](../response.md)
