[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::list, async_list

```cpp
expected<vector<resource>, io::error> list(const string& path = string("/")) const;                         // (1)
async::task<expected<vector<resource>, io::error>> async_list(string path = string("/")) const noexcept;    // (2)
```

The members of a collection (PROPFIND, Depth 1), the collection itself left out, as [resources](../resource.md).

`list` waits on the calling thread; a task awaits `async_list`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the collection |


## Return value

The members; `ENOENT`.

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
    auto members = c.list().value();
    for (auto& r : members) {
        println("{}", r.path);
    }
    srv.close();
    serving.wait();
}
```

Output:

```text
/docs/
/hello.txt
```

## See also

- [stat](stat.md)
- [client](README.md)
