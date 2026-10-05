[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::stat_fs, async_stat_fs

```cpp
expected<file_system_info, io::error> stat_fs(const string& path) const;                                // (1)
async::task<expected<file_system_info, io::error>> async_stat_fs(const string& path) const noexcept;    // (2)
```

What the file system that holds `path` says of itself (`statvfs@openssh.com`): its blocks, the free ones, the
files, the longest name, as a [file_system_info](../file_system_info.md).

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | a path on the server |

## Return value

The file system's counts. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended; `ENOTSUP` from a server without the extension.

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
    net::sftp::file_system_info info = fs.stat_fs("/");
    println("{} {}", info.block_size > 0, info.blocks_available <= info.blocks);
    srv.close();
}
```

Output:

```text
true true
```

## See also

- [file_system_info](../file_system_info.md)
- [has_extension](has_extension.md)
- [sgcl::net::sftp::client](README.md)
