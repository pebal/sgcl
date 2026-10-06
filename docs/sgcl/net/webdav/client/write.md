[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::write, async_write

```cpp
expected<void, io::error> write(const string& path, const string& data) const;                  // (1)
async::task<expected<void, io::error>> async_write(string path, string data) const noexcept;    // (2)
```

A file made or replaced with the bytes (PUT); its parent must exist.

`write` waits on the calling thread; a task awaits `async_write`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `data` | its bytes |


## Return value

Nothing; `ENOENT` for a parent that is not there, `EBUSY` for a file locked.

## Complexity

Linear in the data.

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
    c.write("/docs/report.txt", "Q3: fine").value();
    println("{}", c.read("/docs/report.txt").value());
    srv.close();
    serving.wait();
}
```

Output:

```text
Q3: fine
```

## See also

- [read](read.md)
- [client](README.md)
