[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::mkdir_all, async_mkdir_all

```cpp
expected<void, io::error> mkdir_all(const string& path,                                              // (1)
                                    io::permissions p = io::permissions(0777)) const;
async::task<expected<void, io::error>> async_mkdir_all(const string& path,                           // (2)
                                                       io::permissions p = io::permissions(0777))
    const noexcept;
```

Makes the directory `path` with every parent that is not there, as [io::mkdir_all](../../../io/mkdir_all.md)
does locally; a directory that is there is no error.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the directory's path on the server |
| `p` | the permissions of each directory made |

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended; a file in the way is `net::errc::sftp_failure`.

## Complexity

Two round trips per element of the path.

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
    fs.mkdir_all("/a/b/c");
    fs.mkdir_all("/a/b/c");
    println("{}", fs.stat("/a/b/c")->is_directory());
    srv.close();
}
```

Output:

```text
true
```

## See also

- [mkdir](mkdir.md)
- [remove_all](remove_all.md)
- [sgcl::net::sftp::client](README.md)
