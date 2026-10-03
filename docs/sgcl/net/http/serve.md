[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::serve, async_serve

```cpp
#include "sgcl/net/http/serve.h"   // or "sgcl/net/http.h"

expected<void, io::error> serve(const string& address, const string& directory);                  // (1)
async::task<expected<void, io::error>> async_serve(string address, string directory) noexcept;    // (2)
```

Serves the files of a directory over HTTP, Go's `http.ListenAndServe(address,
http.FileServer(http.Dir(directory)))`: a [server](server.md) of one route, `GET /{path...}`, listening on
`address`.

GET and HEAD of `/a/b.txt` send `directory/a/b.txt`. The name, the path value unescaped, goes through
[io::path::under](../../io/path/under.md): a name that would leave the directory (a `..`, or `..%2f` before it is
unescaped, which the URL's normalization does not see, since it resolves `..` segments and not what `%2f` makes) is
404, and nothing is opened. A directory is its `index.html`, with no listing, and the URL of a directory without its
slash is redirected (301) to the one with it; a file that is not there is 404, and so is a name that is not a regular
file (a FIFO, whose open would wait for a writer, a socket, a device). The Content-Type is given by the
extension, as Go's built-in table and `mime.types` give it (`application/octet-stream` for an unknown one: nothing is
sniffed); `Last-Modified` is the file's time, and an `If-Modified-Since` not before it is 304 with no body. The file
goes by `sendfile` ([response_writer::write](response_writer/write.md)).

1. Blocks the calling thread, a thread of the program's (main's).
2. The same for a task.

There is no handle to the server: it runs until the program ends. A server that is stopped, or serves more than
files, is a [server](server.md) of the program's whose handler does what this one does — the name through
`io::path::under` and a 404 for one that leaves the directory.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address to listen on, `"host:port"`; no host is every address |
| `directory` | the directory whose files are served |

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
$ curl http://localhost:8080/none.txt
Not Found
$ curl http://localhost:8080/..%2fsecret.txt
Not Found
```

## See also

- [serve_tls](serve_tls.md): one handler over https in one call
- [server](server.md): routes of the program's
- [io::path::under](../../io/path/under.md): a name kept inside a directory
