[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [file](README.md)

# sgcl::net::sftp::file::write, async_write

```cpp
expected<size_t, io::error> write(const slice<const byte>& data) const;                                // (1)
async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const noexcept;    // (2)
```

Writes the whole of `data` at the position, which moves past it (or at the end, for a file opened with
`append`): in requests of the server's largest write, all in flight at once, so a large write costs about one
round trip. [mixin::writer](../../../io/mixin/writer/README.md) adds the overloads for a text and one byte.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write |

## Return value

`data.size()`. Or the server's status as an [io::error](../../../io/error/README.md) (`is_not_found()`, `is_permission()`, `ENOTSUP`, `net::errc::sftp_failure` with the server's message), `io::errc::closed` for a file closed or a session ended; the position is then where it was.

## Complexity

Linear in the size of `data`.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    net::sftp::file f = fs.create("/out.txt");
    f.write("first ");
    f.write("second");
    f.close();
    println("{}", fs.read_text("/out.txt").value());
    srv.close();
}
```

Output:

```text
first second
```

## See also

- [write_at](write_at.md)
- [read](read.md)
- [mixin::writer](../../../io/mixin/writer/README.md)
- [sgcl::net::sftp::file](README.md)
