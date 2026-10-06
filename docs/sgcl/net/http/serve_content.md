[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::serve_content

```cpp
#include "sgcl/net/http/serve.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    void serve_content(const request& req, const response_writer& w, const string& name,    // (1)
                       const io::file& content, const serve_options& o = {});
    void serve_content(const request& req, const response_writer& w, const string& name,    // (2)
                       const slice<const byte>& content, const serve_options& o = {});
}
```

Answers a request with content the handler has, Go's `http.ServeContent`: the validators, the conditional requests
and the ranges of RFC 9110, as [serve_file](serve_file.md) lists them, over an open file or bytes in memory.

1. An open regular file: its length and its time from `fstat`, its bytes from its start whatever its position (read
   by `pread`, sent by `sendfile` over TCP). A file that is not a regular one (a pipe has no length to range over) is
   500.
2. Bytes in memory (a `string` converts to them), copied into the response. They have no time but the one the
   handler gives.

- (1–2) `name` gives the `Content-Type` by its extension, when the writer has none; it is not sent. An `ETag` or a
  `Last-Modified` the handler set on the writer first is the validator the conditions and `If-Range` go by (Go reads
  only the handler's `ETag`); without one, the options' `ETag` is made: of the file's size and time, or of the
  bytes' digest (weak, XXH3-64; strong, XXH3-128).

## Parameters

| Parameter | Description |
|---|---|
| `req` | the request answered |
| `w` | its writer; an `ETag`, a `Last-Modified` or a `Content-Type` set on it before is the one used |
| `name` | a name whose extension gives the `Content-Type` |
| `content` | the file, or the bytes |
| `o` | the ETag made and whether ranges are answered ([serve_options](serve_options.md)) |

## Return value

None.

## Complexity

- (1) Constant but for the body; a strong ETag reads the file once while it stays as it is.
- (2) Linear in the size of the content (its digest, its copy).

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto handler = [](net::http::request req, net::http::response_writer w) {
        w.set_header("ETag", "\"v2\"");
        net::http::serve_content(req, w, "notes.txt", string("buy milk, call Ann"));
    };
    net::http::response_recorder whole, part, same;
    handler(net::http::test_request("GET", "/notes"), whole.writer());
    println("{} {} {}", whole.status(), whole.header("Content-Type"), whole.body());

    auto ranged = net::http::test_request("GET", "/notes");
    ranged.set_header("Range", "bytes=10-");
    handler(ranged, part.writer());
    println("{} {} {}", part.status(), part.header("Content-Range"), part.body());

    auto cached = net::http::test_request("GET", "/notes");
    cached.set_header("If-None-Match", "\"v2\"");
    handler(cached, same.writer());
    println("{} [{}]", same.status(), same.body());
}
```

Output:

```text
200 text/plain; charset=utf-8 buy milk, call Ann
206 bytes 10-17/18 call Ann
304 []
```

## See also

- [serve_file](serve_file.md): a file by its path
- [serve_options](serve_options.md): the ETag, the ranges
- [response_writer](response_writer/README.md)
- [sgcl::net::http](README.md)
