[sgcl](../../README.md) › [net](../README.md) › [webdav](README.md)

# sgcl::net::webdav::client_options

```cpp
#include "sgcl/net/webdav/client.h"   // or "sgcl/net/webdav.h"

namespace sgcl::net::webdav {
    struct client_options {
        string user;
        string password;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::webdav::client_options` is the credentials of a [client](client/README.md), sent as HTTP Basic.

## Member objects

| Member | Description |
|---|---|
| `user` | the user; empty by default: none, or the URL's |
| `password` | its password |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/webdav.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    io::mkdir_all("private");
    net::webdav::server dav("private", {.prefix = "/dav", .authorize = [](const net::http::request& r) {
                                            return r.header("Authorization") == "Basic YWxpY2U6c2VjcmV0";  // alice:secret
                                        }});
    net::http::server srv;
    srv.route("/dav/", [dav](net::http::request r, net::http::response_writer w) { return dav.async_serve(r, w); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("http://", l.local_endpoint().to_string(), "/dav/");
    net::webdav::client alice(url, {.user = "alice", .password = "secret"});
    net::webdav::client nobody(url);
    println("{} {}", bool(alice.list()), bool(nobody.list()));
    srv.close();
    serving.wait();
}
```

Output:

```text
true false
```

## See also

- [client](client/README.md)
