[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::set_stat, async_set_stat

```cpp
expected<void, io::error> set_stat(const string& path, const attributes& a) const;            // (1)
async::task<expected<void, io::error>> async_set_stat(const string& path,                     // (2)
                                                      const attributes& a) const noexcept;
```

Changes the attributes of the file at `path` (`SETSTAT`, a symlink followed): what `a` holds is sent, the rest
left as it is — the size (truncated or extended), the owner, the mode, the times in whole seconds.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path on the server |
| `a` | the attributes to change ([attributes](../attributes.md)) |

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended.

## Complexity

One round trip; two when one time is set alone (the other read first).

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
    fs.write_file("/log.txt", "0123456789");
    net::sftp::attributes a;
    a.size = 4;
    a.mode = io::permissions(0600);
    fs.set_stat("/log.txt", a);
    net::sftp::file_info info = fs.stat("/log.txt");
    println("{} {:o}", info.size, unsigned(info.mode));
    srv.close();
}
```

Output:

```text
4 600
```

## See also

- [chmod](chmod.md)
- [set_modified](set_modified.md)
- [file::set_stat](../file/set_stat.md)
- [sgcl::net::sftp::client](README.md)
