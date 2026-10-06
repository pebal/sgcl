[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::move, async_move

```cpp
expected<void, io::error> move(const string& from, const string& to, bool overwrite = true) const;                  // (1)
async::task<expected<void, io::error>> async_move(string from, string to, bool overwrite = true) const noexcept;    // (2)
```

A file or a collection moved or renamed (MOVE).

`move` waits on the calling thread; a task awaits `async_move`.

## Parameters

| Parameter | Description |
|---|---|
| `from` | the resource |
| `to` | its new path |
| `overwrite` | a destination there replaced |


## Return value

Nothing; `ENOENT`, `EEXIST`, `EBUSY`.

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
    c.move("/hello.txt", "/docs/greeting.txt").value();
    println("{}", c.read("/docs/greeting.txt").value());
    srv.close();
    serving.wait();
}
```

Output:

```text
hello, world
```

## See also

- [copy](copy.md)
- [client](README.md)
