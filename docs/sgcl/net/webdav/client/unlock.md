[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::unlock, async_unlock

```cpp
expected<void, io::error> unlock(const string& path, const string& token) const;                  // (1)
async::task<expected<void, io::error>> async_unlock(string path, string token) const noexcept;    // (2)
```

The lock of the token taken off (UNLOCK).

`unlock` waits on the calling thread; a task awaits `async_unlock`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the resource |
| `token` | the lock's token |


## Return value

Nothing; an error for a token the resource has no lock of.

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
    string token = c.lock("/hello.txt").value();
    println("{}", bool(c.unlock("/hello.txt", token)));
    println("{}", bool(c.unlock("/hello.txt", token)));
    srv.close();
    serving.wait();
}
```

Output:

```text
true
false
```

## See also

- [lock](lock.md)
- [client](README.md)
