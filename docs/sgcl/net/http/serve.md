[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::serve, async_serve

```cpp
#include "sgcl/net/http/serve.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    expected<void, io::error> serve(const string& address, const string& directory,         // (1)
                                    const serve_options& o = {});
    async::task<expected<void, io::error>> async_serve(string address, string directory,    // (2)
                                                       serve_options o = {}) noexcept;
}
```

Serves the files of a directory over HTTP, Go's `http.ListenAndServe(address,
http.FileServer(http.Dir(directory)))`: a [server](server/README.md) of one route, `GET /{path...}`, whose handler is a
[file_server](file_server/README.md) of the directory, listening on `address`.

GET and HEAD of `/a/b.txt` send `directory/a/b.txt`. The name, the path value unescaped, goes through
[io::path::under](../../io/path/under.md): a name that would leave the directory (a `..`, or `..%2f` before it is
unescaped, which the URL's normalization does not see, since it resolves `..` segments and not what `%2f` makes) is
404, and nothing is opened. A directory is its `index.html`, with no listing, and the URL of a directory without its
slash is redirected (301) to the one with it; a file that is not there is 404, and so is a name that is not a regular
file (a FIFO, whose open would wait for a writer, a socket, a device). The Content-Type is given by the
extension, as Go's built-in table and `mime.types` give it (`application/octet-stream` for an unknown one: nothing is
sniffed).

Each file is answered as [serve_file](serve_file.md) answers it: `Last-Modified` the file's time, an `ETag` of the
kind the options ask (weak, of the file's size and time, by default), the conditional requests of RFC 9110 —
`If-Match` and `If-Unmodified-Since` (412), `If-None-Match` and `If-Modified-Since` (304) — and the ranges: `Range`
with `If-Range`, one range a 206 with its `Content-Range`, several a `multipart/byteranges` body, a range past the end
416. The file goes by `sendfile` ([response_writer::write](response_writer/write.md)), a range of it too.

1. Blocks the calling thread, a thread of the program's (main's).
2. The same for a task.

There is no handle to the server: it runs until the program ends. A server that is stopped, or serves more than
files, is a [server](server/README.md) of the program's with a [file_server](file_server/README.md) on a route of its
own.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address to listen on, `"host:port"`; no host is every address |
| `directory` | the directory whose files are served |
| `o` | the ETag and whether ranges are answered ([serve_options](serve_options.md)); a weak ETag and ranges by default |

## Return value

Never a value: the error of the listen (an address in use, an address that is not one), or of an accept.

## Complexity

A task for each connection, for as long as it lives.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("public");
    (void)io::write_file("public/hello.txt", "hello from a file\n");
    (void)io::write_file("secret.txt", "not for the web\n");
    auto served = net::http::serve(":8080", "public");
    println("{}", served.error().message());
}
```

A request to it:

```text
$ curl http://localhost:8080/hello.txt
hello from a file
$ curl -r 11- http://localhost:8080/hello.txt
a file
$ curl http://localhost:8080/none.txt
Not Found
$ curl http://localhost:8080/..%2fsecret.txt
Not Found
```

## See also

- [file_server](file_server/README.md): the same handler on a route of a server of the program's
- [serve_file](serve_file.md): one file answered by a handler
- [serve_options](serve_options.md): the ETag, the ranges
- [serve_tls](serve_tls.md): one handler over https in one call
- [server](server/README.md): routes of the program's
- [io::path::under](../../io/path/under.md): a name kept inside a directory
