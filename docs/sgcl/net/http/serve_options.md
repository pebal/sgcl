[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::serve_options

```cpp
#include "sgcl/net/http/serve.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    struct serve_options {
        etag_kind etag = etag_kind::weak;
        bool ranges = true;
    };
}
```

`sgcl::net::http::serve_options` is how files and content are answered by [serve](serve.md),
[file_server](file_server/README.md), [serve_file](serve_file.md) and [serve_content](serve_content.md): a plain
struct, its fields set by name (`{.etag = net::http::etag_kind::strong}`). The validators and the ranges themselves
are RFC 9110's whatever the options say; these choose what the server makes of its own.

## Member objects

| Member | Description |
|---|---|
| `etag` | the `ETag` made for a representation the handler gave none ([etag_kind](etag_kind.md)): `weak` by default, of a file's size and time, nothing read |
| `ranges` | whether `Range` is answered: 206 and `multipart/byteranges`, `Accept-Ranges: bytes`; `false` sends the whole body every time with `Accept-Ranges: none`. `true` by default |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::serve_options o;
    o.ranges = false;
    auto req = net::http::test_request("GET", "/a.txt");
    req.set_header("Range", "bytes=0-1");
    net::http::response_recorder rec;
    net::http::serve_content(req, rec.writer(), "a.txt", string("abcdef"), o);
    println("{} {} {}", rec.status(), rec.header("Accept-Ranges"), rec.body());
}
```

Output:

```text
200 none abcdef
```

## See also

- [etag_kind](etag_kind.md): how the ETag is made
- [serve_file](serve_file.md): what a request gets
- [sgcl::net::http](README.md)
