[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::etag_kind

```cpp
#include "sgcl/net/http/serve.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    enum class etag_kind : uint8_t {
        none,
        weak,
        strong,
    };
}
```

How [serve_file](serve_file.md), [serve_content](serve_content.md), [file_server](file_server/README.md) and
[serve](serve.md) make the `ETag` of a representation the handler gave none ([serve_options](serve_options.md)'s
`etag`). A weak tag (RFC 9110 §8.8.1) says two representations are as good as each other, a strong one that they are
the same bytes: `If-None-Match` takes either, while `If-Match` and `If-Range` take a strong tag alone, so a client
that resumes a download by its tag needs a strong one; with a weak one it resumes by `Last-Modified`.

| Value | Description |
|---|---|
| `none` | no `ETag`: the conditions and `If-Range` go by `Last-Modified` alone |
| `weak` | `W/"<size>-<time>"`, the file's size and its time to the nanosecond in hex, nothing read; of bytes in memory, their XXH3-64 digest. The default |
| `strong` | `"<digest>"`, the XXH3-128 digest of the bytes in hex: a file is read once, and its digest kept for the process while its device, inode, size and time stay as they are (4096 files at most, all dropped past that) |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    for (auto kind : {net::http::etag_kind::none, net::http::etag_kind::weak, net::http::etag_kind::strong}) {
        net::http::response_recorder rec;
        net::http::serve_content(net::http::test_request("GET", "/"), rec.writer(), "a.txt", string("abc"),
                                 {.etag = kind});
        println("[{}]", rec.header("ETag"));
    }
}
```

Output:

```text
[]
[W/"78af5f94892f3950"]
["06b05ab6733a618578af5f94892f3950"]
```

## See also

- [serve_options](serve_options.md)
- [sgcl::net::http](README.md)
