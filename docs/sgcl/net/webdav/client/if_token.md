[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::if_token

```cpp
void if_token(const string& token) const;
```

The token of a lock the calls that change a resource present from now on (the If header); empty: none.

## Parameters

| Parameter | Description |
|---|---|
| `token` | a lock's token, from [lock](lock.md) |


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
    string token = c.lock("/hello.txt").value();
    c.if_token(token);
    c.write("/hello.txt", "changed while locked").value();
    c.if_token("");
    println("{}", c.read("/hello.txt").value());
    srv.close();
    serving.wait();
}
```

Output:

```text
changed while locked
```

## See also

- [lock](lock.md)
- [client](README.md)
