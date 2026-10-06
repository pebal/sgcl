[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [server](README.md)

# sgcl::net::webdav::server::server

```cpp
server() noexcept;                                                    // (1)
explicit server(const string& root, const server_options& o = {});    // (2)
server(const server& other) noexcept;                                 // (3)
```

1. No server: `operator bool` is false; an operation on it is a contract violation.
2. A server of the directory, its paths resolved within it.
3. The same server as `other`.

## Parameters

| Parameter | Description |
|---|---|
| `root` | the directory served |
| `o` | the prefix, read only, the body's limit, the authorization |
| `other` | the handle copied |


## Return value

None.

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
    net::webdav::server same = dav;
    println("{} {}", bool(none), same == dav);
    srv.close();
    serving.wait();
}
```

Output:

```text
false true
```

## See also

- [async_serve](async_serve.md)
- [server](README.md)
