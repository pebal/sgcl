[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::serve_file

```cpp
#include "sgcl/net/http/serve.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    void serve_file(const request& req, const response_writer& w, const string& path,
                    const serve_options& o = {});
}
```

Answers a request with the file at `path`, Go's `http.ServeFile`: what a handler of the program's calls to send one
file with everything RFC 9110 asks of a server that has validators and ranges. The answer is
[serve_content](serve_content.md)'s for the file opened: its length and its time from `fstat`, `Last-Modified` the
time, an `ETag` of the kind the options ask unless the writer has one, `Content-Type` by the extension unless the
writer has one, and then, in the RFC's order:

- `If-Match` that no tag matches by the strong comparison, or else `If-Unmodified-Since` before the file's time: 412.
- `If-None-Match` that a tag matches by the weak comparison (or `*`): 304 to a GET or a HEAD, 412 to another method;
  or else, to a GET or a HEAD, `If-Modified-Since` not before the file's time: 304. A 304 carries the validators and
  no `Content-Type`.
- `Range` of a GET or a HEAD, in bytes, answered when its `If-Range` allows it (the tag by the strong comparison, or a
  date that is the file's time): one range a 206 with `Content-Range` and the bytes by `sendfile`, several a
  `multipart/byteranges` body, each part with its `Content-Type` and `Content-Range`. A range that starts past the
  end is dropped, and none left is 416 with `Content-Range: bytes */size`; a range that is not one (`4-2`, letters)
  is 416; another unit, ranges that overlap to more than the file, or more than 100 of them get the whole file.

The path is used as it is given: a name that comes from a request goes through
[io::path::under](../../io/path/under.md) first, as [file_server](file_server/README.md) does. A path that is not
there, or not a regular file (a directory, a FIFO), is 404, and nothing is opened before the check, since the open of
a FIFO waits for a writer.

## Parameters

| Parameter | Description |
|---|---|
| `req` | the request answered |
| `w` | its writer; an `ETag`, a `Last-Modified` or a `Content-Type` set on it before is the one used |
| `path` | the file |
| `o` | the ETag made and whether ranges are answered ([serve_options](serve_options.md)) |

## Return value

None.

## Complexity

Constant but for the body, which goes by `sendfile` (over TLS and HTTP/2 it is read in blocks); a strong ETag reads
the file once, after which its digest is kept while the file stays as it is.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    (void)io::write_file("report.txt", "the report of the day\n");
    net::http::server srv;
    srv.route("GET /report", [](net::http::request req, net::http::response_writer w) {
        net::http::serve_file(req, w, "report.txt");
    });
    auto served = srv.serve(":8080");
    println("{}", served.error().message());
}
```

A request to it:

```text
$ curl http://localhost:8080/report
the report of the day
$ curl -r 14- http://localhost:8080/report
the day
$ curl -s -o /dev/null -w '%{http_code}\n' -r 100- http://localhost:8080/report
416
```

## See also

- [serve_content](serve_content.md): an open file or bytes in memory, answered the same way
- [file_server](file_server/README.md): the files of a directory on a route
- [serve_options](serve_options.md): the ETag, the ranges
- [sgcl::net::http](README.md)
