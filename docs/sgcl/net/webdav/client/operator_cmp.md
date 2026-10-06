[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::operator== (sgcl::net::webdav::client)

```cpp
friend bool operator==(const client& a, const client& b) noexcept;
```

Whether two handles are the same client; `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they are the same client.

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
    net::webdav::client same = c;
    println("{}", same == c);
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [client](README.md)
