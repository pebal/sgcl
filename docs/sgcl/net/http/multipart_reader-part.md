[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [multipart_reader](multipart_reader/README.md) › part

# sgcl::net::http::multipart_reader::part

```cpp
#include "sgcl/net/http/multipart.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class multipart_reader {
    public:
        struct part {
            string name;
            string filename;
            string content_type;
            http::headers headers;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::multipart_reader::part` is what a part's head says, as [next](multipart_reader/next.md) gives it:
Go's `Part` with its `FormName`, `FileName` and `Header`. Its content is read from the reader. A value: its strings
and fields are slices of the head, copied with it.

## Member objects

| Member | Description |
|---|---|
| `name` | `Content-Disposition`'s `name`, its quoted-string's escapes taken out; empty when there is none |
| `filename` | `Content-Disposition`'s `filename*` (RFC 8187, UTF-8 or US-ASCII) or else `filename`, its last element: what follows the last `/` or `\`, none for `.` and `..`; empty for a field |
| `content_type` | `Content-Type` as sent; empty when there is none (`text/plain` by RFC 7578) |
| `headers` | every field of the head, in its order |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    string body = "--b\r\n"
                  "Content-Disposition: form-data; name=\"q\\\"x\"; filename=\"C:\\\\Users\\\\ann\\\\cv.pdf\"\r\n"
                  "Content-Type: application/pdf\r\n"
                  "X-Note: kept\r\n"
                  "\r\n"
                  "%PDF\r\n"
                  "--b--\r\n";
    net::http::multipart_reader parts(io::reader(make_tracked<io::buffer>(body)), "b");
    net::http::multipart_reader::part p = parts.next().value().value();
    println("{} {} {} {}", p.name, p.filename, p.content_type, p.headers.get("X-Note"));
}
```

Output:

```text
q"x cv.pdf application/pdf kept
```

## See also

- [next, async_next](multipart_reader/next.md): what gives it
- [headers](headers/README.md)
- [multipart_reader](multipart_reader/README.md)
