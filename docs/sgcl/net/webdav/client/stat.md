[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::stat, async_stat

```cpp
expected<resource, io::error> stat(const string& path) const;                         // (1)
async::task<expected<resource, io::error>> async_stat(string path) const noexcept;    // (2)
```

A resource's properties (PROPFIND, Depth 0).

`stat` waits on the calling thread; a task awaits `async_stat`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the resource |


## Return value

The [resource](../resource.md); `ENOENT`.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    println("{} {} {}", r.size, r.content_type, r.collection);
    srv.close();
    serving.wait();
}
```

Output:

```text
12 text/plain false
```

## See also

- [list](list.md)
- [client](README.md)
