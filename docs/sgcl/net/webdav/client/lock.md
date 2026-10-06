[sgcl](../../../README.md) › [net](../../README.md) › [webdav](../README.md) › [client](README.md)

# sgcl::net::webdav::client::lock, async_lock

```cpp
expected<string, io::error> lock(const string& path, duration timeout = std::chrono::hours(1)) const;                         // (1)
async::task<expected<string, io::error>> async_lock(string path, duration timeout = std::chrono::hours(1)) const noexcept;    // (2)
```

An exclusive write lock (LOCK, Depth 0) for the timeout: its token. Another client's change of the resource is
then refused (`EBUSY`); this client's needs the token given to [if_token](if_token.md).

`lock` waits on the calling thread; a task awaits `async_lock`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the resource |
| `timeout` | how long it holds; the server may give less |


## Return value

The token ("urn:uuid:..."); `EBUSY` for a resource locked already.

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
    string token = c.lock("/hello.txt", std::chrono::seconds(60)).value();
    net::webdav::client other(string::concat("http://", l.local_endpoint().to_string(), "/dav/"));
    println("{}", other.write("/hello.txt", "no").error().code() == std::errc::device_or_resource_busy);
    c.if_token(token);
    println("{}", bool(c.write("/hello.txt", "mine")));
    srv.close();
    serving.wait();
}
```

Output:

```text
true
true
```

## See also

- [unlock](unlock.md)
- [if_token](if_token.md)
- [client](README.md)
