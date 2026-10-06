[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::remove, async_remove

```cpp
expected<void, io::error> remove(const string& path) const;                         // (1)
async::task<expected<void, io::error>> async_remove(string path) const noexcept;    // (2)
```

A file or a whole collection removed (DELETE).

`remove` waits on the calling thread; a task awaits `async_remove`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the resource |


## Return value

Nothing; `ENOENT`, `EBUSY`.

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
    c.remove("/hello.txt").value();
    println("{}", c.stat("/hello.txt").error().code() == std::errc::no_such_file_or_directory);
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [mkdir](mkdir.md)
- [client](README.md)
