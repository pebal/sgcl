[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [server](README.md)

# sgcl::net::webdav::server::async_serve

```cpp
async::task<> async_serve(http::request r, http::response_writer w) const noexcept;
```

An HTTP request served: its method's work on the tree, its answer written — 207 multistatus for PROPFIND and
PROPPATCH, 201 for what was made, 204 for what was replaced or removed, 409 for a parent that is not there, 412 for a
destination there under `Overwrite: F`, 423 for a resource locked, 401 for a request the authorization refused.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the request |
| `w` | its response |


## Return value

A task that ends when the answer is written.

## Complexity

Linear in the bytes and the members a request touches.

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
    c.write("/docs/new.txt", "made over WebDAV");
    println("{}", io::read_text("files/docs/new.txt").value());
    srv.close();
    serving.wait();
}
```

Output:

```text
made over WebDAV
```

## See also

- [client](../client/README.md)
- [server](README.md)
