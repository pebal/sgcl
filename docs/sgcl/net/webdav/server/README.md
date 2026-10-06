[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md)

# sgcl::net::webdav::server

```cpp
#include "sgcl/net/webdav/server.h"   // or "sgcl/net/webdav.h"

namespace sgcl::net::webdav {
    class server;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::webdav::server` is a WebDAV server of a directory (RFC 4918, classes 1 and 2), an HTTP handler: a route
of an [http::server](../../http/server/README.md) hands it its requests ([async_serve](async_serve.md)), and it does
OPTIONS, GET, HEAD, PUT, DELETE, MKCOL, COPY, MOVE, PROPFIND, PROPPATCH, LOCK and UNLOCK on the tree.

## Rules

- A handle of one word: copies share the tree, its dead properties and its locks.
- The [options](../server_options.md)' prefix is the URL path the root is served under; the route mounts it there.
- The tree's work runs on the blocking pool, one request at a time against the tree.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](server.md) | a server of a directory |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another server |
| [async_serve](async_serve.md) | an HTTP request served |
| [operator bool](operator_bool.md) | whether the handle holds a server |
| [operator==](operator_cmp.md) | whether two handles are the same server |

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
    auto members = c.list("/").value();
    for (auto& r : members) {
        println("{}{}", r.path, r.collection ? " (collection)" : "");
    }
    srv.close();
    serving.wait();
}
```

Output:

```text
/docs/ (collection)
/hello.txt
```

## See also

- [client](../client/README.md)
- [server_options](../server_options.md)
