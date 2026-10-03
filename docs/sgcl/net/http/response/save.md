[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::save, async_save

```cpp
/*(1)*/ expected<uint64_t, io::error> save(const string& path) const;
/*(2)*/ async::task<expected<uint64_t, io::error>> async_save(string path) const noexcept;
```

Streams the body into the file at `path`, through `path + ".part"` renamed over `path` at its end, so that nothing
half-written is left: a body cut in the middle, or a write that fails, removes the part and leaves the file there
before untouched. Unlike [download](../client/download.md), it saves the body of any status, a 404's page as well.
The body goes in blocks of 32 KB, never whole in memory; its end gives the connection back to the client's pool.
The body is read once: what is saved is what is left of it, so a body read already saves an empty file.

1. Blocks the calling thread: the reading runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file to write; `path + ".part"` is written first |

## Return value

The number of bytes written, or the error: of the body's reading, as for [text](text.md), or of the file (creating the
part, writing it, closing it, renaming it).

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
    srv.route("GET /license.txt", [](net::http::request, net::http::response_writer w) {
        w.write(string("Apache License, Version 2.0\n").repeat(100));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    for (string name : {"license.txt", "missing.txt"}) {
        net::http::response res = web.get(base + "/" + name);
        uint64_t saved = res.save(name);
        println("{}: {}, {} bytes saved, {} on disk", name, res.status(), saved,
                io::stat(name)->size);
    }
    srv.close();
}
```

Output:

```text
license.txt: 200, 2800 bytes saved, 2800 on disk
missing.txt: 404, 10 bytes saved, 10 on disk
```

## See also

- [download](../client/download.md): a GET whose 2xx body goes to a file
- [body](body.md): the body as a stream
- [sgcl::net::http::response](../response.md)
