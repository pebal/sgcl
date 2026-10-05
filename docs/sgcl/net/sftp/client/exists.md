[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::exists, async_exists

```cpp
expected<bool, io::error> exists(const string& path) const;                                // (1)
async::task<expected<bool, io::error>> async_exists(const string& path) const noexcept;    // (2)
```

Whether there is something at `path`: a [stat](stat.md) that succeeds, `false` for no such file.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path on the server |

## Return value

Whether it is there. Or the [io::error](../../../io/error/README.md) of a stat that failed otherwise (a refusal, the session's end).

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
    fs.mkdir("/inbox");
    println("{} {}", *fs.exists("/inbox"), *fs.exists("/outbox"));
    srv.close();
}
```

Output:

```text
true false
```

## See also

- [stat](stat.md)
- [sgcl::net::sftp::client](README.md)
