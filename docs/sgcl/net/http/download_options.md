[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::download_options

```cpp
#include "sgcl/net/http/download.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    struct download_options {
        bool resume = false;
        int retries = 2;
    };
}
```

`sgcl::net::http::download_options` is how a [download](download.md) goes on after its transfer stopped: a plain
struct, its fields set by name (`{.resume = true}`). A download is continued by a request for the bytes after those it
has, `Range: bytes=<size>-`, with `If-Range` and the validator of the response it had them from — a strong `ETag`, or
else `Last-Modified` — so that a file changed meanwhile comes whole (a 200) instead of its end appended to the old
start. A response with neither is never continued: the download starts over.

## Member objects

| Member | Description |
|---|---|
| `resume` | a `path + ".part"` left by an earlier download of the same URL is continued, its URL and validator read from `path + ".part.meta"`; a transfer that fails keeps both for the next call (an error of a status never does). `false` by default: a failure leaves nothing |
| `retries` | a body broken off in this call is continued from where it stopped this many times, when the response had a validator; 2 by default, 0 for none |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server server([](net::http::request req, net::http::response_writer w) {
        w.set_header("ETag", "\"v1\"");
        net::http::serve_content(req, w, "data.txt", string("0123456789"));
    });
    // a part of an earlier download, and what it was a part of
    (void)io::write_file("data.txt.part", "01234");
    (void)io::write_file("data.txt.part.meta", server.url() + "/data.txt\n\"v1\"\n");
    auto saved = net::http::download(server.url() + "/data.txt", "data.txt", {.resume = true});
    println("{} {}", saved->status(), io::read_text("data.txt").value());
}
```

Output:

```text
206 0123456789
```

## See also

- [download](download.md)
- [client::download](client/download.md)
- [sgcl::net::http](README.md)
