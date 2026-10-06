[sgcl](../../README.md) › [net](../README.md) › [webdav](README.md)

# sgcl::net::webdav::server_options

```cpp
#include "sgcl/net/webdav/server.h"   // or "sgcl/net/webdav.h"

namespace sgcl::net::webdav {
    struct server_options {
        string prefix;
        bool read_only = false;
        size_t max_body = size_t(1) << 30;
        function<bool(const http::request&)> authorize;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::webdav::server_options` is how a [server](server/README.md) serves its directory.

## Member objects

| Member | Description |
|---|---|
| `prefix` | the URL path the root is served under ("/dav"); empty by default: "/" |
| `read_only` | only OPTIONS, GET, HEAD and PROPFIND; the rest 403; false by default |
| `max_body` | the largest PUT taken; past it 413; 1 GB by default |
| `authorize` | each request checked (its Authorization); a refusal 401 with a Basic challenge; empty by default: every one |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/webdav.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    io::mkdir_all("public");
    io::write_file("public/readme.txt", "read me");
    net::webdav::server dav("public", {.prefix = "/pub", .read_only = true});
    net::http::server srv;
    srv.route("/pub/", [dav](net::http::request r, net::http::response_writer w) { return dav.async_serve(r, w); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::webdav::client c(string::concat("http://", l.local_endpoint().to_string(), "/pub/"));
    println("{}", c.read("/readme.txt").value());
    println("{}", c.write("/x.txt", "x").error().code() == std::errc::permission_denied);
    srv.close();
    serving.wait();
}
```

Output:

```text
read me
true
```

## See also

- [server](server/README.md)
