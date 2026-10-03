[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [client](../client.md)

# sgcl::net::http::client::download, async_download

```cpp
/*(1)*/ expected<response, io::error> download(const string& url, const string& path) const;
/*(2)*/ async::task<expected<response, io::error>> async_download(string url,
                                                                  string path) const noexcept;
```

Saves the file at `url` to `path`, curl's `-fo path url`: a [get](get.md) whose body is streamed into
`path + ".part"`, renamed over `path` at its end, so that a download cut in the middle leaves no file and the one
there before untouched. A status other than 2xx is an error and writes nothing: the body is given up and no file is
made. The client's pool and settings serve it; the free [download](../download.md) does the same through a client of
the process's.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the URL, `http://` or `https://` |
| `path` | the file to write; `path + ".part"` is written first |

## Return value

The response, its body read and saved. Or the error: that of [send](send.md); `net::errc::http_status` for a status
other than 2xx, the status and its phrase named
(`GET http://127.0.0.1:8080/a.zip (404 Not Found): the response's status is not 2xx`); the error of the file
(creating the part, writing it, renaming it) or of the body's reading, the part removed.

## Complexity

That of [send](send.md), and linear in the size of the body.

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
    net::http::response file = web.download(base + "/license.txt", "LICENSE.txt");
    println("{}, {} bytes on disk", file.status(), io::stat("LICENSE.txt")->size);

    auto missing = web.download(base + "/missing.zip", "missing.zip");
    println("{} {}", missing.error().code() == net::errc::http_status, io::exists("missing.zip"));
    srv.close();
}
```

Output:

```text
200, 2800 bytes on disk
true false
```

## See also

- [download](../download.md): the same through a client of the process's
- [save](../response/save.md): the body of any response into a file
- [sgcl::net::http::client](../client.md)
