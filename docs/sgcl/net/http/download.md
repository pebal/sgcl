[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::download, async_download

```cpp
#include "sgcl/net/http/download.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    expected<response, io::error> download(const string& url, const string& path,                   // (1)
                                           const download_options& o = {});
    async::task<expected<response, io::error>> async_download(string url, string path,              // (2)
                                                              download_options o = {}) noexcept;
}
```

Saves the file at `url` to `path` in one line, curl's `-fo path url`: [client::download](client/download.md) through
a client the process makes at the first download, with the default settings (Go's `http.DefaultClient`), kept for the
rest of the program with its pool. The body is streamed into `path + ".part"`, renamed over `path` at its end, so that
a download cut in the middle leaves no file and the one there before untouched; a status other than 2xx is an error
and writes nothing. A program that needs timeouts, roots of its own or a `dial` makes a [client](client/README.md) and calls
its `download`.

A body broken off is continued from where it stopped: a request for the bytes after those the part has, with
`If-Range` and the validator of the response (a strong `ETag`, or `Last-Modified`), `o.retries` times (2 by default);
a 206 from that byte is appended, a 200 (the file changed) starts the part over. A response with no validator is never
continued. With `o.resume`, a part left by an earlier call is continued too, and a transfer that fails keeps its part
for the next one ([download_options](download_options.md)).

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the URL, `http://` or `https://` |
| `path` | the file to write; `path + ".part"` is written first (and `path + ".part.meta"` with `o.resume`) |
| `o` | how the download goes on after its transfer stopped ([download_options](download_options.md)) |

## Return value

The response, its body read and saved: the last of the exchanges, a 206 when the file was continued (a 416 whose
`Content-Range` is the part's size, when the part was the whole file already). Or the error: that of [client::send](client/send.md);
`net::errc::http_status` for a status other than 2xx, the status and its phrase named
(`GET http://127.0.0.1:8080/a.zip (404 Not Found): the response's status is not 2xx`); the error of the file or of the
body's reading, the part removed.

## Complexity

One exchange, one more for each continuation, and linear in the size of the body.

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
    srv.route("GET /notes.txt", [](net::http::request, net::http::response_writer w) {
        w.write("buy milk\ncall Ann\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    if (auto saved = net::http::download(base + "/notes.txt", "notes.txt"); !saved) {
        eprintln(saved.error().message());
        return 1;
    }
    print("{}", io::read_text("notes.txt").value());

    auto missing = net::http::download(base + "/gone.txt", "gone.txt");
    println("{}", missing.error().code().message());
    srv.close();
}
```

Output:

```text
buy milk
call Ann
the response's status is not 2xx
```

## See also

- [client::download](client/download.md): the same through a client of the program's
- [download_options](download_options.md): the continuations
- [response::save](response/save.md): the body of any response into a file
- [sgcl::net::http](README.md)
