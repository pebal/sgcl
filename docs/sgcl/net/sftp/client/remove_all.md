[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::remove_all, async_remove_all

```cpp
expected<void, io::error> remove_all(const string& path) const;                                // (1)
async::task<expected<void, io::error>> async_remove_all(const string& path) const noexcept;    // (2)
```

Removes `path` with everything under it, as [io::remove_all](../../../io/remove_all.md) does locally: the
directories listed and emptied from the bottom up, a symlink removed and not followed; a path that is not there is
no error.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path on the server |

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended; what was removed before an error stays removed.

## Complexity

Linear in the entries under the path, a round trip or more each.

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
    fs.mkdir_all("/build/obj");
    fs.write_file("/build/obj/a.o", "x");
    fs.remove_all("/build");
    println("{}", *fs.exists("/build"));
    srv.close();
}
```

Output:

```text
false
```

## See also

- [remove](remove.md)
- [mkdir_all](mkdir_all.md)
- [sgcl::net::sftp::client](README.md)
