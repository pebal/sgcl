[sgcl](../../README.md) › [net](../README.md) › [webdav](README.md)

# sgcl::net::webdav::resource

```cpp
#include "sgcl/net/webdav/types.h"   // or "sgcl/net/webdav.h"

namespace sgcl::net::webdav {
    struct resource {
        string path;
        bool collection = false;
        uint64_t size = 0;
        optional<time::datetime> modified;
        string etag;
        string content_type;

        friend bool operator==(const resource&, const resource&) noexcept = default;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::webdav::resource` is a resource as a PROPFIND tells of it ([list](client/list.md), [stat](client/stat.md)).

## Member objects

| Member | Description |
|---|---|
| `path` | its path under the root, the href decoded: "/docs/report.pdf", "/docs/" for a collection |
| `collection` | a directory |
| `size` | getcontentlength; 0 for a collection |
| `modified` | getlastmodified |
| `etag` | getetag, its quotes kept |
| `content_type` | getcontenttype |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/webdav.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    // a directory served under /dav on the loopback
    io::mkdir_all("files/docs");
    io::write_file("files/hello.txt", "hello, world");
    net::webdav::server dav("files", {.prefix = "/dav"});
    net::http::server srv;
    srv.route("/dav/", [dav](net::http::request r, net::http::response_writer w) { return dav.async_serve(r, w); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::webdav::client c(string::concat("http://", l.local_endpoint().to_string(), "/dav/"));
    auto r = c.stat("/hello.txt").value();
    println("{} {} {} {}", r.path, r.size, r.content_type, r.modified.has_value());
    srv.close();
    serving.wait();
}
```

Output:

```text
/hello.txt 12 text/plain true
```

## See also

- [list](client/list.md)
