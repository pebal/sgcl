[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::mkdir, async_mkdir

```cpp
expected<void, io::error> mkdir(const string& path,                                              // (1)
                                io::permissions p = io::permissions(0777)) const;
async::task<expected<void, io::error>> async_mkdir(const string& path,                           // (2)
                                                   io::permissions p = io::permissions(0777))
    const noexcept;
```

Makes the directory `path` (`MKDIR`), as [io::mkdir](../../../io/mkdir.md) does locally: its parent must be
there, and one that is there already is an error.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the new directory's path on the server |
| `p` | its permissions, which the server's umask narrows |

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended; a directory that is there is `net::errc::sftp_failure` (SFTP version 3 has no status for it).

## Complexity

One round trip.

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
    println("{}", fs.mkdir("/new").has_value());
    println("{}", fs.mkdir("/new").error().code() == net::errc::sftp_failure);
    srv.close();
}
```

Output:

```text
true
true
```

## See also

- [mkdir_all](mkdir_all.md)
- [rmdir](rmdir.md)
- [sgcl::net::sftp::client](README.md)
