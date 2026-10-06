[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::client

```cpp
client() noexcept;                                                                 // (1)
explicit client(const string& url, const client_options& o = {});                  // (2)
client(const string& url, const http::client& h, const client_options& o = {});    // (3)
client(const client& other) noexcept;                                              // (4)
```

1. No client: `operator bool` is false; an operation on it is a contract violation.
2. A client of the URL ("https://files.example.com/dav/"), its user and password the credentials when the
   [options](../client_options.md) have none.
3. The same through the program's HTTP client (its TLS, proxy and timeouts).
4. The same client as `other`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the root's URL |
| `o` | the credentials |
| `h` | the HTTP client |
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
    net::webdav::client other(string::concat("http://", l.local_endpoint().to_string(), "/dav/"), net::http::client());
    println("{}", other.read("/hello.txt").value());
    srv.close();
    serving.wait();
}
```

Output:

```text
hello, world
```

## See also

- [list](list.md)
- [client](README.md)
