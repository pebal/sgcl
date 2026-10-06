[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [server](README.md)

# sgcl::net::webdav::server::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a server.

## Parameters

None.

## Return value

Whether it does.

## Complexity

Constant.

## Exceptions

None.

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
    net::webdav::server none;
    println("{} {}", bool(none), bool(dav));
    srv.close();
    serving.wait();
}
```

Output:

```text
false true
```

## See also

- [(constructor)](server.md)
- [server](README.md)
