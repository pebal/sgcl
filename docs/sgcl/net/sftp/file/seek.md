[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [file](README.md)

# sgcl::net::sftp::file::seek

```cpp
expected<uint64_t, io::error> seek(int64_t offset, io::seek_from from = io::seek_from::begin) const;
```

Moves the position to `offset` from the beginning, the position or the end, as
[io::file::seek](../../../io/file/seek.md) does. The position is the client's: SFTP reads and writes at offsets,
so a seek sends nothing, but one from the end asks the server for the size.
[mixin::seeker](../../../io/mixin/seeker/README.md) adds `tell`, `size` and `rewind` over it.

## Parameters

| Parameter | Description |
|---|---|
| `offset` | the distance, negative toward the beginning |
| `from` | where it counts from ([io::seek_from](../../../io/seek_from.md)) |

## Return value

The new position. Or `EINVAL` for one before the beginning, or the error of the size asked for.

## Complexity

Constant; one round trip from the end.

## Exceptions

`std::system_error` when the size is asked for (`from` is `seek_from::end`), the wait starts the scheduler and a worker's thread cannot be started.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/sftp.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("files");  // the directory the server serves
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) {
        if (s.subsystem() == "sftp") {
            (void)net::sftp::serve(s, "files");
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::sftp::client fs = net::sftp::client::connect(l.local_endpoint().to_string(), o);
    fs.write_file("/data", "0123456789");
    net::sftp::file f = fs.open("/data");
    println("{}", f.seek(-3, io::seek_from::end).value());
    println("{}", f.read_all_text().value());
    srv.close();
}
```

Output:

```text
7
789
```

## See also

- [read](read.md)
- [mixin::seeker](../../../io/mixin/seeker/README.md)
- [sgcl::net::sftp::file](README.md)
