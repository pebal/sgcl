[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::mkdir, async_mkdir

```cpp
expected<void, io::error> mkdir(const string& path) const;                         // (1)
async::task<expected<void, io::error>> async_mkdir(string path) const noexcept;    // (2)
```

A collection made (MKCOL); its parent must exist.

`mkdir` waits on the calling thread; a task awaits `async_mkdir`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the collection |


## Return value

Nothing; `EEXIST`, `ENOENT` for a parent that is not there.

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
    c.mkdir("/photos").value();
    println("{}", c.mkdir("/photos").error().code() == std::errc::file_exists);
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [remove](remove.md)
- [client](README.md)
