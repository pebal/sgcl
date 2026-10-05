[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::set_modified, async_set_modified

```cpp
expected<void, io::error> set_modified(const string& path, io::file_time t) const;            // (1)
async::task<expected<void, io::error>> async_set_modified(const string& path,                 // (2)
                                                          io::file_time t) const noexcept;
```

Sets the time of the last modification of the file at `path`, as
[io::set_modified](../../../io/set_modified.md) does locally. SFTP version 3 sends both times together, in whole
seconds: the access time is read first and sent again.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path on the server |
| `t` | the new time; the fraction of a second is dropped |

## Return value

Nothing. Or the server's status as an [io::error](../../../io/error/README.md): `is_not_found()` for no such file, `is_permission()` for a refusal, `ENOTSUP` for an operation the server does not have, `net::errc::sftp_failure` for any other failure (the server's message after the path), `io::errc::closed` once the session ended.

## Complexity

Two round trips.

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
    fs.write_file("/old.txt", "x");
    fs.set_modified("/old.txt", io::file_time(std::chrono::seconds(1700000000)));
    auto since = fs.stat("/old.txt")->modified.time_since_epoch();
    println("{}", std::chrono::duration_cast<std::chrono::seconds>(since).count());
    srv.close();
}
```

Output:

```text
1700000000
```

## See also

- [set_stat](set_stat.md)
- [sgcl::net::sftp::client](README.md)
